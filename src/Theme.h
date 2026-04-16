#pragma once

#include <QColor>
#include <QFont>
#include <QPalette>
#include <QString>
#include <QApplication>
#include <QFontDatabase>

// ============================================================================
// EduSync Design System
//
// Single source of truth for all visual tokens. Every widget reads from here.
// No hardcoded colors, fonts, or spacing anywhere else in the codebase.
// ============================================================================

namespace Theme {

// ---------------------------------------------------------------------------
// Spacing scale (8px base)
// ---------------------------------------------------------------------------
namespace Space {
    inline constexpr int XXS =  2;
    inline constexpr int XS  =  4;
    inline constexpr int S   =  8;
    inline constexpr int M   = 12;
    inline constexpr int L   = 16;
    inline constexpr int XL  = 24;
    inline constexpr int XXL = 32;
    inline constexpr int XXXL= 48;
}

// ---------------------------------------------------------------------------
// Border radius
// ---------------------------------------------------------------------------
namespace Radius {
    inline constexpr int S  =  4;
    inline constexpr int M  =  8;
    inline constexpr int L  = 12;
    inline constexpr int XL = 16;
    inline constexpr int Pill = 999;
}

// ---------------------------------------------------------------------------
// Color palette — each theme provides a full token set
// ---------------------------------------------------------------------------
struct Colors {
    // Surfaces
    QColor bg;            // app background
    QColor surface;       // card / panel background
    QColor surfaceHover;  // card hover state
    QColor surfaceActive; // card pressed state
    QColor surfaceRaised; // elevated card surface
    QColor border;        // default border
    QColor borderSubtle;  // lighter border

    // Text
    QColor text;          // primary text
    QColor textSecondary; // muted / secondary
    QColor textTertiary;  // very subtle labels

    // Brand / accent
    QColor accent;        // primary action color
    QColor accentHover;
    QColor accentText;    // text on accent bg

    // Semantic
    QColor success;
    QColor warning;
    QColor danger;
    QColor info;

