#pragma once

#include <QObject>
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QVector>
#include <QStringList>
#include <functional>
#include "Event.h"

// ============================================================================
// AssistantService — abstract interface for the AI assistant layer
//
// Two implementations:
//   LocalAssistantService  — deterministic planner (always available)
//   LLMAssistantService    — sends context to an LLM API + chat interface
//
// The UI consumes AssistantResponse without caring which backend produced it.
// ============================================================================

struct AssistantResponse {
    QString     summary;       // Natural-language day summary
    QStringList suggestions;   // Actionable bullet suggestions (1-4 items)
    bool        fromLLM = false;

    // ── Legacy: kept for backward compat with existing MainWindow code ───────
    struct ScheduleRequest {
        QString title;
        QTime   time;
    };
    QList<ScheduleRequest> scheduleRequests;

    // ── Structured proposed blocks ─────────────────────────────────────────
    // The assistant can propose concrete blocks to add. Each has a kind
    // (FocusBlock = deep work, TaskBlock = scheduled todo, HabitBlock = daily
    // habit defense, BreakBlock = rest, BufferBlock = transition) plus a
    // human-readable rationale.
    struct ProposedBlock {
        enum Kind { FocusBlock, TaskBlock, HabitBlock, BreakBlock, BufferBlock };

        Kind     kind        = FocusBlock;
        QString  title;
        QDate    date;
        QTime    time;
        int      durationMin = 60;
        QString  reason;     // why this block is proposed
        QString  category;   // "Study", "Work", "Exercise", "Break", ...
    };
    QList<ProposedBlock> proposedBlocks;
};

// ----------------------------------------------------------------------------
// Chat reply used by the conversational interface
// ----------------------------------------------------------------------------
struct ChatReply {
    QString reply;  // assistant's natural language response
    QList<AssistantResponse::ProposedBlock> actions; // concrete events to add
    bool    fromLLM = false;
};

class AssistantService : public QObject {
    Q_OBJECT
public:
    using Callback     = std::function<void(const AssistantResponse&)>;
    using ChatCallback = std::function<void(const ChatReply&)>;

    explicit AssistantService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~AssistantService() = default;

    /// Whether this service is available and configured
    virtual bool    isAvailable() const = 0;
    virtual QString providerName() const = 0;

    /// Generate an analysis/plan for the given day.
    virtual void analyze(const QDate& date,
                         const QVector<Event>& events,
                         Callback callback) = 0;

    /// Whether this service can handle free-form chat ("schedule dinner
    /// tomorrow at 7"). Default: false. LLM service overrides to true.
    virtual bool supportsChat() const { return false; }

    /// Send a user message. Default impl returns a polite not-available reply.
    virtual void chat(const QString& userMessage,
                      const QDate& today,
                      const QVector<Event>& context,
                      ChatCallback callback) {
        Q_UNUSED(userMessage); Q_UNUSED(today); Q_UNUSED(context);
        ChatReply r;
        r.reply = "Chat is disabled — add an API key in Settings to enable conversational scheduling.";
        callback(r);
    }

    /// Reset conversation history (if any).
    virtual void resetChat() {}
};
