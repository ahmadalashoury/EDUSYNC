#pragma once

#include <QObject>
#include <QVector>
#include <QDate>
#include <QDateTime>
#include <QTimer>
#include <QHash>
#include <QMap>
#include <QQueue>
#include <QSet>

#include "Event.h"
#include "CalendarProvider.h"

class EventStore;

// ============================================================================
// SyncManager — bidirectional sync across local + Google + Outlook
//
// Pull: fetch remote → merge into local store (dedup by externalId)
// Push: local dirty events → create/update on remote provider
// Conflict resolution: last-write-wins based on lastModified
// ============================================================================

class SyncManager : public QObject {
    Q_OBJECT
public:
    explicit SyncManager(EventStore* store, QObject* parent = nullptr);

    void addProvider(CalendarProvider* provider);
    void removeProvider(CalendarProvider::Type type);
    CalendarProvider* provider(CalendarProvider::Type type) const;
    CalendarProvider* providerByKey(const QString& accountKey) const;
    QVector<CalendarProvider*> providers() const { return m_providers; }

    /// Start periodic syncing (interval in seconds, default 5 min)
    void startSync(int intervalSec = 300);
    void stopSync();

    /// Trigger immediate sync of all authenticated providers
    void syncNow();
    void syncRange(const QDate& from, const QDate& to);

    /// Push a single local event to its provider
    void pushEvent(int localIndex);

    /// Push all dirty events to their respective providers
    void pushAllDirty();

    QDateTime lastSyncTime() const { return m_lastSyncTime; }

signals:
    void syncStarted();
    void syncComplete(int added, int updated);
    void syncError(const QString& error);
    void statusMessage(const QString& msg);

private slots:
    void onProviderEventsFetched(const QVector<Event>& events);
    void onProviderOperationComplete(bool success, const QString& error);
    void onProviderEventCreated(const QString& externalId);

private:
    QPair<int, int> mergeRemoteEvents(const QVector<Event>& remoteEvents,
                                     CalendarProvider* provider);

    int findByExternalId(const QString& externalId,
                         Event::Source source,
                         const QString& providerKey) const;
    void finishProviderSync(CalendarProvider* provider, bool success, const QString& error = QString());

    EventStore* m_store = nullptr;
    QVector<CalendarProvider*> m_providers;
    QTimer* m_syncTimer = nullptr;
    QDateTime m_lastSyncTime;
    QSet<QString> m_pendingSyncProviders;
    int m_syncAdded = 0;
    int m_syncUpdated = 0;
    QStringList m_syncErrors;

    // FIFO queue of local event ids awaiting an externalId from a create call,
    // keyed by provider. Popped in onProviderEventCreated().
    QMap<CalendarProvider*, QQueue<int>> m_pendingCreates;
};
