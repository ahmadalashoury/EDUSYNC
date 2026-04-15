#pragma once

#include "CalendarProvider.h"
#include <QNetworkAccessManager>

class QTcpServer;
class QTimer;

// ============================================================================
// OutlookCalendarProvider — Microsoft Graph API integration
//
// OAuth2 via Azure AD (public client — no client_secret needed):
//   1. Start local TCP server on random port
//   2. Open browser to Microsoft sign-in
//   3. User authorizes -> Microsoft redirects to localhost with auth code
//   4. Exchange auth code for access + refresh tokens
//   5. Store tokens in QSettings
//
// Credentials are embedded at compile time via Credentials.h.
// ============================================================================

class OutlookCalendarProvider : public CalendarProvider {
    Q_OBJECT
public:
    explicit OutlookCalendarProvider(QObject* parent = nullptr);

    QString name() const override { return "Outlook Calendar"; }
    Type type() const override { return Outlook; }
    bool isAuthenticated() const override;

    void authenticate() override;
    void signOut() override;
    void fetchEvents(const QDate& from, const QDate& to) override;
    void createEvent(const Event& event) override;
    void updateEvent(const QString& externalId, const Event& event) override;
    void deleteEvent(const QString& externalId) override;

    void loadTokens();
    // userEmail() and isAuthenticating() are now provided by CalendarProvider base

private slots:
    void onOAuthConnection();

private:
    void exchangeCodeForTokens(const QString& authCode);
    void refreshAccessToken();
    void fetchUserProfile();
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

    static constexpr const char* kAuthUrl  = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize";
    static constexpr const char* kTokenUrl = "https://login.microsoftonline.com/common/oauth2/v2.0/token";
    static constexpr const char* kGraphApi = "https://graph.microsoft.com/v1.0";
    static constexpr const char* kScope    = "Calendars.ReadWrite offline_access";
};
