#include "GoogleOAuth.h"

#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrlQuery>

#include "CredentialStore.h"
#include "GoogleCalendarLog.h"
#include "GlobalSettings.h"

//the OAuth client of this installation (not committed, see GoogleClientConfig.example.h)
#if __has_include("GoogleClientConfig.h")
#include "GoogleClientConfig.h"
#endif

#ifndef GOOGLE_CLIENT_ID
#define GOOGLE_CLIENT_ID ""
#define GOOGLE_CLIENT_SECRET ""
#endif

//the refresh token of the test server is kept apart, so testing never replaces the real authorization
static QString credentialTarget()
{
	return GlobalSettings::getGoogleCalendar().testOAuthUrl.size() ? "QDento/GoogleCalendarTest" : "QDento/GoogleCalendar";
}
static const char* calendarScope = "https://www.googleapis.com/auth/calendar";

GoogleOAuth::GoogleOAuth(QNetworkAccessManager* network, QObject* parent)
	: QObject(parent), m_network(network)
{
	m_loginTimeout = new QTimer(this);
	m_loginTimeout->setSingleShot(true);
	m_loginTimeout->setInterval(5 * 60 * 1000);

	connect(m_loginTimeout, &QTimer::timeout, this, [this] {
		GoogleCalendarLog::write("Sign-in timed out");
		finishLogin(false, tr("The Google sign-in was not completed in time."));
	});
}

bool GoogleOAuth::testMode() const
{
	return GlobalSettings::getGoogleCalendar().testOAuthUrl.size();
}

bool GoogleOAuth::isConfigured()
{
	return QString(GOOGLE_CLIENT_ID).size() || GlobalSettings::getGoogleCalendar().testOAuthUrl.size();
}

QUrl GoogleOAuth::authorizationUrl() const
{
	auto test = GlobalSettings::getGoogleCalendar().testOAuthUrl;
	return test.size() ? QUrl(QString::fromStdString(test) + "/auth") : QUrl("https://accounts.google.com/o/oauth2/v2/auth");
}

QUrl GoogleOAuth::tokenUrl() const
{
	auto test = GlobalSettings::getGoogleCalendar().testOAuthUrl;
	return test.size() ? QUrl(QString::fromStdString(test) + "/token") : QUrl("https://oauth2.googleapis.com/token");
}

QUrl GoogleOAuth::revokeUrl() const
{
	auto test = GlobalSettings::getGoogleCalendar().testOAuthUrl;
	return test.size() ? QUrl(QString::fromStdString(test) + "/revoke") : QUrl("https://oauth2.googleapis.com/revoke");
}

QString GoogleOAuth::clientId() const
{
	return testMode() ? "qdento-test-client" : GOOGLE_CLIENT_ID;
}

QString GoogleOAuth::clientSecret() const
{
	return testMode() ? "qdento-test-secret" : GOOGLE_CLIENT_SECRET;
}

bool GoogleOAuth::hasRefreshToken() const
{
	auto token = CredentialStore::read(credentialTarget());
	return token && token->size();
}

void GoogleOAuth::createFlow()
{
	delete m_flow;

	m_flow = new QOAuth2AuthorizationCodeFlow(m_network, this);

	m_flow->setAuthorizationUrl(authorizationUrl());
	m_flow->setTokenUrl(tokenUrl());
	m_flow->setClientIdentifier(clientId());
	m_flow->setClientIdentifierSharedKey(clientSecret());
	m_flow->setRequestedScopeTokens({ calendarScope });
	m_flow->setPkceMethod(QOAuth2AuthorizationCodeFlow::PkceMethod::S256);

	//a refresh token is only given with offline access; the consent is asked again so that it is always given
	m_flow->setModifyParametersFunction([](QAbstractOAuth::Stage stage, QMultiMap<QString, QVariant>* parameters) {
		if (stage == QAbstractOAuth::Stage::RequestingAuthorization) {
			parameters->insert("access_type", "offline");
			parameters->insert("prompt", "consent");
		}
	});

	connect(m_flow, &QAbstractOAuth2::serverReportedErrorOccurred, this,
		[this](const QString& error, const QString& description, const QUrl&) {

			//the technical details only in the log (without any token)
			GoogleCalendarLog::write("OAuth error: " + error + " " + description);

			if (m_loggingIn) {
				finishLogin(false, error == "access_denied" ?
					tr("The access to Google Calendar was not allowed.") :
					tr("The Google sign-in failed."));
				return;
			}

		});

	connect(m_flow, &QAbstractOAuth::requestFailed, this, [this](const QAbstractOAuth::Error error) {

		GoogleCalendarLog::write(QString("OAuth request failed (%1)").arg(static_cast<int>(error)));

		if (m_loggingIn) {
			finishLogin(false, tr("The Google sign-in failed. Check the internet connection."));
			return;
		}

	});

	connect(m_flow, &QAbstractOAuth::granted, this, [this] {

		m_accessToken = m_flow->token();

		if (m_loggingIn)
		{
			//Google may let the user refuse single permissions
			auto granted = m_flow->grantedScopeTokens();

			if (granted.size() && !granted.contains(calendarScope)) {
				finishLogin(false, tr("The access to Google Calendar was not allowed."));
				return;
			}

			if (m_flow->refreshToken().isEmpty() || !CredentialStore::write(credentialTarget(), m_flow->refreshToken())) {
				finishLogin(false, tr("The Google authorization could not be stored."));
				return;
			}

			GoogleCalendarLog::write("Authentication successful");
			finishLogin(true, {});
			return;
		}

	});
}

