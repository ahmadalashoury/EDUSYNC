#include "SuperAI.h"
#include <algorithm>  // std::sort, std::min, std::max, std::clamp
#include <cmath>      // std::abs

// ============================================================================
// SuperAI.cpp
// Lightweight “AI” planner/insights engine for EduSync.
// - Analyzes a set of Event blocks to produce quick metrics and tips
// - Generates suggested daily plans from Tasks/Habits + existing events
// - Emits signals that the UI (UltraMainWindow) renders
//
// NOTE:
//  - This file avoids any persistence or network I/O.
//  - The scoring heuristics are intentionally simple and fast.
//  - Keep signatures matching SuperAI.h. If you remove a function here,
//    also remove its declaration in the header.
// ============================================================================


// ---- Simple palette helpers -------------------------------------------------
// Colors are centralized here so the UI can depend on consistent categories.
// (Hex strings chosen to match the app's light/dark neutrals.)
static QColor kTaskBlue()    { return QColor("#2f6feb"); } // task blocks (deep work)
static QColor kHabitGreen()  { return QColor("#22c55e"); } // habit blocks
static QColor kBufferGray()  { return QColor("#9aa3ab"); } // pre/post “buffer” blocks
// static QColor kInfoPurple()  { return QColor("#9b59b6"); } // (unused; removed)

// ----------------------------------------------------------------------------

SuperAI::SuperAI(QObject* p) : QObject(p) {}


// ============================================================================
// Utility helpers
// ============================================================================

/**
 * @brief minutesBetween
 * @return whole minutes between [s, e); clamped at 0 for negative ranges.
 */
int SuperAI::minutesBetween(const QDateTime& s, const QDateTime& e) {
    return std::max(0, int(s.secsTo(e) / 60));
}

/**
 * @brief overlapMin
 * @return overlap in minutes between intervals [a1, a2) and [b1, b2); 0 if none.
 * @note Currently not used by the shipped planner logic, but kept as a handy
 *       utility in case other components (or future scoring) need it.
 */
int SuperAI::overlapMin(const QDateTime& a1, const QDateTime& a2,
                        const QDateTime& b1, const QDateTime& b2) {
    const auto st = std::max(a1, b1);
    const auto en = std::min(a2, b2);
    return std::max(0, int(st.secsTo(en)/60));
}

/**
 * @brief mkEvent
 * Convenience factory: uses the title both as title and description.
 */
Event SuperAI::mkEvent(const QString& title,
                       const QDateTime& s,
                       const QDateTime& e,
                       const QColor& c) {
    return Event(title, title, s, e, c);
}


// ============================================================================
// Public API – called by UltraMainWindow
// ============================================================================

void SuperAI::setTasks(const QVector<Task>& t)   { m_tasks = t; }
void SuperAI::setHabits(const QVector<Habit>& h) { m_habits = h; }

/**
 * @brief analyzeSchedule
 * Quick rollup of counts, total time, day window, and meeting count.
 * Emits: analysisComplete(QString)
 */
void SuperAI::analyzeSchedule(const QVector<Event>& events) {
    int   totalMin = 0;
    int   meetings = 0;
    QTime first, last;

    for (const auto& e : events) {
        const int m = minutesBetween(e.getStartTime(), e.getEndTime());
        totalMin += m;

        if (e.getTitle().contains("Meeting", Qt::CaseInsensitive))
            meetings++;

        const QTime st = e.getStartTime().time();
        const QTime en = e.getEndTime().time();
        if (!first.isValid() || st < first) first = st;
        if (!last.isValid()  || en > last)  last  = en;
    }

    const QString msg = QString("Blocks: %1  |  Total: %2h%3m  |  Window: %4–%5  |  Meetings: %6")
        .arg(events.size())
        .arg(totalMin/60).arg(totalMin%60)
        .arg(first.isValid()? first.toString("hh:mm") : "--")
        .arg(last.isValid()?  last.toString("hh:mm")  : "--")
        .arg(meetings);

    emit analysisComplete(msg);
}

/**
 * @brief generateSmartSuggestions
 * Plans a day using current tasks/habits only (ignores “existing” events here).
 * Emits: suggestionsReady(QList<Event>)
 */
void SuperAI::generateSmartSuggestions(const QDate& date) {
    QVector<Event> emptyExisting;
    auto planned = planDay(date, emptyExisting, m_tasks, m_habits);

    QList<Event> list;
    for (const auto& e : planned) list.push_back(e);
    emit suggestionsReady(list);
}

/**
 * @brief provideInsights
 * Minimal insights based on current events:
 *  - Deep work count (title prefixed “🔵”)
 *  - Total buffer minutes
 *  - Longest single block
 * Emits: insightsReady(QString)
 */
