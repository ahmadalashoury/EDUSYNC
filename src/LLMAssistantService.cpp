#include "LLMAssistantService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QRegularExpression>

// ============================================================================
// Construction / configuration
// ============================================================================

LLMAssistantService::LLMAssistantService(QObject* parent)
    : AssistantService(parent)
    , m_endpoint(kDefaultEndpoint)
    , m_model(kDefaultModel)
{
    m_nam = new QNetworkAccessManager(this);
    loadSettings();
}

bool LLMAssistantService::isAvailable() const { return !m_apiKey.isEmpty(); }

QString LLMAssistantService::providerName() const {
    if (m_endpoint.contains("anthropic")) return "Claude";
    if (m_endpoint.contains("openai"))    return "GPT";
    if (m_endpoint.contains("localhost") || m_endpoint.contains("127.0.0.1"))
        return "Local LLM";
    return "AI Assistant";
}

void LLMAssistantService::setApiKey(const QString& key)    { m_apiKey = key; }
void LLMAssistantService::setEndpoint(const QString& url)  { m_endpoint = url; }
void LLMAssistantService::setModel(const QString& model)   { m_model = model; }

QString LLMAssistantService::apiKey()   const { return m_apiKey; }
QString LLMAssistantService::endpoint() const { return m_endpoint; }
QString LLMAssistantService::model()    const { return m_model; }

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

void LLMAssistantService::resetChat() {
    m_chatHistory = QJsonArray{};
}

// ============================================================================
// Prompt construction
// ============================================================================

static QString formatEventsFor(const QDate& anchor, const QVector<Event>& events,
                               int daysForward = 2) {
    QStringList lines;
    for (const Event& e : events) {
        const QDate sd = e.getStartTime().date();
        if (sd < anchor.addDays(-1) || sd > anchor.addDays(daysForward)) continue;

        const QString desc = e.getDescription();
        const int sep = desc.indexOf("::");
        const QString category = sep >= 0 ? desc.left(sep).trimmed() : "Other";

        lines << QString("  %1  %2-%3  %4  [%5]")
            .arg(sd.toString("yyyy-MM-dd"),
                 e.getStartTime().time().toString("HH:mm"),
                 e.getEndTime().time().toString("HH:mm"),
                 e.getTitle(),
                 category);
    }
    return lines.isEmpty() ? QString("  (no events)") : lines.join("\n");
}

QString LLMAssistantService::buildPlanPrompt(const QDate& date,
                                             const QVector<Event>& events) const {
    const QString dayStr = date.toString("dddd, MMMM d, yyyy");
    const bool isToday = (date == QDate::currentDate());

    return QString(
        "You are a senior scheduling assistant that protects deep work, defends "
        "habits, and produces a task-first day plan. Respond ONLY with a JSON "
        "object — no prose before or after — matching this schema:\n"
        "{\n"
        "  \"summary\": string (1-2 sentences, task-first, <= 40 words),\n"
        "  \"suggestions\": [string, ...] (1-4 concrete actions, <= 15 words each),\n"
        "  \"proposed_blocks\": [\n"
        "    {\n"
        "      \"kind\": \"focus\" | \"task\" | \"habit\" | \"break\" | \"buffer\",\n"
        "      \"title\": string,\n"
        "      \"date\":  \"YYYY-MM-DD\",\n"
        "      \"time\":  \"HH:MM\" (24h),\n"
        "      \"duration_min\": integer (15..180),\n"
        "      \"category\": \"Study\"|\"Work\"|\"Exercise\"|\"Break\"|\"Meeting\"|\"Other\",\n"
        "      \"reason\": string (why this block, <= 18 words)\n"
        "    }, ...\n"
        "  ]\n"
        "}\n"
        "Rules:\n"
        "- Prefer 1 focus block (90min) in the day's longest free window when >= 60min is free.\n"
        "- Add a habit block for exercise/movement if none is scheduled and time allows.\n"
        "- Add a break block on heavy meeting days (>=3 meetings).\n"
        "- Do NOT overlap proposed blocks with existing events.\n"
        "- If the day is already optimized, return an empty proposed_blocks array.\n\n"
        "Date: %1%2\n"
        "Existing events (today +/- 1 day):\n%3\n"
    ).arg(
        dayStr,
        isToday ? " (today)" : "",
        formatEventsFor(date, events)
    );
}

