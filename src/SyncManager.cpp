#include "SyncManager.h"
#include "EventStore.h"

#include <algorithm>

SyncManager::SyncManager(EventStore* store, QObject* parent)
    : QObject(parent)
    , m_store(store)
{
    m_syncTimer = new QTimer(this);
    m_syncTimer->setSingleShot(false);
    connect(m_syncTimer, &QTimer::timeout, this, &SyncManager::syncNow);
}

// ============================================================================
// Provider management
// ============================================================================

void SyncManager::addProvider(CalendarProvider* provider) {
    if (!provider) return;
    if (providerByKey(provider->accountKey()))
        return;

    m_providers.push_back(provider);

    connect(provider, &CalendarProvider::eventsFetched,
            this, &SyncManager::onProviderEventsFetched);
    connect(provider, &CalendarProvider::statusMessage,
            this, &SyncManager::statusMessage);
    connect(provider, &CalendarProvider::operationComplete,
            this, &SyncManager::onProviderOperationComplete);
    connect(provider, &CalendarProvider::eventCreated,
            this, &SyncManager::onProviderEventCreated);
}

void SyncManager::removeProvider(CalendarProvider::Type type) {
    const auto newEnd = std::remove_if(m_providers.begin(), m_providers.end(),
                                       [this, type](CalendarProvider* provider) {
        if (!provider || provider->type() != type)
            return false;

        disconnect(provider, nullptr, this, nullptr);
        m_pendingSyncProviders.remove(provider->accountKey());
        m_pendingCreates.remove(provider);
        return true;
    });
    m_providers.erase(newEnd, m_providers.end());
}

CalendarProvider* SyncManager::provider(CalendarProvider::Type type) const {
    for (auto* p : m_providers)
        if (p->type() == type) return p;
    return nullptr;
}

CalendarProvider* SyncManager::providerByKey(const QString& accountKey) const {
    for (auto* p : m_providers) {
        if (p->accountKey() == accountKey)
            return p;
    }
    return nullptr;
}

// ============================================================================
// Sync control
// ============================================================================

void SyncManager::startSync(int intervalSec) {
    m_syncTimer->start(intervalSec * 1000);
    // Do an immediate sync
    syncNow();
}

void SyncManager::stopSync() {
    m_syncTimer->stop();
}

void SyncManager::syncNow() {
    const QDate today = QDate::currentDate();
    syncRange(today.addDays(-30), today.addDays(90));
}

void SyncManager::syncRange(const QDate& from, const QDate& to) {
    m_pendingSyncProviders.clear();
    m_syncErrors.clear();
    m_syncAdded = 0;
    m_syncUpdated = 0;

    QVector<CalendarProvider*> authenticatedProviders;
    for (CalendarProvider* p : m_providers) {
        if (p->isAuthenticated()) {
            m_pendingSyncProviders.insert(p->accountKey());
            authenticatedProviders.push_back(p);
        }
    }

    if (m_pendingSyncProviders.isEmpty()) {
        emit statusMessage("No calendar accounts connected");
        return;
    }

    emit syncStarted();

    for (CalendarProvider* provider : authenticatedProviders)
        provider->fetchEvents(from, to);
}

// ============================================================================
// Incoming remote events
// ============================================================================

void SyncManager::onProviderEventsFetched(const QVector<Event>& events) {
    auto* provider = qobject_cast<CalendarProvider*>(sender());
    if (!provider) return;

    const auto [added, updated] = mergeRemoteEvents(events, provider);
    m_syncAdded += added;
    m_syncUpdated += updated;
    m_lastSyncTime = QDateTime::currentDateTime();
    finishProviderSync(provider, true);

    // After pulling, push any dirty local events for this provider
    pushAllDirty();
}

// ============================================================================
// Merge: remote events into local store (dedup by externalId)
// ============================================================================

QPair<int, int> SyncManager::mergeRemoteEvents(const QVector<Event>& remoteEvents,
                                               CalendarProvider* provider) {
    int added = 0;
    int updated = 0;
    Event::Source eventSource;
    switch (provider ? provider->type() : CalendarProvider::Local) {
    case CalendarProvider::Google:  eventSource = Event::Google;  break;
    case CalendarProvider::Outlook: eventSource = Event::Outlook; break;
    case CalendarProvider::CalDAV:  eventSource = Event::CalDAV;  break;
    case CalendarProvider::AppleNative: eventSource = Event::AppleNative; break;
    default:                        eventSource = Event::Local;   break;
    }

    for (const Event& remote : remoteEvents) {
        if (remote.externalId().isEmpty()) continue;

        const QString providerKey = provider ? provider->accountKey() : QString();
        const int existingIdx = findByExternalId(remote.externalId(), eventSource, providerKey);

        if (existingIdx >= 0) {
            // Existing event — check if remote is newer (last-write-wins)
            const Event& local = m_store->all()[existingIdx];

            if (remote.lastModified().isValid() && local.lastModified().isValid()
                && remote.lastModified() > local.lastModified())
            {
                // Remote is newer — update local
                Event merged = remote;
                merged.setId(local.getId());
                merged.setSource(eventSource);
                merged.setProviderKey(providerKey);
                merged.setDirty(false);
                m_store->update(existingIdx, merged);
                updated++;
            }
            // else: local is newer or same — keep local, will push later
        } else {
            // New remote event — add to local store
            Event newEvent = remote;
            newEvent.setSource(eventSource);
            newEvent.setProviderKey(providerKey);
            newEvent.setDirty(false);
            m_store->add(newEvent);
            added++;
        }
    }

    return {added, updated};
}

