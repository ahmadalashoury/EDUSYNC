#include "DashboardWidget.h"
#include "StatCard.h"
#include "Theme.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QFrame>
#include <QScrollBar>
#include <QTimer>
#include <algorithm>
#include <cmath>

// ============================================================================
// Construction
// ============================================================================

DashboardWidget::DashboardWidget(QWidget* parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* container = new QWidget;
    m_mainLayout = new QVBoxLayout(container);
    m_mainLayout->setContentsMargins(Theme::Space::L, Theme::Space::XL,
                                     Theme::Space::L, Theme::Space::L);
    m_mainLayout->setSpacing(0);

    // Date context — small, subtle, grounds the panel
    m_dateLabel = new QLabel(container);
    m_dateLabel->setFont(Theme::Font::caption());
    m_mainLayout->addWidget(m_dateLabel);
    m_mainLayout->addSpacing(Theme::Space::M);

    // Summary insight — large, visually dominant
    m_summaryLabel = new QLabel(container);
    QFont sumFont = Theme::Font::heading();
    sumFont.setPointSize(20);
    sumFont.setWeight(QFont::DemiBold);
    m_summaryLabel->setFont(sumFont);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setMinimumHeight(72);
    m_mainLayout->addWidget(m_summaryLabel);
    m_mainLayout->addSpacing(Theme::Space::XL);

    // Metrics card — fewer, sharper metrics
    m_metricsCard = new StatCard("Today at a glance", container);
    m_mainLayout->addWidget(m_metricsCard);
    m_mainLayout->addSpacing(Theme::Space::M);

    // Suggestions card
    m_suggestCard = new StatCard("Suggestions", container);
    m_mainLayout->addWidget(m_suggestCard);
    m_mainLayout->addSpacing(Theme::Space::L);

    // Apply button
    m_applyBtn = new QPushButton("Apply Suggestions", container);
    m_applyBtn->setObjectName("accentBtn");
    m_applyBtn->setCursor(Qt::PointingHandCursor);
    m_applyBtn->setVisible(false);
    connect(m_applyBtn, &QPushButton::clicked, this, &DashboardWidget::applyAISuggestions);
    m_mainLayout->addWidget(m_applyBtn);

    m_mainLayout->addSpacing(Theme::Space::L);

    // ── Chat card — natural-language scheduling ─────────────────────────────
    m_chatCard = new QFrame(container);
    m_chatCard->setObjectName("chatCard");
    m_chatCard->setStyleSheet(
        "QFrame#chatCard {"
        "  background: rgba(79,140,255,0.04);"
        "  border: 1px solid rgba(79,140,255,0.16);"
        "  border-radius: 14px;"
        "}");
    auto* chatLay = new QVBoxLayout(m_chatCard);
    chatLay->setContentsMargins(Theme::Space::M, Theme::Space::M,
                                 Theme::Space::M, Theme::Space::M);
    chatLay->setSpacing(Theme::Space::S);

    auto* chatTitle = new QLabel("Ask the assistant", m_chatCard);
    QFont chatTitleF = Theme::Font::base();
    chatTitleF.setWeight(QFont::DemiBold);
    chatTitleF.setPointSizeF(11.5);
    chatTitle->setFont(chatTitleF);
    chatTitle->setStyleSheet("color: #c8d4e8;");
    chatLay->addWidget(chatTitle);

    auto* chatHint = new QLabel(
        "Try: \"schedule lunch with mom tomorrow at noon\" or "
        "\"block 2 hours of focus tomorrow morning\".", m_chatCard);
    chatHint->setWordWrap(true);
    QFont hintF = Theme::Font::caption();
    hintF.setPointSizeF(10.0);
    chatHint->setFont(hintF);
    chatHint->setStyleSheet("color: #6b7a94;");
    chatLay->addWidget(chatHint);

    // Message log
    auto* logHolder = new QWidget(m_chatCard);
    m_chatLog = new QVBoxLayout(logHolder);
    m_chatLog->setContentsMargins(0, Theme::Space::XS, 0, Theme::Space::XS);
    m_chatLog->setSpacing(Theme::Space::XS);
    m_chatLog->addStretch();
    chatLay->addWidget(logHolder);

    // Composer
    auto* composer = new QWidget(m_chatCard);
    auto* composerLay = new QHBoxLayout(composer);
    composerLay->setContentsMargins(0, 0, 0, 0);
    composerLay->setSpacing(Theme::Space::S);

    m_chatInput = new QLineEdit(composer);
    m_chatInput->setPlaceholderText("Ask the assistant…");
    m_chatInput->setStyleSheet(
        "QLineEdit {"
        "  background: rgba(18,26,42,0.6);"
        "  border: 1px solid rgba(79,140,255,0.22);"
        "  border-radius: 10px;"
        "  padding: 8px 12px;"
        "  color: #dde6f4;"
        "  selection-background-color: rgba(79,140,255,0.45);"
        "}"
        "QLineEdit:focus { border: 1px solid #4f8cff; }");

    m_chatSendBtn = new QPushButton("Send", composer);
    m_chatSendBtn->setObjectName("accentBtn");
    m_chatSendBtn->setCursor(Qt::PointingHandCursor);
    m_chatSendBtn->setFixedHeight(34);

    composerLay->addWidget(m_chatInput, 1);
    composerLay->addWidget(m_chatSendBtn);
    chatLay->addWidget(composer);

    auto sendFn = [this]() {
        const QString text = m_chatInput->text().trimmed();
        if (text.isEmpty()) return;
        m_chatInput->clear();
        appendChatUser(text);
        emit userChatMessage(text);
    };
    connect(m_chatSendBtn, &QPushButton::clicked, this, sendFn);
    connect(m_chatInput,   &QLineEdit::returnPressed, this, sendFn);

    m_mainLayout->addWidget(m_chatCard);

    m_mainLayout->addStretch();
    setWidget(container);
}