void SuperAI::provideInsights(const QVector<Event>& events) {
    int deepWorkBlocks = 0, bufferMin = 0, longest = 0;

    for (const auto& e : events) {
        const auto t = e.getTitle();
        const int m  = minutesBetween(e.getStartTime(), e.getEndTime());

        if (t.startsWith("🔵")) deepWorkBlocks++;
        if (t == "Buffer")      bufferMin += m;

        longest = std::max(longest, m);
    }

    const QString s =
        QString("Deep-work blocks: %1\nBuffers: %2 min\nLongest block: %3 min\n"
                "Tip: keep deep-work blocks ≥ 60m and surround with 5–10m buffers.")
        .arg(deepWorkBlocks).arg(bufferMin).arg(longest);

    emit insightsReady(s);
}

/**
 * @brief suggestGoals
 * Placeholder goals (static for now).
 * Emits: goalsReady(QStringList)
 */
void SuperAI::suggestGoals(const QVector<Event>& events) {
    Q_UNUSED(events)
    QStringList goals{
        "Ship two 60–90m deep-work blocks before noon",
        "Book 30–45m movement break",
        "Protect 1h for admin/email batching"
    };
    emit goalsReady(goals);
}

/**
 * @brief recommendHabits
 * Placeholder habits (static for now).
 * Emits: habitsReady(QStringList)
 */
void SuperAI::recommendHabits(const QVector<Event>& events) {
    Q_UNUSED(events)
    QStringList habits{
        "⚑ Walk 20m after lunch",
        "📚 Read 25m in the evening",
        "🧘 5m breathing before first meeting"
    };
    emit habitsReady(habits);
}

/**
 * @brief analyzeStress
 * Very rough “density vs. recovery” model:
 *  - density ~ total minutes / 6
 *  - recovery ~ gap minutes / 3
 *  - risk ~ density - recovery/2 (clamped 0..100)
 * Emits: stressAnalysisReady(QString)
 */
void SuperAI::analyzeStress(const QVector<Event>& events) {
    int totalMin = 0, gaps = 0;
    QDateTime lastEnd;

    // Work on a sorted copy by start time
    QVector<Event> copy = events;
    std::sort(copy.begin(), copy.end(), [](const Event& a, const Event& b){
        return a.getStartTime() < b.getStartTime();
    });

    for (const auto& e : copy) {
        totalMin += minutesBetween(e.getStartTime(), e.getEndTime());
        if (lastEnd.isValid() && lastEnd < e.getStartTime())
            gaps += minutesBetween(lastEnd, e.getStartTime());
        lastEnd = e.getEndTime();
    }

    const int density  = std::min(100, totalMin / 6);
    const int recovery = std::max(0, std::min(100, gaps / 3));
    const int risk     = std::clamp(density - (recovery/2), 0, 100);

    const QString s =
        QString("Load: %1/100\nRecovery: %2/100\nStress risk: %3/100\n"
                "Tip: add micro-buffers (5–10m) after meetings and one 30m walk.")
        .arg(density).arg(recovery).arg(risk);

    emit stressAnalysisReady(s);
}

/**
 * @brief optimizeWorkLifeBalance
 * Splits day into focus vs. recovery minutes and produces a naive score.
 * Emits: optimizationReady(QString)
 */
void SuperAI::optimizeWorkLifeBalance(const QVector<Event>& events) {
    int focusMin = 0, recoveryMin = 0;

    for (const auto& e : events) {
        const QString t = e.getTitle().toLower();
        const int m = minutesBetween(e.getStartTime(), e.getEndTime());

        if (t.contains("buffer") || t.contains("walk") ||
            t.contains("break")  || t.contains("exercise"))
            recoveryMin += m;
        else
            focusMin += m;
    }

    // Heuristic: more recovery (up to a point) raises score; large mismatch penalizes.
    const int score = std::clamp(70 + (recoveryMin/15) - std::abs(focusMin - recoveryMin)/10, 0, 100);

    const QString s =
        QString("Balance score: %1/100\nFocus: %2m | Recovery: %3m\n"
                "Suggestion: schedule recovery up to ~35%% of total focus time.")
        .arg(score).arg(focusMin).arg(recoveryMin);

    emit optimizationReady(s);
}


// ============================================================================
// Planner core
// ============================================================================

/**
 * @brief freeWindows
 * Builds a list of free Slots in [06:00, 22:00] for a given day,
 * subtracting all overlapping “busy” events. Merges overlaps and enforces a
 * minimum slot length (minBlockMin).
 */