// ============================================================================
// Find local event by external provider ID
// ============================================================================

int SyncManager::findByExternalId(const QString& externalId,
                                  Event::Source source,
                                  const QString& providerKey) const {
    const auto& all = m_store->all();
    for (int i = 0; i < all.size(); ++i) {
        if (all[i].source() == source
            && all[i].externalId() == externalId
            && all[i].providerKey() == providerKey)
            return i;
    }
    return -1;
}

// ============================================================================
// Push: local dirty events to their providers
// ============================================================================

void SyncManager::pushEvent(int localIndex) {
    if (localIndex < 0 || localIndex >= m_store->all().size()) return;

    const Event& ev = m_store->all()[localIndex];
    if (!ev.isDirty()) return;

    CalendarProvider::Type provType;
    switch (ev.source()) {
    case Event::Google:      provType = CalendarProvider::Google;  break;
    case Event::Outlook:     provType = CalendarProvider::Outlook; break;
    case Event::CalDAV:      provType = CalendarProvider::CalDAV;  break;
    case Event::AppleNative: provType = CalendarProvider::AppleNative; break;
    default: return; // Local-only events don't get pushed
    }

    CalendarProvider* prov = !ev.providerKey().isEmpty()
        ? providerByKey(ev.providerKey())
        : provider(provType);
    if (!prov || !prov->isAuthenticated()) return;

    // A newly created Apple item carries a sentinel externalId ("event:" or
    // "reminder:") to tell the provider which kind to produce. Anything after
    // the colon is the real identifier — missing for brand-new items.
    const QString idPart = ev.externalId().section(':', 1);
    const bool isNew = ev.externalId().isEmpty() || idPart.isEmpty();

    if (isNew) {
        // Remember this local event so we can apply the returned externalId.
        m_pendingCreates[prov].enqueue(ev.getId());
        prov->createEvent(ev);
    } else {
        prov->updateEvent(ev.externalId(), ev);
    }

    // Mark as clean
    Event clean = ev;
    clean.setDirty(false);
    m_store->update(localIndex, clean);
}

void SyncManager::pushAllDirty() {
    const auto& all = m_store->all();
    for (int i = 0; i < all.size(); ++i) {
        if (all[i].isDirty() && all[i].source() != Event::Local)
            pushEvent(i);
    }
}

void SyncManager::onProviderOperationComplete(bool success, const QString& error) {
    auto* provider = qobject_cast<CalendarProvider*>(sender());
    if (!provider) return;

    // If a create failed, drop the pending entry — the remote never produced an id.
    if (!success) {
        auto it = m_pendingCreates.find(provider);
        if (it != m_pendingCreates.end() && !it->isEmpty())
            it->dequeue();
    }

    if (!m_pendingSyncProviders.contains(provider->accountKey()))
        return;
    finishProviderSync(provider, success, error);
}

void SyncManager::onProviderEventCreated(const QString& externalId) {
    auto* provider = qobject_cast<CalendarProvider*>(sender());
    if (!provider || externalId.isEmpty()) return;

    auto it = m_pendingCreates.find(provider);
    if (it == m_pendingCreates.end() || it->isEmpty())
        return;

    const int localId = it->dequeue();

    // Find the local event by id and assign the real externalId so future
    // syncs can dedupe correctly.
    const auto& all = m_store->all();
    for (int i = 0; i < all.size(); ++i) {
        if (all[i].getId() == localId) {
            Event updated = all[i];
            updated.setExternalId(externalId);
            updated.setDirty(false);
            m_store->update(i, updated);
            return;
        }
    }
}

void SyncManager::finishProviderSync(CalendarProvider* provider, bool success, const QString& error) {
    if (!provider) return;
    if (!m_pendingSyncProviders.remove(provider->accountKey()))
        return;

    if (!success && !error.isEmpty())
        m_syncErrors << QString("%1: %2").arg(provider->accountDisplayName(), error);

    if (!m_pendingSyncProviders.isEmpty())
        return;

    if (!m_syncErrors.isEmpty())
        emit syncError(m_syncErrors.join('\n'));

    emit syncComplete(m_syncAdded, m_syncUpdated);
}
