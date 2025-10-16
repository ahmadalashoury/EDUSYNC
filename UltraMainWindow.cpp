// UltraMainWindow.cpp
//
// Cleaned & documented version.
// ✅ Nothing removed; all functions/signals/slots/behaviors preserved.
// ✅ Added comments to explain structure, lifecycle, and non-obvious logic.
// ✅ Kept all styling and AI/dashboard code paths intact.
// --------------------------------------------------------------------------------

#include "UltraMainWindow.h"

// Qt core & widgets
#include <QSettings>
#include <QApplication>
#include <QScreen>
#include <QTabWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QTimeEdit>
#include <QCalendarWidget>
#include <QDate>
#include <QFont>
#include <QTimer>
#include <QCursor>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QSizePolicy>
#include <QDebug>
#include <algorithm>
#include <QTableView>
#include <QHeaderView>
#include <QStyleOptionHeader>
#include <QPainter>
#include <QLocale>
#include <QCheckBox>
#include <QDateEdit>
#include <QToolBar>
#include <QAction>
#include <QTextCursor>
#include <QTextList>
#include <QToolTip>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QMessageBox>
#include <QFontDatabase>
#include <QStyle>
#include <QUrl>
#include <QRegularExpression>

// Project headers
#include "ModernCalendarWidget.h"
#include "SuperAI.h"
#include "WeekHeaderView.h"          // (currently not used; kept for future)
#include "UltraDashboardRender.h"    // provides ::buildDailyDashboardHtml(...)
#include <QWebEngineView>


// ---- Local HTML helper forward declarations -------------------------------
// (Definitions remain later in this file; these declarations let us call them
// earlier in the code.)
static QString appendSectionCard(const QString& baseHtml,
                                 const QString& title,
                                 const QString& bodyHtml,
                                 bool lightTheme);

static QString ulList(const QStringList& items);


// ===============================
// ============ Ctor/Dtor =========
// ===============================

UltraMainWindow::UltraMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_selectedDate(QDate::currentDate())
{
    setWindowTitle("🚀 EduSync - AI Calendar");
    setMinimumSize(1600, 1000);
    resize(1920, 1080);

    // Center the window on the available screen area (handles taskbars/docks).
    if (auto *screen = QApplication::primaryScreen()) {
        const QRect g = screen->availableGeometry();
        const int x = std::clamp(g.center().x() - width() / 2,  g.left(),  g.right()  - width());
        const int y = std::clamp(g.center().y() - height() / 2, g.top(),   g.bottom() - height());
        move(x, y);
    }

    // Normalize app font point size; family can be set elsewhere globally.
    {
        QFont appFont = qApp->font();
        appFont.setPointSizeF(11);
        qApp->setFont(appFont);
    }

    // --- Build main UI skeleton (tabs) ---
    setupUltraUI();

    // --- Build the Calendar page (left: calendar, right: inspector/AI) ---
    setupCalendarPage();

    // Clear any stray per-widget styles (we re-apply consistent theming).
    clearLocalStyles();

    // Restore theme (defaults to dark).
    {
        QSettings s;
        const QString t = s.value("theme", "dark").toString();
        applyTheme(t == "light" ? ThemeMode::Light : ThemeMode::Dark);
    }

    // Subtle window animations (no heavy effects).
    setupAnimations();

    // Create AI engine and connect outputs to UI.
    m_superAI = new SuperAI(this);
    connect(m_superAI, &SuperAI::analysisComplete,    this, &UltraMainWindow::onAIAnalysisComplete);
    connect(m_superAI, &SuperAI::suggestionsReady,    this, &UltraMainWindow::onAISuggestionsReady);
    connect(m_superAI, &SuperAI::insightsReady,       this, &UltraMainWindow::onAIInsightsReady);
    connect(m_superAI, &SuperAI::goalsReady,          this, &UltraMainWindow::onAIGoalsReady);
    connect(m_superAI, &SuperAI::habitsReady,         this, &UltraMainWindow::onAIHabitsReady);
    connect(m_superAI, &SuperAI::stressAnalysisReady, this, &UltraMainWindow::onAIStressAnalysisReady);
    connect(m_superAI, &SuperAI::optimizationReady,   this, &UltraMainWindow::onAIOptimizationReady);

    // Periodic timers for analytics/progress mock values, etc.
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &UltraMainWindow::updateAdvancedFeatures);
    m_updateTimer->start(2000);

    // (Optional) Other tabs — stubs are included but not added:
    // buildUltraAITab();
    // buildAnalyticsTab();
    // buildProductivityTab();
    buildSettingsTab();

    // Wire any AI outputs to the parts of UI already constructed.
    bindAIOutputs();

    // Initialize calendar selection to today and perform a first analysis.
    if (m_calendar) {
        m_calendar->setSelectedDate(m_selectedDate);
        onDateSelected(m_selectedDate);
    }

    // Kick off initial AI analysis with current (possibly empty) events list.
    QTimer::singleShot(0, this, [this] {
        if (m_superAI) m_superAI->analyzeSchedule(m_events);
    });
}

UltraMainWindow::~UltraMainWindow() = default;


// ==================================
// ============ UI: Shell ============
// ==================================

/**
 * @brief Creates the outer layout: a central widget with a QTabWidget.
 */
void UltraMainWindow::setupUltraUI() {
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);

    auto* mainLayout = new QHBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    m_mainTabs = new QTabWidget;
    m_mainTabs->setTabPosition(QTabWidget::North);
    m_mainTabs->setDocumentMode(true);

    mainLayout->addWidget(m_mainTabs);
}

/**
 * @brief When using the web dashboard, route HTML to QWebEngineView;
 *        otherwise fallback to QTextEdit (if present).
 */
void UltraMainWindow::setDashboardHtml(const QString& html) {
    if (m_aiWeb) {
        m_aiWeb->setHtml(html, QUrl("about:blank"));
    } else if (m_aiChat) {
        m_aiChat->setHtml(html);
    }
}


// =====================================================
// ============ UI: Calendar + Inspector page ==========
// =====================================================

/**
 * @brief Builds the main calendar page:
 *        - Left: month header + QCalendarWidget (ModernCalendarWidget)
 *        - Right: day list + AI buttons + AI dashboard (QWebEngineView)
 */
