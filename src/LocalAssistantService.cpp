#include "LocalAssistantService.h"
#include "SchedulerEngine.h"

#include <QDateTime>
#include <algorithm>

// ============================================================================
// LocalAssistantService — on-device planner (no network)
//
// Produces a real, prescriptive plan — not just a summary. It:
//   1. Scores the day's load (meetings, focus, breaks)
//   2. Finds the LONGEST contiguous free window and proposes a Focus block
//      (deep-work defense) — Reclaim/Motion-style
//   3. Detects missing daily habits (exercise, break) and proposes slots
//   4. Produces a concise, task-first morning brief
// ============================================================================

namespace {

constexpr int kMinFocusMin     = 45;   // don't propose focus shorter than this
constexpr int kIdealFocusMin   = 90;   // prefer blocks this long
constexpr int kMaxFocusMin     = 120;  // cap single block
constexpr int kHabitTargetMin  = 30;

// Find the single longest free slot on the given day.
SchedulerEngine::TimeSlot longestSlot(const QVector<SchedulerEngine::TimeSlot>& windows) {
    SchedulerEngine::TimeSlot best;
    int bestMin = 0;
    for (const auto& s : windows) {
        const int m = s.durationMin();
        if (m > bestMin) { bestMin = m; best = s; }
    }
    return best;
}

// Pick a reasonable start time within a slot for a focus block.
// Prefer morning edge (9-11am) if the slot spans it; otherwise the slot's start.
QTime focusStartWithin(const SchedulerEngine::TimeSlot& s, int durationMin) {
    const QTime slotStart = s.start.time();
    const QTime slotEnd   = s.end.time();
    const QTime morning(9, 0);
    const QTime morningUpper(11, 0);

    // If slot covers 9am and there's room for the block starting at 9, use 9.
    if (slotStart <= morning && morning.addSecs(durationMin * 60) <= slotEnd)
        return morning;

    // If slot starts in morning window, snap to the slot start but round to :00 or :30.
    if (slotStart <= morningUpper) return slotStart;

    return slotStart;
}

// Parse Event category out of "Category :: description" format.
QString extractCategory(const Event& e) {
    const QString desc = e.getDescription();
    const int sep = desc.indexOf("::");
    return sep >= 0 ? desc.left(sep).trimmed() : "Other";
}

bool hasAnyWithCategory(const QVector<Event>& events, const QDate& date,
                       const QStringList& cats) {
    for (const Event& e : events) {
        if (!e.isOnDate(date)) continue;
        const QString c = extractCategory(e).toLower();
        for (const QString& want : cats) {
            if (c == want.toLower()) return true;
        }
    }
    return false;
}

QString greetingForTime() {
    const int h = QTime::currentTime().hour();
    if (h < 12) return "Good morning.";
    if (h < 17) return "Good afternoon.";
    return "Good evening.";
}

} // namespace

LocalAssistantService::LocalAssistantService(SchedulerEngine* engine, QObject* parent)
    : AssistantService(parent)
    , m_engine(engine)
{
}