QString LLMAssistantService::buildChatSystemPrompt(const QDate& today,
                                                    const QVector<Event>& context) const {
    const QDateTime nowDt = QDateTime::currentDateTime();

    return QString(
        "You are EduSync's conversational scheduling assistant. The user will "
        "create events, reschedule, or ask questions about their calendar in "
        "natural language. Respond ONLY with a JSON object — no prose before "
        "or after — matching:\n"
        "{\n"
        "  \"reply\": string (REQUIRED, never empty — 1-2 short sentences),\n"
        "  \"actions\": [\n"
        "    {\n"
        "      \"kind\": \"focus\" | \"task\" | \"habit\" | \"break\" | \"buffer\",\n"
        "      \"title\": string,\n"
        "      \"date\":  \"YYYY-MM-DD\",\n"
        "      \"time\":  \"HH:MM\" (24h),\n"
        "      \"duration_min\": integer (15..240),\n"
        "      \"category\": \"Study\"|\"Work\"|\"Exercise\"|\"Break\"|\"Meeting\"|\"Other\",\n"
        "      \"reason\": string (<= 18 words)\n"
        "    }, ...\n"
        "  ]\n"
        "}\n"
        "Rules — follow strictly:\n"
        "- The `reply` field MUST always contain a human-readable message, "
        "even when there are no actions. Never return an empty reply.\n"
        "- Resolve relative times against the CURRENT date/time below.\n"
        "- If the user gives a bare time (e.g. \"9pm\", \"at 3\") and that time "
        "is in the past today, assume they mean the NEXT occurrence (tomorrow). "
        "Do NOT refuse with \"in the past\".\n"
        "- If the user explicitly names a past time with a past date, then "
        "politely ask for clarification in the reply and return empty actions.\n"
        "- If the user asks a question (no scheduling intent), answer it "
        "concisely in reply using the known events, and return empty actions.\n"
        "- Questions like \"what's my busiest day\", \"when am I free\", "
        "\"what's on tomorrow\": answer directly from the known events list.\n"
        "- Default duration: 60min meetings, 30min errands, 90min focus blocks.\n"
        "- Choose category from the title; for \"meeting\" use Meeting.\n\n"
        "Current date/time: %1 (%2)\n"
        "Known events (+/- a week):\n%3\n"
    ).arg(
        nowDt.toString("yyyy-MM-dd HH:mm"),
        nowDt.toString("dddd"),
        formatEventsFor(today, context, 6)
    );
}

// ============================================================================
// Response parsing — robust JSON extraction
// ============================================================================

// Extract a JSON object from an arbitrary LLM response. Models occasionally
// wrap output in markdown code fences or preamble; tolerate that.
static QJsonObject extractJsonObject(const QString& text) {
    // Strip code fences
    QString s = text.trimmed();
    if (s.startsWith("```")) {
        const int nl = s.indexOf('\n');
        if (nl >= 0) s = s.mid(nl + 1);
        if (s.endsWith("```")) s.chop(3);
        s = s.trimmed();
    }

    // Try direct parse
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject())
        return doc.object();

    // Fallback: find first '{' and balanced '}'
    const int open = s.indexOf('{');
    if (open < 0) return {};
    int depth = 0;
    int close = -1;
    for (int i = open; i < s.size(); ++i) {
        if (s[i] == '{') ++depth;
        else if (s[i] == '}') {
            if (--depth == 0) { close = i; break; }
        }
    }
    if (close < 0) return {};
    doc = QJsonDocument::fromJson(s.mid(open, close - open + 1).toUtf8(), &err);
    return (err.error == QJsonParseError::NoError && doc.isObject()) ? doc.object() : QJsonObject{};
}

static AssistantResponse::ProposedBlock::Kind kindFromString(const QString& s) {
    const QString t = s.toLower();
    if (t == "task")   return AssistantResponse::ProposedBlock::TaskBlock;
    if (t == "habit")  return AssistantResponse::ProposedBlock::HabitBlock;
    if (t == "break")  return AssistantResponse::ProposedBlock::BreakBlock;
    if (t == "buffer") return AssistantResponse::ProposedBlock::BufferBlock;
    return AssistantResponse::ProposedBlock::FocusBlock;
}

