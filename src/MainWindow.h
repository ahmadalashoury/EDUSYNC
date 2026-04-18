#pragma once

#include <QMainWindow>
#include <QDate>
#include <QDateTime>
#include <QMap>

class QLabel;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTimer;
class QFrame;
class CalendarWidget;
class DayTimelineWidget;
class WeekTimelineWidget;
class SearchPanel;
class DashboardWidget;
class EventStore;
class SchedulerEngine;
class SyncManager;
class AssistantService;
class LocalAssistantService;
class LLMAssistantService;
class GoogleCalendarProvider;
class OutlookCalendarProvider;
class CalDAVProvider;
class AppleNativeProvider;

// ============================================================================
// MainWindow — 3-panel layout shell
//
// LEFT:   Month header + nav arrows + mini calendar + account status
// CENTER: Toolbar (Add/Edit/Delete/Sync/Settings) + day timeline
// RIGHT:  Assistant panel (summary + metrics + suggestions)
// ============================================================================

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    enum class ThemeMode { Light, Dark };

    explicit MainWindow(QWidget* parent = nullptr);

private:
    void buildUI();
    void wireConnections();
    void setupProviders();
    void setupAssistant();
    void reloadCalDAVProviders();

    // Theme
    void applyTheme(ThemeMode mode);
    void updateCalendarStyle();
    void refreshMonthTitle();

    // Event CRUD dialogs
    void openAddDialog();
    void openAddDialogAt(const QTime& time);
    void openAddDialogAt(const QDate& date, const QTime& time);
    void openEditDialog(int eventIndex);
    void openDeleteDialog(int eventIndex);

    // Settings
    void openSettingsDialog(int initialPage = 0);

    // Core logic
    void onDatePicked(const QDate& d);
    void refreshAll();
    void runAutoAI();
    void updateButtonStates();
    void updateSyncStatus();

    // AI chat handling
    void onChatMessage(const QString& text);

    // Data layer
    EventStore*            m_store     = nullptr;
    SchedulerEngine*       m_scheduler = nullptr;
    SyncManager*           m_sync      = nullptr;
    LocalAssistantService* m_localAssistant = nullptr;
    LLMAssistantService*   m_llmAssistant   = nullptr;
    ThemeMode              m_theme     = ThemeMode::Dark;
    QDate                  m_selectedDate;
    int                    m_selectedEventIndex = -1;
    bool                   m_updatingDate = false;

    // Providers
    GoogleCalendarProvider*  m_googleProvider  = nullptr;
    OutlookCalendarProvider* m_outlookProvider = nullptr;
    AppleNativeProvider*     m_appleProvider   = nullptr;
    QVector<CalDAVProvider*> m_caldavProviders;

    // Debounce timer for auto-AI
    QTimer* m_aiDebounce = nullptr;

    // Layout widgets
    QSplitter*          m_splitter       = nullptr;
    CalendarWidget*     m_calendar       = nullptr;
    DayTimelineWidget*  m_timeline       = nullptr;
    WeekTimelineWidget* m_weekTimeline   = nullptr;
    QStackedWidget*     m_timelineStack  = nullptr;  // 0=day, 1=week
    SearchPanel*        m_searchPanel    = nullptr;
    DashboardWidget*    m_dashboard      = nullptr;
    QLabel*             m_monthTitle     = nullptr;
    QPushButton*        m_prevBtn        = nullptr;
    QPushButton*        m_nextBtn        = nullptr;
    QPushButton*        m_todayBtn       = nullptr;

    // Toolbar buttons
    QPushButton* m_btnAdd      = nullptr;
    QPushButton* m_btnEdit     = nullptr;
    QPushButton* m_btnDelete   = nullptr;
    QPushButton* m_btnSync     = nullptr;
    QPushButton* m_btnSettings = nullptr;
    QPushButton* m_btnDay      = nullptr;   // view toggle
    QPushButton* m_btnWeek     = nullptr;
    QPushButton* m_btnPrevDay  = nullptr;
    QPushButton* m_btnNextDay  = nullptr;
    QPushButton* m_btnTodayBar = nullptr;

    QLabel* m_dayLabel   = nullptr;
    QLabel* m_syncStatus = nullptr;
    QLabel* m_yearLabel  = nullptr;   // year sub-label in mini calendar header

    // Left rail — Zoho-style sidebar
    QWidget* m_calSourcesContainer = nullptr;
    QMap<QString, bool> m_sourceVisible;
    void rebuildCalendarSources();

    // Mini calendar day event list
    QLabel*  m_calEventDateLabel = nullptr;
    QLabel*  m_calEventRelLabel  = nullptr;
    QWidget* m_calEventList      = nullptr;
};
