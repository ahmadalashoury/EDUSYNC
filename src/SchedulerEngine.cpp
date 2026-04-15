#include "SchedulerEngine.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>

// ============================================================================
// Construction
// ============================================================================

SchedulerEngine::SchedulerEngine(QObject* parent) : QObject(parent) {}

// ============================================================================
// Configuration
// ============================================================================

void SchedulerEngine::setWorkingHours(int start, int end) {
    m_workStartHour = qBound(0, start, 23);
    m_workEndHour   = qBound(1, end, 24);
}

void SchedulerEngine::setBufferMinutes(int pre, int post) {
    m_bufferPre  = qBound(0, pre, 30);
    m_bufferPost = qBound(0, post, 30);
}

void SchedulerEngine::setFocusProtectionEnabled(bool enabled) {
    m_focusProtection = enabled;
}

// ============================================================================
// Event categorization
// ============================================================================

SchedulerEngine::EventCategory SchedulerEngine::categorize(const Event& e) const {
    const QString desc = e.getDescription().toLower();
    const QString title = e.getTitle().toLower();
    const int sep = desc.indexOf("::");
    const QString cat = (sep >= 0 ? desc.left(sep) : desc).trimmed();

    if (cat == "break")    return Break;
    if (cat == "exercise") return Exercise;

    if (title.contains("meeting") || title.contains("standup") ||
        title.contains("sync")    || title.contains("1:1") ||
        title.contains("review")  || title.contains("retro"))
        return Meeting;

    if (cat == "study" || cat == "work") return Focus;

    return Other;
}

// ============================================================================
// Free slot computation
// ============================================================================

QVector<SchedulerEngine::TimeSlot> SchedulerEngine::computeFreeSlots(
    const QDate& date, const QVector<Event>& busy, int minSlotMin) const
{
    const QDateTime dayStart(date, QTime(m_workStartHour, 0));
    const QDateTime dayEnd(date, QTime(m_workEndHour, 0));

    // Collect and clamp busy intervals
    struct Interval { QDateTime s, e; };
    QVector<Interval> intervals;

    for (const auto& ev : busy) {
        if (!ev.isOnDate(date)) continue;
        const QDateTime s = qMax(dayStart, ev.getStartTime());
        const QDateTime e = qMin(dayEnd, ev.getEndTime());
        if (s < e) intervals.push_back({s, e});
    }

    // Sort by start time
    std::sort(intervals.begin(), intervals.end(),
              [](const Interval& a, const Interval& b) { return a.s < b.s; });

    // Merge overlapping intervals
    QVector<Interval> merged;
    for (const auto& iv : intervals) {
        if (merged.isEmpty() || iv.s > merged.back().e)
            merged.push_back(iv);
        else
            merged.back().e = qMax(merged.back().e, iv.e);
    }

    // Invert to get free slots
    QVector<TimeSlot> free;
    QDateTime cursor = dayStart;
    for (const auto& iv : merged) {
        if (cursor < iv.s) {
            int dur = int(cursor.secsTo(iv.s) / 60);
            if (dur >= minSlotMin)
                free.push_back({cursor, iv.s});
        }
        cursor = qMax(cursor, iv.e);
    }
    if (cursor < dayEnd) {
        int dur = int(cursor.secsTo(dayEnd) / 60);
        if (dur >= minSlotMin)
            free.push_back({cursor, dayEnd});
    }

    return free;
}

// ============================================================================
// Day analysis — the core intelligence
// ============================================================================

