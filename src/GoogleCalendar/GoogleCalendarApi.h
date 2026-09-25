#pragma once

#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QUrl>
#include <functional>
#include <vector>

class QNetworkAccessManager;
class GoogleOAuth;

//A calendar of the Google account
struct GoogleCalendarInfo
{
	QString id;
	QString summary;
	QString accessRole;
	bool primary{ false };
};

//The parts of a Google Calendar event QDento uses
struct GoogleEvent
{
	QString id;
	QString etag;
	QString status;			//confirmed, tentative, cancelled
	QString summary;
	QString updated;
	QDateTime start;		//local time
	QDateTime end;
	bool timed{ false };	//false: all-day event or no times (not usable for an appointment)
	bool recurring{ false };	//a repeating event or one of its occurrences

	//private extended properties written by QDento
	QString appointmentId;
	QString databaseId;

	static GoogleEvent fromJson(const QJsonObject& json);
};

//Asynchronous client of the Google Calendar API (v3). All requests run on the network manager of the
//GUI thread without blocking it. Expired access tokens are refreshed once; temporary errors are
//retried after 1, 2 and 4 seconds, permanent ones are not retried.
class GoogleCalendarApi : public QObject
{
	Q_OBJECT

public:
	enum class Error { None, Network, Authorization, Permission, NotFound, Gone, Precondition, RateLimit, Server, Malformed, Other };

	struct Result
	{
		int httpStatus{ 0 };
		Error error{ Error::None };
		QJsonObject json;
		bool ok() const { return error == Error::None; }
	};

	using Callback = std::function<void(const Result&)>;

	GoogleCalendarApi(GoogleOAuth* auth, QNetworkAccessManager* network, QObject* parent = nullptr);

	void listCalendars(const QString& pageToken, Callback done);
	//syncToken empty: full list; showDeleted is used with the sync token (deleted events are returned then)
	void listEvents(const QString& calendarId, const QString& syncToken, const QString& pageToken, Callback done);
	void getEvent(const QString& calendarId, const QString& eventId, Callback done);
	void insertEvent(const QString& calendarId, const QJsonObject& event, Callback done);
	//ifMatch: the event is changed only if it is still this version (412 otherwise)
	void patchEvent(const QString& calendarId, const QString& eventId, const QJsonObject& event, const QString& ifMatch, Callback done);
	void deleteEvent(const QString& calendarId, const QString& eventId, Callback done);

	//the event body of an appointment (only name, times, "QDento appointment" and the QDento ids)
	static QJsonObject eventBody(const QString& summary, const QDateTime& start, const QDateTime& end,
		long long appointmentRowid, const QString& databaseId);

	//understandable Greek text of an error (no technical details)
	static QString errorText(Error error);

	//the authorization is not valid any more (the user has to sign in again)
	bool needsReauthorization() const { return m_needsReauthorization; }
	void resetAuthorizationState() { m_needsReauthorization = false; }

signals:
	void reauthorizationNeeded();

private:
	struct Request
	{
		QByteArray method;
		QUrl url;
		QByteArray body;
		QString ifMatch;
		Callback done;
		int attempt{ 0 };
		bool refreshed{ false };
	};

	GoogleOAuth* m_auth;
	QNetworkAccessManager* m_network;
	bool m_needsReauthorization{ false };

	//requests waiting for a new access token
	std::vector<Request> m_waiting;

	QUrl baseUrl() const;
	QString encodeId(const QString& id) const;

	void send(Request request);
	void execute(Request request);
	void finished(Request request, int status, const QByteArray& data, bool networkError);
	void onRefreshed();
	void onRefreshFailed(bool needsReauthorization);
};
