#pragma once

#include <QObject>
#include <QDate>
#include <QDateTime>
#include <QVector>
#include <QStringList>

#include "Event.h"
#include "Task.h"
#include "Habit.h"

// ============================================================================
// SchedulerEngine — AI-powered scheduling and rescheduling
//
// Responsibilities:
//   1. Analyze existing events and compute day metrics
//   2. Schedule tasks into optimal free windows
//   3. Place habits in preferred time slots
//   4. Detect and resolve conflicts
//   5. Generate human-like summaries and suggestions
//   6. Auto-reschedule when events change
//
// The engine is stateless per-call — it takes current events + tasks + habits
// and produces an optimized plan. No persistence or network I/O.
// ============================================================================

class SchedulerEngine : public QObject {
    Q_OBJECT
public:
    explicit SchedulerEngine(QObject* parent = nullptr);

    // ── Time slot representation ────────────────────────────────────────────
    struct TimeSlot {
        QDateTime start;
        QDateTime end;
        int durationMin() const {
            return qMax(0, int(start.secsTo(end) / 60));
        }
    };

    // ── Day analysis results ────────────────────────────────────────────────
    struct DayAnalysis {
        int totalEvents    = 0;
        int focusMinutes   = 0;
        int breakMinutes   = 0;
        int exerciseMinutes= 0;
        int meetingCount   = 0;
        int freeMinutes    = 0;

        // Scores (0-100)
        int workload       = 0;
        int balance        = 0;
        int freePercent    = 0;

        // Human-readable output
        QString summary;
        QStringList suggestions;

        // Free windows available for scheduling
        QVector<TimeSlot> freeSlots;
    };

    // ── Scheduling result ───────────────────────────────────────────────────
    struct ScheduleResult {
        QVector<Event> plannedEvents;    // new events to add
        QVector<Event> movedEvents;      // events that were rescheduled
        QVector<int>   conflictIndices;  // indices of conflicting events
        QString summary;
    };

    // ── Core API ────────────────────────────────────────────────────────────

    /// Analyze a day's events and produce metrics + suggestions
    DayAnalysis analyzeDay(const QDate& date,
                           const QVector<Event>& events) const;

    /// Schedule tasks and habits into free time on the given day
    ScheduleResult scheduleTasks(const QDate& date,
                                 const QVector<Event>& existingEvents,
                                 const QVector<Task>& tasks,
                                 const QVector<Habit>& habits) const;

    /// Detect conflicts in the event list (overlapping non-flexible events)
    QVector<QPair<int,int>> detectConflicts(const QVector<Event>& events) const;

    /// Auto-reschedule: move flexible events to resolve conflicts
    ScheduleResult resolveConflicts(const QDate& date,
                                    const QVector<Event>& events) const;

    // ── Configuration ───────────────────────────────────────────────────────

    void setWorkingHours(int startHour, int endHour);
    void setBufferMinutes(int pre, int post);
    void setFocusProtectionEnabled(bool enabled);

signals:
    void analysisReady(const DayAnalysis& analysis);
    void scheduleReady(const ScheduleResult& result);

private:
    // ── Internal helpers ────────────────────────────────────────────────────

    /// Compute free time windows in [workStart, workEnd] for the given day
    QVector<TimeSlot> computeFreeSlots(const QDate& date,
                                        const QVector<Event>& busy,
                                        int minSlotMin = 10) const;

    /// Score a time slot for a given task (higher = better fit)
    double scoreSlotForTask(const TimeSlot& slot, const Task& task) const;

    /// Score a time slot for a habit placement
    double scoreSlotForHabit(const TimeSlot& slot, const Habit& habit) const;

    /// Categorize an event (focus, break, exercise, meeting)
    enum EventCategory { Focus, Break, Exercise, Meeting, Other };
    EventCategory categorize(const Event& e) const;

    /// Generate context-aware summary from analysis
    QString generateSummary(const DayAnalysis& a) const;

    /// Generate smart suggestions from analysis
    QStringList generateSuggestions(const DayAnalysis& a) const;

    // ── Settings ────────────────────────────────────────────────────────────
    int m_workStartHour = 6;
    int m_workEndHour   = 22;
    int m_bufferPre     = 5;   // minutes before task blocks
    int m_bufferPost    = 5;   // minutes after task blocks
    bool m_focusProtection = true;
};