QVector<SuperAI::Slot> SuperAI::freeWindows(const QDate& day,
                                            const QVector<Event>& busy,
                                            int minBlockMin) const {
    const QDateTime dayStart(day, QTime(6,0));
    const QDateTime dayEnd  (day, QTime(22,0));

    struct Seg { QDateTime s,e; };
    QVector<Seg> segs;

    // Clamp each busy event to the day window and collect
    for (const auto& e : busy) {
        const QDateTime s = e.getStartTime();
        const QDateTime t = e.getEndTime();
        if (s.date() > day || t.date() < day) continue;

        const QDateTime clampedS = std::max(dayStart, s);
        const QDateTime clampedE = std::min(dayEnd,   t);
        if (clampedS < clampedE)
            segs.push_back({clampedS, clampedE});
    }

    // Merge overlaps
    std::sort(segs.begin(), segs.end(), [](const Seg& a, const Seg& b){ return a.s < b.s; });

    QVector<Seg> merged;
    for (const auto& s : segs) {
        if (merged.isEmpty() || s.s > merged.back().e) merged.push_back(s);
        else merged.back().e = std::max(merged.back().e, s.e);
    }

    // Invert merged to get free windows
    QVector<Slot> freeSlots;
    QDateTime cur = dayStart;

    for (const auto& m : merged) {
        if (cur < m.s && minutesBetween(cur, m.s) >= minBlockMin)
            freeSlots.push_back({cur, m.s});
        cur = std::max(cur, m.e);
    }

    if (cur < dayEnd && minutesBetween(cur, dayEnd) >= minBlockMin)
        freeSlots.push_back({cur, dayEnd});

    return freeSlots;
}

/**
 * @brief slotScore
 * Scores how suitable a free Slot is for a given Task.
 * Factors:
 *  - priority (strong)
 *  - urgency (deadline proximity)
 *  - circadian anchors (morning/afternoon preference)
 *  - slot length (up to 120m)
 *  - “earliness” (sooner slots get a small boost)
 */
double SuperAI::slotScore(const Slot& window, const Task& t) const {
    const int durMin = minutesBetween(window.start, window.end);
    if (durMin < 15) return -1e9; // unusable

    const int h = window.start.time().hour();

    // Circadian bias
    double circ = 0.0;
    if (t.mustMorning)    circ += (h>=7  && h<=12) ? 1.0 : -0.3;
    if (t.mustAfternoon)  circ += (h>=13 && h<=17) ? 1.0 : -0.3;

    // Deadline urgency (linear within ~1 week)
    double urgency = 0.0;
    if (t.deadline.isValid()) {
        const int minsLeft = int(QDateTime::currentDateTime().secsTo(t.deadline)/60);
        urgency = std::clamp(1.0 - (minsLeft / (60.0*24*7.0)), 0.0, 1.0);
    }

    // Small preference for earlier start & longer windows
    const double early  = 1.0 / std::max(1.0, double(QDateTime::currentDateTime().secsTo(window.start))/3600.0);
    const double length = std::min(1.0, durMin / 120.0);

    // Normalize priority (1..5) to 0..1
    const double pr = (t.priority - 1) / 4.0;

    return 1.8*pr + 1.4*urgency + 0.8*circ + 0.5*length + 0.2*early;
}

/**
 * @brief scheduleTasksIntoWindows
 * Greedy carving of tasks into free windows, inserting small
 * pre/post buffers around each task chunk.
 * - Sorts tasks by priority, deadline, then estimate
 * - Respects t.maxChunkMin and t.splitOK
 */
QVector<Event> SuperAI::scheduleTasksIntoWindows(const QDate& /*day*/,
                                                 const QVector<Slot>& windows,
                                                 QVector<Task> tasks) const {
    QVector<Event> out;
    if (tasks.isEmpty() || windows.isEmpty()) return out;

    // Order tasks: priority ↓, deadline ↑, estimate ↓
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b){
        if (a.priority != b.priority) return a.priority > b.priority;
        if (a.deadline.isValid() && b.deadline.isValid())
            return a.deadline < b.deadline;
        if (a.deadline.isValid() != b.deadline.isValid())
            return a.deadline.isValid();
        return a.estimateMin > b.estimateMin;
    });

    // Mutable copy of windows so we can carve them up
    struct MSlot { QDateTime s,e; };
    QVector<MSlot> pool; pool.reserve(windows.size());
    for (auto& w : windows) pool.push_back({w.start, w.end});

    auto carve = [&](int needMin, const Task& t, QVector<Event>& acc){
        while (needMin > 0) {
            int bestIdx = -1; double bestScore = -1e9;

            // Choose the best slot for this task
            for (int i=0;i<pool.size();++i) {
                Slot w{pool[i].s, pool[i].e};
                if (minutesBetween(w.start, w.end) < 15) continue;
                const double sc = slotScore(w, t);
                if (sc > bestScore) { bestScore = sc; bestIdx = i; }
            }
            if (bestIdx < 0) break; // nowhere to place more time

            auto &chosen = pool[bestIdx];
            const int availMin = minutesBetween(chosen.s, chosen.e);
            const int chunk    = std::min({t.maxChunkMin, needMin, availMin});

            // Carve [s, e) from the start of the chosen window
            const QDateTime s = chosen.s;
            const QDateTime e = s.addSecs(chunk*60);

            // Surround with light buffers (5m before, 10m after)
            const int pre = 5, post = 10;
            const QDateTime sBuf = s.addSecs(-pre*60);
            const QDateTime eBuf = e.addSecs( post*60);

            acc.push_back(mkEvent(QString("🔵 %1").arg(t.title), s, e, kTaskBlue()));
            acc.push_back(mkEvent("Buffer", sBuf, s, kBufferGray()));
            acc.push_back(mkEvent("Buffer", e,   eBuf, kBufferGray()));

            // Advance the chosen window past the buffer tail
            chosen.s = eBuf;
            if (chosen.s >= chosen.e) pool.removeAt(bestIdx);

            needMin -= chunk;
            if (!t.splitOK) break;  // single-chunk only
        }
        return needMin <= 0;
    };

    // Place each task
    for (const auto& t : tasks) {
        const int need = std::max(15, t.estimateMin);
        carve(need, t, out);  // ignore failure; planner is best-effort
    }
    return out;
}