void GoogleOAuth::login()
{
	if (m_loggingIn) return;

	createFlow();

	//loopback redirect on a free port of this computer only
	//(this constructor starts listening itself)
	m_loginHandler = new QOAuthHttpServerReplyHandler(QHostAddress::LocalHost, 0, this);

	if (!m_loginHandler->isListening()) {
		GoogleCalendarLog::write("The loopback server could not be started");
		delete m_loginHandler;
		m_loginHandler = nullptr;
		emit loginFailed(tr("The Google sign-in could not be started."));
		return;
	}

	m_loginHandler->setCallbackText(
		"<html><head><meta charset=\"utf-8\"></head><body style=\"font-family:sans-serif;text-align:center;margin-top:60px\">"
		"<h2>QDento</h2><p>" + tr("The Google Calendar connection is complete. You can close this window.").toHtmlEscaped() +
		"</p></body></html>");

	m_flow->setReplyHandler(m_loginHandler);

	connect(m_flow, &QAbstractOAuth::authorizeWithBrowser, this, [this](const QUrl& url) {

		if (!testMode()) {
			QDesktopServices::openUrl(url);
			return;
		}

		//test server: it answers the authorization request with the redirect to the loopback address
		auto reply = m_network->get(QNetworkRequest(url));
		connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
	});

	m_loggingIn = true;
	m_loginTimeout->start();

	GoogleCalendarLog::write("Sign-in started");

	m_flow->grant();
}

void GoogleOAuth::cancelLogin()
{
	if (!m_loggingIn) return;

	GoogleCalendarLog::write("Sign-in cancelled");
	finishLogin(false, tr("The Google sign-in was cancelled."));
}

void GoogleOAuth::finishLogin(bool ok, const QString& message)
{
	if (!m_loggingIn) return;

	m_loggingIn = false;
	m_loginTimeout->stop();

	if (m_loginHandler) {
		m_loginHandler->close();
		m_loginHandler->deleteLater();
		m_loginHandler = nullptr;
	}

	if (ok) emit loggedIn();
	else emit loginFailed(message);
}

void GoogleOAuth::refresh()
{
	if (m_refreshing || m_loggingIn) return;

	auto token = CredentialStore::read(credentialTarget());

	if (!token || token->isEmpty()) {
		emit refreshFailed(true, tr("The connection to Google Calendar needs to be authorized again."));
		return;
	}

	//a plain token request, so that the answer of Google can be told apart:
	//"invalid_grant" means the authorization was revoked or expired, anything else is temporary
	QNetworkRequest request(tokenUrl());
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
	request.setTransferTimeout(30000);

	QUrlQuery body;
	body.addQueryItem("grant_type", "refresh_token");
	body.addQueryItem("refresh_token", *token);
	body.addQueryItem("client_id", clientId());
	body.addQueryItem("client_secret", clientSecret());

	m_refreshing = true;

	auto reply = m_network->post(request, body.toString(QUrl::FullyEncoded).toUtf8());

	connect(reply, &QNetworkReply::finished, this, [this, reply] {

		reply->deleteLater();
		m_refreshing = false;

		int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		auto json = QJsonDocument::fromJson(reply->readAll()).object();

		if (status == 200 && json["access_token"].toString().size())
		{
			m_accessToken = json["access_token"].toString();

			//Google may return a new refresh token
			if (json["refresh_token"].toString().size()) CredentialStore::write(credentialTarget(), json["refresh_token"].toString());

			GoogleCalendarLog::write("Access token refreshed");
			emit refreshed();
			return;
		}

		m_accessToken.clear();

		auto error = json["error"].toString();

		GoogleCalendarLog::write(QString("Token refresh failed: HTTP %1 %2").arg(status).arg(error));

		bool revoked = error == "invalid_grant" || error == "unauthorized_client" || error == "invalid_client";

		emit refreshFailed(revoked, revoked ?
			tr("The connection to Google Calendar needs to be authorized again.") :
			tr("Google Calendar could not be reached. Check the internet connection."));
	});
}

void GoogleOAuth::logout()
{
	auto token = CredentialStore::read(credentialTarget());

	//the authorization is also revoked at Google (the answer is not needed)
	if (token && token->size())
	{
		QNetworkRequest request(revokeUrl());
		request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

		QUrlQuery body;
		body.addQueryItem("token", *token);

		auto reply = m_network->post(request, body.toString(QUrl::FullyEncoded).toUtf8());
		connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
	}

	CredentialStore::remove(credentialTarget());
	m_accessToken.clear();

	GoogleCalendarLog::write("Signed out, authorization removed");
}
