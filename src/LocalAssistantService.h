#pragma once

#include "AssistantService.h"

class SchedulerEngine;

// ============================================================================
// LocalAssistantService — wraps SchedulerEngine output into AssistantResponse
//
// Always available. No network, no API key, no latency.
// This is the fallback when no LLM provider is configured.
// The output is the same deterministic analysis from SchedulerEngine,
// presented honestly as "Schedule Analysis" rather than "AI".
// ============================================================================

class LocalAssistantService : public AssistantService {
    Q_OBJECT
public:
    explicit LocalAssistantService(SchedulerEngine* engine, QObject* parent = nullptr);

    bool isAvailable() const override { return true; }
    QString providerName() const override { return "Schedule Analysis"; }

    void analyze(const QDate& date,
                 const QVector<Event>& events,
                 Callback callback) override;

private:
    SchedulerEngine* m_engine = nullptr;
};
