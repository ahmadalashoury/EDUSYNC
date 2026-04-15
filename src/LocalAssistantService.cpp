#include "LocalAssistantService.h"
#include "SchedulerEngine.h"

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
    resp.summary = analysis.summary;
    resp.suggestions = analysis.suggestions;
    resp.fromLLM = false;

    callback(resp);
}
