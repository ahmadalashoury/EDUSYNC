#include "MainWindow.h"
#include "Theme.h"
#include "CalendarWidget.h"
#include "DayTimelineWidget.h"
#include "WeekTimelineWidget.h"
#include "SearchPanel.h"
#include "NotificationManager.h"
#include "DashboardWidget.h"
#include "SettingsDialog.h"
#include "EventStore.h"
#include "SchedulerEngine.h"
#include "SyncManager.h"
#include "GoogleCalendarProvider.h"
#include "OutlookCalendarProvider.h"
#include "CalDAVProvider.h"
#include "AppleNativeProvider.h"
#include "LocalAssistantService.h"
#include "LLMAssistantService.h"
#include "Event.h"

#include <QApplication>
#include <QFrame>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTimeEdit>
#include <QDateEdit>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QLocale>
#include <QScrollArea>
#include <QTimer>
#include <algorithm>

namespace {

QString eventEditorStylesheet(bool dark) {
    if (dark) {
        return QStringLiteral(R"(
            QDialog#eventEditorDialog {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                            stop:0 #08111d,
                                            stop:0.45 #0d1626,
                                            stop:1 #071019);
                color: #eaf7ff;
            }

            QFrame#eventHeroCard, QFrame#eventSectionCard {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                            stop:0 rgba(255,255,255,0.08),
                                            stop:0.18 rgba(20,31,49,0.94),
                                            stop:1 rgba(11,18,29,0.98));
                border: 1px solid rgba(92, 220, 255, 0.20);
                border-radius: 20px;
            }

            QLabel#eventEyebrow {
                color: #74dfff;
                font-size: 10px;
                font-weight: 700;
                letter-spacing: 0.18em;
                text-transform: uppercase;
            }

            QLabel#eventHeading {
                color: #f4fbff;
                font-size: 24px;
                font-weight: 700;
            }

            QLabel#eventSubtitle {
                color: #8fb1c9;
                font-size: 13px;
                line-height: 1.35em;
            }

            QLabel#eventMicroLabel {
                color: #7bcdf6;
                font-size: 10px;
                font-weight: 700;
                letter-spacing: 0.12em;
                text-transform: uppercase;
            }

            QLineEdit#eventTitleField,
            QLineEdit#eventField,
            QTextEdit#eventField,
            QDateEdit#eventField,
            QTimeEdit#eventField,
            QComboBox#eventField {
                background: rgba(7, 15, 25, 0.90);
                color: #f4fbff;
                border: 1px solid rgba(90, 217, 255, 0.18);
                border-radius: 15px;
                padding: 10px 14px;
                selection-background-color: rgba(85, 217, 255, 0.28);
                font-size: 14px;
            }

            QLineEdit#eventTitleField {
                border-radius: 18px;
                padding: 13px 16px;
                font-size: 18px;
                font-weight: 600;
            }

            QLineEdit#eventTitleField:focus,
            QLineEdit#eventField:focus,
            QTextEdit#eventField:focus,
            QDateEdit#eventField:focus,
            QTimeEdit#eventField:focus,
            QComboBox#eventField:focus {
                border: 1px solid rgba(112, 230, 255, 0.70);
                background: rgba(10, 21, 34, 0.96);
            }

            QTextEdit#eventField {
                padding-top: 14px;
            }

            QLineEdit#eventTitleField::placeholder,
            QLineEdit#eventField::placeholder,
            QTextEdit#eventField {
                color: #6f8aa1;
            }

            QComboBox#eventField::drop-down {
                border: none;
                width: 26px;
            }

            QComboBox#eventField::down-arrow {
                image: none;
                width: 0;
                height: 0;
                border-left: 5px solid transparent;
                border-right: 5px solid transparent;
                border-top: 6px solid #7bdfff;
                margin-right: 10px;
            }

            QComboBox#eventField QAbstractItemView {
                background: #0c1420;
                color: #eef8ff;
                border: 1px solid rgba(92, 220, 255, 0.32);
                selection-background-color: rgba(93, 220, 255, 0.26);
                selection-color: #ffffff;
                outline: 0;
                padding: 6px;
            }

            QCheckBox#eventToggle {
                color: #eaf7ff;
                spacing: 10px;
                font-size: 14px;
                font-weight: 600;
            }

            QCheckBox#eventToggle::indicator {
                width: 22px;
                height: 22px;
                border-radius: 11px;
                border: 1px solid rgba(90, 217, 255, 0.36);
                background: rgba(7, 15, 25, 0.95);
            }

            QCheckBox#eventToggle::indicator:checked {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                            stop:0 #72e6ff,
                                            stop:1 #2f7cff);
                border: 1px solid rgba(178, 243, 255, 0.90);
            }

            QPushButton#eventSecondaryBtn {
                background: rgba(13, 21, 33, 0.88);
                color: #d2e8f5;
                border: 1px solid rgba(103, 219, 255, 0.18);
                border-radius: 15px;
                padding: 10px 18px;
                font-weight: 600;
                font-size: 14px;
            }

            QPushButton#eventSecondaryBtn:hover {
                background: rgba(18, 29, 44, 0.98);
                border-color: rgba(112, 230, 255, 0.42);
            }

            QPushButton#eventPrimaryBtn {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                            stop:0 #72e4ff,
                                            stop:1 #2f7cff);
                color: white;
                border: 1px solid rgba(197, 244, 255, 0.55);
                border-radius: 15px;
                padding: 11px 22px;
                font-weight: 700;
                font-size: 14px;
            }

            QPushButton#eventPrimaryBtn:hover {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                            stop:0 #89ecff,
                                            stop:1 #3d8cff);
            }

            QPushButton#eventDangerBtn {
                background: transparent;
                color: #ff8a8a;
                border: 1px solid rgba(255, 110, 110, 0.22);
                border-radius: 15px;
                padding: 10px 18px;
                font-weight: 600;
                font-size: 14px;
            }

            QPushButton#eventDangerBtn:hover {
                background: rgba(255, 86, 86, 0.12);
                border-color: rgba(255, 124, 124, 0.46);
            }
        )");
    }

    return QStringLiteral(R"(
        QDialog#eventEditorDialog {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #f7fbff,
                                        stop:0.52 #eef5ff,
                                        stop:1 #e9f1fb);
            color: #132033;
        }

        QFrame#eventHeroCard, QFrame#eventSectionCard {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 rgba(255,255,255,0.94),
                                        stop:1 rgba(245,250,255,0.92));
            border: 1px solid rgba(85, 154, 255, 0.18);
            border-radius: 20px;
        }

        QLabel#eventEyebrow {
            color: #2d7cff;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 0.18em;
            text-transform: uppercase;
        }

        QLabel#eventHeading {
            color: #10233d;
            font-size: 24px;
            font-weight: 700;
        }

        QLabel#eventSubtitle {
            color: #5f7694;
            font-size: 13px;
            line-height: 1.35em;
        }

        QLabel#eventMicroLabel {
            color: #2d8df8;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 0.12em;
            text-transform: uppercase;
        }

        QLineEdit#eventTitleField,
        QLineEdit#eventField,
        QTextEdit#eventField,
        QDateEdit#eventField,
        QTimeEdit#eventField,
        QComboBox#eventField {
            background: rgba(255,255,255,0.96);
            color: #132033;
            border: 1px solid rgba(91, 141, 218, 0.20);
            border-radius: 15px;
            padding: 10px 14px;
            selection-background-color: rgba(58, 132, 255, 0.18);
            font-size: 14px;
        }

        QLineEdit#eventTitleField {
            border-radius: 18px;
            padding: 13px 16px;
            font-size: 18px;
            font-weight: 600;
        }

        QLineEdit#eventTitleField:focus,
        QLineEdit#eventField:focus,
        QTextEdit#eventField:focus,
        QDateEdit#eventField:focus,
        QTimeEdit#eventField:focus,
        QComboBox#eventField:focus {
            border: 1px solid rgba(61, 139, 255, 0.72);
            background: rgba(255,255,255,0.99);
        }

        QTextEdit#eventField {
            padding-top: 14px;
        }

        QLineEdit#eventTitleField::placeholder,
        QLineEdit#eventField::placeholder,
        QTextEdit#eventField {
            color: #8ba1ba;
        }

        QComboBox#eventField::drop-down {
            border: none;
            width: 26px;
        }

        QComboBox#eventField::down-arrow {
            image: none;
            width: 0;
            height: 0;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 6px solid #2f7cff;
            margin-right: 10px;
        }

        QComboBox#eventField QAbstractItemView {
            background: #ffffff;
            color: #132033;
            border: 1px solid rgba(79, 144, 233, 0.30);
            selection-background-color: rgba(55, 136, 255, 0.18);
            selection-color: #10233d;
            outline: 0;
            padding: 6px;
        }

        QCheckBox#eventToggle {
            color: #203149;
            spacing: 10px;
            font-size: 14px;
            font-weight: 600;
        }

        QCheckBox#eventToggle::indicator {
            width: 22px;
            height: 22px;
            border-radius: 11px;
            border: 1px solid rgba(72, 138, 236, 0.28);
            background: rgba(255,255,255,0.95);
        }

        QCheckBox#eventToggle::indicator:checked {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #72d8ff,
                                        stop:1 #3b7eff);
            border: 1px solid rgba(163, 220, 255, 0.95);
        }

        QPushButton#eventSecondaryBtn {
            background: rgba(255,255,255,0.84);
            color: #28415f;
            border: 1px solid rgba(90, 143, 220, 0.18);
            border-radius: 15px;
            padding: 10px 18px;
            font-weight: 600;
            font-size: 14px;
        }

        QPushButton#eventSecondaryBtn:hover {
            background: rgba(255,255,255,0.98);
            border-color: rgba(72, 138, 236, 0.42);
        }

        QPushButton#eventPrimaryBtn {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #68d9ff,
                                        stop:1 #3b7eff);
            color: white;
            border: 1px solid rgba(170, 227, 255, 0.68);
            border-radius: 15px;
            padding: 11px 22px;
            font-weight: 700;
            font-size: 14px;
        }

        QPushButton#eventPrimaryBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #84e4ff,
                                        stop:1 #4a8aff);
        }

        QPushButton#eventDangerBtn {
            background: rgba(255,255,255,0.80);
            color: #e45555;
            border: 1px solid rgba(255, 126, 126, 0.18);
            border-radius: 15px;
            padding: 10px 18px;
            font-weight: 600;
            font-size: 14px;
        }

        QPushButton#eventDangerBtn:hover {
            background: rgba(255, 240, 240, 0.95);
            border-color: rgba(244, 107, 107, 0.38);
        }
    )");
}

