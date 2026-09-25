#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QOAuth2AuthorizationCodeFlow;
class QOAuthHttpServerReplyHandler;
class QTimer;

//Google sign-in of QDento (OAuth 2.0 for installed applications, PKCE, loopback redirect).
//The user signs in in the system browser; QDento never sees the password.
//The refresh token is kept only in the credential store of the system, the access token only in memory.
class GoogleOAuth : public QObject
{
	Q_OBJECT

public:
	explicit GoogleOAuth(QNetworkAccessManager* network, QObject* parent = nullptr);

	//an OAuth client is built into this copy of QDento (or a test server is configured)
	static bool isConfigured();

	bool hasRefreshToken() const;
	QString accessToken() const { return m_accessToken; }

	//opens the browser for the Google sign-in and consent
	void login();
	void cancelLogin();
	bool isLoggingIn() const { return m_loggingIn; }

	//new access token with the stored refresh token
	void refresh();
	bool isRefreshing() const { return m_refreshing; }

	//revokes the authorization at Google and removes the stored refresh token
	void logout();

signals:
	void loggedIn();
	void loginFailed(const QString& message);
	void refreshed();
	void refreshFailed(bool needsReauthorization, const QString& message);

private:
	QNetworkAccessManager* m_network;
	QOAuth2AuthorizationCodeFlow* m_flow{ nullptr };
	QOAuthHttpServerReplyHandler* m_loginHandler{ nullptr };
	QTimer* m_loginTimeout{ nullptr };

	QString m_accessToken;
	bool m_loggingIn{ false };
	bool m_refreshing{ false };

	QUrl authorizationUrl() const;
	QUrl tokenUrl() const;
	QUrl revokeUrl() const;
	bool testMode() const;
	QString clientId() const;
	QString clientSecret() const;

	void createFlow();
	void finishLogin(bool ok, const QString& message);
};