    // Calendar specific
    QColor headerBg;
    QColor headerText;
    QColor headerBorder;
    QColor todayRing;
    QColor todayBg;
    QColor todayText;
    QColor spillover;     // dimmed text for adjacent-month days
    QColor selectedBg;
    QColor hoverBg;
    QColor eventChip;
    QColor eventChipText;
    QColor eventDot;
};

// ---------------------------------------------------------------------------
// Dark theme tokens
// ---------------------------------------------------------------------------
inline Colors dark() {
    return Colors{
        // Surfaces — layered elevation (deeper → raised)
        .bg           = QColor("#0d0f13"),   // deepest layer
        .surface      = QColor("#161a22"),
        .surfaceHover = QColor("#1c212b"),
        .surfaceActive= QColor("#202632"),
        .surfaceRaised= QColor("#202632"),
        .border       = QColor("#293241"),
        .borderSubtle = QColor("#202734"),

        // Text — high contrast hierarchy
        .text          = QColor("#e8ecf2"),
        .textSecondary = QColor("#8b95a5"),
        .textTertiary  = QColor("#505a68"),

        // Brand
        .accent       = QColor("#4f8cff"),
        .accentHover  = QColor("#3c79e8"),
        .accentText   = QColor("#ffffff"),

        // Semantic
        .success = QColor("#22c55e"),
        .warning = QColor("#f59e0b"),
        .danger  = QColor("#ef4444"),
        .info    = QColor("#3b82f6"),

        // Calendar — Deep Space Neon aesthetic
        .headerBg     = QColor("#06090f"),   // near-black with blue tint
        .headerText   = QColor("#3d5780"),   // muted neon blue
        .headerBorder = QColor("#0f1520"),   // barely-visible separator
        .todayRing    = QColor(79, 140, 255, 60), // neon bloom outer ring
        .todayBg      = QColor("#4f8cff"),   // bright neon blue fill
        .todayText    = QColor("#ffffff"),
        .spillover    = QColor("#1e2530"),   // nearly invisible out-of-month
        .selectedBg   = QColor(79, 140, 255, 22),
        .hoverBg      = QColor(79, 140, 255, 12),
        .eventChip    = QColor("#4f8cff"),
        .eventChipText= QColor("#ffffff"),
        .eventDot     = QColor("#4f8cff"),   // neon blue event bars
    };
}

// ---------------------------------------------------------------------------
// Light theme tokens
// ---------------------------------------------------------------------------
inline Colors light() {
    return Colors{
        // Surfaces — warm, layered (avoid pure white everywhere)
        .bg           = QColor("#f3f2ee"),   // warm stone background
        .surface      = QColor("#f7f5f1"),
        .surfaceHover = QColor("#efede8"),
        .surfaceActive= QColor("#e7e3dd"),
        .surfaceRaised= QColor("#fcfbf8"),
        .border       = QColor("#d9d4cb"),
        .borderSubtle = QColor("#ebe7e0"),

        // Text — Apple-style near-black hierarchy
        .text          = QColor("#1c1c1e"),
        .textSecondary = QColor("#636366"),
        .textTertiary  = QColor("#8e8e93"),

        // Brand
        .accent       = QColor("#2f6fed"),
        .accentHover  = QColor("#255cd1"),
        .accentText   = QColor("#ffffff"),

        // Semantic
        .success = QColor("#16a34a"),
        .warning = QColor("#d97706"),
        .danger  = QColor("#dc2626"),
        .info    = QColor("#2563eb"),

        // Calendar
        .headerBg     = QColor("#eeede9"),
        .headerText   = QColor("#636366"),
        .headerBorder = QColor("#dbd8d2"),
        .todayRing    = QColor(0, 0, 0, 22),
        .todayBg      = QColor("#fef1f0"),
        .todayText    = QColor("#dc2626"),
        .spillover    = QColor("#bcbab5"),
        .selectedBg   = QColor(0, 0, 0, 8),
        .hoverBg      = QColor(0, 0, 0, 4),
        .eventChip    = QColor("#2563eb"),
        .eventChipText= QColor("#ffffff"),
        .eventDot     = QColor("#6366f1"),
    };
}

// ---------------------------------------------------------------------------
// Category colors — single source of truth for event coloring
// ---------------------------------------------------------------------------
namespace Category {
    inline QColor color(const QString& cat) {
        const QString c = cat.trimmed().toLower();
        if (c == "study")    return QColor("#3b82f6");
        if (c == "work")     return QColor("#8b5cf6");
        if (c == "break")    return QColor("#f59e0b");
        if (c == "exercise") return QColor("#22c55e");
        if (c == "personal") return QColor("#ec4899");
        return QColor("#6366f1");
    }
}

// ---------------------------------------------------------------------------
// Typography
// ---------------------------------------------------------------------------
namespace Font {
    inline QFont base() {
        QFont f = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        f.setPointSize(11);
        f.setStyleStrategy(QFont::PreferAntialias);
        return f;
    }
    inline QFont heading() {
        QFont f = base();
        f.setPointSize(28);
        f.setWeight(QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, -0.4);
        return f;
    }
    inline QFont title() {
        QFont f = base();
        f.setPointSize(20);
        f.setWeight(QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, -0.3);
        return f;
    }
    inline QFont subheading() {
        QFont f = base();
        f.setPointSize(14);
        f.setWeight(QFont::DemiBold);
        return f;
    }
    inline QFont caption() {
        QFont f = base();
        f.setPointSize(10);
        f.setWeight(QFont::Medium);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
        return f;
    }
    inline QFont mono() {
        QFont f("Menlo", 11);
        f.setStyleHint(QFont::Monospace);
        return f;
    }
}

// ---------------------------------------------------------------------------
// Build a QPalette from our tokens
// ---------------------------------------------------------------------------
inline QPalette buildPalette(const Colors& c) {
    QPalette p;
    p.setColor(QPalette::Window,          c.bg);
    p.setColor(QPalette::WindowText,      c.text);
    p.setColor(QPalette::Base,            c.surface);
    p.setColor(QPalette::AlternateBase,   c.surfaceHover);
    p.setColor(QPalette::Text,            c.text);
    p.setColor(QPalette::Button,          c.surface);
    p.setColor(QPalette::ButtonText,      c.text);
    p.setColor(QPalette::Highlight,       c.accent);
    p.setColor(QPalette::HighlightedText, c.accentText);
    p.setColor(QPalette::ToolTipBase,     c.surface);
    p.setColor(QPalette::ToolTipText,     c.text);
    p.setColor(QPalette::PlaceholderText, c.textTertiary);

    // Disabled states
    p.setColor(QPalette::Disabled, QPalette::WindowText, c.textTertiary);
    p.setColor(QPalette::Disabled, QPalette::Text,       c.textTertiary);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c.textTertiary);
    return p;
}

