#pragma once

#include <QString>
#include <QUuid>

// ============================================================================
// Habit — a recurring block the scheduler places daily/weekly
//
// Habits have frequency, duration, and a preferred time window.
// The scheduler attempts to protect habit time from being overwritten.
// ============================================================================

struct Habit {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString title;
    int     durationMin    = 20;    // target minutes per occurrence
    int     priority       = 3;     // 1..5

    // Frequency
    enum Frequency { Daily, Weekdays, Weekly };
    Frequency frequency = Daily;

    // Preferred time window
    enum Window { AnyTime, Morning, Midday, Afternoon, Evening };
    Window preferredWindow = AnyTime;

    // Whether the scheduler can move this if conflicts arise
    bool isProtected = false;
};