SchedulerEngine::DayAnalysis SchedulerEngine::analyzeDay(
    const QDate& date, const QVector<Event>& events) const
{
    DayAnalysis a;

    QVector<const Event*> todays;
    for (const auto& e : events)
        if (e.isOnDate(date)) todays.push_back(&e);

    std::sort(todays.begin(), todays.end(),
              [](const Event* a, const Event* b) { return a->getStartTime() < b->getStartTime(); });

    a.totalEvents = todays.size();

    QTime firstStart, lastEnd;

    for (const Event* e : todays) {
        const int dur = qMax(0, int(e->getStartTime().secsTo(e->getEndTime()) / 60));

        switch (categorize(*e)) {
        case Focus:    a.focusMinutes += dur; break;
        case Break:    a.breakMinutes += dur; break;
        case Exercise: a.exerciseMinutes += dur; break;
        case Meeting:  a.meetingCount++; a.focusMinutes += dur; break;
        case Other:    a.focusMinutes += dur; break;
        }

        if (!firstStart.isValid() || e->getStartTime().time() < firstStart)
            firstStart = e->getStartTime().time();
        if (!lastEnd.isValid() || e->getEndTime().time() > lastEnd)
            lastEnd = e->getEndTime().time();
    }

    // Compute free time
    const int daySpan = (firstStart.isValid() && lastEnd.isValid())
        ? int(QTime(0,0).secsTo(lastEnd)/60 - QTime(0,0).secsTo(firstStart)/60)
        : 0;
    const int activeMin = a.focusMinutes + a.breakMinutes + a.exerciseMinutes;
    const int totalDayMin = (m_workEndHour - m_workStartHour) * 60;
    a.freeMinutes = qMax(0, daySpan > 0 ? daySpan - activeMin : totalDayMin - activeMin);

    // Scores
    a.workload = qBound(0, a.focusMinutes / 8 + a.totalEvents * 4 + a.meetingCount * 6, 100);
    a.balance = qBound(0,
        70 + a.exerciseMinutes / 12
           + a.breakMinutes / 8
           - qAbs(a.focusMinutes - (a.breakMinutes + a.exerciseMinutes) * 2) / 12,
        100);
    a.freePercent = totalDayMin > 0
        ? qBound(0, a.freeMinutes * 100 / totalDayMin, 100)
        : (a.totalEvents == 0 ? 100 : 50);

    // Free slots for potential scheduling
    a.freeSlots = computeFreeSlots(date, events);

    // Generate outputs
    a.summary = generateSummary(a);
    a.suggestions = generateSuggestions(a);

    return a;
}

// ============================================================================
// Summary generation — human-like, context-aware
// ============================================================================

QString SchedulerEngine::generateSummary(const DayAnalysis& a) const {
    if (a.totalEvents == 0)
        return "Your day is wide open. Perfect for deep work, planning, or a well-earned break.";

    if (a.workload >= 85)
        return QString("Intense day ahead with %1 of focused time. "
                       "Make sure to protect your breaks.")
               .arg(a.focusMinutes >= 60
                    ? QString("%1h %2m").arg(a.focusMinutes/60).arg(a.focusMinutes%60)
                    : QString("%1m").arg(a.focusMinutes));

    if (a.workload >= 65)
        return QString("Busy day with %1 event%2. "
                       "You have %3 of free time — use it wisely.")
               .arg(a.totalEvents)
               .arg(a.totalEvents > 1 ? "s" : "")
               .arg(a.freeMinutes >= 60
                    ? QString("%1h %2m").arg(a.freeMinutes/60).arg(a.freeMinutes%60)
                    : QString("%1m").arg(a.freeMinutes));

    if (a.balance >= 70 && a.workload >= 25)
        return QString("Well-balanced day. %1 of focus time with healthy breaks built in.")
               .arg(a.focusMinutes >= 60
                    ? QString("%1h %2m").arg(a.focusMinutes/60).arg(a.focusMinutes%60)
                    : QString("%1m").arg(a.focusMinutes));

    if (a.totalEvents <= 2)
        return "Light schedule today. A great opportunity for creative or strategic work.";

    if (a.meetingCount >= 3)
        return QString("Meeting-heavy day (%1 meetings). "
                       "Try to batch them and protect focus blocks.")
               .arg(a.meetingCount);

    return QString("Solid day with %1 event%2 and %3 of breathing room.")
           .arg(a.totalEvents)
           .arg(a.totalEvents > 1 ? "s" : "")
           .arg(a.freeMinutes >= 60
                ? QString("%1h %2m").arg(a.freeMinutes/60).arg(a.freeMinutes%60)
                : QString("%1m").arg(a.freeMinutes));
}

// ============================================================================
// Suggestion generation — actionable, non-generic
// ============================================================================

