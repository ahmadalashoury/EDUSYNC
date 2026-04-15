#pragma once

#include <QString>
#include <QDateTime>
#include <QUuid>

// ============================================================================
// Task — a flexible work unit the scheduler can place into free time
//
// Tasks have deadlines, priorities, time preferences, and can optionally
// be split across multiple time slots.
// ============================================================================

struct Task {
    QString   id        = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString   title;
    int       durationMin  = 30;     // total effort needed
    int       priority     = 3;      // 1 (low) .. 5 (critical)
    QDateTime deadline;              // optional hard deadline
    bool      isFlexible   = true;   // can be moved by the scheduler
    bool      canSplit     = true;   // can be carved into chunks
    int       maxChunkMin  = 120;    // largest single chunk

    // Soft time preference
    enum TimePreference { Any, Morning, Afternoon, Evening };
    TimePreference preferredTime = Any;

    // Source tracking
    enum Source { Local, Google, Outlook };
    Source source = Local;
    QString providerTaskId;          // external ID from provider

    // Computed urgency (set by scheduler)
    double urgencyScore = 0.0;
};