void UltraMainWindow::setupCalendarPage() {
    // --- Page scaffold -------------------------------------------------------
    QWidget *calPage = new QWidget(this);
    QHBoxLayout *calLy = new QHBoxLayout(calPage);
    calLy->setContentsMargins(12, 12, 12, 12);
    calLy->setSpacing(12);

    // --- LEFT COLUMN: Month title + calendar --------------------------------
    QWidget *leftCol = new QWidget(calPage);
    QVBoxLayout *leftLy = new QVBoxLayout(leftCol);
    leftLy->setContentsMargins(0, 0, 0, 0);
    leftLy->setSpacing(8);

    // Title bar with prev/next + month title
    QWidget *titleBar = new QWidget(leftCol);
    QHBoxLayout *titleRow = new QHBoxLayout(titleBar);
    titleRow->setContentsMargins(14, 10, 14, 10);
    titleRow->setSpacing(10);

    m_monthTitle = new QLabel(titleBar);
    {
        QFont f = qApp->font();
        f.setPointSize(28);
        f.setWeight(QFont::DemiBold);
        m_monthTitle->setFont(f);
    }

    m_prevBtn = new QPushButton(QString::fromUtf8("◀"), titleBar);
    m_nextBtn = new QPushButton(QString::fromUtf8("▶"), titleBar);
    for (auto *b : { m_prevBtn, m_nextBtn }) {
        b->setFixedWidth(36);
        b->setCursor(Qt::PointingHandCursor);
    }

    titleRow->addWidget(m_prevBtn);
    titleRow->addSpacing(8);
    titleRow->addWidget(m_monthTitle, 1);
    titleRow->addSpacing(8);
    titleRow->addWidget(m_nextBtn);
    leftLy->addWidget(titleBar);

    // A thin divider (light theme only)
    QFrame *rule = new QFrame(leftCol);
    rule->setFixedHeight(1);
    rule->setFrameStyle(QFrame::NoFrame);
    leftLy->addWidget(rule);

    // Calendar creation & baseline configuration
    m_calendar = new ModernCalendarWidget(leftCol);
    m_calendar->setObjectName("UltraCalendar");
    m_calendar->setSelectionMode(QCalendarWidget::SingleSelection);
    m_calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    m_calendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    m_calendar->setFirstDayOfWeek(Qt::Monday);
    m_calendar->setGridVisible(true);
    m_calendar->setFocusPolicy(Qt::StrongFocus);
    m_calendar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Hide Qt’s built-in nav bar (we have our own buttons).
    if (auto *nav = m_calendar->findChild<QWidget*>("qt_calendar_navigationbar"))
        nav->hide();

    // Internal view tuning (no native selection highlight, hover handling, etc.)
    if (QTableView *calView = m_calendar->findChild<QTableView*>()) {
        calView->setObjectName("CalView");
        calView->setMouseTracking(true);
        calView->viewport()->setMouseTracking(true);
        calView->viewport()->setAttribute(Qt::WA_Hover, true);
        calView->setFocusPolicy(Qt::StrongFocus);

        // Vertical header stretches rows uniformly (6 rows).
        if (auto *vh = calView->verticalHeader())
            vh->setSectionResizeMode(QHeaderView::Stretch);

        calView->setStyleSheet(R"(
            QTableView::item:selected         { background:transparent; border:0; }
            QTableView::item:active:selected  { background:transparent; border:0; }
            QTableView::item:!active:selected { background:transparent; border:0; }
            QTableView::item:focus            { outline:0; }
            QAbstractItemView::item           { background:transparent; }
        )");
    }

    leftLy->addWidget(m_calendar, 1);
    calLy->addWidget(leftCol, 7);

    // --- RIGHT COLUMN: Day list + AI controls + dashboard -------------------
    QWidget *right = new QWidget(calPage);
    QVBoxLayout *rLy = new QVBoxLayout(right);
    rLy->setSpacing(8);

    // Selected day label
    QLabel *dayLabel = new QLabel(right);
    dayLabel->setStyleSheet("font-weight:600; font-size:16px;");
    dayLabel->setText(m_selectedDate.isValid() ? m_selectedDate.toString("dddd, MMM d")
                                               : "Select a day");

    // Day events list
    m_dayEvents = new QListWidget(right);
    m_dayEvents->setMinimumHeight(160);
    m_dayEvents->setMouseTracking(true);
    m_dayEvents->viewport()->setMouseTracking(true);
    m_dayEvents->viewport()->setAttribute(Qt::WA_Hover, true);
    m_dayEvents->viewport()->installEventFilter(this);

    // Helper to make small action buttons
    auto mkBtn = [](const QString& t) {
        auto *b = new QPushButton(t);
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };

    // AI & CRUD buttons
    m_aiAnalyzeButton  = mkBtn("Analyze");
    m_aiSuggestButton  = mkBtn("Suggest");  // kept (hidden) to preserve interface compatibility
    m_aiSuggestButton->hide();              // ⬅ Suggest is intentionally hidden as requested
    m_aiInsightsButton = mkBtn("Insights");
    m_aiGoalsButton    = mkBtn("Goals");
    m_aiHabitsButton   = mkBtn("Habits");
    m_aiStressButton   = mkBtn("Stress");
    m_aiOptimizeButton = mkBtn("Optimize");
    QPushButton *addBtn    = mkBtn("Add");
    QPushButton *editBtn   = mkBtn("Edit");
    QPushButton *deleteBtn = mkBtn("Delete");

    // Button row layout
    QHBoxLayout *btnRow = new QHBoxLayout();
    for (auto *b : { m_aiAnalyzeButton, m_aiSuggestButton, m_aiInsightsButton,
                     m_aiGoalsButton,   m_aiHabitsButton,  m_aiStressButton,
                     m_aiOptimizeButton, addBtn, editBtn, deleteBtn }) {
        btnRow->addWidget(b);
    }

    // Dashboard is rendered in a web view (clean visuals, CSS-friendly)
    m_aiWeb = new QWebEngineView(right);
    m_aiWeb->setObjectName("AiDashboardWeb");
    m_aiWeb->setMinimumHeight(220);
    m_aiWeb->setAttribute(Qt::WA_OpaquePaintEvent, false);
    m_aiWeb->setStyleSheet("background: transparent; border: 0;");
    if (m_aiWeb->page())
        m_aiWeb->page()->setBackgroundColor(Qt::transparent);

    // Assemble right column
    rLy->addWidget(dayLabel);
    rLy->addWidget(m_dayEvents);
    rLy->addLayout(btnRow);
    rLy->addWidget(m_aiWeb, 1);     // ONLY the web view; QTextEdit fallback is possible but unused now
    calLy->addWidget(right, 3);

    // Insert into the tab widget
    m_mainTabs->insertTab(0, calPage, "📅 Calendar");
    m_mainTabs->setCurrentIndex(0);

    // ---------------------------
    // Helpers (captured by value)
    // ---------------------------

    // Update the large month title when page changes
    auto updateMonthTitle = [this] {
        if (!m_calendar || !m_monthTitle) return;
        const int y = m_calendar->yearShown();
        const int m = m_calendar->monthShown();
        const QString name = QLocale().standaloneMonthName(m, QLocale::LongFormat).toUpper();
        m_monthTitle->setText(QString("%1    •  %2  •").arg(name).arg(y));
    };

    // Calendar CSS that’s theme-neutral baseline
    auto styleCalendar = [this] {
        const bool light   = (m_theme == ThemeMode::Light);
        const QString base = light ? "#ffffff" : "#20262c";
        const QString bd   = light ? "#e5e7eb" : "#2f3540";
        const QString txt  = light ? "#111111" : "#e6e6eb";

        m_calendar->setStyleSheet(QString(R"(
            QWidget#qt_calendar_navigationbar { height:0; min-height:0; max-height:0; padding:0; margin:0; border:0; background:transparent; }
            QCalendarWidget QTableView#CalView { background:%1; border:1px solid %2; gridline-color:%2; outline:0; }
            QCalendarWidget QAbstractItemView::item { margin:0; padding:0; }
            QCalendarWidget QAbstractItemView::item:selected { background:transparent; border:0; color:%3; }
        )").arg(base, bd, txt));
    };

    // Theme-dependent chrome (title/btns) + action buttons
    auto styleChrome = [this, rule] {
        const bool light = (m_theme == ThemeMode::Light);
        if (rule) rule->setStyleSheet(light ? "background:#e5e7eb;" : "background:transparent;");
        if (m_monthTitle) {
            m_monthTitle->setStyleSheet(light
                ? "color:#111111; letter-spacing:3px;"
                : "color:#e6e6eb; letter-spacing:3px;");
        }
        const QString btnCss = light
          ? "QPushButton{ background:transparent; color:#111; border:1px solid #d0d7de; border-radius:6px; padding:2px 6px; }"
            "QPushButton:hover{ background:#f6f8fa; }"
          : "QPushButton{ background:transparent; color:#e6e6eb; border:1px solid #3a4047; border-radius:6px; padding:2px 6px; }"
            "QPushButton:hover{ background:#2b3138; }";
        if (m_prevBtn) m_prevBtn->setStyleSheet(btnCss);
        if (m_nextBtn) m_nextBtn->setStyleSheet(btnCss);
        styleActionButtons();
    };

    // Extract "Category::Notes" → "Notes"
    auto extractNotes = [](const QString& packed) {
        const int i = packed.indexOf("::");
        return (i >= 0) ? packed.mid(i + 2).trimmed() : QString();
    };

    // Refresh day list on the right for m_selectedDate
    auto refreshDayList = [=] {
        m_dayEvents->clear();
        if (!m_selectedDate.isValid()) return;

        QVector<const Event*> todays; todays.reserve(m_events.size());
        for (const auto& e : m_events)
            if (e.isOnDate(m_selectedDate)) todays.push_back(&e);

        std::sort(todays.begin(), todays.end(),
                  [](const Event* a, const Event* b){ return a->getStartTime() < b->getStartTime(); });

        for (const Event* e : todays) {
            const QString timeRange = QString("%1–%2")
                .arg(e->getStartTime().toString("hh:mm"),
                     e->getEndTime().toString("hh:mm"));
            auto *it = new QListWidgetItem(QString("%1  —  %2").arg(e->getTitle(), timeRange));
            const QString notes = extractNotes(e->getDescription());
            if (!notes.isEmpty()) it->setToolTip(notes);
            m_dayEvents->addItem(it);
        }
    };

    // Helpers to detect "series" (same title + same time window)
    auto sameSeries = [&](const Event& a, const Event& b) {
        return a.getTitle() == b.getTitle()
            && a.getStartTime().time() == b.getStartTime().time()
            && a.getEndTime().time()   == b.getEndTime().time();
    };
    auto isSeriesInstance = [&](const Event& target) {
        int count = 0; for (const auto& e : m_events) if (sameSeries(e, target)) ++count;
        return count > 1;
    };
    auto hasSeries = isSeriesInstance; // alias

    // ---------------------------
    // Signals/Slots wiring
    // ---------------------------

    // Month navigation buttons
    connect(m_prevBtn, &QPushButton::clicked, m_calendar, &QCalendarWidget::showPreviousMonth);
    connect(m_nextBtn, &QPushButton::clicked, m_calendar, &QCalendarWidget::showNextMonth);

    // When a date is clicked/selected:
    auto onPickDate = [=](const QDate& d) {
        if (m_calendar) m_calendar->setSelectedDate(d);
        m_selectedDate = d;
        dayLabel->setText(d.toString("dddd, MMM d"));
        refreshDayList();
        setDashboardHtml(buildDailyDashboardHtml(m_selectedDate));
        if (m_superAI) m_superAI->generateSmartSuggestions(d); // Suggest is hidden but this preserves behavior
    };
    connect(m_calendar, &QCalendarWidget::clicked,           this, onPickDate);
    connect(m_calendar, &QCalendarWidget::selectionChanged,  this, [=]{ onPickDate(m_calendar->selectedDate()); });

    // When month (page) changes: restyle + refresh formats and title
    connect(m_calendar, &QCalendarWidget::currentPageChanged, this,
            [this, styleCalendar, updateMonthTitle, styleChrome](int, int) {
                styleCalendar();
                refreshMonthFormats();
                updateMonthTitle();
                styleChrome();
                updateCalendarChrome();
            });

    // Apply chrome when theme changes (signal emitted in applyTheme)
    connect(this, &UltraMainWindow::themeChanged, this, [this] {
        updateCalendarChrome();
    });

    // Tooltips for items in the day list (hover & click)
    connect(m_dayEvents, &QListWidget::itemEntered, this, [=](QListWidgetItem* it) {
        if (!it) return;
        QToolTip::showText(QCursor::pos(), it->toolTip(), m_dayEvents);
    });
    connect(m_dayEvents, &QListWidget::itemClicked, this, [=](QListWidgetItem* it) {
        if (!it) return;
        QToolTip::showText(QCursor::pos(), it->toolTip(), m_dayEvents);
        if (!m_aiChat) return; // currently dashboard uses webview
        const QString tip = it->toolTip();
        if (tip.isEmpty()) return;
        // (Optional) could echo description to chat; left minimal here.
    });

    // Delete event (supports single instance vs. whole series)
    connect(deleteBtn, &QPushButton::clicked, this, [=] {
        if (!m_selectedDate.isValid()) return;
        const int row = m_dayEvents->currentRow(); if (row < 0) return;

        QVector<int> todayIdx;
        for (int i = 0; i < m_events.size(); ++i)
            if (m_events[i].isOnDate(m_selectedDate)) todayIdx.push_back(i);
        if (row >= todayIdx.size()) return;

        const int idx = todayIdx[row];
        const Event target = m_events[idx];

        if (!isSeriesInstance(target)) {
            m_events.removeAt(idx);
        } else {
            QMessageBox box(this);
            box.setWindowTitle("Apply changes");
            box.setText("Apply changes to just this event or the whole series?");
            QPushButton* btnThis   = box.addButton("This event",    QMessageBox::ActionRole);
            QPushButton* btnAll    = box.addButton("All in series", QMessageBox::ActionRole);
            QPushButton* btnCancel = box.addButton(QMessageBox::Cancel);
            box.setDefaultButton(btnThis);
            box.setEscapeButton(btnCancel);
            box.exec();

            if (box.clickedButton() == btnThis) {
                m_events.removeAt(idx);
            } else if (box.clickedButton() == btnAll) {
                for (int i = m_events.size() - 1; i >= 0; --i)
                    if (sameSeries(m_events[i], target)) m_events.removeAt(i);
            } else {
                return; // canceled
            }
        }

        if (m_calendar) {
            m_calendar->setEvents(m_events);
            m_calendar->setSelectedDate(m_selectedDate);
            m_calendar->update();
        }
        refreshMonthFormats();
        refreshDayList();
        setDashboardHtml(buildDailyDashboardHtml(m_selectedDate));
    });

    // Edit event (single item or entire series handling occurs after dialog)
    connect(editBtn, &QPushButton::clicked, this, [=]{
        if (!m_selectedDate.isValid()) return;
        const int row = m_dayEvents->currentRow(); if (row < 0) return;

        QVector<int> todayIdx; todayIdx.reserve(m_events.size());
        for (int i = 0; i < m_events.size(); ++i)
            if (m_events[i].isOnDate(m_selectedDate)) todayIdx.push_back(i);
        if (row >= todayIdx.size()) return;

        const int idx = todayIdx[row];
        Event original = m_events[idx];
        Event updated  = original;

        if (!openEditEventDialog(updated)) return;

        if (!hasSeries(original)) {
            m_events[idx] = updated;
        } else {
            QMessageBox box(this);
            box.setWindowTitle("Apply changes");
            box.setText("Apply changes to just this event or the whole series?");
            QPushButton* btnThis   = box.addButton("This event",    QMessageBox::ActionRole);
            QPushButton* btnAll    = box.addButton("All in series", QMessageBox::ActionRole);
            QPushButton* btnCancel = box.addButton(QMessageBox::Cancel);
            box.setDefaultButton(btnThis);
            box.setEscapeButton(btnCancel);
            box.exec();

            if (box.clickedButton() == btnThis) {
                m_events[idx] = updated;
            } else if (box.clickedButton() == btnAll) {
                const QTime newStartT = updated.getStartTime().time();
                const QTime newEndT   = updated.getEndTime().time();
                for (auto& e : m_events) {
                    if (!sameSeries(e, original)) continue;
                    e.setTitle(updated.getTitle());
                    e.setDescription(updated.getDescription());
                    e.setColor(colorForCategory(descCategory(updated)));
                    const QDate ds = e.getStartTime().date();
                    const QDate de = e.getEndTime().date();
                    e.setStartTime(QDateTime(ds, newStartT));
                    e.setEndTime(  QDateTime(de, newEndT));
                }
            } else {
                return; // canceled
            }
        }

        if (m_calendar) {
            m_calendar->setEvents(m_events);
            m_calendar->setSelectedDate(m_selectedDate);
            m_calendar->update();
        }
        refreshMonthFormats();
        refreshDayList();
        setDashboardHtml(buildDailyDashboardHtml(m_selectedDate));
    });

    // Add a new event (with recurrence expansion)
    connect(addBtn, &QPushButton::clicked, this, [=]{
        openNewEventDialog(m_selectedDate);
        dayLabel->setText(m_selectedDate.isValid() ? m_selectedDate.toString("dddd, MMM d")
                                                   : "Select a day");
        refreshDayList();
        setDashboardHtml(buildDailyDashboardHtml(m_selectedDate));
    });

    // AI action buttons
    connect(m_aiAnalyzeButton,  &QPushButton::clicked, this, [=] {
        if (m_superAI) m_superAI->analyzeSchedule(m_events);
    });

    // Even though Suggest is hidden, keep the slot to preserve behavior and not break connections.
    connect(m_aiSuggestButton, &QPushButton::clicked, this, [=]{
        if (!m_selectedDate.isValid()) return;

        // Local suggestions immediately (based on your actual events)
        if (m_aiChat) {
            QString html = buildDailyDashboardHtml(m_selectedDate)
                         + buildLocalSuggestionsHtml(m_selectedDate);
            m_aiChat->setHtml(html);
        }

        // Then ask the AI as well; when it returns, onAISuggestionsReady enhances the view.
        if (m_superAI) m_superAI->generateSmartSuggestions(m_selectedDate);
    });

    connect(m_aiInsightsButton, &QPushButton::clicked, this, [=]{
        if (m_superAI) m_superAI->provideInsights(m_events);
    });
    connect(m_aiGoalsButton,    &QPushButton::clicked, this, [=]{
        if (m_superAI) m_superAI->suggestGoals(m_events);
    });
    connect(m_aiHabitsButton,   &QPushButton::clicked, this, [=]{
        if (m_superAI) m_superAI->recommendHabits(m_events);
    });
    connect(m_aiStressButton,   &QPushButton::clicked, this, [=]{
        if (m_superAI) m_superAI->analyzeStress(m_events);
    });
    connect(m_aiOptimizeButton, &QPushButton::clicked, this, [=]{
        if (m_superAI) m_superAI->optimizeWorkLifeBalance(m_events);
    });

    // Initial render after building the page
    updateMonthTitle();
    styleChrome();
    styleCalendar();
    refreshMonthFormats();
    styleActionButtons();

    // Seed the right panel with today's info
    m_selectedDate = m_calendar->selectedDate();
    dayLabel->setText(m_selectedDate.isValid() ? m_selectedDate.toString("dddd, MMM d") : "Select a day");
    setDashboardHtml(buildDailyDashboardHtml(m_selectedDate));
}


// =======================================
// ============ Event Filter =============
// =======================================

/**
 * @brief Handles tooltips for the day events list viewport.
 */
bool UltraMainWindow::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == (m_dayEvents ? m_dayEvents->viewport() : nullptr)) {
        if (ev->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent*>(ev);
            const QPoint p = me->pos();
            if (QListWidgetItem* it = m_dayEvents->itemAt(p)) {
                const QString tip = it->toolTip();
                if (!tip.isEmpty())
                    QToolTip::showText(m_dayEvents->mapToGlobal(p), tip, m_dayEvents);
            }
        } else if (ev->type() == QEvent::ToolTip) {
            auto *he = static_cast<QHelpEvent*>(ev);
            const QPoint p = he->pos();
            if (QListWidgetItem* it = m_dayEvents->itemAt(p)) {
                const QString tip = it->toolTip();
                if (!tip.isEmpty()) {
                    QToolTip::showText(m_dayEvents->mapToGlobal(p), tip, m_dayEvents);
                    return true; // handled
                }
            } else {
                QToolTip::hideText();
                return true; // suppress default
            }
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}


// ==============================================
// ============ AI Outputs Bindings =============
// ==============================================

/**
 * @brief Bind SuperAI signals to panels (AI tab and calendar dashboard).
 */
void UltraMainWindow::bindAIOutputs() {
    if (!m_superAI) return;

    // Optional AI tab wires (exist only if tab was created)
    if (m_btnAnalyze)  connect(m_btnAnalyze,  &QPushButton::clicked, this, [=]{ m_superAI->analyzeSchedule(m_events); });
    if (m_btnSuggest)  connect(m_btnSuggest,  &QPushButton::clicked, this, [=]{ m_superAI->generateSmartSuggestions(m_selectedDate); });
    if (m_btnInsights) connect(m_btnInsights, &QPushButton::clicked, this, [=]{ m_superAI->provideInsights(m_events); });
    if (m_btnGoals)    connect(m_btnGoals,    &QPushButton::clicked, this, [=]{ m_superAI->suggestGoals(m_events); });
    if (m_btnHabits)   connect(m_btnHabits,   &QPushButton::clicked, this, [=]{ m_superAI->recommendHabits(m_events); });
    if (m_btnStress)   connect(m_btnStress,   &QPushButton::clicked, this, [=]{ m_superAI->analyzeStress(m_events); });
    if (m_btnOptimize) connect(m_btnOptimize, &QPushButton::clicked, this, [=]{ m_superAI->optimizeWorkLifeBalance(m_events); });

    // Pipe to AI panel if it exists
    auto toAiPanel = [this](const QString& s) { if (m_aiPanel) m_aiPanel->setPlainText(s); };
    connect(m_superAI, &SuperAI::analysisComplete,    this, toAiPanel);
    connect(m_superAI, &SuperAI::insightsReady,       this, toAiPanel);
    connect(m_superAI, &SuperAI::stressAnalysisReady, this, toAiPanel);
    connect(m_superAI, &SuperAI::optimizationReady,   this, toAiPanel);

    connect(m_superAI, &SuperAI::goalsReady,  this, [=](const QStringList& gl){
        if (m_aiPanel) m_aiPanel->setPlainText("🎯 GOALS:\n- " + gl.join("\n- "));
        onAIGoalsReady(gl);
    });
    connect(m_superAI, &SuperAI::habitsReady, this, [=](const QStringList& hb){
        if (m_aiPanel) m_aiPanel->setPlainText("🔁 HABITS:\n- " + hb.join("\n- "));
        onAIHabitsReady(hb);
    });

    // Calendar-side dashboard: show details block appended to ::buildDailyDashboardHtml
    connect(m_superAI, &SuperAI::analysisComplete, this, [=](const QString& s){
        QString html = buildDailyDashboardHtml(m_selectedDate);
        if (!s.isEmpty()) {
            html += QString(
                "<div style='margin-top:12px; padding:12px; border:1px solid rgba(0,0,0,0.08);"
                "border-radius:12px;'><div style='font-size:12px; opacity:.7; "
                "text-transform:uppercase; letter-spacing:.04em;'>Details</div>"
                "<pre style='white-space:pre-wrap; margin-top:6px;'>%1</pre></div>")
                .arg(s.toHtmlEscaped());
        }
        setDashboardHtml(html);
    });
}


// ==================================
// ============ Animations ==========
// ==================================

void UltraMainWindow::setupAnimations() {
    m_fadeAnimation = new QPropertyAnimation(this, "windowOpacity");
    m_fadeAnimation->setDuration(500);
    m_fadeAnimation->setEasingCurve(QEasingCurve::OutCubic);

    m_glowAnimation = new QPropertyAnimation(this, "geometry");
    m_glowAnimation->setDuration(1000);
    m_glowAnimation->setEasingCurve(QEasingCurve::InOutSine);
    m_glowAnimation->setLoopCount(-1);

    m_shimmerAnimation = new QPropertyAnimation(this, "geometry");
    m_shimmerAnimation->setDuration(2000);
    m_shimmerAnimation->setEasingCurve(QEasingCurve::InOutSine);
    m_shimmerAnimation->setLoopCount(-1);
}

// Placeholders to keep API surface intact (no-ops).
void UltraMainWindow::setupEffects() {}
void UltraMainWindow::applyGlassmorphism() {}

void UltraMainWindow::applyStunningEffects() {
    // Ensure standard window frame (undo translucent/frameless if set elsewhere)
    setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint & ~Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
}


// ==================================
// ============ Slots/UI ============
// ==================================

/**
 * @brief Selection callback (also kicked during initialization).
 */
void UltraMainWindow::onDateSelected(const QDate& date) {
    m_selectedDate = date;
    if (m_superAI) m_superAI->analyzeSchedule(m_events);
}

/**
 * @brief Append the AI "Details" section to the daily dashboard card.
 */
void UltraMainWindow::onAIAnalysisComplete(const QString& analysis) {
    const bool light = (m_theme == ThemeMode::Light);
    QString html = buildDailyDashboardHtml(m_selectedDate);
    if (!analysis.isEmpty()) {
        html = appendSectionCard(html, "Details",
                                 "<pre style='white-space:pre-wrap;margin:0;'>"
                                 + analysis.toHtmlEscaped() + "</pre>", light);
    }
    setDashboardHtml(html);
}

/**
 * @brief If AI returns concrete suggestions (as Events), render them as a section.
 *        Otherwise we leave the local suggestions/dashboard as-is.
 */
void UltraMainWindow::onAISuggestionsReady(const QList<Event>& aiSuggestions) {
    if (!m_aiChat) return; // dashboard now uses webview; this preserves compatibility

    if (!aiSuggestions.isEmpty()) {
        QStringList lines;
        for (const auto& e : aiSuggestions) {
            const QString when = QString("%1–%2")
                .arg(e.getStartTime().time().toString("hh:mm"))
                .arg(e.getEndTime().time().toString("hh:mm"));
            const QString notes = descNotes(e);
            lines << QString("• <b>%1</b>  <span style='opacity:.7'>(%2)</span>%3")
                        .arg(e.getTitle().toHtmlEscaped(),
                             when,
                             notes.isEmpty() ? "" : QString("<br>&nbsp;&nbsp;&nbsp;%1").arg(notes.toHtmlEscaped()));
        }

        QString html = buildDailyDashboardHtml(m_selectedDate);
        html += QString(
            "<div style='margin-top:12px; padding:12px; border:1px solid rgba(0,0,0,0.08);"
            "border-radius:12px;'>"
            "<div style='font-size:12px; opacity:.7; text-transform:uppercase; letter-spacing:.04em;'>AI Suggestions</div>"
            "<div style='margin-top:8px; line-height:1.55;'>%1</div>"
            "</div>").arg(lines.join("<br>"));

        m_aiChat->setHtml(html);
    }
}

/**
 * @brief Add "Insights" section to dashboard.
 */
void UltraMainWindow::onAIInsightsReady(const QString& insights) {
    const bool light = (m_theme == ThemeMode::Light);
    QString html = buildDailyDashboardHtml(m_selectedDate);
    if (!insights.isEmpty()) {
        html = appendSectionCard(html, "Insights",
                                 "<pre style='white-space:pre-wrap;margin:0;'>"
                                 + insights.toHtmlEscaped() + "</pre>", light);
    }
    setDashboardHtml(html);
}

/**
 * @brief Add "Stress" section to dashboard.
 */
void UltraMainWindow::onAIStressAnalysisReady(const QString& text) {
    const bool light = (m_theme == ThemeMode::Light);
    QString html = buildDailyDashboardHtml(m_selectedDate);
    if (!text.isEmpty()) {
        html = appendSectionCard(html, "Stress",
                                 "<pre style='white-space:pre-wrap;margin:0;'>"
                                 + text.toHtmlEscaped() + "</pre>", light);
    }
    setDashboardHtml(html);
}

/**
 * @brief Add "Optimization" section to dashboard.
 */
void UltraMainWindow::onAIOptimizationReady(const QString& text) {
    const bool light = (m_theme == ThemeMode::Light);
    QString html = buildDailyDashboardHtml(m_selectedDate);
    if (!text.isEmpty()) {
        html = appendSectionCard(html, "Optimization",
                                 "<pre style='white-space:pre-wrap;margin:0;'>"
                                 + text.toHtmlEscaped() + "</pre>", light);
    }
    setDashboardHtml(html);
}

/**
 * @brief Update right-side list and append "Goals" section to dashboard.
 */
void UltraMainWindow::onAIGoalsReady(const QStringList& goals) {
    if (m_goalsPanel) { m_goalsPanel->clear(); m_goalsPanel->addItems(goals); }
    const bool light = (m_theme == ThemeMode::Light);
    QString html = buildDailyDashboardHtml(m_selectedDate);
    html = appendSectionCard(html, "Goals", ulList(goals), light);
    setDashboardHtml(html);
}

/**
 * @brief Update right-side list and append "Habits" section to dashboard.
 */
void UltraMainWindow::onAIHabitsReady(const QStringList& habits) {
    if (m_habitsPanel) { m_habitsPanel->clear(); m_habitsPanel->addItems(habits); }
    const bool light = (m_theme == ThemeMode::Light);
    QString html = buildDailyDashboardHtml(m_selectedDate);
    html = appendSectionCard(html, "Habits", ulList(habits), light);
    setDashboardHtml(html);
}


// ==========================================
// ============ Periodic Updates ============
// ==========================================

void UltraMainWindow::updateAdvancedFeatures() {
    updateAI();
    updateAnalytics();
    updateProductivity();
    updateTeam();
    updateSettings();
}

void UltraMainWindow::updateAI() {
    // Intentionally empty: only run AI from buttons or when user changes events.
}

void UltraMainWindow::updateAnalytics() {
    // Demo values for progress bars (if those panels exist)
    if (m_pbDaily)   m_pbDaily->setValue(QRandomGenerator::global()->bounded(70, 100));
    if (m_pbWeekly)  m_pbWeekly->setValue(QRandomGenerator::global()->bounded(60, 95));
    if (m_pbMonthly) m_pbMonthly->setValue(QRandomGenerator::global()->bounded(50, 90));
    if (m_pbBalance) m_pbBalance->setValue(QRandomGenerator::global()->bounded(40, 100));
}

void UltraMainWindow::updateProductivity() { /* reserved */ }
void UltraMainWindow::updateTeam()         { /* reserved */ }
void UltraMainWindow::updateSettings()     { /* reserved */ }


// ============================================
// ============ Styles for Aux Tabs ===========
// ============================================

static const char* kBtnStyle =
    "QPushButton{ background:#2E3136; border:1px solid #3C4046; border-radius:10px;"
    "            color:#FFFFFF; font-weight:600; padding:10px 14px; }"
    "QPushButton:hover{ background:#3A3E44; }"
    "QPushButton:pressed{ background:#2A2D32; }";

static const char* kPanelStyle =
    "QTextEdit{ background:#212429; border:1px solid #3C4046; border-radius:10px;"
    "           color:#EEF1F5; padding:12px; font-size:13px; }";

static const char* kListStyle =
    "QListWidget{ background:#212429; border:1px solid #3C4046; border-radius:10px;"
    "             color:#EEF1F5; padding:8px; }"
    "QListWidget::item{ padding:8px; margin:2px; border-radius:6px; }"
    "QListWidget::item:selected{ background:#3A3E44; }";

// (Optional) AI tab; kept here intact but not constructed by default.
void UltraMainWindow::buildUltraAITab() {
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(12);

    m_aiPanel = new QTextEdit;
    m_aiPanel->setReadOnly(true);
    m_aiPanel->setStyleSheet(kPanelStyle);
    m_aiPanel->setPlainText(
        "🤖 AI ASSISTANT\n\n"
        "Welcome to the most advanced AI calendar system!\n\n"
        "✨ Features:\n"
        "• AI Analysis\n"
        "• Smart Suggestions\n"
        "• Productivity Insights\n"
        "• Goal Recommendations\n"
        "• Habit Tracking\n"
        "• Stress Analysis\n"
        "• Work-life Balance Optimization\n\n"
        "Use the buttons below."
    );
    lay->addWidget(m_aiPanel, 1);

    auto* grid = new QGridLayout; lay->addLayout(grid);
    m_btnAnalyze  = new QPushButton("🧠 Analyze Schedule");
    m_btnSuggest  = new QPushButton("✨ Get Suggestions");
    m_btnSuggest->hide(); // Suggest removed from UI (kept in code for compatibility)
    m_btnInsights = new QPushButton("💡 Get Insights");
    m_btnGoals    = new QPushButton("🎯 Set Goals");
    m_btnHabits   = new QPushButton("🔄 Track Habits");
    m_btnStress   = new QPushButton("😌 Stress Analysis");
    m_btnOptimize = new QPushButton("⚖️ Optimize Balance");

    for (QPushButton* b : { m_btnAnalyze, m_btnSuggest, m_btnInsights, m_btnGoals,
                            m_btnHabits,  m_btnStress,  m_btnOptimize })
        b->setStyleSheet(kBtnStyle);

    grid->addWidget(m_btnAnalyze,  0, 0);
    grid->addWidget(m_btnSuggest,  0, 1);
    grid->addWidget(m_btnInsights, 0, 2);
    grid->addWidget(m_btnGoals,    1, 0);
    grid->addWidget(m_btnHabits,   1, 1);
    grid->addWidget(m_btnStress,   1, 2);
    grid->addWidget(m_btnOptimize, 2, 1);

    // Wire actions
    connect(m_btnAnalyze, &QPushButton::clicked, this, [=]{
        qDebug() << "[UI] Analyze clicked";
        if (m_superAI) m_superAI->analyzeSchedule(m_events);
    });
    connect(m_btnSuggest,  &QPushButton::clicked, this, [=]{ if (m_superAI) m_superAI->generateSmartSuggestions(m_selectedDate); });
    connect(m_btnInsights, &QPushButton::clicked, this, [=]{ if (m_superAI) m_superAI->provideInsights(m_events); });
    connect(m_btnGoals,    &QPushButton::clicked, this, [=]{ if (m_superAI) m_superAI->suggestGoals(m_events); });
    connect(m_btnHabits,   &QPushButton::clicked, this, [=]{ if (m_superAI) m_superAI->recommendHabits(m_events); });
    connect(m_btnStress,   &QPushButton::clicked, this, [=]{ if (m_superAI) m_superAI->analyzeStress(m_events); });
    connect(m_btnOptimize, &QPushButton::clicked, this, [=]{ if (m_superAI) m_superAI->optimizeWorkLifeBalance(m_events); });

    // Mirror AI outputs to this panel as plain text
    connect(m_superAI, &SuperAI::analysisComplete,    this, [=](const QString& s){ if (m_aiPanel) m_aiPanel->setPlainText(s); });
    connect(m_superAI, &SuperAI::insightsReady,       this, [=](const QString& s){ if (m_aiPanel) m_aiPanel->setPlainText(s); });
    connect(m_superAI, &SuperAI::stressAnalysisReady, this, [=](const QString& s){ if (m_aiPanel) m_aiPanel->setPlainText(s); });
    connect(m_superAI, &SuperAI::optimizationReady,   this, [=](const QString& s){ if (m_aiPanel) m_aiPanel->setPlainText(s); });
    connect(m_superAI, &SuperAI::goalsReady,  this, [=](const QStringList& gl){
        if (!m_aiPanel) return;
        m_aiPanel->setPlainText("🎯 GOALS:\n- " + gl.join("\n- "));
    });
    connect(m_superAI, &SuperAI::habitsReady, this, [=](const QStringList& hb){
        if (!m_aiPanel) return;
        m_aiPanel->setPlainText("🧱 HABITS:\n- " + hb.join("\n- "));
    });

    m_mainTabs->addTab(w, "🤖 EduSync AI");
}

// (Optional) analytics & productivity tabs (not constructed by default).
void UltraMainWindow::buildAnalyticsTab() {
    auto* w = new QWidget; auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12); lay->setSpacing(12);

    m_analyticsPanel = new QTextEdit; m_analyticsPanel->setReadOnly(true); m_analyticsPanel->setStyleSheet(kPanelStyle);
    m_analyticsPanel->setPlainText("📊 ANALYTICS DASHBOARD\n\nReal-time insights and performance metrics…");
    lay->addWidget(m_analyticsPanel, 1);

    auto* grid = new QGridLayout; lay->addLayout(grid);

    auto makeLabel = [](const QString& t){
        auto* l = new QLabel(t);
        l->setStyleSheet("color:#EEF1F5; font-weight:600;");
        return l;
    };

    m_pbDaily   = new QProgressBar;
    m_pbWeekly  = new QProgressBar;
    m_pbMonthly = new QProgressBar;
    m_pbBalance = new QProgressBar;

    const char* kPB =
        "QProgressBar{ background:#212429; border:1px solid #3C4046; border-radius:8px;"
        "              color:#EEF1F5; text-align:center; height:18px; }"
        "QProgressBar::chunk{ background:#5887FF; border-radius:6px; }";

    for (auto* pb : {m_pbDaily, m_pbWeekly, m_pbMonthly, m_pbBalance}) pb->setStyleSheet(kPB);

    grid->addWidget(makeLabel("Daily Progress:"),   0,0); grid->addWidget(m_pbDaily,   0,1);
    grid->addWidget(makeLabel("Weekly Progress:"),  1,0); grid->addWidget(m_pbWeekly,  1,1);
    grid->addWidget(makeLabel("Monthly Progress:"), 2,0); grid->addWidget(m_pbMonthly, 2,1);
    grid->addWidget(makeLabel("Work-Life Balance:"),3,0); grid->addWidget(m_pbBalance, 3,1);

    connect(m_updateTimer, &QTimer::timeout, this, [=]{
        if (!isVisible()) return;
        if (m_pbDaily)   m_pbDaily->setValue(QRandomGenerator::global()->bounded(70, 100));
        if (m_pbWeekly)  m_pbWeekly->setValue(QRandomGenerator::global()->bounded(60, 95));
        if (m_pbMonthly) m_pbMonthly->setValue(QRandomGenerator::global()->bounded(50, 90));
        if (m_pbBalance) m_pbBalance->setValue(QRandomGenerator::global()->bounded(40, 100));
    });

    m_mainTabs->addTab(w, "📊 Analytics");
}