void styleEventEditorDialog(QDialog& dlg, bool dark) {
    dlg.setObjectName("eventEditorDialog");
    dlg.setAttribute(Qt::WA_StyledBackground, true);
    dlg.setStyleSheet(eventEditorStylesheet(dark));
}

}

// ============================================================================
// Construction
// ============================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_selectedDate(QDate::currentDate())
{
    setWindowTitle("EduSync");
    setMinimumSize(1400, 900);
    resize(1700, 1000);

    if (auto* screen = QApplication::primaryScreen()) {
        const QRect g = screen->availableGeometry();
        move(g.center().x() - width()/2, g.center().y() - height()/2);
    }

    m_store     = new EventStore(this);
    m_scheduler = new SchedulerEngine(this);
    m_sync      = new SyncManager(m_store, this);

    m_aiDebounce = new QTimer(this);
    m_aiDebounce->setSingleShot(true);
    m_aiDebounce->setInterval(300);

    NotificationManager::instance().requestAuthorization();

    setupProviders();
    setupAssistant();
    buildUI();
    wireConnections();

    // Load persisted events from disk
    if (!m_store->load())
        qWarning("EduSync: failed to load events from %s", qPrintable(m_store->storagePath()));

    QSettings s("EduSync", "EduSync");
    applyTheme(s.value("theme", "dark").toString() == "light"
               ? ThemeMode::Light : ThemeMode::Dark);

    m_updatingDate = true;
    m_calendar->setSelectedDate(m_selectedDate);
    m_updatingDate = false;
    refreshAll();
    updateSyncStatus();

    // Deferred refresh after layout settles — fixes stale event geometry on startup.
    // The splitter hasn't distributed its final sizes yet during construction,
    // so the timeline canvas computes block rects with the wrong width.
    QTimer::singleShot(0, this, [this] {
        refreshAll();
        m_aiDebounce->start();
    });

    const auto providers = m_sync->providers();
    const bool hasConnectedProvider = std::any_of(providers.begin(), providers.end(), [](CalendarProvider* provider) {
        return provider && provider->isAuthenticated();
    });
    if (hasConnectedProvider)
        m_sync->startSync(300);  // every 5 min
}

// ============================================================================
// Provider setup
// ============================================================================

void MainWindow::setupProviders() {
    m_appleProvider   = new AppleNativeProvider(this);
    m_googleProvider  = new GoogleCalendarProvider(this);
    m_outlookProvider = new OutlookCalendarProvider(this);

    m_sync->addProvider(m_appleProvider);
    m_sync->addProvider(m_googleProvider);
    m_sync->addProvider(m_outlookProvider);
    reloadCalDAVProviders();
}

void MainWindow::setupAssistant() {
    m_localAssistant = new LocalAssistantService(m_scheduler, this);
    m_llmAssistant   = new LLMAssistantService(this);
}

// ============================================================================
// 3-panel layout
// ============================================================================