static AssistantResponse::ProposedBlock parseBlock(const QJsonObject& o) {
    AssistantResponse::ProposedBlock b;
    b.kind        = kindFromString(o.value("kind").toString());
    b.title       = o.value("title").toString();
    b.date        = QDate::fromString(o.value("date").toString(), "yyyy-MM-dd");
    b.time        = QTime::fromString(o.value("time").toString(), "HH:mm");
    if (!b.time.isValid())
        b.time = QTime::fromString(o.value("time").toString(), "h:mm");
    b.durationMin = qBound(5, o.value("duration_min").toInt(60), 480);
    b.category    = o.value("category").toString();
    b.reason      = o.value("reason").toString();
    return b;
}

AssistantResponse LLMAssistantService::parseAnalyzeResponse(const QString& text) const {
    AssistantResponse resp;
    resp.fromLLM = true;

    const QJsonObject root = extractJsonObject(text);
    resp.summary = root.value("summary").toString().trimmed();

    for (const QJsonValue& v : root.value("suggestions").toArray()) {
        const QString s = v.toString().trimmed();
        if (!s.isEmpty()) resp.suggestions.append(s);
    }

    for (const QJsonValue& v : root.value("proposed_blocks").toArray()) {
        auto b = parseBlock(v.toObject());
        if (b.title.isEmpty() || !b.date.isValid() || !b.time.isValid()) continue;
        resp.proposedBlocks.append(b);

        // Also populate legacy scheduleRequests for backward compat
        AssistantResponse::ScheduleRequest leg;
        leg.title = b.title;
        leg.time  = b.time;
        resp.scheduleRequests.append(leg);
    }

    // Fallback: if JSON parse failed, salvage text as summary.
    if (resp.summary.isEmpty() && resp.suggestions.isEmpty()
        && resp.proposedBlocks.isEmpty()) {
        resp.summary = text.left(240).trimmed();
    }
    return resp;
}

ChatReply LLMAssistantService::parseChatResponse(const QString& text) const {
    ChatReply r;
    r.fromLLM = true;

    const QJsonObject root = extractJsonObject(text);
    r.reply = root.value("reply").toString().trimmed();

    for (const QJsonValue& v : root.value("actions").toArray()) {
        auto b = parseBlock(v.toObject());
        if (b.title.isEmpty() || !b.date.isValid() || !b.time.isValid()) continue;
        r.actions.append(b);
    }

    if (r.reply.isEmpty()) r.reply = text.left(240).trimmed();
    return r;
}

// ============================================================================
// Network helper
// ============================================================================