// ---------------------------------------------------------------------------
// Global stylesheet built from tokens (applied once via qApp->setStyleSheet)
// ---------------------------------------------------------------------------
inline QString buildStylesheet(const Colors& c) {
    // Compute derived colors that Qt CSS can't do with opacity
    QColor accentHoverBg = c.accentHover;
    QColor accentPressedBg = c.accentHover.darker(110);
    QColor accentWash = c.accent;
    accentWash.setAlpha(c.bg.lightness() < 128 ? 38 : 26);
    QColor disabledBg = c.surfaceHover;
    QColor disabledBorder = c.borderSubtle;
    QColor dangerBg = c.danger;
    QColor dangerHover = c.danger.darker(115);

    return QString(R"(
        * { outline: none; }

        QWidget {
            color: %2;
        }

        QLabel, QCheckBox, QRadioButton, QGroupBox {
            background: transparent;
        }

        QWidget#leftRail, QWidget#rightRail, QWidget#toolbarSurface, QWidget#centerSurface,
        QWidget#settingsPage {
            background: %1;
        }

        QFrame#accountHubCard, QFrame#settingsCard {
            background: %24;
            border: 1px solid %25;
            border-radius: %13px;
        }

        QFrame#calendarCard {
            background: transparent;
            border: none;
            border-radius: 0;
        }

        /* ── Scrollbars ─────────────────────────────────────── */
        QScrollBar:vertical {
            background: transparent;
            width: 6px;
            margin: 4px 0;
        }
        QScrollBar::handle:vertical {
            background: %3;
            border-radius: 3px;
            min-height: 32px;
        }
        QScrollBar::handle:vertical:hover {
            background: %5;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
            height: 0;
        }
        QScrollBar:horizontal {
            background: transparent;
            height: 6px;
            margin: 0 4px;
        }
        QScrollBar::handle:horizontal {
            background: %3;
            border-radius: 3px;
            min-width: 32px;
        }
        QScrollBar::handle:horizontal:hover {
            background: %5;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
            width: 0;
        }

        /* ── Splitter ───────────────────────────────────────── */
        QSplitter::handle {
            background: %6;
            width: 1px;
        }
        QSplitter::handle:hover {
            background: %9;
        }

        /* ── Tabs ───────────────────────────────────────────── */
        QTabWidget::pane {
            border: none;
            background: %1;
        }
        QTabBar::tab {
            background: %4;
            color: %5;
            border: 1px solid %6;
            border-radius: %7px;
            padding: 8px 16px;
            margin: 3px 2px;
            font-weight: 500;
        }
        QTabBar::tab:selected {
            background: %8;
            color: %2;
            border-color: %9;
        }
        QTabBar::tab:hover:!selected {
            background: %10;
        }

        /* ── Buttons (default) ──────────────────────────────── */
        QPushButton {
            background: %4;
            color: %2;
            border: 1px solid %25;
            border-radius: %11px;
            padding: 8px 16px;
            font-weight: 600;
            font-size: 12px;
        }
        QPushButton:hover {
            background: %10;
            border-color: %5;
        }
        QPushButton:pressed {
            background: %12;
        }
        QPushButton:disabled {
            background: %17;
            color: %18;
            border-color: %19;
        }

        /* ── Accent button (primary action) ─────────────────── */
        QPushButton#accentBtn {
            background: %9;
            color: white;
            border: 1px solid %9;
            font-weight: 600;
        }
        QPushButton#accentBtn:hover {
            background: %20;
            border-color: %20;
        }
        QPushButton#accentBtn:pressed {
            background: %21;
            border-color: %21;
        }
        QPushButton#accentBtn:disabled {
            background: %17;
            color: %18;
            border-color: %19;
        }

        /* ── Ghost button (borderless) ──────────────────────── */
        QPushButton#ghostBtn {
            background: transparent;
            color: %5;
            border: 1px solid transparent;
            font-weight: 500;
        }

        QPushButton#subtleBtn {
            background: %24;
            color: %2;
            border: 1px solid %25;
            font-weight: 600;
        }
        QPushButton#subtleBtn:hover {
            background: %10;
        }
        QPushButton#ghostBtn:hover {
            background: %10;
            color: %2;
            border-color: %6;
        }
        QPushButton#ghostBtn:pressed {
            background: %12;
        }

        /* ── Danger button (destructive action) ─────────────── */
        QPushButton#dangerBtn {
            background: transparent;
            color: %22;
            border: 1px solid %6;
            font-weight: 500;
        }
        QPushButton#dangerBtn:hover {
            background: %22;
            color: white;
            border-color: %22;
        }
        QPushButton#dangerBtn:pressed {
            background: %23;
            color: white;
            border-color: %23;
        }

        /* ── Nav buttons (calendar arrows) ──────────────────── */
        QPushButton#navBtn {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 4px;
            font-size: 14px;
            font-weight: 600;
            color: %5;
        }
        QPushButton#navBtn:hover {
            background: %10;
            border-color: %6;
            color: %2;
        }
        QPushButton#navBtn:pressed {
            background: %12;
        }

        /* ── Today button ───────────────────────────────────── */
        QPushButton#todayBtn {
            background: transparent;
            border: 1px solid %6;
            border-radius: 6px;
            padding: 4px 12px;
            font-size: 11px;
            font-weight: 600;
            color: %5;
        }
        QPushButton#todayBtn:hover {
            background: %10;
            color: %2;
        }

        /* ── Theme toggle ───────────────────────────────────── */
        QPushButton#themeBtn {
            background: transparent;
            border: 1px solid %6;
            border-radius: 8px;
            padding: 4px;
            font-size: 16px;
        }
        QPushButton#themeBtn:hover {
            background: %10;
        }

        /* ── Lists ──────────────────────────────────────────── */
        QListWidget {
            background: transparent;
            border: none;
            padding: 4px;
        }
        QListWidget::item {
            padding: 8px 12px;
            border-radius: %14px;
            margin: 1px 2px;
        }
        QListWidget::item:selected {
            background: %10;
        }
        QListWidget::item:hover:!selected {
            background: %15;
        }

        /* ── Text edit ──────────────────────────────────────── */
        QTextEdit {
            background: %4;
            border: 1px solid %6;
            border-radius: %13px;
            padding: 12px;
            selection-background-color: %9;
        }

        /* ── Tooltips ───────────────────────────────────────── */
        QToolTip {
            background: %4;
            color: %2;
            border: 1px solid %6;
            border-radius: 6px;
            padding: 6px 10px;
        }

        /* ── Inputs ─────────────────────────────────────────── */
        QLineEdit, QTimeEdit, QDateEdit, QComboBox {
            background: %24;
            color: %2;
            border: 1px solid %25;
            border-radius: %14px;
            padding: 8px 12px;
            font-size: 12px;
        }
        QLineEdit:focus, QTimeEdit:focus, QDateEdit:focus, QComboBox:focus {
            border-color: %9;
        }

        /* ── Progress bars ──────────────────────────────────── */
        QProgressBar {
            background: %4;
            border: 1px solid %6;
            border-radius: %14px;
            color: %2;
            text-align: center;
            height: 12px;
        }
        QProgressBar::chunk {
            background: %9;
            border-radius: %16px;
        }

        /* ── Dialogs ────────────────────────────────────────── */
        QDialog {
            background: %1;
        }

        QListWidget#settingsSidebar {
            background: transparent;
            border: none;
            padding: 18px 14px;
            outline: 0;
        }
        QListWidget#settingsSidebar::item {
            padding: 10px 16px;
            border-radius: 10px;
            font-size: 13px;
            font-weight: 600;
        }
        QListWidget#settingsSidebar::item:selected {
            background: %26;
            color: %2;
        }
        QListWidget#settingsSidebar::item:hover:!selected {
            background: %15;
        }

        /* ── Form labels ────────────────────────────────────── */
        QFormLayout QLabel {
            font-size: 11px;
            color: %5;
        }
    )")
    .arg(c.bg.name())              // %1 - app bg
    .arg(c.text.name())            // %2 - primary text
    .arg(c.borderSubtle.name())    // %3 - scrollbar handle (subtle)
    .arg(c.surface.name())         // %4 - surface bg
    .arg(c.textSecondary.name())   // %5 - muted text
    .arg(c.border.name())          // %6 - border
    .arg(Radius::L)                // %7 - tab radius
    .arg(c.surfaceHover.name())    // %8 - selected tab bg
    .arg(c.accent.name())          // %9 - accent
    .arg(c.surfaceHover.name())    // %10 - hover bg
    .arg(Radius::M)                // %11 - button radius
    .arg(c.surfaceActive.name())   // %12 - pressed bg
    .arg(Radius::L)                // %13 - list/textedit radius
    .arg(Radius::M)                // %14 - input radius
    .arg(c.borderSubtle.name())    // %15 - very subtle hover
    .arg(Radius::S)                // %16 - progress chunk radius
    .arg(disabledBg.name())        // %17 - disabled bg
    .arg(c.textTertiary.name())    // %18 - disabled text
    .arg(disabledBorder.name())    // %19 - disabled border
    .arg(accentHoverBg.name())     // %20 - accent hover
    .arg(accentPressedBg.name())   // %21 - accent pressed
    .arg(dangerBg.name())          // %22 - danger color
    .arg(dangerHover.name())       // %23 - danger hover
    .arg(c.surfaceRaised.name())   // %24 - raised surface
    .arg(c.borderSubtle.name())    // %25 - quiet border
    .arg(accentWash.name(QColor::HexArgb)) // %26 - accent wash
    ;
}

// ---------------------------------------------------------------------------
// Apply a full theme to the application
// ---------------------------------------------------------------------------
inline void apply(bool isDark) {
    const Colors c = isDark ? dark() : light();

    QFont appFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    appFont.setPointSize(11);
    appFont.setStyleStrategy(QFont::PreferAntialias);
    qApp->setFont(appFont);

    qApp->setPalette(buildPalette(c));
    qApp->setStyleSheet(buildStylesheet(c));
}

} // namespace Theme