void MainWindow::buildUI() {
    auto* central = new QWidget;
    setCentralWidget(central);
    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setChildrenCollapsible(false);
    rootLayout->addWidget(m_splitter);

    // ── LEFT PANEL — Zoho-style sidebar ────────────────────────────────────
    auto* leftPanel = new QWidget;
    leftPanel->setObjectName("leftRail");
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    // ── BRAND ROW ────────────────────────────────────────────────────────
    {
        auto* brandRow = new QWidget;
        auto* brandLay = new QHBoxLayout(brandRow);
        brandLay->setContentsMargins(14, 18, 14, 8);
        brandLay->setSpacing(10);

        auto* brandGlyph = new QLabel("E");
        brandGlyph->setObjectName("brandGlyph");
        brandGlyph->setAlignment(Qt::AlignCenter);
        brandGlyph->setFixedSize(30, 30);
        brandLay->addWidget(brandGlyph);

        auto* brandStack = new QVBoxLayout;
        brandStack->setContentsMargins(0, 0, 0, 0);
        brandStack->setSpacing(1);

        auto* brandTitle = new QLabel("EduSync");
        brandTitle->setObjectName("brandTitle");
        brandStack->addWidget(brandTitle);

        auto* brandMeta = new QLabel(QString("SPRING \xC2\xB7 %1").arg(QDate::currentDate().year()));
        brandMeta->setObjectName("brandMeta");
        brandStack->addWidget(brandMeta);
        brandLay->addLayout(brandStack, 1);

        m_btnSync = new QPushButton;
        m_btnSync->setObjectName("navBtn");
        m_btnSync->setCursor(Qt::PointingHandCursor);
        m_btnSync->setFixedSize(28, 28);
        m_btnSync->setText(QString::fromUtf8("\xE2\x86\xBB"));
        m_btnSync->setToolTip("Sync now");
        brandLay->addWidget(m_btnSync);

        leftLayout->addWidget(brandRow);
    }

    // ── NEW EVENT button ─────────────────────────────────────────────────
    {
        auto* topRow = new QWidget;
        auto* topLay = new QHBoxLayout(topRow);
        topLay->setContentsMargins(14, 0, 14, 14);

        m_btnAdd = new QPushButton("New event");
        m_btnAdd->setObjectName("accentBtn");
        m_btnAdd->setCursor(Qt::PointingHandCursor);
        m_btnAdd->setFixedHeight(42);
        m_btnAdd->setStyleSheet(
            "QPushButton#accentBtn {"
            "  border-radius: 21px;"
            "  font-size: 13px;"
            "  font-weight: 700;"
            "  letter-spacing: 0.02em;"
            "}");
        topLay->addWidget(m_btnAdd);
        leftLayout->addWidget(topRow);
    }

    // ── MINI CALENDAR ────────────────────────────────────────────────────
    {
        auto* calCard = new QFrame;
        calCard->setObjectName("calendarCard");
        auto* calCardLay = new QVBoxLayout(calCard);
        calCardLay->setContentsMargins(12, 8, 12, 8);
        calCardLay->setSpacing(0);

        // Row 1: ← [month name large] →
        auto* monthRow = new QHBoxLayout;
        monthRow->setContentsMargins(0, 0, 0, 0);
        monthRow->setSpacing(4);

        m_prevBtn = new QPushButton("\xe2\x80\xb9"); // ‹
        m_prevBtn->setObjectName("navBtn");
        m_prevBtn->setFixedSize(26, 26);
        m_prevBtn->setCursor(Qt::PointingHandCursor);
        m_prevBtn->setToolTip("Previous month");

        m_nextBtn = new QPushButton("\xe2\x80\xba"); // ›
        m_nextBtn->setObjectName("navBtn");
        m_nextBtn->setFixedSize(26, 26);
        m_nextBtn->setCursor(Qt::PointingHandCursor);
        m_nextBtn->setToolTip("Next month");

        // Month name label
        m_monthTitle = new QLabel;
        QFont monthF = Theme::Font::title();
        monthF.setPointSizeF(22.0);
        monthF.setWeight(QFont::DemiBold);
        monthF.setLetterSpacing(QFont::AbsoluteSpacing, -0.25);
        m_monthTitle->setFont(monthF);
        m_monthTitle->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        monthRow->addWidget(m_prevBtn);
        monthRow->addWidget(m_monthTitle, 1);
        monthRow->addWidget(m_nextBtn);
        calCardLay->addLayout(monthRow);

        // Row 2: year (small, muted) + Today button
        auto* yearRow = new QHBoxLayout;
        yearRow->setContentsMargins(2, 0, 2, 6);
        yearRow->setSpacing(0);

        m_yearLabel = new QLabel;
        QFont yearF = Theme::Font::mono();
        yearF.setPointSizeF(10.0);
        yearF.setWeight(QFont::Medium);
        yearF.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        m_yearLabel->setFont(yearF);
        m_yearLabel->setStyleSheet("color: #3d5780;");
        yearRow->addWidget(m_yearLabel, 1);

        m_todayBtn = new QPushButton("Today");
        m_todayBtn->setObjectName("todayBtn");
        m_todayBtn->setFixedHeight(22);
        m_todayBtn->setCursor(Qt::PointingHandCursor);
        m_todayBtn->setToolTip("Go to today");
        yearRow->addWidget(m_todayBtn);
        calCardLay->addLayout(yearRow);

        // Calendar grid (compact)
        m_calendar = new CalendarWidget(calCard);
        m_calendar->setSelectionMode(QCalendarWidget::SingleSelection);
        m_calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
        m_calendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
        m_calendar->setFirstDayOfWeek(Qt::Monday);
        m_calendar->setGridVisible(false);
        m_calendar->setFocusPolicy(Qt::StrongFocus);
        m_calendar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        m_calendar->setMaximumHeight(210);

        if (auto* nav = m_calendar->findChild<QWidget*>("qt_calendar_navigationbar"))
            nav->hide();
        calCardLay->addWidget(m_calendar);

        leftLayout->addWidget(calCard);
    }

    // ── CALENDAR SOURCES ────────────────────────────────────────────────
    {
        auto* srcSep = new QFrame;
        srcSep->setFrameShape(QFrame::HLine);
        srcSep->setFixedHeight(1);
        srcSep->setStyleSheet("background: #0f1520; border: none;");
        leftLayout->addWidget(srcSep);

        m_calSourcesContainer = new QWidget;
        m_calSourcesContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        leftLayout->addWidget(m_calSourcesContainer);
    }

    // ── DAY EVENT LIST (below mini calendar, iOS style) ──────────────────
    {
        auto* evSep = new QFrame;
        evSep->setFrameShape(QFrame::HLine);
        evSep->setFixedHeight(1);
        evSep->setStyleSheet("background: #0f1520; border: none;");
        leftLayout->addWidget(evSep);

        auto* evSection = new QWidget;
        auto* evLay = new QVBoxLayout(evSection);
        evLay->setContentsMargins(14, 10, 14, 10);
        evLay->setSpacing(2);

        // Date + "+" button row
        auto* dateRow = new QHBoxLayout;
        dateRow->setSpacing(4);

        m_calEventDateLabel = new QLabel("SELECT A DATE");
        QFont dateF;
        dateF.setPointSizeF(10.5);
        dateF.setWeight(QFont::Bold);
        dateF.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
        m_calEventDateLabel->setFont(dateF);
        dateRow->addWidget(m_calEventDateLabel, 1);

        auto* calAddBtn = new QPushButton("+");
        calAddBtn->setObjectName("navBtn");
        calAddBtn->setFixedSize(22, 22);
        calAddBtn->setCursor(Qt::PointingHandCursor);
        connect(calAddBtn, &QPushButton::clicked, this, &MainWindow::openAddDialog);
        dateRow->addWidget(calAddBtn);
        evLay->addLayout(dateRow);

        // Relative date label: "TODAY" / "3 DAYS FROM TODAY"
        m_calEventRelLabel = new QLabel;
        QFont relF = Theme::Font::base();
        relF.setPointSizeF(9.5);
        m_calEventRelLabel->setFont(relF);
        m_calEventRelLabel->setStyleSheet("color: #3d5780;");
        evLay->addWidget(m_calEventRelLabel);

        // Scrollable event list
        m_calEventList = new QWidget;
        auto* evListLay = new QVBoxLayout(m_calEventList);
        evListLay->setContentsMargins(0, 6, 0, 0);
        evListLay->setSpacing(4);
        evLay->addWidget(m_calEventList);

        leftLayout->addWidget(evSection);
    }

    leftLayout->addStretch();

    // ── FOOTER (status + settings) ───────────────────────────────────────
    {
        auto* footerSep = new QFrame;
        footerSep->setFrameShape(QFrame::HLine);
        footerSep->setFixedHeight(1);
        footerSep->setStyleSheet("background: #0f1520; border: none;");
        leftLayout->addWidget(footerSep);

        auto* footer = new QWidget;
        auto* footerLay = new QVBoxLayout(footer);
        footerLay->setContentsMargins(14, 10, 14, 14);
        footerLay->setSpacing(6);

        m_syncStatus = new QLabel("No calendars connected");
        QFont statusF = Theme::Font::base();
        statusF.setPointSizeF(10.5);
        m_syncStatus->setFont(statusF);
        m_syncStatus->setWordWrap(true);
        m_syncStatus->setStyleSheet("color: #3d5780;");
        footerLay->addWidget(m_syncStatus);

        m_btnSettings = new QPushButton("Settings");
        m_btnSettings->setObjectName("subtleBtn");
        m_btnSettings->setCursor(Qt::PointingHandCursor);
        m_btnSettings->setFixedHeight(30);
        m_btnSettings->setToolTip("Open settings");

        footerLay->addWidget(m_btnSettings);

        leftLayout->addWidget(footer);
    }

    // ── CENTER PANEL ───────────────────────────────────────────────────────
    auto* centerPanel = new QWidget;
    centerPanel->setObjectName("centerSurface");
    auto* centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    auto* toolbarContainer = new QWidget;
    toolbarContainer->setObjectName("toolbarSurface");
    auto* toolbarOuter = new QVBoxLayout(toolbarContainer);
    toolbarOuter->setContentsMargins(0, 0, 0, 0);
    toolbarOuter->setSpacing(0);

    auto* toolbar = new QWidget;
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(Theme::Space::XL, Theme::Space::L,
                                      Theme::Space::XL, Theme::Space::L);
    toolbarLayout->setSpacing(Theme::Space::S);

    m_dayLabel = new QLabel;
    m_dayLabel->setFont(Theme::Font::title());

    // Edit / Delete act on the selected event in the timeline
    m_btnEdit = new QPushButton("Edit");
    m_btnEdit->setObjectName("ghostBtn");
    m_btnEdit->setCursor(Qt::PointingHandCursor);
    m_btnEdit->setEnabled(false);

    m_btnDelete = new QPushButton("Delete");
    m_btnDelete->setObjectName("dangerBtn");
    m_btnDelete->setCursor(Qt::PointingHandCursor);
    m_btnDelete->setEnabled(false);

    // View toggle (Day / Week)
    m_btnDay = new QPushButton("Day");
    m_btnDay->setObjectName("accentBtn");
    m_btnDay->setFixedHeight(28);
    m_btnDay->setCursor(Qt::PointingHandCursor);
    m_btnWeek = new QPushButton("Week");
    m_btnWeek->setObjectName("subtleBtn");
    m_btnWeek->setFixedHeight(28);
    m_btnWeek->setCursor(Qt::PointingHandCursor);

    toolbarLayout->addWidget(m_dayLabel, 1);
    toolbarLayout->addWidget(m_btnDay);
    toolbarLayout->addWidget(m_btnWeek);
    toolbarLayout->addSpacing(Theme::Space::L);
    toolbarLayout->addWidget(m_btnEdit);
    toolbarLayout->addWidget(m_btnDelete);

    toolbarOuter->addWidget(toolbar);

    // Toolbar bottom separator
    auto* toolSep = new QFrame;
    toolSep->setFrameShape(QFrame::HLine);
    toolSep->setFixedHeight(1);
    toolSep->setStyleSheet("background: transparent; border: none;");
    toolSep->setObjectName("toolbarSep");
    toolbarOuter->addWidget(toolSep);

    centerLayout->addWidget(toolbarContainer);

    // Timeline stack (day / week)
    m_timelineStack = new QStackedWidget(centerPanel);
    m_timeline = new DayTimelineWidget(centerPanel);
    m_weekTimeline = new WeekTimelineWidget(centerPanel);
    m_timelineStack->addWidget(m_timeline);     // index 0 = day
    m_timelineStack->addWidget(m_weekTimeline); // index 1 = week
    centerLayout->addWidget(m_timelineStack, 1);

    // Search panel overlay (child of centerPanel so it floats on top)
    m_searchPanel = new SearchPanel(centerPanel);

    // ── RIGHT PANEL ────────────────────────────────────────────────────────
    m_dashboard = new DashboardWidget;
    m_dashboard->setObjectName("rightRail");

    // ── Splitter ───────────────────────────────────────────────────────────
    m_splitter->addWidget(leftPanel);
    m_splitter->addWidget(centerPanel);
    m_splitter->addWidget(m_dashboard);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    m_splitter->setSizes({296, 900, 360});
}

// ============================================================================
// Wire connections
// ============================================================================