void UltraMainWindow::buildProductivityTab() {
    auto* w = new QWidget; auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(12, 12, 12, 12); hl->setSpacing(12);

    m_productivityPanel = new QTextEdit; m_productivityPanel->setReadOnly(true); m_productivityPanel->setStyleSheet(kPanelStyle);
    m_productivityPanel->setPlainText("⚡ ULTRA PRODUCTIVITY\n\nFocus scores, streaks, and blockers.");
    hl->addWidget(m_productivityPanel, 1);

    auto* side = new QVBoxLayout; side->setSpacing(8); hl->addLayout(side, 1);

    auto* goalsLabel  = new QLabel("🎯 Goals");  goalsLabel->setStyleSheet("color:#EEF1F5; font-weight:700;");
    auto* habitsLabel = new QLabel("🔄 Habits"); habitsLabel->setStyleSheet("color:#EEF1F5; font-weight:700;");

    m_goalsPanel  = new QListWidget; m_goalsPanel->setStyleSheet(kListStyle);
    m_habitsPanel = new QListWidget; m_habitsPanel->setStyleSheet(kListStyle);

    side->addWidget(goalsLabel);
    side->addWidget(m_goalsPanel, 1);
    side->addWidget(habitsLabel);
    side->addWidget(m_habitsPanel, 1);

    connect(m_superAI, &SuperAI::goalsReady,  this, [=](const QStringList& g){
        if (m_goalsPanel)  { m_goalsPanel->clear();  m_goalsPanel->addItems(g); }});
    connect(m_superAI, &SuperAI::habitsReady, this, [=](const QStringList& h){
        if (m_habitsPanel) { m_habitsPanel->clear(); m_habitsPanel->addItems(h); }});

    m_mainTabs->addTab(w, "⚡ Productivity");
}

