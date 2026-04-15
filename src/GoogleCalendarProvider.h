#pragma once

#include "CalendarProvider.h"
#include <QNetworkAccessManager>

class QTcpServer;
class QTimer;

// ============================================================================
// GoogleCalendarProvider — full Google Calendar API integration
//
// OAuth2 flow:
//   1. Start local TCP server on random port
//   2. Open browser to Google consent screen
//   3. User authorizes -> Google redirects to localhost with auth code
//   4. Exchange auth code for access + refresh tokens
//   5. Store tokens in QSettings
//
// Credentials are embedded at compile time via Credentials.h.
// Users never input Client ID or Secret.
// ============================================================================

class GoogleCalendarProvider : public CalendarProvider {
    Q_OBJECT
public:
    explicit GoogleCalendarProvider(QObject* parent = nullptr);

    QString name() const override { return "Google Calendar"; }
    Type type() const override { return Google; }
    bool isAuthenticated() const override;

    void authenticate() override;
    void signOut() override;
    void fetchEvents(const QDate& from, const QDate& to) override;
    void createEvent(const Event& event) override;
    void updateEvent(const QString& externalId, const Event& event) override;
    void deleteEvent(const QString& externalId) override;

    /// Load stored tokens from QSettings (called on construction)
    void loadTokens();

    // userEmail() and isAuthenticating() are now provided by CalendarProvider base

private slots:
    void onOAuthConnection();

private:
    void exchangeCodeForTokens(const QString& authCode);
    void refreshAccessToken();
    void fetchUserEmail();
    void makeAuthenticatedRequest(const QString& method,
                                   const QUrl& url,
                                   const QByteArray& body,
                                   std::function<void(const QJsonDocument&, int)> callback);
    void cleanupAuthServer();

    QNetworkAccessManager* m_nam = nullptr;
    QTcpServer* m_authServer = nullptr;
    QTimer* m_authTimeout = nullptr;

    QString m_accessToken;
    QString m_refreshToken;
    QDateTime m_tokenExpiry;
    int m_redirectPort = 0;

    static constexpr const char* kAuthUrl     = "https://accounts.google.com/o/oauth2/v2/auth";
    static constexpr const char* kTokenUrl    = "https://oauth2.googleapis.com/token";
    static constexpr const char* kCalendarApi = "https://www.googleapis.com/calendar/v3";
    static constexpr const char* kScope       = "https://www.googleapis.com/auth/calendar";
    static constexpr const char* kUserInfoUrl = "https://www.googleapis.com/oauth2/v2/userinfo";
};
