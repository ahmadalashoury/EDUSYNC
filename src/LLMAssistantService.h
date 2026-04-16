#pragma once

#include "AssistantService.h"
#include <QJsonArray>
#include <QNetworkAccessManager>

// ============================================================================
// LLMAssistantService — LLM-backed scheduler + chat
//
// Two capabilities:
//   analyze(date, events) → structured plan (summary, suggestions, proposed
//                            blocks) using a JSON schema
//   chat(userMessage)      → conversational event creation ("schedule lunch
//                            with mom tomorrow at noon") with a rolling
//                            conversation history
//
// OpenAI-compatible chat/completions API. Configured via QSettings.
// ============================================================================

class LLMAssistantService : public AssistantService {
    Q_OBJECT
public:
    explicit LLMAssistantService(QObject* parent = nullptr);

    bool    isAvailable() const override;
    QString providerName() const override;

    void analyze(const QDate& date,
                 const QVector<Event>& events,
                 Callback callback) override;

    // Chat interface
    bool supportsChat() const override { return true; }
    void chat(const QString& userMessage,
              const QDate& today,
              const QVector<Event>& context,
              ChatCallback callback) override;
    void resetChat() override;

    // Configuration
    void setApiKey(const QString& key);
    void setEndpoint(const QString& url);
    void setModel(const QString& model);

    QString apiKey()   const;
    QString endpoint() const;
    QString model()    const;

    void loadSettings();
    void saveSettings() const;

private:
    QString buildPlanPrompt(const QDate& date, const QVector<Event>& events) const;
    QString buildChatSystemPrompt(const QDate& today,
                                  const QVector<Event>& context) const;
    AssistantResponse parseAnalyzeResponse(const QString& text) const;
    ChatReply         parseChatResponse(const QString& text) const;

    // Low-level: POST to endpoint with given messages array, return content text.
    void postChat(const QJsonArray& messages,
                  int maxTokens,
                  double temperature,
                  std::function<void(const QString& content, const QString& error)> cb);

    QNetworkAccessManager* m_nam = nullptr;
    QString m_apiKey;
    QString m_endpoint;
    QString m_model;

    // Rolling chat history (user + assistant pairs, not including system msg)
    QJsonArray m_chatHistory;
    static constexpr int kMaxHistoryMsgs = 16;

    static constexpr const char* kDefaultEndpoint = "https://api.openai.com/v1/chat/completions";
    static constexpr const char* kDefaultModel    = "gpt-4o-mini";
};