// ============================================================================
// Chat panel
// ============================================================================

static QLabel* makeBubble(const QString& text, bool fromUser, QWidget* parent) {
    auto* bubble = new QLabel(text, parent);
    bubble->setWordWrap(true);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont f = Theme::Font::base();
    f.setPointSizeF(10.5);
    bubble->setFont(f);
    bubble->setStyleSheet(QString(
        "QLabel {"
        "  background: %1;"
        "  border-radius: 10px;"
        "  padding: 7px 11px;"
        "  color: %2;"
        "}")
        .arg(fromUser ? "rgba(79,140,255,0.22)" : "rgba(255,255,255,0.04)")
        .arg(fromUser ? "#dde6f4" : "#c8d4e8"));
    bubble->setMaximumWidth(320);
    return bubble;
}

void DashboardWidget::appendChatUser(const QString& text) {
    if (!m_chatLog) return;
    // Remove trailing stretch, add bubble, re-add stretch
    auto* row = new QWidget;
    auto* rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->addStretch();
    rowLay->addWidget(makeBubble(text, /*fromUser=*/true, row));
    const int n = m_chatLog->count();
    m_chatLog->insertWidget(n - 1, row);
}

void DashboardWidget::appendChatAssistant(const QString& text) {
    if (!m_chatLog) return;
    auto* row = new QWidget;
    auto* rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->addWidget(makeBubble(text, /*fromUser=*/false, row));
    rowLay->addStretch();
    const int n = m_chatLog->count();
    m_chatLog->insertWidget(n - 1, row);

    // Auto-scroll to bottom
    QTimer::singleShot(0, this, [this]{
        if (auto* vbar = verticalScrollBar()) vbar->setValue(vbar->maximum());
    });
}

void DashboardWidget::clearChat() {
    if (!m_chatLog) return;
    // Remove everything except the trailing stretch
    while (m_chatLog->count() > 1) {
        QLayoutItem* it = m_chatLog->takeAt(0);
        if (auto* w = it->widget()) w->deleteLater();
        delete it;
    }
    m_pendingBubbles.clear();
}