void MainWindow::wireConnections() {
    // Calendar navigation
    connect(m_prevBtn, &QPushButton::clicked,
            m_calendar, &QCalendarWidget::showPreviousMonth);
    connect(m_nextBtn, &QPushButton::clicked,
            m_calendar, &QCalendarWidget::showNextMonth);
    connect(m_todayBtn, &QPushButton::clicked, this, [this] {
        onDatePicked(QDate::currentDate());
        m_calendar->setCurrentPage(QDate::currentDate().year(),
                                   QDate::currentDate().month());
        m_calendar->setSelectedDate(QDate::currentDate());
    });

    connect(m_calendar, &QCalendarWidget::selectionChanged, this, [this] {
        if (!m_updatingDate)
            onDatePicked(m_calendar->selectedDate());
    });
    connect(m_calendar, &QCalendarWidget::currentPageChanged, this, [this](int, int) {
        refreshMonthTitle();
        updateCalendarStyle();
    });

    // Event store changes
    connect(m_store, &EventStore::changed, this, [this] {
        m_calendar->setEvents(m_store->all());
        m_calendar->update();
        refreshAll();
        m_aiDebounce->start();
        NotificationManager::instance().scheduleEventReminders(m_store->all());
    });

    // Timeline interactions (day view)
    connect(m_timeline, &DayTimelineWidget::eventSelected, this, [this](int idx) {
        m_selectedEventIndex = idx;
        updateButtonStates();
    });
    connect(m_timeline, &DayTimelineWidget::selectionCleared, this, [this] {
        m_selectedEventIndex = -1;
        updateButtonStates();
    });
    connect(m_timeline, &DayTimelineWidget::eventActivated, this, [this](int idx) {
        openEditDialog(idx);
    });
    connect(m_timeline, &DayTimelineWidget::emptySlotClicked, this, [this](const QTime& time) {
        openAddDialogAt(time);
    });
    connect(m_timeline, &DayTimelineWidget::eventRescheduled,
            this, [this](int idx, const QDateTime& newStart, const QDateTime& newEnd) {
        auto events = m_store->all();
        if (idx < 0 || idx >= events.size()) return;
        Event updated = events[idx];
        updated.setStartTime(newStart);
        updated.setEndTime(newEnd);
        updated.setDirty(true);
        m_store->update(idx, updated);
    });

    // Week view interactions
    connect(m_weekTimeline, &WeekTimelineWidget::eventActivated, this, [this](int idx) {
        openEditDialog(idx);
    });
    connect(m_weekTimeline, &WeekTimelineWidget::emptySlotClicked,
            this, [this](QDate date, QTime time) {
        onDatePicked(date);
        openAddDialogAt(date, time);
    });

    // View toggle
    connect(m_btnDay, &QPushButton::clicked, this, [this] {
        m_timelineStack->setCurrentIndex(0);
        m_btnDay->setObjectName("accentBtn");
        m_btnWeek->setObjectName("subtleBtn");
        // re-apply styles
        m_btnDay->setStyle(m_btnDay->style());
        m_btnWeek->setStyle(m_btnWeek->style());
    });
    connect(m_btnWeek, &QPushButton::clicked, this, [this] {
        m_timelineStack->setCurrentIndex(1);
        m_weekTimeline->setWeekContaining(m_selectedDate);
        m_weekTimeline->setEvents(m_store->all());
        m_btnWeek->setObjectName("accentBtn");
        m_btnDay->setObjectName("subtleBtn");
        m_btnDay->setStyle(m_btnDay->style());
        m_btnWeek->setStyle(m_btnWeek->style());
    });

    // Search panel (Cmd+F)
    auto* searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, this, [this] {
        m_searchPanel->setEvents(m_store->all());
        m_searchPanel->popup();
    });
    connect(m_searchPanel, &SearchPanel::dateNavigated, this, &MainWindow::onDatePicked);
    connect(m_searchPanel, &SearchPanel::eventSelected, this, [this](int idx) {
        m_selectedEventIndex = idx;
        updateButtonStates();
    });

    // CRUD
    connect(m_btnAdd, &QPushButton::clicked, this, &MainWindow::openAddDialog);
    connect(m_btnEdit, &QPushButton::clicked, this, [this] {
        if (m_selectedEventIndex >= 0) openEditDialog(m_selectedEventIndex);
    });
    connect(m_btnDelete, &QPushButton::clicked, this, [this] {
        if (m_selectedEventIndex >= 0) openDeleteDialog(m_selectedEventIndex);
    });

    // Sync button
    connect(m_btnSync, &QPushButton::clicked, this, [this] {
        m_sync->syncNow();
    });

    connect(m_btnSettings, &QPushButton::clicked, this, [this] { openSettingsDialog(2); });

    // Auto-AI
    connect(m_aiDebounce, &QTimer::timeout, this, &MainWindow::runAutoAI);

    // Dashboard apply
    connect(m_dashboard, &DashboardWidget::applyAISuggestions, this, [this] {
        if (!m_selectedDate.isValid()) return;
        auto result = m_scheduler->scheduleTasks(
            m_selectedDate, m_store->all(), {}, {});
        if (!result.plannedEvents.isEmpty())
            m_store->addBatch(result.plannedEvents);
    });

    // Dashboard chat composer
    connect(m_dashboard, &DashboardWidget::userChatMessage,
            this, &MainWindow::onChatMessage);

    // Sync status updates
    connect(m_sync, &SyncManager::syncStarted, this, [this] {
        m_syncStatus->setText("Syncing...");
    });
    connect(m_sync, &SyncManager::syncComplete, this, [this](int added, int updated) {
        updateSyncStatus();
        if (added > 0 || updated > 0) {
            m_calendar->setEvents(m_store->all());
            refreshAll();
        }
    });
    connect(m_sync, &SyncManager::syncError, this, [this](const QString& err) {
        m_syncStatus->setText("Sync error");
        Q_UNUSED(err)
    });
    connect(m_sync, &SyncManager::statusMessage, this, [this](const QString& msg) {
        m_syncStatus->setText(msg);
    });

    // Provider auth results
    connect(m_googleProvider, &CalendarProvider::authenticationComplete,
            this, [this](bool ok, const QString& err) {
        if (ok) {
            m_syncStatus->setText("Google connected. Syncing now...");
            m_sync->syncNow();
        } else if (!err.isEmpty()) {
            m_syncStatus->setText(err);
        }
        updateSyncStatus();
    });
    connect(m_appleProvider, &CalendarProvider::authenticationComplete,
            this, [this](bool ok, const QString& err) {
        if (ok) {
            m_syncStatus->setText("Apple Calendar and Reminders connected. Syncing now...");
            m_sync->syncNow();
        } else if (!err.isEmpty()) {
            m_syncStatus->setText(err);
        }
        updateSyncStatus();
    });
    connect(m_outlookProvider, &CalendarProvider::authenticationComplete,
            this, [this](bool ok, const QString& err) {
        if (ok) {
            m_syncStatus->setText("Outlook connected. Syncing now...");
            m_sync->syncNow();
        } else if (!err.isEmpty()) {
            m_syncStatus->setText(err);
        }
        updateSyncStatus();
    });

    // Live state updates from providers
    connect(m_appleProvider, &CalendarProvider::connectionStateChanged,
            this, [this] { updateSyncStatus(); });
    connect(m_googleProvider, &CalendarProvider::connectionStateChanged,
            this, [this] { updateSyncStatus(); });
    connect(m_outlookProvider, &CalendarProvider::connectionStateChanged,
            this, [this] { updateSyncStatus(); });
}

void MainWindow::reloadCalDAVProviders() {
    m_sync->removeProvider(CalendarProvider::CalDAV);
    qDeleteAll(m_caldavProviders);
    m_caldavProviders.clear();

    const QList<CalDAVProvider::AccountConfig> accounts = CalDAVProvider::loadAccounts();
    for (const auto& account : accounts) {
        auto* provider = new CalDAVProvider(account, this);
        m_caldavProviders.push_back(provider);
        m_sync->addProvider(provider);

        connect(provider, &CalendarProvider::authenticationComplete, this,
                [this, provider](bool ok, const QString& err) {
            if (ok) {
                m_syncStatus->setText(QString("%1 connected. Syncing now...").arg(provider->accountDisplayName()));
                m_sync->syncNow();
            } else if (!err.isEmpty()) {
                m_syncStatus->setText(err);
            }
            updateSyncStatus();
        });
        connect(provider, &CalendarProvider::connectionStateChanged, this,
                [this](CalendarProvider::ConnectionState) { updateSyncStatus(); });
    }
}

// ============================================================================
// Calendar source list (left sidebar)
// ============================================================================

