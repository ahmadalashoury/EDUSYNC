#pragma once

#include <QObject>
#include <QVector>
#include <QDate>
#include <QDateTime>
#include <QString>

#include "Event.h"

// ============================================================================
// CalendarProvider — abstract interface for external calendar sources
//
// All operations are async. Results come via signals.
//
// Connection state machine:
//   Disconnected ──▶ Connecting ──▶ Connected ──▶ Syncing
//        ▲               │              │            │
//        └───────────────┴──── Error ◄──┴────────────┘
//
// The connectionState() accessor and connectionStateChanged() signal drive
// all UI feedback (status labels, button states, account badges).
// ============================================================================

class CalendarProvider : public QObject {
    Q_OBJECT
public:
    explicit CalendarProvider(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~CalendarProvider() = default;

    enum Type { Local, Google, Outlook, CalDAV, AppleNative };

    enum ConnectionState {
        Disconnected,   // no tokens, never connected or signed out
        Connecting,     // OAuth flow in progress (browser open)
        Connected,      // authenticated, idle
        Syncing,        // actively fetching or pushing events
        Error           // last operation failed (errorMessage() has details)
    };
    Q_ENUM(ConnectionState)

    virtual QString name() const = 0;
    virtual Type type() const = 0;
    virtual bool isAuthenticated() const = 0;

    // Connection state
    ConnectionState connectionState() const { return m_connState; }
    QString errorMessage() const { return m_errorMessage; }
    QString accountKey() const { return m_accountKey; }
    QDateTime lastSyncTime() const { return m_lastSyncTime; }
    QString statusDetail() const { return m_statusDetail; }

    /// Start OAuth2 authentication flow
    virtual void authenticate() = 0;

    /// Sign out and clear tokens
    virtual void signOut() = 0;

    /// Fetch events in the given date range
    virtual void fetchEvents(const QDate& from, const QDate& to) = 0;

    /// CRUD operations
    virtual void createEvent(const Event& event) = 0;
    virtual void updateEvent(const QString& externalId, const Event& event) = 0;
    virtual void deleteEvent(const QString& externalId) = 0;

    // Convenience for checking older API
    bool isAuthenticating() const { return m_connState == Connecting; }

    // User identity (set by subclass after auth)
    virtual QString userEmail() const { return m_userEmail; }
    virtual QString accountDisplayName() const {
        return !m_userEmail.isEmpty() ? m_userEmail : name();
    }

signals:
    void authenticationComplete(bool success, const QString& error);
    void eventsFetched(const QVector<Event>& events);
    void eventCreated(const QString& externalId);
    void operationComplete(bool success, const QString& error);
    void statusMessage(const QString& msg);
    void connectionStateChanged(ConnectionState state);

protected:
    void setConnectionState(ConnectionState s) {
        if (m_connState != s) {
            m_connState = s;
            if (s != Error) m_errorMessage.clear();
            emit connectionStateChanged(s);
        }
    }
    void setError(const QString& msg) {
        m_errorMessage = msg;
        m_statusDetail = msg;
        setConnectionState(Error);
    }
    void setUserEmail(const QString& email) { m_userEmail = email; }
    void setAccountKey(const QString& key) { m_accountKey = key; }
    void setLastSyncTime(const QDateTime& dt) { m_lastSyncTime = dt; }
    void setStatusDetail(const QString& detail) { m_statusDetail = detail; }

private:
    ConnectionState m_connState = Disconnected;
    QString m_errorMessage;
    QString m_userEmail;
    QString m_accountKey;
    QString m_statusDetail;
    QDateTime m_lastSyncTime;
};
