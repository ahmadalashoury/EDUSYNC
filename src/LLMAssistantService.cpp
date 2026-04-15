#include "LLMAssistantService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

// ============================================================================
// Construction
// ============================================================================

LLMAssistantService::LLMAssistantService(QObject* parent)
    : AssistantService(parent)
    , m_endpoint(kDefaultEndpoint)
    , m_model(kDefaultModel)
{
    m_nam = new QNetworkAccessManager(this);
    loadSettings();
}

// ============================================================================
// Configuration
// ============================================================================

bool LLMAssistantService::isAvailable() const {
    return !m_apiKey.isEmpty();
}

QString LLMAssistantService::providerName() const {
    if (m_endpoint.contains("anthropic")) return "Claude";
    if (m_endpoint.contains("openai"))    return "GPT";
    if (m_endpoint.contains("localhost") || m_endpoint.contains("127.0.0.1"))
        return "Local LLM";
    return "AI Assistant";
}

void LLMAssistantService::setApiKey(const QString& key) { m_apiKey = key; }
void LLMAssistantService::setEndpoint(const QString& url) { m_endpoint = url; }
void LLMAssistantService::setModel(const QString& model) { m_model = model; }

QString LLMAssistantService::apiKey() const { return m_apiKey; }
QString LLMAssistantService::endpoint() const { return m_endpoint; }
QString LLMAssistantService::model() const { return m_model; }

void LLMAssistantService::loadSettings() {
    QSettings s("EduSync", "EduSync");
    m_apiKey   = s.value("assistant/api_key").toString();
    m_endpoint = s.value("assistant/endpoint", kDefaultEndpoint).toString();
    m_model    = s.value("assistant/model", kDefaultModel).toString();
}

void LLMAssistantService::saveSettings() const {
    QSettings s("EduSync", "EduSync");
    s.setValue("assistant/api_key",  m_apiKey);
    s.setValue("assistant/endpoint", m_endpoint);
    s.setValue("assistant/model",    m_model);
}

// ============================================================================
// Prompt construction — minimal, privacy-conscious
// ============================================================================

QString LLMAssistantService::buildPrompt(const QDate& date,
                                          const QVector<Event>& events) const {
    QStringList eventLines;
    for (const auto& e : events) {
        if (!e.isOnDate(date)) continue;
        const QString timeRange = QString("%1-%2")
            .arg(e.getStartTime().time().toString("HH:mm"),
                 e.getEndTime().time().toString("HH:mm"));

        // Extract category from description
        const QString desc = e.getDescription();
        const int sep = desc.indexOf("::");
        const QString category = sep >= 0 ? desc.left(sep).trimmed() : "Other";

        eventLines << QString("  %1  %2  [%3]").arg(timeRange, e.getTitle(), category);
    }

    const QString dayStr = date.toString("dddd, MMMM d, yyyy");
    const bool isToday = (date == QDate::currentDate());

    QString prompt = QString(
        "You are a concise schedule assistant. Analyze this %1 schedule and respond with:\n"
        "1. A brief, natural summary (1-2 sentences, max 40 words)\n"
        "2. 1-3 actionable suggestions (each max 15 words)\n\n"
        "Date: %2%3\n"
        "Events:\n%4\n\n"
        "Format your response as:\n"
        "SUMMARY: <your summary>\n"
        "SUGGEST: <suggestion 1>\n"
        "SUGGEST: <suggestion 2>\n"
        "SUGGEST: <suggestion 3>\n"
        "SCHEDULE: <event title> at HH:MM (optional — only if a concrete block makes sense)\n\n"
        "Be specific and practical. Reference actual events by name when relevant. "
        "If the day has free time and a study/work block would help, include one SCHEDULE: line."
    ).arg(
        isToday ? "today's" : "day's",
        dayStr,
        isToday ? " (today)" : "",
        eventLines.isEmpty() ? "  (no events scheduled)" : eventLines.join("\n")
    );

    return prompt;
}

// ============================================================================
// Response parsing
// ============================================================================

AssistantResponse LLMAssistantService::parseResponse(const QString& text) const {
    AssistantResponse resp;
    resp.fromLLM = true;

    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith("SUMMARY:", Qt::CaseInsensitive)) {
            resp.summary = trimmed.mid(8).trimmed();
        } else if (trimmed.startsWith("SUGGEST:", Qt::CaseInsensitive)) {
            const QString suggestion = trimmed.mid(8).trimmed();
            if (!suggestion.isEmpty())
                resp.suggestions.append(suggestion);
        } else if (trimmed.startsWith("SCHEDULE:", Qt::CaseInsensitive)) {
            // Parse "SCHEDULE: <title> at HH:MM"
            const QString raw = trimmed.mid(9).trimmed();
            const int atPos = raw.lastIndexOf(" at ", -1, Qt::CaseInsensitive);
            if (atPos > 0) {
                const QString title   = raw.left(atPos).trimmed();
                const QString timeStr = raw.mid(atPos + 4).trimmed();
                QTime t = QTime::fromString(timeStr, "HH:mm");
                if (!t.isValid()) t = QTime::fromString(timeStr, "h:mm ap");
                if (!t.isValid()) t = QTime::fromString(timeStr, "h ap");
                if (t.isValid() && !title.isEmpty()) {
                    AssistantResponse::ScheduleRequest req;
                    req.title = title;
                    req.time  = t;
                    resp.scheduleRequests.append(req);
                }
            }
        }
    }

    // Fallback if parsing failed
    if (resp.summary.isEmpty()) {
        resp.summary = text.left(200).trimmed();
        if (resp.summary.length() == 200)
            resp.summary += "...";
    }

    return resp;
}

// ============================================================================
// API call — OpenAI-compatible chat completions
// ============================================================================

void LLMAssistantService::analyze(const QDate& date,
                                   const QVector<Event>& events,
                                   Callback callback) {
    if (!isAvailable()) {
        callback(AssistantResponse{"AI assistant not configured.", {}, false});
        return;
    }

    const QString prompt = buildPrompt(date, events);

    // Build request
    QJsonObject message;
    message["role"]    = "user";
    message["content"] = prompt;

    QJsonObject body;
    body["model"]       = m_model;
    body["messages"]    = QJsonArray{message};
    body["max_tokens"]  = 300;
    body["temperature"] = 0.3;

    QUrl endpoint(m_endpoint);
    QNetworkRequest req(endpoint);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    QNetworkReply* reply = m_nam->post(req,
        QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, callback] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            callback(AssistantResponse{
                QString("Assistant unavailable: %1").arg(reply->errorString()),
                {}, false
            });
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject root = doc.object();

        // Extract content from OpenAI-compatible response format
        const QJsonArray choices = root.value("choices").toArray();
        if (choices.isEmpty()) {
            callback(AssistantResponse{"No response from assistant.", {}, false});
            return;
        }

        const QString content = choices[0].toObject()
            .value("message").toObject()
            .value("content").toString();

        callback(parseResponse(content));
    });
}