/**
 * @brief Small Settings tab (theme toggles + reset).
 */
void UltraMainWindow::buildSettingsTab() {
    auto* w = new QWidget; auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12); lay->setSpacing(12);

    m_settingsPanel = new QTextEdit; m_settingsPanel->setReadOnly(true); m_settingsPanel->setStyleSheet(kPanelStyle);
    m_settingsPanel->setPlainText(
        "⚙️ SETTINGS\n\n"
        "• Theme: Dark (default)\n"
        "• AI Verbosity: Balanced\n"
        "• Notifications: On\n"
        "• Data: Local runtime (no persistence)\n\n"
        "This is a placeholder panel; wire real settings here later."
    );
    lay->addWidget(m_settingsPanel, 1);

    auto* row = new QHBoxLayout; lay->addLayout(row);

    m_btnThemeLight  = new QPushButton("🌤️ Light Theme");
    m_btnThemeDark   = new QPushButton("🌙 Dark Theme");
    m_btnResetPanels = new QPushButton("♻️ Reset Panels");

    row->addWidget(m_btnThemeLight);
    row->addWidget(m_btnThemeDark);
    row->addWidget(m_btnResetPanels);

    connect(m_btnThemeLight, &QPushButton::clicked, this, [=]{
        clearLocalStyles();
        applyTheme(ThemeMode::Light);
        QSettings s; s.setValue("theme", "light");
    });

    connect(m_btnThemeDark, &QPushButton::clicked, this, [=]{
        clearLocalStyles();
        applyTheme(ThemeMode::Dark);
        QSettings s; s.setValue("theme", "dark");
    });

    connect(m_btnResetPanels, &QPushButton::clicked, this, [=]{ resetPanels(); });

    m_mainTabs->addTab(w, "⚙️Settings");
}


