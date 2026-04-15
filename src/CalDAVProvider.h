#pragma once

#include "CalendarProvider.h"
#include <QList>
#include <QNetworkAccessManager>

// ============================================================================
// CalDAVProvider — account-based CalDAV sync
//
// Supports multiple saved CalDAV accounts (iCloud, Yahoo, Fastmail, etc.).
// Credentials are stored per account profile so the Accounts settings page can
// add, edit, remove, and reconnect individual sources cleanly.
// ============================================================================

class CalDAVProvider : public CalendarProvider {
    Q_OBJECT
public:
    struct AccountConfig {
        QString id;
        QString name;
        QString serverUrl;
        QString username;
        QString password;
        QString calendarHomePath;
        QString principalUrl;
        bool authenticated = false;
    };

    explicit CalDAVProvider(const AccountConfig& config, QObject* parent = nullptr);

    QString name() const override { return "CalDAV"; }
    Type type() const override { return CalDAV; }
    bool isAuthenticated() const override;
    QString accountDisplayName() const override;

    void authenticate() override;
    void signOut() override;
    void fetchEvents(const QDate& from, const QDate& to) override;
    void createEvent(const Event& event) override;
    void updateEvent(const QString& externalId, const Event& event) override;
    void deleteEvent(const QString& externalId) override;

    void setAccountName(const QString& name) { m_accountName = name.trimmed(); }
    void setServerUrl(const QString& url);
    void setUsername(const QString& user);
    void setPassword(const QString& pass);

    QString accountId() const { return m_accountId; }
    QString accountName() const { return m_accountName; }
    QString serverUrl() const { return m_serverUrl; }
    QString username() const { return m_username; }
    QString displayName() const;
    AccountConfig config() const;

    void testConnection();

    static QList<AccountConfig> loadAccounts();
    static void saveAccounts(const QList<AccountConfig>& accounts);
    static void upsertAccount(const AccountConfig& account);
    static void removeAccount(const QString& id);
    static QString createAccountId();

private:
    QUrl resolvedUrl(const QString& hrefOrUrl) const;
    QByteArray basicAuthHeader() const;
    void persist() const;
    void discoverHomeAndContinue(const std::function<void(bool)>& continuation);

    QNetworkAccessManager* m_nam = nullptr;
    QString m_accountId;
    QString m_accountName;
    QString m_serverUrl;
    QString m_username;
    QString m_password;
    QString m_calendarHomePath;
    QString m_principalUrl;
    bool m_authenticated = false;
};
