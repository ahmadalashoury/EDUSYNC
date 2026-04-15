#pragma once

#include <QObject>
#include <QDate>
#include <QTime>
#include <QVector>
#include <QStringList>
#include <functional>
#include "Event.h"

// ============================================================================
// AssistantService — abstract interface for the AI assistant layer
//
// This replaces the misleading "AI" label on the SchedulerEngine.
// SchedulerEngine remains the deterministic scheduling/metrics engine.
// AssistantService is the layer that produces natural-language output.
//
// Two implementations:
//   LocalAssistantService  — wraps SchedulerEngine output as-is (always available)
//   LLMAssistantService    — sends context to an LLM API for richer responses
//
// The UI (DashboardWidget) consumes AssistantResponse without caring which
// backend produced it.
// ============================================================================

struct AssistantResponse {
    QString summary;               // Natural-language day summary
    QStringList suggestions;       // Actionable suggestions (1-4 items)
    bool fromLLM = false;          // Whether this came from a real LLM

    struct ScheduleRequest {
        QString title;
        QTime   time;
    };
    QList<ScheduleRequest> scheduleRequests; // SCHEDULE: lines from LLM
};

class AssistantService : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(const AssistantResponse&)>;

    explicit AssistantService(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~AssistantService() = default;

    /// Whether this service is available and configured
    virtual bool isAvailable() const = 0;

    /// Human-readable name for the UI
    virtual QString providerName() const = 0;

    /// Generate a response for the given day context.
    /// The callback is invoked on the main thread when the response is ready.
    /// For local service this is synchronous; for LLM it's async (network).
    virtual void analyze(const QDate& date,
                         const QVector<Event>& events,
                         Callback callback) = 0;
};