// =======================================
// ============ Theme & Styles ===========
// =======================================

/**
 * @brief Map category → color used for event chips.
 */
QColor UltraMainWindow::colorForCategory(const QString& cat) const {
    const QString c = cat.trimmed().toLower();
    if (c == "study")    return QColor(66, 165, 245);
    if (c == "work")     return QColor(156, 39, 176);
    if (c == "break")    return QColor(255, 193, 7);
    if (c == "exercise") return QColor(76, 175, 80);
    return QColor(120, 144, 156); // personal/default
}

/**
 * @brief Apply Light/Dark theme (palette + global stylesheet), then restyle calendar chrome.
 */
void UltraMainWindow::applyTheme(ThemeMode m) {
    m_theme = m;

    if (m == ThemeMode::Light) {
        QPalette pal;
        pal.setColor(QPalette::Window,        QColor("#ffffff"));
        pal.setColor(QPalette::WindowText,    QColor("#111"));
        pal.setColor(QPalette::Base,          QColor("#ffffff"));
        pal.setColor(QPalette::Text,          QColor("#111"));
        pal.setColor(QPalette::Button,        QColor("#ffffff"));
        pal.setColor(QPalette::ButtonText,    QColor("#111"));
        pal.setColor(QPalette::Highlight,     QColor("#2f6feb"));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        qApp->setPalette(pal);

        qApp->setStyleSheet(R"(
            QWidget { background:#ffffff; color:#111; }
            QListWidget, QTextEdit {
                background:#ffffff; border:1px solid #d0d7de; border-radius:8px;
            }
            QPushButton {
                background:#ffffff; border:1px solid #d0d7de; border-radius:8px;
                padding:6px 10px; color:#111;
            }
            QPushButton:hover  { background:#f6f8fa; }
            QPushButton:pressed{ background:#eaeef2; }
            QTabWidget::pane{ border:0; }
            QTabBar::tab{
                background:#ffffff; color:#111; border:1px solid #d0d7de;
                border-radius:10px; padding:6px 12px; margin:4px;
            }
            QTabBar::tab:selected{ background:#f6f8fa; }
            QProgressBar{
                background:#ffffff; border:1px solid #d0d7de; border-radius:8px;
                color:#111; text-align:center; height:18px;
            }
            QProgressBar::chunk{ background:#2f6feb; border-radius:6px; }
        )");

        if (m_settingsPanel) m_settingsPanel->append("\nApplied Light theme.");
    } else {
        // Dark theme
        QPalette pal;
        pal.setColor(QPalette::Window,        QColor("#15181b"));
        pal.setColor(QPalette::WindowText,    QColor("#e6e6eb"));
        pal.setColor(QPalette::Base,          QColor("#202427"));
        pal.setColor(QPalette::Text,          QColor("#e6e6eb"));
        pal.setColor(QPalette::Button,        QColor("#2a2f35"));
        pal.setColor(QPalette::ButtonText,    QColor("#e6e6eb"));
        pal.setColor(QPalette::Highlight,     QColor("#5887FF"));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        qApp->setPalette(pal);

        qApp->setStyleSheet(R"(
            QWidget { background:#15181b; color:#e6e6eb; }
            QListWidget, QTextEdit { background:#202427; border:1px solid #2c3136; border-radius:8px; }
            QPushButton { background:#2a2f35; border:1px solid #3a4047; border-radius:8px; padding:6px 10px; }
            QPushButton:hover { background:#333941; }
            QPushButton:pressed { background:#262b30; }
            QTabWidget::pane{ border:0; }
            QTabBar::tab{
                background:#2E3136; color:#fff; border:1px solid #3C4046;
                border-radius:10px; padding:6px 12px; margin:4px;
            }
            QTabBar::tab:selected{ background:#3A3E44; }
            QProgressBar{
                background:#212429; border:1px solid #3C4046; border-radius:8px;
                color:#EEF1F5; text-align:center; height:18px;
            }
            QProgressBar::chunk{ background:#5887FF; border-radius:6px; }
        )");

        if (m_settingsPanel) m_settingsPanel->append("\nApplied Dark theme.");
    }

    // Calendar chrome and buttons
    updateCalendarChrome();
    styleActionButtons();

    // Tell listeners to re-style (calendar page listens to this).
    Q_EMIT themeChanged();
}

/**
 * @brief Reset text areas and progress bars. Does not touch events or calendar.
 */
void UltraMainWindow::resetPanels() {
    if (m_aiPanel)           m_aiPanel->clear();
    if (m_analyticsPanel)    m_analyticsPanel->clear();
    if (m_productivityPanel) m_productivityPanel->clear();

    if (m_pbDaily)   m_pbDaily->setValue(0);
    if (m_pbWeekly)  m_pbWeekly->setValue(0);
    if (m_pbMonthly) m_pbMonthly->setValue(0);
    if (m_pbBalance) m_pbBalance->setValue(0);

    if (m_goalsPanel)  m_goalsPanel->clear();
    if (m_habitsPanel) m_habitsPanel->clear();

    if (m_settingsPanel) m_settingsPanel->append("\nPanels cleared.");
}

/**
 * @brief Clears widget-level styles (so theme sheet can fully control visuals).
 */
void UltraMainWindow::clearLocalStyles() {
    const QList<QWidget*> widgets{
        m_centralWidget,
        m_mainTabs,
        m_aiChat,
        m_analyticsPanel,
        m_productivityPanel,
        m_settingsPanel
    };

    for (QWidget* w : widgets) {
        if (w) w->setStyleSheet("");
    }

    if (m_pbDaily)   m_pbDaily->setStyleSheet("");
    if (m_pbWeekly)  m_pbWeekly->setStyleSheet("");
    if (m_pbMonthly) m_pbMonthly->setStyleSheet("");
    if (m_pbBalance) m_pbBalance->setStyleSheet("");

    for (QPushButton* b : {
        m_aiAnalyzeButton, m_aiSuggestButton, m_aiInsightsButton,
        m_aiGoalsButton, m_aiHabitsButton, m_aiStressButton, m_aiOptimizeButton,
        m_btnThemeLight,  m_btnThemeDark,    m_btnResetPanels
    }) {
        if (b) b->setStyleSheet("");
    }
}

/**
 * @brief Applies calendar table & header CSS and ensures headers/rows stretch.
 *        Also re-applies date formats for today/spillover transparency.
 */
void UltraMainWindow::updateCalendarChrome() {
    if (!m_calendar) return;

    const bool light = (m_theme == ThemeMode::Light);

    // Titlebar visuals
    if (auto *tb = m_monthTitle ? m_monthTitle->parentWidget() : nullptr) {
        tb->setAttribute(Qt::WA_StyledBackground, true);
        tb->setStyleSheet("QWidget{ background:transparent; border:0; }");

        if (m_monthTitle) {
            m_monthTitle->setStyleSheet(light
                ? "color:#111111; letter-spacing:3px;"
                : "color:#e6e6eb; letter-spacing:3px;");
        }

        if (m_prevBtn && m_nextBtn) {
            const QString btnCss = light
              ? "QPushButton{ background:transparent; color:#111; border:1px solid #d0d7de; border-radius:6px; padding:2px 6px; }"
                "QPushButton:hover{ background:#f6f8fa; }"
              : "QPushButton{ background:transparent; color:#e6e6eb; border:1px solid #3a4047; border-radius:6px; padding:2px 6px; }"
                "QPushButton:hover{ background:#2b3138; }";
            m_prevBtn->setStyleSheet(btnCss);
            m_nextBtn->setStyleSheet(btnCss);
        }
    }

    const QString base   = light ? "#ffffff" : "#20262c";
    const QString border = light ? "#e5e7eb" : "#2f3540";
    const QString text   = light ? "#111111" : "#e6e6eb";

    // Calendar grid & fixed weekday header palette
    m_calendar->setStyleSheet(QString(R"(
        QWidget#qt_calendar_navigationbar {
          height:0; min-height:0; max-height:0; padding:0; margin:0; border:0; background:transparent;
        }

        /* Calendar grid */
        QCalendarWidget QTableView#CalView {
          background:%1; border:1px solid %2; gridline-color:%2; outline:0;
        }
        QCalendarWidget QAbstractItemView::item { margin:0; padding:0; }
        QCalendarWidget QAbstractItemView::item:selected { background:transparent; border:0; color:%3; }

        /* Weekday header – fixed gray in both themes */
        QCalendarWidget QHeaderView#CalHeader { background:%4; border:0; }
        QCalendarWidget QHeaderView#CalHeader::section {
          background:%4; color:%5; border:0;
          border-bottom:1px solid %6;
          padding:6px 0;
          font-weight:600; text-transform:uppercase; letter-spacing:.04em;
        }
        QCalendarWidget QHeaderView#CalHeader::section:hover,
        QCalendarWidget QHeaderView#CalHeader::section:focus,
        QCalendarWidget QHeaderView#CalHeader::section:selected {
          background:%4; color:%5;
        }
    )")
        .arg(base)                                         // %1
        .arg(border)                                       // %2
        .arg(text)                                         // %3
        .arg(light ? "#f3f4f6" : "#2a3036")                // %4 header bg
        .arg(light ? "#6b7280" : "#9aa3ab")                // %5 header fg
        .arg(light ? "#e5e7eb" : "#2f3540"));              // %6 header border
    // Header sizing/alignment
    if (auto *view = m_calendar->findChild<QTableView*>()) {
        if (auto *hh = view->horizontalHeader()) {
            hh->setObjectName("CalHeader");
            hh->setSectionResizeMode(QHeaderView::Stretch);
            hh->setFixedHeight(28);
            hh->setDefaultAlignment(Qt::AlignCenter);

            const bool isLight = light;
            hh->setStyleSheet(QString(
                "QHeaderView#CalHeader{background:%1;border:0;}"
                "QHeaderView#CalHeader::section{"
                    "background:%1; color:%2; border:0;"
                    "border-bottom:1px solid %3;"
                    "padding:6px 0; font-weight:600;"
                    "text-transform:uppercase; letter-spacing:.04em;"
                "}"
                "QHeaderView#CalHeader::section:hover,"
                "QHeaderView#CalHeader::section:pressed{background:%1;}"
            ).arg(isLight ? "#f3f4f6" : "#2a3036",
                  isLight ? "#6b7280" : "#9aa3ab",
                  isLight ? "#e5e7eb" : "#2f3540"));

            hh->setAutoFillBackground(true);
            if (auto *vp = hh->viewport()) vp->setAutoFillBackground(true);
            if (auto *st = hh->style()) { st->unpolish(hh); st->polish(hh); }
            hh->update();
        }

        if (auto *vh = view->verticalHeader())
            vh->setSectionResizeMode(QHeaderView::Stretch);

        view->setSelectionMode(QAbstractItemView::NoSelection);
        view->setStyleSheet(R"(
            QTableView::item:selected            { background: transparent; border: 0; }
            QTableView::item:active:selected     { background: transparent; border: 0; }
            QTableView::item:!active:selected    { background: transparent; border: 0; }
            QTableView::item:focus               { outline: 0; }
            QAbstractItemView::item              { background: transparent; }
        )");
    }

    // Re-apply per-date formats (today background, hide spillovers).
    refreshMonthFormats();
}

/**
 * @brief Apply rounded/action styles to the right-side buttons (theme-aware).
 */
void UltraMainWindow::styleActionButtons() {
    QList<QPushButton*> btns = {
        m_aiAnalyzeButton, m_aiSuggestButton, m_aiInsightsButton,
        m_aiGoalsButton,   m_aiHabitsButton,  m_aiStressButton,
        m_aiOptimizeButton
    };

    const QString light = R"(
        QPushButton {
            background:#ffffff;
            color:#111;
            border:1px solid #d0d7de;
            border-radius:16px;
            padding:8px 16px;
            font-weight:600;
        }
        QPushButton:hover  { background:#f6f8fa; }
        QPushButton:pressed{ background:#eaeef2; }
    )";

    const QString dark = R"(
        QPushButton {
            background:#2a2f35;
            color:#ffffff;
            border:1px solid #3a4047;
            border-radius:16px;
            padding:8px 16px;
            font-weight:600;
        }
        QPushButton:hover  { background:#333941; }
        QPushButton:pressed{ background:#262b30; }
    )";

    const QString ss = (m_theme == ThemeMode::Light) ? light : dark;
    for (auto* b : btns) if (b) b->setStyleSheet(ss);
}

/**
 * @brief Re-compute QCalendarWidget date text formats for the visible grid.
 *        - Hide spillover days completely (transparent text, minimal bg).
 *        - Highlight today with a subtle theme-aware background and bold/red-ish text.
 */
void UltraMainWindow::refreshMonthFormats() {
    if (!m_calendar) return;

    const int y = m_calendar->yearShown();
    const int m = m_calendar->monthShown();
    const QDate first(y, m, 1);

    // Clear a safe range around the month
    for (int d = -20; d <= first.daysInMonth() + 20; ++d)
        m_calendar->setDateTextFormat(first.addDays(d), QTextCharFormat());

    // Compute grid start respecting the QCalendar firstDayOfWeek setting
    const int fdow = static_cast<int>(m_calendar->firstDayOfWeek()); // Mon=1..Sun=7
    const int off  = ((first.dayOfWeek() - fdow + 7) % 7);
    const QDate gridStart = first.addDays(-off);

    const bool  light   = (m_theme == ThemeMode::Light);
    const QColor todayBg = light ? QColor("#fff2f2") : QColor("#2d1f1f");
    const QColor todayFg = light ? QColor("#b71c1c") : QColor("#ff8a80");

    for (int i = 0; i < 42; ++i) {
        const QDate d = gridStart.addDays(i);
        QTextCharFormat fmt;

        if (d.month() != m) {
            // Hide spillover numbers entirely
            fmt.setForeground(Qt::transparent);
            fmt.setBackground(QColor(0,0,0,1));  // tiny alpha to force paint
            m_calendar->setDateTextFormat(d, fmt);
            continue;
        }

        fmt.setForeground(QBrush(m_calendar->palette().color(QPalette::Text)));
        fmt.setFontWeight(QFont::Normal);

        if (d == QDate::currentDate()) {
            fmt.setBackground(todayBg);
            fmt.setForeground(todayFg);
            fmt.setFontWeight(QFont::DemiBold);
        }

        m_calendar->setDateTextFormat(d, fmt);
    }
}


// =======================================
// ============ Qt Events ================
// =======================================

/**
 * @brief On minimize/restore, ensure the calendar internals keep mouse tracking
 *        and re-apply formats safely (queued restyle).
 */
void UltraMainWindow::changeEvent(QEvent* e) {
    QMainWindow::changeEvent(e);
    if (e->type() == QEvent::WindowStateChange) {
        QTimer::singleShot(0, this, [this]{
            if (auto *view = m_calendar ? m_calendar->findChild<QTableView*>() : nullptr) {
                view->setMouseTracking(true);
                if (auto *vp = view->viewport()) {
                    vp->setMouseTracking(true);
                    vp->setAttribute(Qt::WA_Hover, true);
                }
            }
            // Ask ModernCalendarWidget to restyle its internals safely
            if (auto *mc = qobject_cast<ModernCalendarWidget*>(m_calendar))
                QMetaObject::invokeMethod(mc, "scheduleRestyle", Qt::QueuedConnection);

            refreshMonthFormats();
        });
    }
}

void UltraMainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    QTimer::singleShot(0, this, [this]{
        // Reserved for responsive tweaks if needed later
    });
}


// =======================================
// ============ HTML builders ============
// =======================================

/**
 * @brief Helper: minutes → human-friendly e.g., "1h 30m"
 */
QString UltraMainWindow::mm(int minutes) {
    if (minutes <= 0) return "0m";
    int h = minutes / 60;
    int m = minutes % 60;
    if (h && m) return QString("%1h %2m").arg(h).arg(m);
    if (h)       return QString("%1h").arg(h);
    return QString("%1m").arg(m);
}

/**
 * @brief Thin wrapper to external dashboard renderer (kept to allow swapping).
 */
QString UltraMainWindow::buildDailyDashboardHtml(const QDate& d) const {
    const bool light = (m_theme == ThemeMode::Light);
    return ::buildDailyDashboardHtml(m_events, light, d); // note the "::"
}

/**
 * @brief Build local, deterministic suggestions based only on today's events.
 *        Used as fallback/instant content before AI responds.
 */
QString UltraMainWindow::buildLocalSuggestionsHtml(const QDate& d) const {
    // Gather today's events (sorted)
    QVector<const Event*> todays; todays.reserve(m_events.size());
    for (const auto& e : m_events) if (e.isOnDate(d)) todays.push_back(&e);
    std::sort(todays.begin(), todays.end(),
              [](const Event* a, const Event* b){ return a->getStartTime() < b->getStartTime(); });

    // Totals
    int focus = 0, br = 0, ex = 0, sessions = 0;
    for (const Event* e : todays) {
        const int dur = e->getStartTime().secsTo(e->getEndTime())/60;
        const QString cat = descCategory(*e).toLower();
        if      (cat == "break")    br += dur;
        else if (cat == "exercise") ex += dur;
        else { focus += dur; sessions++; }
    }

    // Find free gaps within a reasonable workday window
    auto gapOf = [&](int minMinutes)->QString {
        const QTime dayStart(9,0), dayEnd(17,0);
        QTime cur = dayStart;
        for (const Event* e : todays) {
            const QTime s = e->getStartTime().time();
            if (s > cur) {
                int gap = cur.secsTo(s)/60;
                if (gap >= minMinutes) return QString("%1–%2").arg(cur.toString("hh:mm"), s.toString("hh:mm"));
            }
            cur = std::max(cur, e->getEndTime().time());
        }
        if (cur < dayEnd) {
            int gap = cur.secsTo(dayEnd)/60;
            if (gap >= minMinutes) return QString("%1–%2").arg(cur.toString("hh:mm"), dayEnd.toString("hh:mm"));
        }
        return {};
    };

    QStringList tips;
    if (todays.isEmpty()) {
        tips << "Block a 90m deep-work session 09:00–10:30 on your top priority."
             << "Add two 10m recovery breaks (late morning & mid-afternoon)."
             << "Schedule 30–45m exercise around 17:30.";
    } else {
        if (br < 20 && focus >= 60) tips << "Insert two 10m recovery breaks (e.g., 10:50 and 14:50).";
        const QString wslot = gapOf(30);
        if (ex < 30) tips << QString("Add a 30–45m workout%1.").arg(wslot.isEmpty() ? "" : " in " + wslot);
        const QString dw = gapOf(90);
        if (!dw.isEmpty()) tips << QString("Schedule a 90m deep-work block in %1.").arg(dw);
        if (sessions >= 3 && focus >= 90 && dw.isEmpty())
            tips << "Batch small tasks into a single 60–90m block to cut context switching.";
        tips << "Add a 10m daily review at 17:20 to prep tomorrow.";
    }

    QString li;
    for (const QString& t : tips) li += "<li>" + t.toHtmlEscaped() + "</li>";

    return QString(
        "<div style='margin-top:12px; padding:12px; border:1px solid rgba(0,0,0,0.08);"
        "border-radius:12px;'>"
        "<div style='font-size:12px; opacity:.7; text-transform:uppercase; letter-spacing:.04em;'>Suggestions</div>"
        "<ul style='margin:8px 0 0 16px; line-height:1.55;'>%1</ul>"
        "</div>").arg(li);
}


// =====================================================
// ============ New Section Card Utilities =============
// =====================================================

/**
 * @brief Append a styled "card" section to an existing HTML string.
 */
static QString appendSectionCard(const QString& baseHtml,
                                 const QString& title,
                                 const QString& bodyHtml,
                                 bool lightTheme)
{
    const QString border  = lightTheme ? "#e5e7eb" : "rgba(255,255,255,0.06)";
    const QString cardBg  = lightTheme ? "#ffffff" : "#202427";
    const QString titleFx = "font-size:12px;opacity:.7;text-transform:uppercase;letter-spacing:.04em;";

    QString block;
    block += "<div style='margin-top:12px;padding:12px;border:1px solid " + border + ";"
             "border-radius:12px;background:" + cardBg + ";'>";
    block += "<div style='" + titleFx + "'>" + title.toHtmlEscaped() + "</div>";
    block += "<div style='margin-top:8px;'>" + bodyHtml + "</div>";
    block += "</div>";

    return baseHtml + block;
}

/**
 * @brief Convert a QStringList into a simple HTML UL.
 */
static QString ulList(const QStringList& items)
{
    QString out = "<ul style='margin:12px 0 0 18px;padding:0;line-height:1.55;'>";
    for (const auto& it : items) out += "<li>" + it.toHtmlEscaped() + "</li>";
    out += "</ul>";
    return out;
}

/**
 * @brief Convert messy blob text into a cleaned list of bullet items (not used directly here,
 *        but kept for future AI text parsing—requested to keep code).
 */
static QStringList parseLooseList(const QString& blob)
{
    QString s = blob;

    // Strip headings if present
    s.remove(QRegularExpression("^\\s*Goals\\s*", QRegularExpression::CaseInsensitiveOption));
    s.remove(QRegularExpression("^\\s*Habits\\s*", QRegularExpression::CaseInsensitiveOption));

    // Normalize HTML-ish list bits to newlines
    s.replace(QRegularExpression("</li>\\s*<li>", QRegularExpression::CaseInsensitiveOption), "\n");
    s.replace(QRegularExpression("</?li>",       QRegularExpression::CaseInsensitiveOption), "\n");
    s.replace(QRegularExpression("</?ul>",       QRegularExpression::CaseInsensitiveOption), "\n");

    // Also split on bullets
    QStringList lines = s.split(QRegularExpression("[\\n•]+"), Qt::SkipEmptyParts);

    // Trim and discard empty artifacts
    for (QString& L : lines) {
        L = L.trimmed();
        if (L == "-" || L == "—") L.clear();
    }
    lines.erase(std::remove_if(lines.begin(), lines.end(),
                               [](const QString& x){ return x.trimmed().isEmpty(); }),
                lines.end());
    return lines;
}


// =================================================
// ============ Event Dialogs (Add/Edit) ===========
// =================================================

/**
 * @brief Open "New event" dialog, insert event(s) based on recurrence.
 */
void UltraMainWindow::openNewEventDialog(const QDate& date)
{
    if (!date.isValid()) return;

    QDialog dlg(this);
    dlg.setWindowTitle("New event");
    dlg.setModal(true);

    // --- Dialog content scaffold --------------------------------------------
    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // Card wrapper (visual)
    auto *card = new QWidget(&dlg);
    auto *cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(16, 16, 16, 16);
    cardLay->setSpacing(12);
    card->setStyleSheet(R"(
        QWidget {
          background: palette(base);
          border: 1px solid rgba(0,0,0,0.12);
          border-radius: 12px;
        }
    )");
    root->addWidget(card);

    // Title
    auto *titleEdit = new QLineEdit(card);
    titleEdit->setPlaceholderText("Event title");
    titleEdit->setText("New Event");
    titleEdit->setMinimumHeight(34);
    titleEdit->setStyleSheet("QLineEdit{font-weight:600;}");
    cardLay->addWidget(titleEdit);

    // Row: category + group + all-day
    auto *row1 = new QHBoxLayout; row1->setSpacing(10);
    cardLay->addLayout(row1);

    auto *category = new QComboBox(card);
    category->addItems({"Study","Work","Break","Exercise","Personal"});
    category->setMinimumWidth(140);

    auto *groupCombo = new QComboBox(card);
    groupCombo->addItems({"Personal","School","Work"});
    groupCombo->setMinimumWidth(120);

    auto *allDay = new QCheckBox("All day", card);

    row1->addWidget(category);
    row1->addWidget(groupCombo);
    row1->addStretch(1);
    row1->addWidget(allDay);

    // Row: date + time range + recurrence
    auto *row2 = new QHBoxLayout; row2->setSpacing(10);
    cardLay->addLayout(row2);

    auto *dateEdit = new QDateEdit(card);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDate(date);

    auto *startTime = new QTimeEdit(card);
    startTime->setDisplayFormat("hh:mm");
    startTime->setTime(QTime(9, 0));

    auto *toLabel = new QLabel("to", card);

    auto *endTime = new QTimeEdit(card);
    endTime->setDisplayFormat("hh:mm");
    endTime->setTime(QTime(10, 0));

    auto *recur = new QComboBox(card);
    recur->addItems({"Does not repeat", "Every day", "Every week on this day", "Every month on this date"});
    recur->setMinimumWidth(210);

    row2->addWidget(dateEdit);
    row2->addStretch(1);
    row2->addWidget(startTime);
    row2->addWidget(toLabel);
    row2->addWidget(endTime);
    row2->addStretch(1);
    row2->addWidget(recur);

    // All-day toggle behavior
    connect(allDay, &QCheckBox::toggled, this, [=]{
        const bool on = allDay->isChecked();
        startTime->setEnabled(!on);
        endTime->setEnabled(!on);
        if (on) {
            startTime->setTime(QTime(0, 0));
            endTime->setTime(QTime(23, 59));
        } else {
            if (startTime->time() == QTime(0, 0))  startTime->setTime(QTime(9, 0));
            if (endTime->time()   == QTime(23, 59)) endTime->setTime(QTime(10, 0));
        }
    });

    // Description + tiny toolbar
    auto *descLabel = new QLabel("Description", card);
    descLabel->setStyleSheet("color:palette(mid);");
    cardLay->addWidget(descLabel);

    auto *toolbar = new QToolBar(card);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->setStyleSheet("QToolBar{border:0;}");
    cardLay->addWidget(toolbar);

    auto *desc = new QTextEdit(card);
    desc->setPlaceholderText("Notes, bullets, links…");
    desc->setMinimumHeight(120);
    cardLay->addWidget(desc);

    // Basic formatting actions
    auto mk = [&](const QString& text) {
        auto *a = toolbar->addAction(text);
        a->setCheckable(true);
        return a;
    };
    QAction* actB = mk("B");
    QAction* actI = mk("I");
    QAction* actU = mk("U");
    QAction* actBullets = toolbar->addAction("•");

    connect(actB, &QAction::toggled, desc, [=](bool on){
        QTextCharFormat fmt; fmt.setFontWeight(on ? QFont::Bold : QFont::Normal);
        QTextCursor c = desc->textCursor(); if (!c.hasSelection()) c.select(QTextCursor::WordUnderCursor);
        c.mergeCharFormat(fmt); desc->mergeCurrentCharFormat(fmt);
    });
    connect(actI, &QAction::toggled, desc, [=](bool on){
        QTextCharFormat fmt; fmt.setFontItalic(on);
        QTextCursor c = desc->textCursor(); if (!c.hasSelection()) c.select(QTextCursor::WordUnderCursor);
        c.mergeCharFormat(fmt); desc->mergeCurrentCharFormat(fmt);
    });
    connect(actU, &QAction::toggled, desc, [=](bool on){
        QTextCharFormat fmt; fmt.setFontUnderline(on);
        QTextCursor c = desc->textCursor(); if (!c.hasSelection()) c.select(QTextCursor::WordUnderCursor);
        c.mergeCharFormat(fmt); desc->mergeCurrentCharFormat(fmt);
    });
    connect(actBullets, &QAction::triggered, desc, [=]{
        QTextCursor c = desc->textCursor();
        QTextListFormat lf; lf.setStyle(QTextListFormat::ListDisc);
        c.createList(lf);
    });

    // Buttons row
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save, &dlg);
    root->addWidget(buttons);

    if (auto *save = buttons->button(QDialogButtonBox::Save)) {
        save->setText("Save");
        save->setStyleSheet(R"(
            QPushButton {
              background:#2f6feb; color:white; border-radius:8px; padding:6px 14px;
            }
            QPushButton:hover { background:#295fce; }
        )");
    }

    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    // On Save: validate & append event(s) based on recurrence
    connect(buttons, &QDialogButtonBox::accepted, &dlg, [&, this]{
        const QString t = titleEdit->text().trimmed();
        if (t.isEmpty()) { titleEdit->setFocus(); return; }

        QDate d = dateEdit->date();
        QTime s = startTime->time();
        QTime e = endTime->time();
        if (!allDay->isChecked() && s >= e) {
            e = s.addSecs(30*60);
        }

        const QString cat   = category->currentText();
        const QString notes = desc->toPlainText().trimmed();
        const QColor  col   = colorForCategory(cat);
        const QString packedDesc = notes.isEmpty() ? cat : (cat + "::" + notes);

        const QDateTime s0(d, s);
        const QDateTime e0(d, e);

        auto appendEvent = [&](const QDateTime& st, const QDateTime& en){
            Event ev(t, packedDesc, st, en, col);
            m_events.append(ev);
        };

        switch (recur->currentIndex()) {
        case 0: // Does not repeat
            appendEvent(s0, e0);
            break;
        case 1: // Every day — next 365 days
            for (int i = 0; i < 365; ++i)
                appendEvent(s0.addDays(i), e0.addDays(i));
            break;
        case 2: // Every week on this day — next 52 weeks
            for (int w = 0; w < 52; ++w)
                appendEvent(s0.addDays(7*w), e0.addDays(7*w));
            break;
        case 3: // Every month on this date — next 12 months
            for (int m = 0; m < 12; ++m) {
                QDate ds = s0.date().addMonths(m);
                QDate de = e0.date().addMonths(m);
                if (!ds.isValid() || !de.isValid()) continue;
                appendEvent(QDateTime(ds, s0.time()), QDateTime(de, e0.time()));
            }
            break;
        }

        if (m_calendar) {
            m_calendar->setEvents(m_events);
            m_calendar->setSelectedDate(d);
            m_calendar->update();
        }
        refreshMonthFormats();
        dlg.accept();
    });

    dlg.exec();
}

/**
 * @brief Open "Edit event" dialog; writes back changes to the given event object.
 */
bool UltraMainWindow::openEditEventDialog(Event& e)
{
    QDialog dlg(this);
    dlg.setWindowTitle("Edit event");
    dlg.setModal(true);

    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto *card = new QWidget(&dlg);
    auto *cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(16, 16, 16, 16);
    cardLay->setSpacing(12);
    card->setStyleSheet(R"(
        QWidget {
          background: palette(base);
          border: 1px solid rgba(0,0,0,0.12);
          border-radius: 12px;
        }
    )");
    root->addWidget(card);

    // Title
    auto *titleEdit = new QLineEdit(card);
    titleEdit->setPlaceholderText("Event title");
    titleEdit->setText(e.getTitle());
    titleEdit->setMinimumHeight(34);
    titleEdit->setStyleSheet("QLineEdit{font-weight:600;}");
    cardLay->addWidget(titleEdit);

    // Category & all day
    auto *row1 = new QHBoxLayout; row1->setSpacing(10);
    cardLay->addLayout(row1);

    auto *category = new QComboBox(card);
    category->addItems({"Study","Work","Break","Exercise","Personal"});

    // Unpack description → category + notes
    const QString desc = e.getDescription();
    QString curCat = desc;
    QString curNotes;
    const int idxSep = desc.indexOf("::");
    if (idxSep >= 0) { curCat = desc.left(idxSep); curNotes = desc.mid(idxSep+2); }
    int catIndex = category->findText(curCat, Qt::MatchFixedString);
    if (catIndex < 0) catIndex = 0;
    category->setCurrentIndex(catIndex);

    auto *allDay = new QCheckBox("All day", card);

    row1->addWidget(category);
    row1->addStretch(1);
    row1->addWidget(allDay);

    // Date/time row
    auto *row2 = new QHBoxLayout; row2->setSpacing(10);
    cardLay->addLayout(row2);

    auto *dateEdit  = new QDateEdit(card);  dateEdit->setCalendarPopup(true);
    auto *startTime = new QTimeEdit(card);  startTime->setDisplayFormat("hh:mm");
    auto *toLabel   = new QLabel("to", card);
    auto *endTime   = new QTimeEdit(card);  endTime->setDisplayFormat("hh:mm");

    dateEdit->setDate(e.getStartTime().date());
    startTime->setTime(e.getStartTime().time());
    endTime->setTime(e.getEndTime().time());

    row2->addWidget(dateEdit);
    row2->addStretch(1);
    row2->addWidget(startTime);
    row2->addWidget(toLabel);
    row2->addWidget(endTime);

    // Description box
    auto *descLabel = new QLabel("Description", card);
    descLabel->setStyleSheet("color:palette(mid);");
    cardLay->addWidget(descLabel);

    auto *descEdit = new QTextEdit(card);
    descEdit->setMinimumHeight(120);
    descEdit->setPlainText(curNotes);
    cardLay->addWidget(descEdit);

    // Buttons
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save, &dlg);
    root->addWidget(buttons);
    if (auto *save = buttons->button(QDialogButtonBox::Save)) {
        save->setText("Save");
        save->setStyleSheet("QPushButton { background:#2f6feb; color:white; border-radius:8px; padding:6px 14px; }"
                            "QPushButton:hover { background:#295fce; }");
    }

    // All-day toggle
    connect(allDay, &QCheckBox::toggled, this, [=]{
        const bool on = allDay->isChecked();
        startTime->setEnabled(!on);
        endTime->setEnabled(!on);
        if (on) {
            startTime->setTime(QTime(0,0));
            endTime->setTime(QTime(23,59));
        }
    });

    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    // Save modifies the input Event& e (single instance)
    connect(buttons, &QDialogButtonBox::accepted, &dlg, [&, this]{
        const QString t = titleEdit->text().trimmed();
        if (t.isEmpty()) { titleEdit->setFocus(); return; }

        QDate d = dateEdit->date();
        QTime s = startTime->time();
        QTime en= endTime->time();
        if (!allDay->isChecked() && s >= en) en = s.addSecs(30*60);

        const QString cat   = category->currentText();
        const QString notes = descEdit->toPlainText().trimmed();
        const QString packed = notes.isEmpty() ? cat : (cat + "::" + notes);

        e.setTitle(t);
        e.setDescription(packed);
        e.setStartTime(QDateTime(d, s));
        e.setEndTime(QDateTime(d, en));
        e.setColor(colorForCategory(cat));

        dlg.accept();
    });

    return dlg.exec() == QDialog::Accepted;
}


// =====================================================
// ============ Small helpers for tooltips =============
// =====================================================

/**
 * @brief Build a multi-line tooltip for a given date aggregating its events.
 */
QString UltraMainWindow::tooltipForDate(const QDate& d) const {
    QStringList lines;
    for (const auto& e : m_events) {
        if (!e.isOnDate(d)) continue;
        const QString notes = descNotes(e);
        const QString line = QString("• %1  (%2–%3)%4")
            .arg(e.getTitle(),
                 e.getStartTime().time().toString("hh:mm"),
                 e.getEndTime().time().toString("hh:mm"),
                 notes.isEmpty() ? "" : QString("\n    %1").arg(notes));
        lines << line;
    }
    return lines.join("\n");
}

/**
 * @brief From "Category::Notes" → "Category".
 */
QString UltraMainWindow::descCategory(const Event& e) const {
    const QString d = e.getDescription();
    const int i = d.indexOf("::");
    return (i < 0 ? d : d.left(i)).trimmed();
}

/**
 * @brief From "Category::Notes" → "Notes".
 */
QString UltraMainWindow::descNotes(const Event& e) const {
    const QString d = e.getDescription();
    const int i = d.indexOf("::");
    return (i < 0 ? QString() : d.mid(i + 2)).trimmed();
}


// =====================================================
// ============ Recurrence Expansion API ===============
// =====================================================

/**
 * @brief Append base event with recurrence pattern index:
 *        0=none, 1=daily(365), 2=weekly(52), 3=monthly(12).
 */
void UltraMainWindow::addEventWithRecurrence(const Event& base, int recurIndex) {
    auto appendIf = [&](const QDateTime& st, const QDateTime& en){
        Event ev(base.getTitle(), base.getDescription(), st, en, base.getColor());
        m_events.append(ev);
    };

    const QDateTime s0 = base.getStartTime();
    const QDateTime e0 = base.getEndTime();

    switch (recurIndex) {
    case 0: // Does not repeat
        appendIf(s0, e0);
        break;

    case 1: { // Every day — next 365 days (incl. today)
        for (int i = 0; i < 365; ++i)
            appendIf(s0.addDays(i), e0.addDays(i));
        break;
    }

    case 2: { // Every week on this day — next 52 weeks
        for (int w = 0; w < 52; ++w)
            appendIf(s0.addDays(7*w), e0.addDays(7*w));
        break;
    }

    case 3: { // Every month on this date — next 12 months
        for (int m = 0; m < 12; ++m) {
            const QDate ds = s0.date().addMonths(m);
            const QDate de = e0.date().addMonths(m);
            if (!ds.isValid() || !de.isValid()) continue;
            appendIf(QDateTime(ds, s0.time()), QDateTime(de, e0.time()));
        }
        break;
    }
    }
}

// NOTE: Custom header helper (disabled but preserved for reference).
// void UltraMainWindow::ensureWeekHeader() {
//     return;  // TEMP: disable custom header completely
// }
