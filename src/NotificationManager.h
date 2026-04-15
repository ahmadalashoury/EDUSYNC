#pragma once

#include <QVector>
#include "Event.h"

// ============================================================================
// NotificationManager — macOS local event reminders
//
// Uses UNUserNotificationCenter (macOS 10.14+) to schedule a local
// notification 15 minutes before each event.
// Call requestAuthorization() once on startup.
// Call scheduleEventReminders() whenever the event store changes.
// ============================================================================

class NotificationManager {
public:
    static NotificationManager& instance();

    /// Request UNUserNotificationCenter authorization (shows system permission dialog once)
    void requestAuthorization();

    /// Cancel all pending EduSync notifications then reschedule for upcoming events
    void scheduleEventReminders(const QVector<Event>& events);

    /// Cancel all pending EduSync notifications
    void cancelAll();

private:
    NotificationManager() = default;
    bool m_authorized = false;
};
