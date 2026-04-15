#include "EventStore.h"

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

// ============================================================================
// Construction
// ============================================================================

EventStore::EventStore(QObject* parent) : QObject(parent) {}

// ============================================================================
// Persistence — atomic save / load
// ============================================================================

QString EventStore::storagePath() const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + "/events.json";
}

bool EventStore::load() {
    const QString path = storagePath();
    QFile file(path);
    if (!file.exists()) return true;  // first launch — no data yet
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) return false;
    if (!doc.isObject()) return false;

    const QJsonObject root = doc.object();
    const QJsonArray arr = root.value("events").toArray();

    m_events.clear();
    m_events.reserve(arr.size());
    for (const auto& val : arr)
        m_events.push_back(Event::fromJson(val.toObject()));

    m_nextId = root.value("next_id").toInt(1);

    // Ensure nextId is above all loaded event IDs
    for (const auto& e : m_events)
        if (e.getId() >= m_nextId) m_nextId = e.getId() + 1;

    emit changed();
    return true;
}

bool EventStore::save() const {
    const QString path = storagePath();

    // Ensure directory exists
    QDir().mkpath(QFileInfo(path).absolutePath());

    // QSaveFile writes to a temp file then atomically renames on commit().
    // If the app crashes mid-write, the original file stays intact.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QJsonArray arr;
    for (const auto& e : m_events)
        arr.append(e.toJson());

    QJsonObject root;
    root["version"] = 1;
    root["next_id"] = m_nextId;
    root["events"]  = arr;

    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }

    return file.commit();
}

void EventStore::persist() {
    save();
    emit changed();
}

// ============================================================================
// Read
// ============================================================================

QVector<const Event*> EventStore::forDate(const QDate& d) const {
    QVector<const Event*> result;
    result.reserve(m_events.size());
    for (const auto& e : m_events)
        if (e.isOnDate(d)) result.push_back(&e);
    std::sort(result.begin(), result.end(),
              [](const Event* a, const Event* b) { return a->getStartTime() < b->getStartTime(); });
    return result;
}

// ============================================================================
// Write (each mutation auto-saves)
// ============================================================================

void EventStore::add(const Event& e) {
    Event copy = e;
    if (copy.getId() < 0) copy.setId(m_nextId++);
    m_events.push_back(copy);
    persist();
}

void EventStore::addBatch(const QVector<Event>& events) {
    for (const auto& e : events) {
        Event copy = e;
        if (copy.getId() < 0) copy.setId(m_nextId++);
        m_events.push_back(copy);
    }
    persist();
}

void EventStore::update(int index, const Event& e) {
    if (index >= 0 && index < m_events.size()) {
        m_events[index] = e;
        persist();
    }
}

void EventStore::remove(int index) {
    if (index >= 0 && index < m_events.size()) {
        m_events.removeAt(index);
        persist();
    }
}

void EventStore::removeWhere(std::function<bool(const Event&)> pred) {
    int before = m_events.size();
    m_events.erase(
        std::remove_if(m_events.begin(), m_events.end(), pred),
        m_events.end());
    if (m_events.size() != before)
        persist();
}

// ============================================================================
// Series helpers
// ============================================================================

bool EventStore::sameSeries(const Event& a, const Event& b) {
    return a.getTitle() == b.getTitle()
        && a.getStartTime().time() == b.getStartTime().time()
        && a.getEndTime().time()   == b.getEndTime().time();
}

bool EventStore::isSeries(const Event& target) const {
    int count = 0;
    for (const auto& e : m_events)
        if (sameSeries(e, target) && ++count > 1) return true;
    return false;
}

void EventStore::updateSeries(const Event& original, std::function<void(Event&)> mutator) {
    for (auto& e : m_events)
        if (sameSeries(e, original)) mutator(e);
    persist();
}

void EventStore::removeSeries(const Event& target) {
    removeWhere([&](const Event& e) { return sameSeries(e, target); });
}
