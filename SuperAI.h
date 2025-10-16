#pragma once

#include <QObject>
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QColor>

#include "Event.h" // Event(title, description, start, end, color)

/**
 * @brief SuperAI
 * Lightweight planner/insights engine for EduSync.
 *
 * Responsibilities
 *  - Analyze existing Event blocks and emit quick summaries/tips
 *  - Plan a “best” day by scheduling Tasks/Habits into free time
 *  - Emit signals consumed by UI (UltraMainWindow)
 *
 * Notes
 *  - All heuristics are intentionally simple and fast.
 *  - No I/O (network/persistence) lives here.
 */
class SuperAI : public QObject {
    Q_OBJECT
public:
    explicit SuperAI(QObject* parent = nullptr);

    // ---------------------------------------------------------------------
    // Domain types
    // ---------------------------------------------------------------------

    /**
     * @brief Task
     * Unit of work you want scheduled today.
     */
    struct Task {
        QString   id;                 ///< optional external id
        QString   title;              ///< human title
        int       estimateMin = 30;   ///< effort in minutes (total, may be split)
        int       priority    = 3;    ///< 1..5 (5 = highest)
        QDateTime deadline;           ///< optional, affects urgency
        bool      mustMorning   = false; ///< soft bias for morning placement
        bool      mustAfternoon = false; ///< soft bias for afternoon placement
        bool      flexible      = true;  ///< can be moved today (reserved for future use)
        bool      splitOK       = true;  ///< may be split across windows
        int       maxChunkMin   = 120;   ///< cap for a single chunk
        QString   notes;                ///< free-form notes (not used in scoring)
    };

    /**
     * @brief Habit
     * Small recurring block to encourage, typically once per day.
     */
    struct Habit {
        QString title;                ///< habit name
        int     targetMinPerDay = 20; ///< desired minutes to schedule
        QString anchor;               ///< "morning", "after-lunch", "evening" (soft bias)
        int     priority = 3;         ///< 1..5 (5 = highest)
    };

    // ---------------------------------------------------------------------
    // Public API called from the UI
    // ---------------------------------------------------------------------

    /**
     * @brief analyzeSchedule
     * Computes quick totals and day window from events and emits a summary.
     * Emits: analysisComplete(QString)
     */
    void analyzeSchedule(const QVector<Event>& events);

    /**
     * @brief generateSmartSuggestions
     * Plans the given date using current pools (m_tasks/m_habits) only.
     * Emits: suggestionsReady(QList<Event>)
     */
    void generateSmartSuggestions(const QDate& date);

    /**
     * @brief provideInsights
     * Emits simple insights (deep-work count, buffers, longest block).
     * Emits: insightsReady(QString)
     */
    void provideInsights(const QVector<Event>& events);

    /**
     * @brief suggestGoals
     * Emits a few static, generic goals for now.
     * Emits: goalsReady(QStringList)
     */
    void suggestGoals(const QVector<Event>& events);

    /**
     * @brief recommendHabits
     * Emits a few static, generic habits for now.
     * Emits: habitsReady(QStringList)
     */
    void recommendHabits(const QVector<Event>& events);

    /**
     * @brief analyzeStress
     * Naive density vs. recovery model -> “risk” signal.
     * Emits: stressAnalysisReady(QString)
     */
    void analyzeStress(const QVector<Event>& events);

    /**
     * @brief optimizeWorkLifeBalance
     * Simple focus vs. recovery minutes -> score and suggestion.
     * Emits: optimizationReady(QString)
     */
    void optimizeWorkLifeBalance(const QVector<Event>& events);

    /**
     * @brief planDay
     * Orchestrates planning for a day:
     *  1) Compute free windows from existing
     *  2) Schedule tasks (with light buffers)
     *  3) Recompute free windows and place habits
     *  4) Emit summary + planned blocks
     *
     * Emits:
     *  - analysisComplete(QString)
     *  - plannedEventsReady(QVector<Event>)
     *  - suggestionsReady(QList<Event>)
     */
    QVector<Event> planDay(const QDate& day,
                           const QVector<Event>& existing,
                           const QVector<Task>& tasks,
                           const QVector<Habit>& habits);

    // ---------------------------------------------------------------------
    // Pools (optional). The planner uses these when task/habit args are empty.
    // ---------------------------------------------------------------------
    void setTasks(const QVector<Task>& tasks);
    void setHabits(const QVector<Habit>& habits);
    const QVector<Task>&   tasks()  const { return m_tasks;  }
    const QVector<Habit>&  habits() const { return m_habits; }

signals:
    // High-level text outputs
    void analysisComplete(const QString& text);
    void insightsReady(const QString& text);
    void goalsReady(const QStringList& goals);
    void habitsReady(const QStringList& habits);
    void stressAnalysisReady(const QString& text);
    void optimizationReady(const QString& text);

    // Concrete block suggestions (used by multiple UI surfaces)
    void suggestionsReady(const QList<Event>& events);
    void plannedEventsReady(const QVector<Event>& suggestions);

private:
    // ---------------------------------------------------------------------
    // Internal helpers
    // ---------------------------------------------------------------------

    /// Simple time window used during planning.
    struct Slot { QDateTime start; QDateTime end; };

    /**
     * @brief freeWindows
     * Compute free Slots in [06:00, 22:00] for @day given busy events.
     * Merges overlaps and enforces minBlockMin.
     */
    QVector<Slot> freeWindows(const QDate& day,
                              const QVector<Event>& busy,
                              int minBlockMin = 15) const;

    /**
     * @brief mkEvent
     * Convenience factory: uses title for both title and description.
     */
    static Event mkEvent(const QString& title,
                         const QDateTime& s,
                         const QDateTime& e,
                         const QColor& c);

    /**
     * @brief minutesBetween
     * Whole minutes between [s, e); clamped at 0 for negative ranges.
     */
    static int minutesBetween(const QDateTime& s, const QDateTime& e);

    /**
     * @brief overlapMin
     * Overlap in minutes between [a1, a2) and [b1, b2); 0 if none.
     * (Not used by current heuristics, but kept available.)
     */
    static int overlapMin(const QDateTime& a1, const QDateTime& a2,
                          const QDateTime& b1, const QDateTime& b2);

    /**
     * @brief slotScore
     * Suitability of a free Slot for a Task (priority, urgency, circadian, length, earliness).
     */
    double slotScore(const Slot& window, const Task& t) const;

    /**
     * @brief scheduleTasksIntoWindows
     * Greedy carving of tasks into free windows, inserting small pre/post buffers.
     */
    QVector<Event> scheduleTasksIntoWindows(const QDate& day,
                                            const QVector<Slot>& windows,
                                            QVector<Task> tasks) const;

    /**
     * @brief scheduleHabits
     * Places one block per habit into remaining windows using a light bias by anchor/priority.
     */
    QVector<Event> scheduleHabits(const QDate& day,
                                  const QVector<Slot>& windows,
                                  const QVector<Habit>& habits) const;

private:
    QVector<Task>  m_tasks;   ///< task pool used by generateSmartSuggestions/planDay
    QVector<Habit> m_habits;  ///< habit pool used by generateSmartSuggestions/planDay
};