void MainWindow::rebuildCalendarSources() {
    if (!m_calSourcesContainer) return;

    const bool isDark = (m_theme == ThemeMode::Dark);
    const auto c = isDark ? Theme::dark() : Theme::light();

    // Text colors adapted to theme
    const QString sectionColor = isDark ? "#2d4060" : c.textTertiary.name();
    const QString nameOnColor  = isDark ? "#c8d4e8" : c.text.name();
    const QString nameOffColor = isDark ? "#354560" : c.textSecondary.name();
    const QString hoverRgba    = isDark ? "rgba(79,140,255,0.08)"
                                        : "rgba(0,0,0,0.04)";
    const QString pressRgba    = isDark ? "rgba(79,140,255,0.14)"
                                        : "rgba(0,0,0,0.08)";
    const QString indOffStyle  = isDark
        ? "background:transparent; border:1px solid #2d4060; border-radius:3px;"
        : "background:transparent; border:1px solid " + c.border.name() + "; border-radius:3px;";

    // Clear existing children
    delete m_calSourcesContainer->layout();
    for (QObject* child : m_calSourcesContainer->children()) {
        if (auto* w = qobject_cast<QWidget*>(child))
            w->deleteLater();
    }

    auto* lay = new QVBoxLayout(m_calSourcesContainer);
    lay->setContentsMargins(0, 4, 0, 4);
    lay->setSpacing(0);

    // ── Section header helper ───────────────────────────────────────────
    auto addSection = [&](const QString& text) {
        auto* lbl = new QLabel(text);
        QFont f = Theme::Font::caption();
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
        f.setWeight(QFont::Bold);
        lbl->setFont(f);
        lbl->setContentsMargins(18, 10, 18, 4);
        lbl->setStyleSheet("color: " + sectionColor + ";");
        lay->addWidget(lbl);
    };

    // ── Source row helper ────────────────────────────────────────────────
    auto addRow = [&](const QString& key, const QString& name,
                      const QColor& color, bool connected) {
        const bool visible = m_sourceVisible.value(key, true);

        auto* btn = new QPushButton;
        btn->setCheckable(true);
        btn->setChecked(visible);
        btn->setFixedHeight(38);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFlat(true);
        btn->setStyleSheet(QString(
            "QPushButton { background:transparent; border:none; text-align:left; padding:0; }"
            "QPushButton:hover { background:%1; }"
            "QPushButton:pressed { background:%2; }").arg(hoverRgba, pressRgba));

        auto* rl = new QHBoxLayout(btn);
        rl->setContentsMargins(18, 0, 14, 0);
        rl->setSpacing(12);

        // Left: colored circle dot
        auto* dot = new QFrame;
        dot->setFixedSize(12, 12);
        dot->setAttribute(Qt::WA_TransparentForMouseEvents);
        QColor dc = color; dc.setAlpha(visible ? 255 : 80);
        dot->setStyleSheet(QString(
            "background:%1; border-radius:6px;").arg(dc.name(QColor::HexArgb)));
        rl->addWidget(dot);

        // Name label
        auto* nameLbl = new QLabel(connected ? name : name + " ·");
        QFont nf = Theme::Font::base();
        nf.setPointSizeF(12.0);
        nameLbl->setFont(nf);
        nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        nameLbl->setStyleSheet("color: " + (visible ? nameOnColor : nameOffColor) + ";");
        rl->addWidget(nameLbl, 1);

        // Right: solid colored square indicator (filled = visible, outline = hidden)
        auto* indicator = new QFrame;
        indicator->setFixedSize(12, 12);
        indicator->setAttribute(Qt::WA_TransparentForMouseEvents);
        indicator->setStyleSheet(visible
            ? QString("background:%1; border-radius:3px;").arg(color.name())
            : indOffStyle);
        rl->addWidget(indicator);

        connect(btn, &QPushButton::toggled, this,
                [this, key, dot, nameLbl, indicator, color,
                 nameOnColor, nameOffColor, indOffStyle](bool checked) {
            m_sourceVisible[key] = checked;
            QColor dc = color; dc.setAlpha(checked ? 255 : 80);
            dot->setStyleSheet(QString(
                "background:%1; border-radius:6px;").arg(dc.name(QColor::HexArgb)));
            nameLbl->setStyleSheet("color: " + (checked ? nameOnColor : nameOffColor) + ";");
            indicator->setStyleSheet(checked
                ? QString("background:%1; border-radius:3px;").arg(color.name())
                : indOffStyle);
            refreshAll();
        });

        lay->addWidget(btn);
    };

    // ── MY CALENDARS ─────────────────────────────────────────────────────
    addSection("MY CALENDARS");
    addRow("local",   "EduSync",          QColor("#6366f1"), true);
    addRow("apple",   "Apple Calendar",   QColor("#fc3d39"),
           m_appleProvider && m_appleProvider->isAuthenticated());
    addRow("google",  "Google Calendar",  QColor("#4285f4"),
           m_googleProvider && m_googleProvider->isAuthenticated());
    addRow("outlook", "Outlook Calendar", QColor("#0078d4"),
           m_outlookProvider && m_outlookProvider->isAuthenticated());

    // ── APP CALENDARS (CalDAV) ────────────────────────────────────────────
    if (!m_caldavProviders.isEmpty()) {
        lay->addSpacing(4);
        addSection("OTHER CALENDARS");
        const QList<QColor> caldavPalette = {
            QColor("#22c55e"), QColor("#f59e0b"), QColor("#ec4899"),
            QColor("#14b8a6"), QColor("#a855f7")
        };
        int ci = 0;
        for (auto* prov : m_caldavProviders) {
            const QString key = "caldav_" + prov->accountDisplayName();
            addRow(key, prov->accountDisplayName(),
                   caldavPalette[ci % caldavPalette.size()],
                   prov->isAuthenticated());
            ++ci;
        }
    }
}

// ============================================================================
// Sync status
// ============================================================================

void MainWindow::updateSyncStatus() {
    int connectedCount = 0;
    int syncingCount   = 0;
    int errorCount     = 0;

    const QVector<CalendarProvider*> providers = m_sync->providers();
    for (auto* p : providers) {
        if (p->isAuthenticated())                                 ++connectedCount;
        if (p->connectionState() == CalendarProvider::Syncing ||
            p->connectionState() == CalendarProvider::Connecting) ++syncingCount;
        if (p->connectionState() == CalendarProvider::Error)      ++errorCount;
    }

    // Footer status label
    if (connectedCount == 0) {
        m_syncStatus->setText("No calendars linked. Connect one in Settings.");
        m_btnSync->setEnabled(false);
    } else if (errorCount > 0) {
        m_syncStatus->setText(QString("%1 source%2 need attention")
                              .arg(errorCount).arg(errorCount == 1 ? "" : "s"));
        m_btnSync->setEnabled(true);
    } else if (syncingCount > 0) {
        m_syncStatus->setText("Syncing your calendars…");
        m_btnSync->setEnabled(false);
    } else {
        const auto lastSync = m_sync->lastSyncTime();
        m_syncStatus->setText(lastSync.isValid()
            ? "Last sync at " + lastSync.toString("h:mm ap")
            : QString("%1 calendar%2 linked")
                  .arg(connectedCount).arg(connectedCount == 1 ? "" : "s"));
        m_btnSync->setEnabled(true);
    }

    // Refresh the source list (connection state may have changed)
    rebuildCalendarSources();
}

// ============================================================================
// Settings window
// ============================================================================

void MainWindow::openSettingsDialog(int initialPage) {
    SettingsDialog dlg(m_googleProvider,
                       m_outlookProvider,
                       m_appleProvider,
                       &m_caldavProviders,
                       m_llmAssistant,
                       m_theme == ThemeMode::Dark,
                       static_cast<SettingsDialog::Page>(initialPage),
                       this);

    connect(&dlg, &SettingsDialog::themeChanged, this, [this](bool isDark) {
        applyTheme(isDark ? ThemeMode::Dark : ThemeMode::Light);
        QSettings("EduSync", "EduSync").setValue("theme", isDark ? "dark" : "light");
    });
    connect(&dlg, &SettingsDialog::accountsChanged, this, [this] {
        reloadCalDAVProviders();
        updateSyncStatus();
    });

    dlg.exec();
    updateSyncStatus();

    // Refresh AI availability — user may have just added an API key
    if (m_dashboard) {
        const bool chatOn = m_llmAssistant->isAvailable();
        m_dashboard->setChatEnabled(true,
            chatOn ? QString("Ask the assistant…")
                   : QString("Add an API key in Settings to enable AI chat"));
    }
}

// ============================================================================
// Date / refresh / AI
// ============================================================================

void MainWindow::onDatePicked(const QDate& d) {
    if (m_updatingDate || !d.isValid()) return;
    m_updatingDate = true;

    m_selectedDate = d;
    if (m_calendar && m_calendar->selectedDate() != d)
        m_calendar->setSelectedDate(d);
    m_selectedEventIndex = -1;
    refreshAll();
    m_aiDebounce->start();

    m_updatingDate = false;
}

void MainWindow::refreshAll() {
    if (!m_selectedDate.isValid()) return;

    if (m_selectedDate == QDate::currentDate())
        m_dayLabel->setText("Today, " + m_selectedDate.toString("MMMM d"));
    else
        m_dayLabel->setText(m_selectedDate.toString("dddd, MMMM d"));

    const auto& allEvents = m_store->all();

    // Filter by source visibility (sidebar toggles)
    QList<Event> visibleEvents;
    visibleEvents.reserve(allEvents.size());
    for (const Event& e : allEvents) {
        const bool show = [&]() -> bool {
            switch (e.source()) {
            case Event::Local:       return m_sourceVisible.value("local",   true);
            case Event::Google:      return m_sourceVisible.value("google",  true);
            case Event::Outlook:     return m_sourceVisible.value("outlook", true);
            case Event::AppleNative: return m_sourceVisible.value("apple",   true);
            case Event::CalDAV: {
                const QString key = "caldav_" + e.providerKey();
                return m_sourceVisible.value(key, true);
            }
            }
            return true;
        }();
        if (show) visibleEvents.append(e);
    }

    m_timeline->setEvents(visibleEvents);
    m_timeline->setDate(m_selectedDate);
    m_timeline->clearSelection();

    if (m_timelineStack->currentIndex() == 1) {
        m_weekTimeline->setWeekContaining(m_selectedDate);
        m_weekTimeline->setEvents(visibleEvents);
    }

    m_dashboard->refresh(m_selectedDate, visibleEvents);

    refreshMonthTitle();

    // ── Sidebar day event list (below mini calendar) ─────────────────────
    if (m_calEventDateLabel && m_calEventList) {
        const QDate today = QDate::currentDate();

        // Date label: "THURSDAY APR 16"
        m_calEventDateLabel->setText(
            m_selectedDate.toString("dddd MMM d").toUpper());

        // Relative label
        if (m_calEventRelLabel) {
            const int diff = today.daysTo(m_selectedDate);
            QString rel;
            if      (diff == 0)  rel = "TODAY";
            else if (diff == 1)  rel = "TOMORROW";
            else if (diff == -1) rel = "YESTERDAY";
            else if (diff >  0)  rel = QString("%1 DAYS FROM NOW").arg(diff);
            else                 rel = QString("%1 DAYS AGO").arg(-diff);
            m_calEventRelLabel->setText(rel);
        }

        // Clear and rebuild event rows
        auto* listLay = qobject_cast<QVBoxLayout*>(m_calEventList->layout());
        if (listLay) {
            while (auto* item = listLay->takeAt(0)) {
                delete item->widget();
                delete item;
            }
        } else {
            listLay = new QVBoxLayout(m_calEventList);
            listLay->setContentsMargins(0, 0, 0, 0);
            listLay->setSpacing(4);
        }

        int count = 0;
        for (const auto& ev : m_store->all()) {
            if (!ev.isOnDate(m_selectedDate)) continue;
            if (++count > 5) break;

            auto* row = new QWidget;
            auto* rl  = new QHBoxLayout(row);
            rl->setContentsMargins(0, 2, 0, 2);
            rl->setSpacing(8);

            // Colored left bar (like iOS screenshot)
            auto* bar = new QFrame;
            bar->setFixedSize(3, 34);
            const QColor col = ev.getColor().isValid()
                ? ev.getColor() : QColor("#6366f1");
            bar->setStyleSheet(
                QString("background:%1; border-radius:2px;").arg(col.name()));
            rl->addWidget(bar);

            // Title + time stacked
            auto* textCol = new QWidget;
            auto* tl      = new QVBoxLayout(textCol);
            tl->setContentsMargins(0, 0, 0, 0);
            tl->setSpacing(0);

            auto* titleLbl = new QLabel(ev.getTitle());
            QFont tf = Theme::Font::base();
            tf.setPointSizeF(11.0);
            tf.setWeight(QFont::DemiBold);
            titleLbl->setFont(tf);
            titleLbl->setWordWrap(false);
            tl->addWidget(titleLbl);

            auto* timeLbl = new QLabel(
                ev.getStartTime().time().toString("h:mm ap"));
            QFont sf = Theme::Font::base();
            sf.setPointSizeF(9.5);
            timeLbl->setFont(sf);
            timeLbl->setStyleSheet("color: #3d5780;");
            tl->addWidget(timeLbl);

            rl->addWidget(textCol, 1);
            listLay->addWidget(row);
        }

        if (count == 0) {
            auto* emptyLbl = new QLabel("No events");
            QFont ef = Theme::Font::base();
            ef.setPointSizeF(10.5);
            emptyLbl->setFont(ef);
            emptyLbl->setStyleSheet("color: #3d5780;");
            listLay->addWidget(emptyLbl);
        }
    }

    m_selectedEventIndex = -1;
    updateButtonStates();
}