QStringList SchedulerEngine::generateSuggestions(const DayAnalysis& a) const {
    QStringList suggestions;

    // Break deficit
    if (a.breakMinutes < 15 && a.focusMinutes > 60)
        suggestions << "Add a 10-minute buffer after your longest block";

    // Exercise gap
    if (a.exerciseMinutes == 0 && a.totalEvents > 0 && a.freeMinutes >= 30)
        suggestions << "Block 30 minutes for movement — you have the space";

    // Overloaded
    if (a.freePercent < 15 && a.totalEvents > 3)
        suggestions << "Consider deferring one task to tomorrow";

    // Long focus without breaks
    if (a.focusMinutes > 240 && a.breakMinutes < 30)
        suggestions << "Split your long focus stretch with a walk break";

    // Available deep work time
    if (a.freeMinutes >= 90 && a.focusMinutes < 60 && a.totalEvents > 0)
        suggestions << "Block 90 minutes in the morning for deep work";

    // Meeting fragmentation
    if (a.meetingCount >= 3)
        suggestions << "Stack meetings together to create unbroken focus time";

    // Empty day
    if (a.totalEvents == 0)
        suggestions << "Plan your top 3 priorities for the day";

    // Large free slots
    for (const auto& slot : a.freeSlots) {
        if (slot.durationMin() >= 120 && suggestions.size() < 3) {
            suggestions << QString("You have a %1h open block at %2 — ideal for deep work")
                           .arg(slot.durationMin() / 60)
                           .arg(slot.start.time().toString("h:mm AP"));
            break;
        }
    }

    // Default positive
    if (suggestions.isEmpty())
        suggestions << "Looking good — keep this rhythm going";

    // Cap at 4
    while (suggestions.size() > 4)
        suggestions.removeLast();

    return suggestions;
}

// ============================================================================
// Slot scoring — how well does this slot fit a task?
// ============================================================================

double SchedulerEngine::scoreSlotForTask(const TimeSlot& slot, const Task& task) const {
    const int dur = slot.durationMin();
    if (dur < 10) return -1e9;

    const int hour = slot.start.time().hour();
    double score = 0.0;

    // Priority weight (strongest factor)
    score += 2.0 * ((task.priority - 1) / 4.0);

    // Deadline urgency
    if (task.deadline.isValid()) {
        const int minsLeft = int(QDateTime::currentDateTime().secsTo(task.deadline) / 60);
        score += 1.5 * qBound(0.0, 1.0 - (minsLeft / (60.0 * 24 * 7)), 1.0);
    }

    // Time preference match
    switch (task.preferredTime) {
    case Task::Morning:
        score += (hour >= 6 && hour <= 11) ? 1.0 : -0.3;
        break;
    case Task::Afternoon:
        score += (hour >= 12 && hour <= 17) ? 1.0 : -0.3;
        break;
    case Task::Evening:
        score += (hour >= 17 && hour <= 22) ? 1.0 : -0.3;
        break;
    case Task::Any:
        break;
    }

    // Prefer slots that fit the task duration well (avoid fragmentation)
    const double fitRatio = qMin(1.0, double(task.durationMin) / dur);
    score += 0.6 * fitRatio;

    // Slight preference for earlier slots
    score += 0.3 * (1.0 - (hour - m_workStartHour) / double(m_workEndHour - m_workStartHour));

    return score;
}

double SchedulerEngine::scoreSlotForHabit(const TimeSlot& slot, const Habit& habit) const {
    const int dur = slot.durationMin();
    if (dur < habit.durationMin) return -1e9;

    const int hour = slot.start.time().hour();
    double score = 0.0;

    score += 0.5 * ((habit.priority - 1) / 4.0);

    switch (habit.preferredWindow) {
    case Habit::Morning:   score += (hour >= 6  && hour <= 10) ? 1.5 : -0.3; break;
    case Habit::Midday:    score += (hour >= 11 && hour <= 14) ? 1.5 : -0.3; break;
    case Habit::Afternoon: score += (hour >= 14 && hour <= 17) ? 1.5 : -0.3; break;
    case Habit::Evening:   score += (hour >= 17 && hour <= 21) ? 1.5 : -0.3; break;
    case Habit::AnyTime:   break;
    }

    // Prefer shorter slots (don't waste large blocks on short habits)
    score += 0.4 * (1.0 - qMin(1.0, dur / 240.0));

    return score;
}

// ============================================================================
// Task scheduling — greedy placement with buffer insertion
// ============================================================================

