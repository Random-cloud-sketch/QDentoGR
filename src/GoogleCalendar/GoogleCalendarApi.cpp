#include "GoogleCalendarApi.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimeZone>
#include <QTimer>
#include <QUrlQuery>

#include "GoogleOAuth.h"
#include "GoogleCalendarLog.h"
#include "GlobalSettings.h"

GoogleEvent GoogleEvent::fromJson(const QJsonObject& json)
{
	GoogleEvent e;

	e.id = json["id"].toString();
	e.etag = json["etag"].toString();
	e.status = json["status"].toString();
	e.summary = json["summary"].toString();
	e.updated = json["updated"].toString();
	e.recurring = json.contains("recurrence") || json.contains("recurringEventId");

	auto start = json["start"].toObject();
	auto end = json["end"].toObject();

	//only events with a start and end time can be appointments (not all-day events)
	if (start.contains("dateTime") && end.contains("dateTime"))
	{
		e.start = QDateTime::fromString(start["dateTime"].toString(), Qt::ISODate).toLocalTime();
		e.end = QDateTime::fromString(end["dateTime"].toString(), Qt::ISODate).toLocalTime();

		//QDento keeps whole minutes
		e.start.setTime(QTime(e.start.time().hour(), e.start.time().minute()));
		e.end.setTime(QTime(e.end.time().hour(), e.end.time().minute()));

		e.timed = e.start.isValid() && e.end.isValid() && e.end > e.start;
	}

	auto properties = json["extendedProperties"].toObject()["private"].toObject();

	e.appointmentId = properties["qdento_appointment_id"].toString();
	e.databaseId = properties["qdento_database_id"].toString();

	return e;
}

GoogleCalendarApi::GoogleCalendarApi(GoogleOAuth* auth, QNetworkAccessManager* network, QObject* parent)
	: QObject(parent), m_auth(auth), m_network(network)
{
	connect(m_auth, &GoogleOAuth::refreshed, this, &GoogleCalendarApi::onRefreshed);
	connect(m_auth, &GoogleOAuth::refreshFailed, this, [this](bool needsReauthorization, const QString&) {
		onRefreshFailed(needsReauthorization);
	});

	//after a new sign-in the requests work again
	connect(m_auth, &GoogleOAuth::loggedIn, this, [this] { m_needsReauthorization = false; });
}

QUrl GoogleCalendarApi::baseUrl() const
{
	auto test = GlobalSettings::getGoogleCalendar().testApiUrl;
	return QUrl(test.size() ? QString::fromStdString(test) : QString("https://www.googleapis.com/calendar/v3"));
}

QString GoogleCalendarApi::encodeId(const QString& id) const
{
	return QString::fromUtf8(QUrl::toPercentEncoding(id));
}

void GoogleCalendarApi::listCalendars(const QString& pageToken, Callback done)
{
	QUrl url(baseUrl().toString() + "/users/me/calendarList");

	QUrlQuery query;
	query.addQueryItem("minAccessRole", "writer");
	if (pageToken.size()) query.addQueryItem("pageToken", pageToken);
	url.setQuery(query);

	send({ "GET", url, {}, {}, done });
}

void GoogleCalendarApi::listEvents(const QString& calendarId, const QString& syncToken, const QString& pageToken, Callback done)
{
	QUrl url(baseUrl().toString() + "/calendars/" + encodeId(calendarId) + "/events");

	QUrlQuery query;
	query.addQueryItem("maxResults", "2500");

	//with a sync token only the changes since the last synchronization are returned;
	//deleted events are returned too (as "cancelled"), also by a full synchronization
	if (syncToken.size()) query.addQueryItem("syncToken", syncToken);
	query.addQueryItem("showDeleted", "true");

	if (pageToken.size()) query.addQueryItem("pageToken", pageToken);

	url.setQuery(query);

	send({ "GET", url, {}, {}, done });
}

void GoogleCalendarApi::getEvent(const QString& calendarId, const QString& eventId, Callback done)
{
	send({ "GET", QUrl(baseUrl().toString() + "/calendars/" + encodeId(calendarId) + "/events/" + encodeId(eventId)), {}, {}, done });
}

void GoogleCalendarApi::insertEvent(const QString& calendarId, const QJsonObject& event, Callback done)
{
	send({ "POST", QUrl(baseUrl().toString() + "/calendars/" + encodeId(calendarId) + "/events"),
		QJsonDocument(event).toJson(QJsonDocument::Compact), {}, done });
}

void GoogleCalendarApi::patchEvent(const QString& calendarId, const QString& eventId, const QJsonObject& event, const QString& ifMatch, Callback done)
{
	send({ "PATCH", QUrl(baseUrl().toString() + "/calendars/" + encodeId(calendarId) + "/events/" + encodeId(eventId)),
		QJsonDocument(event).toJson(QJsonDocument::Compact), ifMatch, done });
}

void GoogleCalendarApi::deleteEvent(const QString& calendarId, const QString& eventId, Callback done)
{
	send({ "DELETE", QUrl(baseUrl().toString() + "/calendars/" + encodeId(calendarId) + "/events/" + encodeId(eventId)), {}, {}, done });
}

QJsonObject GoogleCalendarApi::eventBody(const QString& summary, const QDateTime& start, const QDateTime& end,
	long long appointmentRowid, const QString& databaseId)
{
	//local times with the time zone of the computer: Google applies the time zone rules (no fixed offsets)
	QString timeZone = QString::fromUtf8(QTimeZone::systemTimeZoneId());

	auto time = [&](const QDateTime& dt) {
		QJsonObject o;
		o["dateTime"] = dt.toString("yyyy-MM-ddTHH:mm:ss");
		if (timeZone.size()) o["timeZone"] = timeZone;
		return o;
	};

	QJsonObject properties;
	properties["qdento_appointment_id"] = QString::number(appointmentRowid);
	properties["qdento_database_id"] = databaseId;

	QJsonObject extended;
	extended["private"] = properties;

	QJsonObject event;
	event["summary"] = summary;
	event["description"] = "QDento appointment";
	event["start"] = time(start);
	event["end"] = time(end);
	event["extendedProperties"] = extended;

	return event;
}

