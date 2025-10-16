#ifndef ULTRAMAINWINDOW_H
#define ULTRAMAINWINDOW_H

#include <QMainWindow>
#include <QDate>
#include <QVector>
#include <QTextEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QTimer>
#include <QTabWidget>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QWebEngineView>


#include "Event.h"                // needs full type for QVector<Event>

class QLabel;            
class QTabWidget;
class QTextEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTimer;
class QTableView;
class QCalendarWidget;
class ModernCalendarWidget;  
class SuperAI;
class Event;
class WeekHeaderView;


class UltraMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    enum class ThemeMode { Light, Dark };

    explicit UltraMainWindow(QWidget *parent = nullptr);
    ~UltraMainWindow();
    void buildUltraAITab();
    void buildAnalyticsTab();
    void buildProductivityTab();
    void buildSettingsTab();

    // UI build
    void setupUltraUI();
    void setupCalendarPage();
    void bindAIOutputs();
    void setupAnimations();
    void setupEffects();
    void applyGlassmorphism();
    void applyStunningEffects();

    // Theme / chrome helpers
    void applyTheme(ThemeMode);
    void resetPanels();
    void clearLocalStyles();
    void ensureWeekdayHeaderStyled();
    void updateCalendarChrome();
    void styleActionButtons();
    void refreshMonthFormats();
    bool openEditEventDialog(Event& e);
    void styleCalendarWeekdayHeader();
    void ensureWeekHeader();
    void setDashboardHtml(const QString& html);  
    QString buildLocalSuggestionsHtml(const QDate& d) const;

    // Dialogs
    void openNewEventDialog(const QDate& date);

    // Misc
    QColor  colorForCategory(const QString& cat) const;
    QString buildDailyDashboardHtml(const QDate& d) const;

    // simple formatter that can be used from const methods
    static QString mm(int minutes);

signals:
    void themeChanged();   
    
protected:
    void changeEvent(QEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;  

private slots:
    // calendar & AI
    void onDateSelected(const QDate& date);
    void onAIAnalysisComplete(const QString& analysis);
    void onAISuggestionsReady(const QList<Event>& suggestions);
    void onAIInsightsReady(const QString& insights);
    void onAIGoalsReady(const QStringList& goals);
    void onAIHabitsReady(const QStringList& habits);
    void onAIStressAnalysisReady(const QString& analysis);
    void onAIOptimizationReady(const QString& optimization);

private: // periodic updates
    void updateAdvancedFeatures();
    void updateAI();
    void updateAnalytics();
    void updateProductivity();
    void updateTeam();
    void updateSettings();
    QString descCategory(const Event& e) const;
    QString descNotes(const Event& e) const;
    QString tooltipForDate(const QDate& d) const;
    void addEventWithRecurrence(const Event& base, int recurIndex);
    void forceGrayWeekdayHeader();
    QWebEngineView* m_aiWeb = nullptr;   // NEW: right-side dashboard
    
private: // widgets & state
    QWidget*              m_centralWidget = nullptr;
    QTabWidget*           m_mainTabs = nullptr;
    ModernCalendarWidget* m_calendar = nullptr;
    WeekHeaderView* m_weekHeader = nullptr;

    QLabel* m_monthTitle = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;


    
    // right-side (calendar page)
    QTextEdit*  m_aiChat = nullptr;
    QPushButton *m_aiAnalyzeButton = nullptr, *m_aiSuggestButton = nullptr,
                *m_aiInsightsButton = nullptr, *m_aiGoalsButton = nullptr,
                *m_aiHabitsButton = nullptr,  *m_aiStressButton = nullptr,
                *m_aiOptimizeButton = nullptr;

    // other tabs
    QTextEdit *m_aiPanel = nullptr, *m_analyticsPanel = nullptr,
              *m_productivityPanel = nullptr, *m_settingsPanel = nullptr;

    QListWidget *m_goalsPanel = nullptr, *m_habitsPanel = nullptr;
    
    QListWidget* m_dayEvents = nullptr;

    QProgressBar *m_pbDaily = nullptr, *m_pbWeekly = nullptr,
                 *m_pbMonthly = nullptr, *m_pbBalance = nullptr;

    QPushButton *m_btnThemeLight = nullptr, *m_btnThemeDark = nullptr,
                *m_btnResetPanels = nullptr;

    // EduSync AI tab buttons (optional)
    QPushButton *m_btnAnalyze = nullptr, *m_btnSuggest = nullptr,
                *m_btnInsights = nullptr, *m_btnGoals = nullptr,
                *m_btnHabits = nullptr, *m_btnStress = nullptr,
                *m_btnOptimize = nullptr;

    // runtime
    ThemeMode     m_theme = ThemeMode::Dark;
    QDate         m_selectedDate;
    QVector<Event> m_events;
    SuperAI*      m_superAI = nullptr;
    QTimer*       m_updateTimer = nullptr;

    // fun animations
    QPropertyAnimation *m_fadeAnimation = nullptr,
                       *m_glowAnimation = nullptr,
                       *m_shimmerAnimation = nullptr;
};

#endif // ULTRAMAINWINDOW_H
