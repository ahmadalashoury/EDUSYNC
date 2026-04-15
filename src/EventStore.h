#pragma once

#include <QObject>
#include <QVector>
#include <QDate>
#include <QString>
#include "Event.h"

// ============================================================================
// EventStore — single source of truth for all events
//
// Owns the event collection. All mutations go through here and emit signals
// so the UI reacts automatically. No widget should hold its own copy of events.
//
// Persistence: events are saved to a JSON file in the platform-standard app
// data directory. Saves use atomic write (temp file + rename) to prevent
// corruption on crash or partial write. Load runs once at startup.
// ============================================================================

class EventStore : public QObject {
    Q_OBJECT
public:
    explicit EventStore(QObject* parent = nullptr);

    // Persistence
    bool load();                          // Load from disk; returns false on error
    bool save() const;                    // Atomic save to disk
    QString storagePath() const;          // Full path to the data file

    // Read
    const QVector<Event>& all() const { return m_events; }
    QVector<const Event*> forDate(const QDate& d) const;
    int count() const { return m_events.size(); }

    // Write (each mutation auto-saves to disk)
    void add(const Event& e);
    void addBatch(const QVector<Event>& events);
    void update(int index, const Event& e);
    void remove(int index);
    void removeWhere(std::function<bool(const Event&)> pred);

    // Series helpers
    bool isSeries(const Event& target) const;
    void updateSeries(const Event& original, std::function<void(Event&)> mutator);
    void removeSeries(const Event& target);

signals:
    void changed();  // emitted after any mutation

private:
    void persist();   // save + emit changed()

    QVector<Event> m_events;
    int m_nextId = 1;

    static bool sameSeries(const Event& a, const Event& b);
};