void LLMAssistantService::postChat(const QJsonArray& messages,
                                    int maxTokens,
                                    double temperature,
                                    std::function<void(const QString&, const QString&)> cb) {
    QJsonObject body;
    body["model"]       = m_model;
    body["messages"]    = messages;
    body["max_tokens"]  = maxTokens;
    body["temperature"] = temperature;

    // Only send response_format on models we know accept it. Older gpt-3.5,
    // custom proxies, Groq/Together llama endpoints, and Ollama often reject
    // the parameter with a 400 "unsupported parameter" error.
    const QString m = m_model.toLower();
    const bool supportsJsonMode =
        m.contains("gpt-4o") || m.contains("gpt-4-turbo")
        || m.contains("gpt-4.1") || m.startsWith("o1") || m.startsWith("o3")
        || m.contains("gpt-3.5-turbo-1106") || m.contains("gpt-3.5-turbo-0125");
    if (supportsJsonMode) {
        QJsonObject respFmt;
        respFmt["type"] = "json_object";
        body["response_format"] = respFmt;
    }

    QNetworkRequest req{QUrl(m_endpoint)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    QNetworkReply* reply = m_nam->post(req,
        QJsonDocument(body).toJson(QJsonDocument::Compact));

    QObject::connect(reply, &QNetworkReply::finished, this, [reply, cb] {
        reply->deleteLater();

        const QByteArray rawBody = reply->readAll();
        const int httpStatus = reply
            ->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // Helper: extract a useful error message from an OpenAI-style body.
        auto extractApiError = [&](const QByteArray& b) -> QString {
            if (b.isEmpty()) return {};
            QJsonParseError je;
            const auto doc = QJsonDocument::fromJson(b, &je);
            if (je.error != QJsonParseError::NoError) return QString::fromUtf8(b.left(300));
            const QJsonObject root = doc.object();
            // OpenAI / OpenRouter / most proxies nest under "error"
            const QJsonObject err = root.value("error").toObject();
            if (!err.isEmpty()) {
                QString msg = err.value("message").toString();
                const QString code = err.value("code").toString();
                const QString type = err.value("type").toString();
                if (!code.isEmpty()) msg += QString(" (%1)").arg(code);
                else if (!type.isEmpty()) msg += QString(" (%1)").arg(type);
                return msg;
            }
            // Some providers put the message at the top level
            const QString topMsg = root.value("message").toString();
            if (!topMsg.isEmpty()) return topMsg;
            return QString::fromUtf8(b.left(300));
        };

        if (reply->error() != QNetworkReply::NoError) {
            QString msg = extractApiError(rawBody);
            if (msg.isEmpty()) msg = reply->errorString();
            if (httpStatus > 0)
                msg = QString("HTTP %1 — %2").arg(httpStatus).arg(msg);
            cb(QString(), msg);
            return;
        }

        // 2xx but might still be empty / malformed
        const QJsonDocument doc = QJsonDocument::fromJson(rawBody);
        const QJsonObject root = doc.object();

        // Some providers return a top-level "error" even with HTTP 200
        if (root.contains("error")) {
            cb(QString(), extractApiError(rawBody));
            return;
        }

        const QJsonArray choices = root.value("choices").toArray();
        if (choices.isEmpty()) {
            cb(QString(), QString("Empty response from provider (HTTP %1)").arg(httpStatus));
            return;
        }
        const QString content = choices[0].toObject()
            .value("message").toObject().value("content").toString();
        cb(content, QString());
    });
}

// ============================================================================
// Public API
// ============================================================================

void LLMAssistantService::analyze(const QDate& date,
                                   const QVector<Event>& events,
                                   Callback callback) {
    if (!isAvailable()) {
        callback(AssistantResponse{"AI assistant not configured.", {}, false, {}, {}});
        return;
    }

    QJsonObject sys;
    sys["role"]    = "system";
    sys["content"] = "You are an expert calendar planner. Respond only in the JSON schema specified in the user message.";

    QJsonObject user;
    user["role"]    = "user";
    user["content"] = buildPlanPrompt(date, events);

    postChat({sys, user}, 600, 0.2,
        [this, callback](const QString& content, const QString& err) {
            if (!err.isEmpty()) {
                AssistantResponse r;
                r.summary = QString("Assistant unavailable: %1").arg(err);
                callback(r);
                return;
            }
            callback(parseAnalyzeResponse(content));
        });
}

void LLMAssistantService::chat(const QString& userMessage,
                                const QDate& today,
                                const QVector<Event>& context,
                                ChatCallback callback) {
    if (!isAvailable()) {
        ChatReply r;
        r.reply = "Chat is disabled — add an API key in Settings to enable conversational scheduling.";
        callback(r);
        return;
    }

    QJsonObject sys;
    sys["role"]    = "system";
    sys["content"] = buildChatSystemPrompt(today, context);

    QJsonArray messages;
    messages.append(sys);
    // Append rolling history
    for (const QJsonValue& v : m_chatHistory) messages.append(v);

    // Append this user turn
    QJsonObject u;
    u["role"]    = "user";
    u["content"] = userMessage;
    messages.append(u);

    postChat(messages, 400, 0.3,
        [this, callback, userMessage](const QString& content, const QString& err) {
            if (!err.isEmpty()) {
                ChatReply r;
                r.reply = QString("Assistant unavailable: %1").arg(err);
                callback(r);
                return;
            }

            ChatReply r = parseChatResponse(content);

            // Commit to history so follow-ups have context
            QJsonObject userTurn;
            userTurn["role"] = "user";
            userTurn["content"] = userMessage;
            m_chatHistory.append(userTurn);

            QJsonObject asstTurn;
            asstTurn["role"] = "assistant";
            asstTurn["content"] = content;
            m_chatHistory.append(asstTurn);

            // Trim history
            while (m_chatHistory.size() > kMaxHistoryMsgs) m_chatHistory.removeFirst();

            callback(r);
        });
}