int DashboardWidget::appendChatPending() {
    if (!m_chatLog) return 0;
    auto* row = new QWidget;
    auto* rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    auto* bubble = makeBubble("…", /*fromUser=*/false, row);
    rowLay->addWidget(bubble);
    rowLay->addStretch();
    const int n = m_chatLog->count();
    m_chatLog->insertWidget(n - 1, row);

    const int id = m_nextPendingId++;
    m_pendingBubbles.insert(id, bubble);
    return id;
}

void DashboardWidget::resolveChatPending(int id, const QString& text) {
    auto it = m_pendingBubbles.find(id);
    if (it == m_pendingBubbles.end()) {
        appendChatAssistant(text);
        return;
    }
    QLabel* bubble = it.value();
    m_pendingBubbles.erase(it);
    if (bubble) bubble->setText(text.isEmpty() ? QStringLiteral("Done.") : text);
}

void DashboardWidget::setChatEnabled(bool enabled, const QString& placeholder) {
    if (!m_chatInput) return;
    m_chatInput->setEnabled(enabled);
    m_chatSendBtn->setEnabled(enabled);
    if (!placeholder.isEmpty()) m_chatInput->setPlaceholderText(placeholder);
}

// ============================================================================
// Helpers
// ============================================================================

QString DashboardWidget::formatMinutes(int min) {
    if (min <= 0) return "0m";
    int h = min / 60, m = min % 60;
    if (h && m) return QString("%1h %2m").arg(h).arg(m);
    if (h) return QString("%1h").arg(h);
    return QString("%1m").arg(m);
}

// ============================================================================
// Core UI rebuild
// ============================================================================

void DashboardWidget::rebuildUI(const QDate& date,
                                 const QString& summary,
                                 const QStringList& suggestions,
                                 int workload, int balance,
                                 int freeMinutes, int freePercent,
                                 int totalEvents, int meetingCount) {
    // Date context
    if (date.isValid()) {
        const bool isToday = (date == QDate::currentDate());
        const QString dateStr = isToday
            ? "Today's schedule"
            : date.toString("dddd, MMMM d");
        m_dateLabel->setText(dateStr);
    }

    // Summary
    m_summaryLabel->setText(summary);

    // Metrics: arc gauges for percentage values, plain rows for counts
    m_metricsCard->clearRows();
    m_metricsCard->addMetricRow("Workload",  QString("%1%").arg(workload),  workload,  QColor("#4f8cff"));
    m_metricsCard->addRow("Free time", formatMinutes(freeMinutes));
    m_metricsCard->addRow("Meetings",  QString::number(meetingCount));
    m_metricsCard->addMetricRow("Balance",   QString("%1%").arg(balance),   balance,   QColor("#22c55e"));

    // Suggestions
    m_suggestCard->clearRows();
    for (int i = 0; i < suggestions.size() && i < 3; ++i)
        m_suggestCard->addBullet(suggestions[i]);

    if (suggestions.isEmpty())
        m_suggestCard->addBullet("Your schedule has breathing room. Keep it that way.");

    // Show apply button only when there are actionable suggestions
    bool hasActionable = !suggestions.isEmpty()
                         && suggestions.first() != "Looking good — keep this rhythm going";
    m_applyBtn->setVisible(hasActionable);
}

// ============================================================================
// Event-based refresh (fallback)
// ============================================================================

