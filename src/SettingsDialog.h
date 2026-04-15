#pragma once

#include <QDialog>
#include <QVector>

class QStackedWidget;
class QListWidget;
class CalendarProvider;
class GoogleCalendarProvider;
class OutlookCalendarProvider;
class AppleNativeProvider;
class CalDAVProvider;
class LLMAssistantService;

// ============================================================================
// SettingsDialog — full settings window with tabbed navigation
//
// Tabs:
//   General    — first day of week, default event duration, time format
//   Appearance — theme toggle (dark/light)
//   Accounts   — native Apple/macOS, Google, Outlook, and CalDAV linking
//   Assistant  — LLM endpoint, model, API key
//   Advanced   — custom integration credentials and advanced connection tools
// ============================================================================

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    enum class Page {
        General = 0,
        Appearance,
        Accounts,
        AI,
        Advanced
    };

    SettingsDialog(GoogleCalendarProvider* google,
                   OutlookCalendarProvider* outlook,
                   AppleNativeProvider* apple,
                   QVector<CalDAVProvider*>* caldavProviders,
                   LLMAssistantService* llm,
                   bool isDarkTheme,
                   Page initialPage = Page::General,
                   QWidget* parent = nullptr);

signals:
    void themeChanged(bool isDark);
    void accountsChanged();

private:
    void buildSidebar();
    QWidget* buildGeneralPage();
    QWidget* buildAppearancePage();
    QWidget* buildAccountsPage();
    QWidget* buildAIPage();
    QWidget* buildAdvancedPage();

    QListWidget*    m_sidebar = nullptr;
    QStackedWidget* m_stack   = nullptr;

    GoogleCalendarProvider*  m_google  = nullptr;
    OutlookCalendarProvider* m_outlook = nullptr;
    AppleNativeProvider*     m_apple   = nullptr;
    QVector<CalDAVProvider*>* m_caldavProviders = nullptr;
    LLMAssistantService*    m_llm     = nullptr;

    bool m_isDark;
};
