#pragma once

#include "AssistantService.h"
#include <QNetworkAccessManager>

// ============================================================================
// LLMAssistantService — sends schedule context to an LLM for rich analysis
//
// Supports any OpenAI-compatible chat completions API (OpenAI, Anthropic
// via proxy, Ollama local, etc). Configured via:
//   - API key (stored in QSettings)
//   - Endpoint URL (defaults to OpenAI)
//   - Model name (defaults to gpt-4o-mini for cost efficiency)
//
// The prompt is carefully scoped: it receives only the day's event titles,
// times, and categories — no descriptions, no personal data beyond what's
// on the calendar. The LLM produces a summary and suggestions.
//
// If the API call fails or the key is missing, the service reports
// isAvailable() = false and the UI falls back to LocalAssistantService.
// ============================================================================

class LLMAssistantService : public AssistantService {
    Q_OBJECT
public:
    explicit LLMAssistantService(QObject* parent = nullptr);

    bool isAvailable() const override;
    QString providerName() const override;

    void analyze(const QDate& date,
                 const QVector<Event>& events,
                 Callback callback) override;

    // Configuration
    void setApiKey(const QString& key);
    void setEndpoint(const QString& url);
    void setModel(const QString& model);

    QString apiKey() const;
    QString endpoint() const;
    QString model() const;

    /// Load/save configuration from QSettings
    void loadSettings();
    void saveSettings() const;

private:
    QString buildPrompt(const QDate& date, const QVector<Event>& events) const;
    AssistantResponse parseResponse(const QString& text) const;

    QNetworkAccessManager* m_nam = nullptr;
    QString m_apiKey;
    QString m_endpoint;
    QString m_model;

    static constexpr const char* kDefaultEndpoint = "https://api.openai.com/v1/chat/completions";
    static constexpr const char* kDefaultModel    = "gpt-4o-mini";
};