QString GoogleCalendarApi::errorText(Error error)
{
	switch (error)
	{
		case Error::Network: return tr("Google Calendar could not be reached. Check the internet connection.");
		case Error::Authorization: return tr("The connection to Google Calendar needs to be authorized again.");
		case Error::Permission: return tr("There is no permission to change this Google calendar.");
		case Error::NotFound: return tr("The Google calendar or event was not found.");
		case Error::Gone: return tr("The Google calendar data changed; a full synchronization is needed.");
		case Error::Precondition: return tr("The event was changed in Google Calendar in the meantime.");
		case Error::RateLimit: return tr("Google Calendar is busy. The synchronization will be repeated later.");
		case Error::Server: return tr("Google Calendar is temporarily unavailable.");
		case Error::Malformed: return tr("Google Calendar returned unexpected data.");
		default: return tr("Google Calendar synchronization error.");
	}
}

void GoogleCalendarApi::send(Request request)
{
	if (m_needsReauthorization) {
		Result r;
		r.error = Error::Authorization;
		request.done(r);
		return;
	}

	//no access token yet (or it expired): a new one first
	if (m_auth->accessToken().isEmpty())
	{
		m_waiting.push_back(request);
		m_auth->refresh();
		return;
	}

	execute(request);
}

void GoogleCalendarApi::execute(Request request)
{
	QNetworkRequest net(request.url);
	net.setRawHeader("Authorization", "Bearer " + m_auth->accessToken().toUtf8());
	net.setRawHeader("Accept", "application/json");
	net.setTransferTimeout(30000);

	if (request.body.size()) net.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	if (request.ifMatch.size()) net.setRawHeader("If-Match", request.ifMatch.toUtf8());

	QNetworkReply* reply = nullptr;

	if (request.method == "GET") reply = m_network->get(net);
	else if (request.method == "POST") reply = m_network->post(net, request.body);
	else if (request.method == "DELETE") reply = m_network->deleteResource(net);
	else reply = m_network->sendCustomRequest(net, request.method, request.body);

	connect(reply, &QNetworkReply::finished, this, [this, reply, request] {

		int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

		//no HTTP answer at all: connection problem
		bool networkError = status == 0 && reply->error() != QNetworkReply::NoError;

		QByteArray data = reply->readAll();

		reply->deleteLater();

		finished(request, status, data, networkError);
	});
}

void GoogleCalendarApi::finished(Request request, int status, const QByteArray& data, bool networkError)
{
	Result result;
	result.httpStatus = status;

	QJsonParseError parseError;
	auto document = QJsonDocument::fromJson(data, &parseError);
	if (document.isObject()) result.json = document.object();

	QString reason = result.json["error"].toObject()["errors"].toArray().first().toObject()["reason"].toString();

	if (networkError) result.error = Error::Network;
	else if (status >= 200 && status < 300) {
		//an answer with content must be JSON
		if (data.size() && !document.isObject()) result.error = Error::Malformed;
	}
	else if (status == 401) result.error = Error::Authorization;
	else if (status == 403 && (reason == "rateLimitExceeded" || reason == "userRateLimitExceeded")) result.error = Error::RateLimit;
	else if (status == 403) result.error = Error::Permission;
	else if (status == 404) result.error = Error::NotFound;
	else if (status == 410) result.error = Error::Gone;
	else if (status == 412) result.error = Error::Precondition;
	else if (status == 429) result.error = Error::RateLimit;
	else if (status >= 500) result.error = Error::Server;
	else result.error = Error::Other;

	if (!result.ok()) {
		GoogleCalendarLog::write(QString("%1 %2 -> HTTP %3 %4")
			.arg(QString(request.method), request.url.path())
			.arg(status)
			.arg(reason.size() ? reason : result.json["error"].toObject()["message"].toString()));
	}

	//expired access token: a new one and the request once more
	if (result.error == Error::Authorization && !request.refreshed)
	{
		request.refreshed = true;
		m_waiting.push_back(request);
		m_auth->refresh();
		return;
	}

	//even a new access token is not accepted
	if (result.error == Error::Authorization && request.refreshed && !m_needsReauthorization) {
		m_needsReauthorization = true;
		emit reauthorizationNeeded();
	}

	//temporary problems: at most 3 more attempts, after 1, 2 and 4 seconds
	bool temporary = result.error == Error::Network || result.error == Error::RateLimit || result.error == Error::Server;

	if (temporary && request.attempt < 3)
	{
		int delay = 1000 << request.attempt;
		request.attempt++;

		QTimer::singleShot(delay, this, [this, request] { execute(request); });
		return;
	}

	request.done(result);
}

void GoogleCalendarApi::onRefreshed()
{
	auto waiting = std::move(m_waiting);
	m_waiting.clear();

	for (auto& request : waiting) execute(request);
}

void GoogleCalendarApi::onRefreshFailed(bool needsReauthorization)
{
	if (needsReauthorization && !m_needsReauthorization) {
		m_needsReauthorization = true;
		emit reauthorizationNeeded();
	}

	auto waiting = std::move(m_waiting);
	m_waiting.clear();

	Result result;
	result.error = needsReauthorization ? Error::Authorization : Error::Network;

	for (auto& request : waiting) request.done(result);
}
