#pragma once

#include <QScrollArea>
#include <QDate>
#include <QVector>
#include <QStringList>
#include <QHash>
#include "Event.h"

class StatCard;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QLineEdit;
class QFrame;

// ============================================================================
// DashboardWidget — AI assistant panel (right panel)
//
// Premium layout:
//   1. Greeting / date context (small, subtle)
//   2. Summary insight (large, visually dominant)
//   3. Three arc gauges in a metrics card
//   4. Key stats row (total events, meetings, free blocks)
//   5. Smart suggestions card
//   6. Apply button (conditional)
//
// Two refresh paths:
//   - refresh() — inline fallback computation
//   - refreshFromAnalysis() — preferred, from SchedulerEngine
//   - refreshFromAssistant() — LLM-generated summary (when available)
// ============================================================================

class DashboardWidget : public QScrollArea {
    Q_OBJECT
public:
    explicit DashboardWidget(QWidget* parent = nullptr);

    void refresh(const QDate& date, const QVector<Event>& events);

    void refreshFromAnalysis(const QString& summary,
                             const QStringList& suggestions,
                             int workload, int balance,
                             int freeMinutes, int freePercent);

    void refreshFromAssistant(const QString& summary,
                              const QStringList& suggestions);

    // Chat interface
    void appendChatUser(const QString& text);
    void appendChatAssistant(const QString& text);
    /// Show a temporary "thinking" bubble; returns an id to replace later.
    int  appendChatPending();
    /// Replace a pending bubble with the final text (or append if id invalid).
    void resolveChatPending(int id, const QString& text);
    void clearChat();
    void setChatEnabled(bool enabled, const QString& placeholder);

signals:
    void applyAISuggestions();
    void userChatMessage(const QString& text);

private:
    void rebuildUI(const QDate& date,
                   const QString& summary, const QStringList& suggestions,
                   int workload, int balance, int freeMinutes, int freePercent,
                   int totalEvents, int meetingCount);

    static QString formatMinutes(int min);

    QDate        m_currentDate;
    QVBoxLayout* m_mainLayout   = nullptr;
    QLabel*      m_dateLabel    = nullptr;
    QLabel*      m_summaryLabel = nullptr;
    StatCard*    m_metricsCard  = nullptr;
    StatCard*    m_suggestCard  = nullptr;
    QPushButton* m_applyBtn     = nullptr;

    // Chat
    QFrame*      m_chatCard     = nullptr;
    QLabel*      m_chatTitle    = nullptr;
    QLabel*      m_chatHint     = nullptr;
    QVBoxLayout* m_chatLog      = nullptr;
    QLineEdit*   m_chatInput    = nullptr;
    QPushButton* m_chatSendBtn  = nullptr;
    QHash<int, QLabel*> m_pendingBubbles;
    int          m_nextPendingId = 1;

    // Cache last analysis for assistant overlay
    int m_lastWorkload    = 0;
    int m_lastBalance     = 0;
    int m_lastFreeMin     = 0;
    int m_lastFreePercent = 0;
    int m_lastTotalEvents = 0;
    int m_lastMeetings    = 0;
};