void LocalAssistantService::analyze(const QDate& date,
                                     const QVector<Event>& events,
                                     Callback callback) {
    auto analysis = m_engine->analyzeDay(date, events);

    AssistantResponse resp;
    resp.fromLLM = false;

    // ── 1. Build a task-first summary ──────────────────────────────────────
    QStringList summaryParts;

    const bool isToday = (date == QDate::currentDate());
    if (isToday) summaryParts << greetingForTime();

    if (analysis.totalEvents == 0) {
        summaryParts << "Your calendar is clear.";
    } else {
        const int focusH = analysis.focusMinutes / 60;
        const int meet   = analysis.meetingCount;
        QStringList chips;
        if (meet > 0)   chips << QString("%1 meeting%2").arg(meet).arg(meet == 1 ? "" : "s");
        if (focusH > 0) chips << QString("%1h focus").arg(focusH);
        if (!chips.isEmpty())
            summaryParts << QString("You have %1 on %2.")
                .arg(chips.join(" and "),
                     isToday ? "the docket" : "this day");
    }

    const int freeH = analysis.freeMinutes / 60;
    if (freeH >= 2)
        summaryParts << QString("%1h unclaimed — I'll defend some of it for deep work.").arg(freeH);
    else if (analysis.freeMinutes >= 60)
        summaryParts << "Just under an hour of free time — tight day.";

    resp.summary = summaryParts.join(" ");
    if (resp.summary.isEmpty()) resp.summary = analysis.summary;

    // ── 2. Start from SchedulerEngine suggestions ──────────────────────────
    resp.suggestions = analysis.suggestions;

    // ── 3. Deep-work defense: propose biggest free window as a Focus block ─
    const auto best = longestSlot(analysis.freeSlots);
    const int bestMin = best.durationMin();
    if (bestMin >= kMinFocusMin) {
        const int dur = qBound(kMinFocusMin, bestMin, kMaxFocusMin);
        const int idealDur = qMin(dur, kIdealFocusMin);

        AssistantResponse::ProposedBlock b;
        b.kind        = AssistantResponse::ProposedBlock::FocusBlock;
        b.title       = "Deep Work";
        b.date        = date;
        b.time        = focusStartWithin(best, idealDur);
        b.durationMin = idealDur;
        b.category    = "Work";
        b.reason      = QString("You have a %1-minute free window — reserving %2 min for focused work.")
                            .arg(bestMin).arg(idealDur);
        resp.proposedBlocks.append(b);

        resp.suggestions.prepend(
            QString("Block %1–%2 for deep work (%3 min protected).")
                .arg(b.time.toString("h:mm ap"),
                     b.time.addSecs(idealDur * 60).toString("h:mm ap"),
                     QString::number(idealDur)));

        // Back-compat: legacy schedule request
        AssistantResponse::ScheduleRequest leg;
        leg.title = b.title;
        leg.time  = b.time;
        resp.scheduleRequests.append(leg);
    }

    // ── 4. Habit defense: suggest exercise/break if missing ────────────────
    const bool hasExercise = hasAnyWithCategory(events, date, {"Exercise", "Workout"});
    const bool hasBreak    = hasAnyWithCategory(events, date, {"Break", "Rest"});

    if (!hasExercise && analysis.freeMinutes >= 45) {
        // Pick a free slot in the late afternoon if possible
        QTime when(17, 30);
        for (const auto& s : analysis.freeSlots) {
            if (s.start.time() >= QTime(16, 0) && s.durationMin() >= kHabitTargetMin) {
                when = s.start.time();
                break;
            }
        }

        AssistantResponse::ProposedBlock b;
        b.kind        = AssistantResponse::ProposedBlock::HabitBlock;
        b.title       = "Move / Exercise";
        b.date        = date;
        b.time        = when;
        b.durationMin = kHabitTargetMin;
        b.category    = "Exercise";
        b.reason      = "No movement on your calendar — small window protects the habit.";
        resp.proposedBlocks.append(b);

        resp.suggestions.append(QString("Protect 30 min for movement around %1.")
                                    .arg(when.toString("h:mm ap")));
    }

    if (!hasBreak && analysis.meetingCount >= 3) {
        // Insert a break after the 2nd meeting if possible
        QTime breakTime(15, 0);
        AssistantResponse::ProposedBlock b;
        b.kind        = AssistantResponse::ProposedBlock::BreakBlock;
        b.title       = "Break";
        b.date        = date;
        b.time        = breakTime;
        b.durationMin = 15;
        b.category    = "Break";
        b.reason      = "Heavy meeting day — a 15-min reset helps sustain focus.";
        resp.proposedBlocks.append(b);

        resp.suggestions.append("Add a 15-min break mid-afternoon to reset.");
    }

    // ── 5. Trim to 4 suggestions max ────────────────────────────────────────
    while (resp.suggestions.size() > 4) resp.suggestions.removeLast();

    callback(resp);
}