void MainWindow::runAutoAI() {
    if (!m_selectedDate.isValid()) return;

    // 1. Always run deterministic engine for metrics (arc gauges)
    auto analysis = m_scheduler->analyzeDay(m_selectedDate, m_store->all());
    m_dashboard->refreshFromAnalysis(analysis.summary, analysis.suggestions,
                                      analysis.workload, analysis.balance,
                                      analysis.freeMinutes, analysis.freePercent);

    // 2. Run the local planner (smarter — produces deep-work + habit proposals)
    const QDate capturedDate = m_selectedDate;
    m_localAssistant->analyze(capturedDate, m_store->all(),
        [this, capturedDate](const AssistantResponse& resp) {
            if (m_selectedDate != capturedDate) return;
            m_dashboard->refreshFromAssistant(resp.summary, resp.suggestions);
        });

    // 3. If LLM is configured, upgrade the summary + suggestions async
    if (m_llmAssistant->isAvailable()) {
        m_llmAssistant->analyze(capturedDate, m_store->all(),
            [this, capturedDate](const AssistantResponse& resp) {
                if (m_selectedDate != capturedDate || !resp.fromLLM) return;
                m_dashboard->refreshFromAssistant(resp.summary, resp.suggestions);
            });
    }

    // Update chat placeholder (composer stays enabled — if no key, we reply
    // inline telling the user to add one)
    if (m_dashboard) {
        const bool chatOn = m_llmAssistant->isAvailable();
        m_dashboard->setChatEnabled(true,
            chatOn ? QString("Ask the assistant…")
                   : QString("Add an API key in Settings to enable AI chat"));
    }
}

// ============================================================================
// Chat handler — natural-language scheduling
// ============================================================================

void MainWindow::onChatMessage(const QString& text) {
    if (text.trimmed().isEmpty()) return;

    if (!m_llmAssistant->isAvailable()) {
        m_dashboard->appendChatAssistant(
            "Chat needs an AI provider. Open Settings and add an API key.");
        return;
    }

    const QDate today = m_selectedDate.isValid() ? m_selectedDate : QDate::currentDate();

    const int pendingId = m_dashboard->appendChatPending();
    m_llmAssistant->chat(text, today, m_store->all(),
        [this, pendingId](const ChatReply& r) {
            m_dashboard->resolveChatPending(pendingId,
                r.reply.isEmpty() ? QStringLiteral("Done.") : r.reply);

            // Apply proposed actions
            int added = 0;
            QVector<Event> batch;
            for (const auto& b : r.actions) {
                if (!b.date.isValid() || !b.time.isValid()) continue;
                QDateTime start(b.date, b.time);
                QDateTime end = start.addSecs(qMax(5, b.durationMin) * 60);

                QString descPrefix = b.category.isEmpty() ? "Other" : b.category;
                QString desc = descPrefix + " :: " + b.reason;

                QColor color;
                switch (b.kind) {
                case AssistantResponse::ProposedBlock::FocusBlock: color = QColor("#4f8cff"); break;
                case AssistantResponse::ProposedBlock::HabitBlock: color = QColor("#22c55e"); break;
                case AssistantResponse::ProposedBlock::BreakBlock: color = QColor("#eab308"); break;
                case AssistantResponse::ProposedBlock::BufferBlock: color = QColor("#94a3b8"); break;
                default: color = QColor("#a78bfa"); break;
                }

                Event ev(b.title, desc, start, end, color);
                ev.setSource(Event::Local);
                batch.append(ev);
                ++added;
            }

            if (!batch.isEmpty()) {
                m_store->addBatch(batch);
                m_calendar->setEvents(m_store->all());
                refreshAll();
                m_dashboard->appendChatAssistant(
                    QString("Added %1 event%2 to your calendar.")
                        .arg(added).arg(added == 1 ? "" : "s"));
            }
        });
}

void MainWindow::updateButtonStates() {
    const bool hasSelection = (m_selectedEventIndex >= 0
                               && m_selectedEventIndex < m_store->all().size());
    m_btnEdit->setEnabled(hasSelection);
    m_btnDelete->setEnabled(hasSelection);
}

// ============================================================================
// Theme
// ============================================================================

void MainWindow::applyTheme(ThemeMode mode) {
    m_theme = mode;
    const bool isDark = (mode == ThemeMode::Dark);
    Theme::apply(isDark);

    const auto c = isDark ? Theme::dark() : Theme::light();

    updateCalendarStyle();
    refreshMonthTitle();

    // Month title: accent color in dark, primary text in light
    if (m_monthTitle) {
        m_monthTitle->setStyleSheet(isDark
            ? "color: #4f8cff;"
            : "color: " + c.text.name() + ";");
    }
    if (m_yearLabel) {
        m_yearLabel->setStyleSheet(isDark
            ? "color: #3d5780;"
            : "color: " + c.textTertiary.name() + ";");
    }

    // Sidebar separators
    const QString sidebarSepStyle = isDark
        ? "background: #0f1520; border: none;"
        : "background: " + c.border.name() + "; border: none;";
    for (auto* f : findChildren<QFrame*>()) {
        if (f->frameShape() == QFrame::HLine && f->objectName().isEmpty())
            f->setStyleSheet(sidebarSepStyle);
    }

    // Update separator colors
    const QString sepStyle = "background: " + c.borderSubtle.name() + "; border: none;";
    if (auto* toolSep = findChild<QFrame*>("toolbarSep"))
        toolSep->setStyleSheet(sepStyle);

    // Sync status label
    if (m_syncStatus) {
        m_syncStatus->setStyleSheet(isDark
            ? "color: #3d5780;"
            : "color: " + c.textTertiary.name() + ";");
    }
    if (m_calEventRelLabel) {
        m_calEventRelLabel->setStyleSheet(isDark
            ? "color: #3d5780;"
            : "color: " + c.textTertiary.name() + ";");
    }

    // Rebuild source list with correct theme colors
    rebuildCalendarSources();

    // Update sync status text color (palette fallback)
    if (m_syncStatus) {
        auto pal = m_syncStatus->palette();
        pal.setColor(QPalette::WindowText, c.textTertiary);
        m_syncStatus->setPalette(pal);
    }
}

void MainWindow::updateCalendarStyle() {
    if (!m_calendar) return;
    const bool light = (m_theme == ThemeMode::Light);
    const auto c = light ? Theme::light() : Theme::dark();
    m_calendar->applyTheme(light);
    // The calendarCard QFrame provides the border/background; the inner table is transparent.
    m_calendar->setStyleSheet(QString(
        "QWidget#qt_calendar_navigationbar {"
        "  height:0; min-height:0; max-height:0; padding:0; margin:0; border:0; background:transparent;"
        "}"
        "QCalendarWidget QTableView {"
        "  background: transparent; border: none; gridline-color: transparent; outline: 0;"
        "  selection-background-color: transparent; selection-color: %1;"
        "}"
        "QCalendarWidget QAbstractItemView::item {"
        "  margin: 0; padding: 0;"
        "}"
        "QCalendarWidget QAbstractItemView::item:selected {"
        "  background: transparent; border: 0; color: %1;"
        "}")
        .arg(c.text.name()));
}

void MainWindow::refreshMonthTitle() {
    if (!m_calendar || !m_monthTitle) return;
    const int y = m_calendar->yearShown();
    const int mo = m_calendar->monthShown();
    // Month name ALL-CAPS, colored accent when dark
    const QString name = QLocale().standaloneMonthName(mo, QLocale::LongFormat);
    m_monthTitle->setText(name);
    if (m_yearLabel)
        m_yearLabel->setText(QString::number(y));
}

// ============================================================================
// Event dialogs (unchanged)
// ============================================================================

void MainWindow::openAddDialog() { openAddDialogAt(QTime(9, 0)); }

void MainWindow::openAddDialogAt(const QDate& date, const QTime& time) {
    if (!date.isValid()) return;
    const QDate saved = m_selectedDate;
    m_selectedDate = date;
    openAddDialogAt(time);
    m_selectedDate = saved;
}