/**
 * @brief scheduleHabits
 * Chooses one window per habit; light scoring by anchor and priority.
 */
QVector<Event> SuperAI::scheduleHabits(const QDate& /*day*/,
                                       const QVector<Slot>& windows,
                                       const QVector<Habit>& habits) const {
    QVector<Event> out;

    for (const auto& h : habits) {
        int    bestIdx = -1;
        double best    = -1e9;

        for (int i=0;i<windows.size();++i) {
            const int startH = windows[i].start.time().hour();

            // Prefer longer windows a bit; bias per “anchor”; plus priority
            double sc = 0.2 * (minutesBetween(windows[i].start, windows[i].end)/60.0);
            if (h.anchor=="morning")     sc += (startH<=11)                  ? 1.0 : -0.2;
            if (h.anchor=="after-lunch") sc += (startH>=12 && startH<=15)    ? 1.0 : -0.2;
            if (h.anchor=="evening")     sc += (startH>=17)                  ? 1.0 : -0.2;
            sc += 0.5 * ((h.priority-1)/4.0);

            if (sc > best) { best = sc; bestIdx = i; }
        }

        if (bestIdx >= 0) {
            const QDateTime s = windows[bestIdx].start;
            const QDateTime e = s.addSecs(h.targetMinPerDay*60);
            out.push_back(mkEvent(QString("🟢 %1").arg(h.title), s, e, kHabitGreen()));
        }
    }

    return out;
}

/**
 * @brief planDay
 * High-level orchestration:
 *  1) Find free windows from existing events
 *  2) Schedule tasks into windows (with buffers)
 *  3) Recompute free windows and place habits
 *  4) Emit a compact summary + the planned events
 *
 * Emits:
 *  - analysisComplete(QString)  : textual summary (“Planned X min …”)
 *  - plannedEventsReady(QVector<Event>) : raw planned events
 *  - suggestionsReady(QList<Event>)     : same events for the “suggestions” flow
 */
QVector<Event> SuperAI::planDay(const QDate& day,
                                const QVector<Event>& existing,
                                const QVector<Task>& tasks,
                                const QVector<Habit>& habits) {
    // 1) Free windows before planning
    const auto freeSlots = freeWindows(day, existing, /*minBlockMin*/15);

    // 2) Tasks into those windows
    const auto plannedTasks  = scheduleTasksIntoWindows(day,
                                                        freeSlots,
                                                        tasks.isEmpty()? m_tasks : tasks);

    // 3) Recompute free windows with the new task blocks
    QVector<Event> busy = existing; busy += plannedTasks;
    const auto freeAfterTasks = freeWindows(day, busy, 15);

    //    Habits in the remaining time
    const auto plannedHabits = scheduleHabits(day,
                                              freeAfterTasks,
                                              habits.isEmpty()? m_habits : habits);

    // 4) Merge & summarize
    QVector<Event> all = plannedTasks; all += plannedHabits;

    int totalTaskMin = 0;
    for (const auto& e : plannedTasks)
        if (!e.getTitle().contains("Buffer"))
            totalTaskMin += minutesBetween(e.getStartTime(), e.getEndTime());

    const QString sum = QString("Planned %1 task min and %2 habit block(s) for %3.")
        .arg(totalTaskMin)
        .arg(plannedHabits.size())
        .arg(day.toString(Qt::ISODate));

    emit analysisComplete(sum);
    emit plannedEventsReady(all);

    QList<Event> ql; for (const auto& e : all) ql.push_back(e);
    emit suggestionsReady(ql);

    return all;
}