void DashboardWidget::refresh(const QDate& date, const QVector<Event>& events) {
    m_currentDate = date;

    int sessions = 0, focusMin = 0, breakMin = 0, exerciseMin = 0, meetingCount = 0;
    QTime firstStart, lastEnd;

    for (const auto& e : events) {
        if (!e.isOnDate(date)) continue;
        const int dur = qMax(0, int(e.getStartTime().secsTo(e.getEndTime()) / 60));
        const QString desc = e.getDescription().toLower();
        const int sep = desc.indexOf("::");
        const QString cat = (sep >= 0 ? desc.left(sep) : desc).trimmed();
        const QString title = e.getTitle().toLower();

        if (cat == "break")         breakMin += dur;
        else if (cat == "exercise") exerciseMin += dur;
        else { focusMin += dur; sessions++; }

        // Detect meetings
        if (title.contains("meeting") || title.contains("standup") ||
            title.contains("sync") || title.contains("1:1") ||
            title.contains("review") || title.contains("retro"))
            meetingCount++;

        if (!firstStart.isValid() || e.getStartTime().time() < firstStart)
            firstStart = e.getStartTime().time();
        if (!lastEnd.isValid() || e.getEndTime().time() > lastEnd)
            lastEnd = e.getEndTime().time();
    }

    const int daySpan = (firstStart.isValid() && lastEnd.isValid())
        ? int(QTime(0,0).secsTo(lastEnd)/60 - QTime(0,0).secsTo(firstStart)/60)
        : 0;
    const int activeMin = focusMin + breakMin + exerciseMin;
    const int freeMin = qMax(0, daySpan > 0 ? daySpan - activeMin : 16*60 - activeMin);

    int workload = qBound(0, focusMin / 8 + sessions * 4, 100);
    int balance = qBound(0, 70 + exerciseMin/12 + breakMin/8
                            - qAbs(focusMin - (breakMin + exerciseMin)*2)/12, 100);
    int freePercent = daySpan > 0 ? qBound(0, freeMin * 100 / (16*60), 100)
                                  : (sessions == 0 ? 100 : 50);

    QString summary;
    if (sessions == 0)
        summary = "Your day is wide open. Perfect for deep work, planning, or rest.";
    else if (workload >= 80)
        summary = QString("Intense day with %1 of focus time. Protect your breaks.")
                  .arg(formatMinutes(focusMin));
    else if (balance >= 70 && workload >= 25)
        summary = QString("Well-balanced day with %1 of free time.")
                  .arg(formatMinutes(freeMin));
    else if (sessions <= 2)
        summary = "Light schedule — great for creative or strategic work.";
    else
        summary = "Solid day ahead. Room to breathe between sessions.";

    QStringList suggestions;
    if (breakMin < 15 && focusMin > 60)
        suggestions << "Add a 10-minute buffer after your longest block";
    if (exerciseMin == 0 && sessions > 0 && freeMin >= 30)
        suggestions << "Block 30 minutes for movement";
    if (sessions == 0)
        suggestions << "Plan your top 3 priorities for the day";
    if (meetingCount >= 3)
        suggestions << QString("Consider consolidating your %1 meetings").arg(meetingCount);
    if (suggestions.isEmpty())
        suggestions << "Looking good — keep this rhythm going";
    while (suggestions.size() > 4)
        suggestions.removeLast();

    m_lastWorkload = workload;
    m_lastBalance = balance;
    m_lastFreeMin = freeMin;
    m_lastFreePercent = freePercent;
    m_lastTotalEvents = sessions;
    m_lastMeetings = meetingCount;

    rebuildUI(date, summary, suggestions, workload, balance, freeMin, freePercent,
              sessions, meetingCount);
}

// ============================================================================
// SchedulerEngine analysis path (preferred)
// ============================================================================

void DashboardWidget::refreshFromAnalysis(const QString& summary,
                                           const QStringList& suggestions,
                                           int workload, int balance,
                                           int freeMinutes, int freePercent) {
    m_lastWorkload = workload;
    m_lastBalance = balance;
    m_lastFreeMin = freeMinutes;
    m_lastFreePercent = freePercent;

    rebuildUI(m_currentDate, summary, suggestions, workload, balance, freeMinutes, freePercent,
              m_lastTotalEvents, m_lastMeetings);
}

// ============================================================================
// LLM assistant overlay — replaces summary + suggestions only
// ============================================================================

void DashboardWidget::refreshFromAssistant(const QString& summary,
                                            const QStringList& suggestions) {
    // Keep metrics from the last analysis, replace text with LLM output
    rebuildUI(m_currentDate, summary, suggestions,
              m_lastWorkload, m_lastBalance, m_lastFreeMin, m_lastFreePercent,
              m_lastTotalEvents, m_lastMeetings);
}