void MainWindow::openAddDialogAt(const QTime& defaultTime) {
    if (!m_selectedDate.isValid()) return;

    QDialog dlg(this);
    dlg.setWindowTitle("New Event");
    dlg.setModal(true);
    dlg.setMinimumWidth(640);
    styleEventEditorDialog(dlg, m_theme == ThemeMode::Dark);

    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(Theme::Space::L, Theme::Space::L,
                             Theme::Space::L, Theme::Space::L);
    root->setSpacing(Theme::Space::L);

    auto* heroCard = new QFrame;
    heroCard->setObjectName("eventHeroCard");
    auto* heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(Theme::Space::L, Theme::Space::L,
                                   Theme::Space::L, Theme::Space::L);
    heroLayout->setSpacing(Theme::Space::M);

    auto* header = new QVBoxLayout;
    header->setSpacing(Theme::Space::XS);
    auto* eyebrow = new QLabel("EVENT");
    eyebrow->setObjectName("eventEyebrow");
    auto* heading = new QLabel("Add event");
    heading->setObjectName("eventHeading");
    auto* subtitle = new QLabel(QString("Set the details for %1 and drop it into your day.")
                                .arg(m_selectedDate.toString("dddd, MMMM d")));
    subtitle->setObjectName("eventSubtitle");
    subtitle->setWordWrap(true);
    header->addWidget(eyebrow);
    header->addWidget(heading);
    header->addWidget(subtitle);
    heroLayout->addLayout(header);

    auto* titleEdit = new QLineEdit;
    titleEdit->setPlaceholderText("Event title");
    titleEdit->setMinimumHeight(42);
    titleEdit->setObjectName("eventTitleField");
    heroLayout->addWidget(titleEdit);
    root->addWidget(heroCard);

    auto* metaCard = new QFrame;
    metaCard->setObjectName("eventSectionCard");
    auto* metaLayout = new QVBoxLayout(metaCard);
    metaLayout->setContentsMargins(Theme::Space::M, Theme::Space::M,
                                   Theme::Space::M, Theme::Space::M);
    metaLayout->setSpacing(Theme::Space::M);

    auto* row1 = new QHBoxLayout;
    row1->setSpacing(Theme::Space::M);
    auto* category = new QComboBox;
    category->addItems({"Study", "Work", "Break", "Exercise", "Personal"});
    category->setMinimumHeight(36);
    category->setObjectName("eventField");
    auto* allDay = new QCheckBox("All day");
    allDay->setCursor(Qt::PointingHandCursor);
    allDay->setObjectName("eventToggle");
    row1->addWidget(category);
    row1->addStretch();
    row1->addWidget(allDay);
    metaLayout->addLayout(row1);

    // Save destination: EduSync only, or push to a connected calendar.
    auto* destRow = new QHBoxLayout;
    destRow->setSpacing(Theme::Space::M);
    auto* saveToLabel = new QLabel("Save to");
    saveToLabel->setObjectName("eventMicroLabel");
    destRow->addWidget(saveToLabel);
    auto* destination = new QComboBox;
    destination->setMinimumHeight(36);
    destination->setObjectName("eventField");
    destination->addItem("EduSync (local only)", QVariant(QString("local")));
    if (m_appleProvider && m_appleProvider->isAuthenticated()) {
        if (m_appleProvider->hasCalendarAccess())
            destination->addItem("Apple Calendar", QVariant(QString("apple/event")));
        if (m_appleProvider->hasReminderAccess())
            destination->addItem("Apple Reminders", QVariant(QString("apple/reminder")));
    }
    destRow->addWidget(destination, 1);
    metaLayout->addLayout(destRow);

    auto* row2 = new QHBoxLayout;
    row2->setSpacing(Theme::Space::M);
    auto* dateLabel = new QLabel("Schedule");
    dateLabel->setObjectName("eventMicroLabel");
    metaLayout->addWidget(dateLabel);
    auto* dateEdit  = new QDateEdit;
    dateEdit->setCalendarPopup(true);
    dateEdit->setDate(m_selectedDate);
    dateEdit->setMinimumHeight(36);
    dateEdit->setObjectName("eventField");
    auto* startTime = new QTimeEdit;
    startTime->setDisplayFormat("hh:mm");
    startTime->setTime(defaultTime);
    startTime->setMinimumHeight(36);
    startTime->setObjectName("eventField");
    auto* endTime = new QTimeEdit;
    endTime->setDisplayFormat("hh:mm");
    endTime->setTime(defaultTime.addSecs(60 * 60));
    endTime->setMinimumHeight(36);
    endTime->setObjectName("eventField");
    auto* recur = new QComboBox;
    recur->addItems({"Does not repeat", "Every day", "Every week", "Every month"});
    recur->setMinimumHeight(36);
    recur->setObjectName("eventField");
    auto* toLabel = new QLabel("to");
    toLabel->setObjectName("eventMicroLabel");

    row2->addWidget(dateEdit);
    row2->addWidget(startTime);
    row2->addWidget(toLabel);
    row2->addWidget(endTime);
    row2->addStretch();
    row2->addWidget(recur);
    metaLayout->addLayout(row2);
    root->addWidget(metaCard);

    connect(allDay, &QCheckBox::toggled, this, [=](bool on) {
        startTime->setEnabled(!on);
        endTime->setEnabled(!on);
        if (on) { startTime->setTime(QTime(0,0)); endTime->setTime(QTime(23,59)); }
        else {
            if (startTime->time() == QTime(0,0))  startTime->setTime(QTime(9,0));
            if (endTime->time() == QTime(23,59))   endTime->setTime(QTime(10,0));
        }
    });

    auto* notesCard = new QFrame;
    notesCard->setObjectName("eventSectionCard");
    auto* notesLayout = new QVBoxLayout(notesCard);
    notesLayout->setContentsMargins(Theme::Space::M, Theme::Space::M,
                                    Theme::Space::M, Theme::Space::M);
    notesLayout->setSpacing(Theme::Space::S);
    auto* notesLabel = new QLabel("Notes");
    notesLabel->setObjectName("eventMicroLabel");
    notesLayout->addWidget(notesLabel);
    auto* desc = new QTextEdit;
    desc->setPlaceholderText("Notes...");
    desc->setMinimumHeight(150);
    desc->setMaximumHeight(180);
    desc->setObjectName("eventField");
    notesLayout->addWidget(desc);
    root->addWidget(notesCard);

    auto* btnRow0 = new QHBoxLayout;
    auto* cancelBtn0 = new QPushButton("Cancel");
    cancelBtn0->setObjectName("eventSecondaryBtn");
    cancelBtn0->setCursor(Qt::PointingHandCursor);
    auto* saveBtn0 = new QPushButton("Save Event");
    saveBtn0->setObjectName("eventPrimaryBtn");
    saveBtn0->setCursor(Qt::PointingHandCursor);
    btnRow0->addStretch();
    btnRow0->addWidget(cancelBtn0);
    btnRow0->addWidget(saveBtn0);
    root->addLayout(btnRow0);

    connect(cancelBtn0, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn0, &QPushButton::clicked, &dlg, [&] {
        const QString t = titleEdit->text().trimmed();
        if (t.isEmpty()) { titleEdit->setFocus(); return; }

        QDate d = dateEdit->date();
        QTime s = startTime->time();
        QTime e = endTime->time();
        if (!allDay->isChecked() && s >= e) e = s.addSecs(30*60);

        const QString cat = category->currentText();
        const QString notes = desc->toPlainText().trimmed();
        const QColor col = Theme::Category::color(cat);
        const QString packed = notes.isEmpty() ? cat : (cat + "::" + notes);

        const QString dest = destination->currentData().toString();
        const bool toApple = dest.startsWith("apple/");
        const bool asReminder = (dest == "apple/reminder");

        QVector<Event> batch;
        auto mkEvent = [&](const QDateTime& st, const QDateTime& en) {
            Event ev(t, packed, st, en, col);
            if (toApple) {
                ev.setSource(Event::AppleNative);
                ev.setProviderKey("apple/native");
                // Sentinel — AppleNativeProvider::createEvent reads this to
                // decide between EKEvent and EKReminder.
                ev.setExternalId(asReminder ? "reminder:" : "event:");
                ev.setLastModified(QDateTime::currentDateTimeUtc());
                ev.setDirty(true);
            }
            batch.append(ev);
        };

        const QDateTime s0(d, s), e0(d, e);
        switch (recur->currentIndex()) {
        case 0: mkEvent(s0, e0); break;
        case 1: for (int i = 0; i < 365; ++i) mkEvent(s0.addDays(i), e0.addDays(i)); break;
        case 2: for (int w = 0; w < 52; ++w) mkEvent(s0.addDays(7*w), e0.addDays(7*w)); break;
        case 3:
            for (int m = 0; m < 12; ++m) {
                QDate ds = d.addMonths(m);
                if (ds.isValid()) mkEvent(QDateTime(ds, s), QDateTime(ds, e));
            }
            break;
        }

        const int indexBefore = m_store->all().size();
        m_store->addBatch(batch);

        // If destined for Apple, push each new event immediately so it shows
        // up in Calendar.app / Reminders.app without waiting for the next sync.
        if (toApple) {
            const int indexAfter = m_store->all().size();
            for (int i = indexBefore; i < indexAfter; ++i)
                m_sync->pushEvent(i);
        }

        dlg.accept();
    });

    dlg.exec();
}