SchedulerEngine::ScheduleResult SchedulerEngine::scheduleTasks(
    const QDate& date,
    const QVector<Event>& existingEvents,
    const QVector<Task>& tasks,
    const QVector<Habit>& habits) const
{
    ScheduleResult result;
    if (tasks.isEmpty() && habits.isEmpty()) return result;

    // Sort tasks by priority (desc), deadline (asc), duration (desc)
    QVector<Task> sortedTasks = tasks;
    std::sort(sortedTasks.begin(), sortedTasks.end(), [](const Task& a, const Task& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        if (a.deadline.isValid() && b.deadline.isValid())
            return a.deadline < b.deadline;
        if (a.deadline.isValid() != b.deadline.isValid())
            return a.deadline.isValid();
        return a.durationMin > b.durationMin;
    });

    // Compute free slots from existing events
    QVector<TimeSlot> freeSlots = computeFreeSlots(date, existingEvents);

    // Mutable copy for carving
    struct MutableSlot { QDateTime s, e; };
    QVector<MutableSlot> pool;
    for (const auto& fs : freeSlots)
        pool.push_back({fs.start, fs.end});

    // Place tasks
    int totalPlannedMin = 0;
    for (const Task& task : sortedTasks) {
        int remaining = task.durationMin;

        while (remaining > 0) {
            // Find best slot
            int bestIdx = -1;
            double bestScore = -1e9;
            for (int i = 0; i < pool.size(); ++i) {
                TimeSlot ts{pool[i].s, pool[i].e};
                if (ts.durationMin() < 15) continue;
                double sc = scoreSlotForTask(ts, task);
                if (sc > bestScore) { bestScore = sc; bestIdx = i; }
            }
            if (bestIdx < 0) break;

            auto& chosen = pool[bestIdx];
            const int available = int(chosen.s.secsTo(chosen.e) / 60);
            const int chunk = qMin(task.maxChunkMin, qMin(remaining, available - m_bufferPost));

            if (chunk < 10) break;

            // Create task event
            const QDateTime taskStart = chosen.s.addSecs(m_bufferPre * 60);
            const QDateTime taskEnd = taskStart.addSecs(chunk * 60);

            Event ev(task.title, "Work", taskStart, taskEnd,
                     Theme::Category::color("Work"));
            result.plannedEvents.push_back(ev);
            totalPlannedMin += chunk;

            // Advance the slot past task + post buffer
            chosen.s = taskEnd.addSecs(m_bufferPost * 60);
            if (chosen.s >= chosen.e)
                pool.removeAt(bestIdx);

            remaining -= chunk;
            if (!task.canSplit) break;
        }
    }

    // Rebuild free slots after task placement
    QVector<Event> allBusy = existingEvents;
    allBusy.append(result.plannedEvents);
    QVector<TimeSlot> remainingSlots = computeFreeSlots(date, allBusy);

    // Place habits in remaining time
    for (const Habit& habit : habits) {
        int bestIdx = -1;
        double bestScore = -1e9;
        for (int i = 0; i < remainingSlots.size(); ++i) {
            double sc = scoreSlotForHabit(remainingSlots[i], habit);
            if (sc > bestScore) { bestScore = sc; bestIdx = i; }
        }
        if (bestIdx >= 0) {
            const QDateTime hStart = remainingSlots[bestIdx].start;
            const QDateTime hEnd = hStart.addSecs(habit.durationMin * 60);

            Event ev(habit.title, "Exercise", hStart, hEnd,
                     Theme::Category::color("Exercise"));
            result.plannedEvents.push_back(ev);
        }
    }

    result.summary = QString("Planned %1 minutes of tasks and %2 habit%3.")
                     .arg(totalPlannedMin)
                     .arg(habits.size())
                     .arg(habits.size() != 1 ? "s" : "");

    return result;
}

// ============================================================================
// Conflict detection
// ============================================================================

QVector<QPair<int,int>> SchedulerEngine::detectConflicts(
    const QVector<Event>& events) const
{
    QVector<QPair<int,int>> conflicts;

    // Sort indices by start time
    QVector<int> indices(events.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](int a, int b) {
        return events[a].getStartTime() < events[b].getStartTime();
    });

    for (int i = 0; i < indices.size(); ++i) {
        for (int j = i + 1; j < indices.size(); ++j) {
            const Event& a = events[indices[i]];
            const Event& b = events[indices[j]];

            // If b starts after a ends, no more overlaps for a
            if (b.getStartTime() >= a.getEndTime()) break;

            // They overlap
            conflicts.push_back({indices[i], indices[j]});
        }
    }

    return conflicts;
}

// ============================================================================
// Conflict resolution — move flexible events
// ============================================================================

SchedulerEngine::ScheduleResult SchedulerEngine::resolveConflicts(
    const QDate& date, const QVector<Event>& events) const
{
    ScheduleResult result;
    auto conflicts = detectConflicts(events);

    if (conflicts.isEmpty()) {
        result.summary = "No conflicts detected.";
        return result;
    }

    // For now, report conflicts. Full rescheduling requires event flexibility metadata.
    result.summary = QString("Found %1 scheduling conflict%2.")
                     .arg(conflicts.size())
                     .arg(conflicts.size() > 1 ? "s" : "");

    for (const auto& [i, j] : conflicts) {
        result.conflictIndices.push_back(i);
        result.conflictIndices.push_back(j);
    }

    return result;
}