void MainWindow::openEditDialog(int idx) {
    if (idx < 0 || idx >= m_store->all().size()) return;

    const Event original = m_store->all()[idx];
    QDialog dlg(this);
    dlg.setWindowTitle("Edit Event");
    dlg.setModal(true);
    dlg.setMinimumWidth(640);
    styleEventEditorDialog(dlg, m_theme == ThemeMode::Dark);

    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(Theme::Space::L, Theme::Space::L,
                             Theme::Space::L, Theme::Space::L);
    root->setSpacing(Theme::Space::L);

    auto* heroCard = new QFrame;
    heroCard->setObjectName("eventHeroCard");
    auto* heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(Theme::Space::L, Theme::Space::L,
                                   Theme::Space::L, Theme::Space::L);
    heroLayout->setSpacing(Theme::Space::M);

    auto* header = new QVBoxLayout;
    header->setSpacing(Theme::Space::XS);
    auto* eyebrow = new QLabel("EVENT");
    eyebrow->setObjectName("eventEyebrow");
    auto* heading = new QLabel("Edit event");
    heading->setObjectName("eventHeading");
    auto* subtitle = new QLabel(QString("Refine the details for %1.")
                                .arg(original.getStartTime().date().toString("dddd, MMMM d")));
    subtitle->setObjectName("eventSubtitle");
    subtitle->setWordWrap(true);
    header->addWidget(eyebrow);
    header->addWidget(heading);
    header->addWidget(subtitle);
    heroLayout->addLayout(header);

    auto* titleEdit = new QLineEdit;
    titleEdit->setText(original.getTitle());
    titleEdit->setMinimumHeight(42);
    titleEdit->setObjectName("eventTitleField");
    heroLayout->addWidget(titleEdit);
    root->addWidget(heroCard);

    const QString fullDesc = original.getDescription();
    const int sep = fullDesc.indexOf("::");
    const QString curCat = sep >= 0 ? fullDesc.left(sep) : fullDesc;
    const QString curNotes = sep >= 0 ? fullDesc.mid(sep + 2) : QString();

    auto* metaCard = new QFrame;
    metaCard->setObjectName("eventSectionCard");
    auto* metaLayout = new QVBoxLayout(metaCard);
    metaLayout->setContentsMargins(Theme::Space::M, Theme::Space::M,
                                   Theme::Space::M, Theme::Space::M);
    metaLayout->setSpacing(Theme::Space::M);

    auto* row1 = new QHBoxLayout;
    row1->setSpacing(Theme::Space::M);
    auto* category = new QComboBox;
    category->addItems({"Study", "Work", "Break", "Exercise", "Personal"});
    int catIdx = category->findText(curCat, Qt::MatchFixedString);
    category->setCurrentIndex(catIdx >= 0 ? catIdx : 0);
    category->setMinimumHeight(36);
    category->setObjectName("eventField");
    row1->addWidget(category);
    row1->addStretch();
    metaLayout->addLayout(row1);

    auto* row2 = new QHBoxLayout;
    row2->setSpacing(Theme::Space::M);
    auto* dateLabel = new QLabel("Schedule");
    dateLabel->setObjectName("eventMicroLabel");
    metaLayout->addWidget(dateLabel);
    auto* dateEdit  = new QDateEdit;
    dateEdit->setCalendarPopup(true);
    dateEdit->setDate(original.getStartTime().date());
    dateEdit->setMinimumHeight(36);
    dateEdit->setObjectName("eventField");
    auto* startTime = new QTimeEdit;
    startTime->setDisplayFormat("hh:mm");
    startTime->setTime(original.getStartTime().time());
    startTime->setMinimumHeight(36);
    startTime->setObjectName("eventField");
    auto* endTime = new QTimeEdit;
    endTime->setDisplayFormat("hh:mm");
    endTime->setTime(original.getEndTime().time());
    endTime->setMinimumHeight(36);
    endTime->setObjectName("eventField");
    auto* toLabel = new QLabel("to");
    toLabel->setObjectName("eventMicroLabel");

    row2->addWidget(dateEdit);
    row2->addWidget(startTime);
    row2->addWidget(toLabel);
    row2->addWidget(endTime);
    row2->addStretch();
    metaLayout->addLayout(row2);
    root->addWidget(metaCard);

    auto* notesCard = new QFrame;
    notesCard->setObjectName("eventSectionCard");
    auto* notesLayout = new QVBoxLayout(notesCard);
    notesLayout->setContentsMargins(Theme::Space::M, Theme::Space::M,
                                    Theme::Space::M, Theme::Space::M);
    notesLayout->setSpacing(Theme::Space::S);
    auto* notesLabel = new QLabel("Notes");
    notesLabel->setObjectName("eventMicroLabel");
    notesLayout->addWidget(notesLabel);
    auto* descEdit = new QTextEdit;
    descEdit->setPlainText(curNotes);
    descEdit->setMinimumHeight(132);
    descEdit->setMaximumHeight(160);
    descEdit->setObjectName("eventField");
    notesLayout->addWidget(descEdit);
    root->addWidget(notesCard);

    auto* btnRow = new QHBoxLayout;
    auto* deleteBtn = new QPushButton("Delete");
    deleteBtn->setObjectName("eventDangerBtn");
    deleteBtn->setCursor(Qt::PointingHandCursor);
    btnRow->addWidget(deleteBtn);
    btnRow->addStretch();
    auto* cancelBtn1 = new QPushButton("Cancel");
    cancelBtn1->setObjectName("eventSecondaryBtn");
    cancelBtn1->setCursor(Qt::PointingHandCursor);
    auto* saveBtn1 = new QPushButton("Save Changes");
    saveBtn1->setObjectName("eventPrimaryBtn");
    saveBtn1->setCursor(Qt::PointingHandCursor);
    btnRow->addWidget(cancelBtn1);
    btnRow->addWidget(saveBtn1);
    root->addLayout(btnRow);

    connect(deleteBtn, &QPushButton::clicked, &dlg, [&] {
        dlg.reject();
        openDeleteDialog(idx);
    });
    connect(cancelBtn1, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn1, &QPushButton::clicked, &dlg, [&] {
        const QString t = titleEdit->text().trimmed();
        if (t.isEmpty()) { titleEdit->setFocus(); return; }

        QDate d = dateEdit->date();
        QTime s = startTime->time();
        QTime e = endTime->time();
        if (s >= e) e = s.addSecs(30*60);

        const QString cat = category->currentText();
        const QString notes = descEdit->toPlainText().trimmed();
        const QString packed = notes.isEmpty() ? cat : (cat + "::" + notes);

        Event updated = original;
        updated.setTitle(t);
        updated.setDescription(packed);
        updated.setStartTime(QDateTime(d, s));
        updated.setEndTime(QDateTime(d, e));
        updated.setColor(Theme::Category::color(cat));
        updated.setLastModified(QDateTime::currentDateTimeUtc());
        updated.setDirty(true);

        if (m_store->isSeries(original)) {
            QMessageBox box(this);
            box.setWindowTitle("Edit Series");
            box.setText("Apply to this event only or the whole series?");
            auto* btnThis = box.addButton("This event", QMessageBox::ActionRole);
            auto* btnAll  = box.addButton("All in series", QMessageBox::ActionRole);
            box.addButton(QMessageBox::Cancel);
            box.exec();

            if (box.clickedButton() == btnThis) {
                m_store->update(idx, updated);
                if (updated.source() != Event::Local)
                    m_sync->pushEvent(idx);
            } else if (box.clickedButton() == btnAll) {
                const QTime newStartT = updated.getStartTime().time();
                const QTime newEndT   = updated.getEndTime().time();
                m_store->updateSeries(original, [&](Event& ev) {
                    ev.setTitle(updated.getTitle());
                    ev.setDescription(updated.getDescription());
                    ev.setColor(updated.getColor());
                    ev.setStartTime(QDateTime(ev.getStartTime().date(), newStartT));
                    ev.setEndTime(QDateTime(ev.getEndTime().date(), newEndT));
                    ev.setLastModified(QDateTime::currentDateTimeUtc());
                    ev.setDirty(true);
                });
                // Push every remote-sourced event in the series.
                const auto& all = m_store->all();
                for (int i = 0; i < all.size(); ++i) {
                    if (all[i].source() != Event::Local && all[i].isDirty())
                        m_sync->pushEvent(i);
                }
            } else {
                return;
            }
        } else {
            m_store->update(idx, updated);
            if (updated.source() != Event::Local)
                m_sync->pushEvent(idx);
        }
        dlg.accept();
    });

    dlg.exec();
}

void MainWindow::openDeleteDialog(int idx) {
    if (idx < 0 || idx >= m_store->all().size()) return;

    const Event target = m_store->all()[idx];

    // If it's a remote event, also delete on provider
    if (!target.externalId().isEmpty()) {
        CalendarProvider* prov = !target.providerKey().isEmpty()
            ? m_sync->providerByKey(target.providerKey())
            : nullptr;
        if (!prov && target.source() == Event::Google)
            prov = m_googleProvider;
        else if (!prov && target.source() == Event::Outlook)
            prov = m_outlookProvider;
        else if (!prov && target.source() == Event::AppleNative)
            prov = m_appleProvider;
        if (prov && prov->isAuthenticated())
            prov->deleteEvent(target.externalId());
    }

    if (!m_store->isSeries(target)) {
        m_store->remove(idx);
        m_selectedEventIndex = -1;
        updateButtonStates();
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle("Delete Event");
    box.setText("Delete this event only or the whole series?");
    auto* btnThis = box.addButton("This event", QMessageBox::ActionRole);
    auto* btnAll  = box.addButton("All in series", QMessageBox::ActionRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();

    if (box.clickedButton() == btnThis)
        m_store->remove(idx);
    else if (box.clickedButton() == btnAll)
        m_store->removeSeries(target);

    m_selectedEventIndex = -1;
    updateButtonStates();
}
