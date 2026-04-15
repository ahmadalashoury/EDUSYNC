#include "CalendarWidget.h"
#include "Theme.h"

#include <QTableView>
#include <QHeaderView>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QShowEvent>
#include <QHoverEvent>
#include <QTextCharFormat>
#include <QFontMetrics>
#include <QCursor>
#include <QTimer>
#include <QLinearGradient>
#include <QRadialGradient>

// Map table cell -> real date
static inline QDate dateForIndex(QTableView* view, const QModelIndex& idx,
                                 int shownYear, int shownMonth) {
    if (!view || !idx.isValid()) return {};
    const auto* model = view->model();
    const int day = model ? model->data(idx, Qt::DisplayRole).toInt() : 0;
    if (day <= 0) return {};
    const int r = idx.row();
    if (r == 0 && day > 7) {
        QDate prev = QDate(shownYear, shownMonth, 1).addMonths(-1);
        return QDate(prev.year(), prev.month(), day);
    }
    if (r >= 4 && day <= 14) {
        QDate next = QDate(shownYear, shownMonth, 1).addMonths(1);
        return QDate(next.year(), next.month(), day);
    }
    return QDate(shownYear, shownMonth, day);
}

// Circle rect for the day number badge — centered horizontally near the top
static QRect dayCircleRect(const QRect& cell) {
    const int sz = qMin(30, qMax(24, qMin(cell.width(), cell.height()) / 2));
    const int cx = cell.center().x();
    return QRect(cx - sz / 2, cell.top() + 7, sz, sz);
}

// ============================================================================
// Construction
// ============================================================================

CalendarWidget::CalendarWidget(QWidget* parent)
    : QCalendarWidget(parent)
{
    setupInternals();

    m_month = QDate(yearShown(), monthShown(), 1);
    m_selected = selectedDate();
    connect(this, &QCalendarWidget::selectionChanged, this, [this] {
        m_selected = selectedDate();
        update();
    });

    connect(this, &QCalendarWidget::currentPageChanged, this, [this](int year, int month) {
        m_month = QDate(year, month, 1);
        m_hovered = QDate();
        setupInternals();
        styleHeader();
        update();
    });
}

void CalendarWidget::setupInternals() {
    const auto newView = findChild<QTableView*>();
    if (m_view == newView && m_viewport)
        return;

    if (m_viewport)
        m_viewport->removeEventFilter(this);

    m_view = newView;
    m_hHeader = nullptr;
    m_viewport = nullptr;
    if (!m_view) return;

    m_view->setMouseTracking(true);
    m_view->setShowGrid(false);
    m_view->setSelectionMode(QAbstractItemView::NoSelection);
    m_view->setStyleSheet(
        "QTableView::item:selected    { background: transparent; border: 0; }"
        "QTableView::item:focus       { outline: 0; }"
        "QAbstractItemView::item      { background: transparent; }");

    m_hHeader = m_view->horizontalHeader();
    if (m_hHeader) m_hHeader->setObjectName("CalHeader");

    m_viewport = m_view->viewport();
    if (m_viewport) {
        m_viewport->setMouseTracking(true);
        m_viewport->setAttribute(Qt::WA_Hover, true);
        m_viewport->installEventFilter(this);
    }
}

// ============================================================================
// Theme
// ============================================================================

void CalendarWidget::applyTheme(bool light) {
    m_light = light;
    styleHeader();

    // Normalize weekend text
    QTextCharFormat fmt;
    fmt.clearBackground();
    fmt.setForeground(palette().color(QPalette::Text));
    setWeekdayTextFormat(Qt::Saturday, fmt);
    setWeekdayTextFormat(Qt::Sunday, fmt);

    update();
}

void CalendarWidget::styleHeader() {
    if (!m_hHeader) return;

    const auto c = m_light ? Theme::light() : Theme::dark();
    const QColor glassTop = m_light ? QColor(255, 255, 255, 230) : QColor(24, 30, 42, 228);
    const QColor glassBottom = m_light ? QColor(246, 243, 238, 210) : QColor(12, 16, 24, 240);
    const QColor glowBorder = m_light ? QColor(70, 134, 255, 34) : QColor(87, 214, 255, 62);
    const QColor headerText = m_light ? c.headerText : QColor("#bfeaff");

    m_hHeader->setSectionsClickable(false);
    m_hHeader->setHighlightSections(false);
    m_hHeader->setSectionResizeMode(QHeaderView::Stretch);
    m_hHeader->setDefaultAlignment(Qt::AlignCenter);
    m_hHeader->setFixedHeight(36);

    m_hHeader->setStyleSheet(QString(
        "QHeaderView {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2);"
        "  border: 1px solid %3;"
        "  border-left: 0;"
        "  border-right: 0;"
        "}"
        "QHeaderView::section {"
        "  background: transparent; color: %4; border: 0;"
        "  border-bottom: 1px solid %5;"
        "  padding: 8px 0 7px 0; font-weight: 600;"
        "  text-transform: uppercase; letter-spacing: 0.14em;"
        "}"
        "QHeaderView::section:hover { background: transparent; }")
        .arg(glassTop.name(QColor::HexArgb),
             glassBottom.name(QColor::HexArgb),
             glowBorder.name(QColor::HexArgb),
             headerText.name(),
             c.headerBorder.name()));

    if (auto* vh = m_view ? m_view->verticalHeader() : nullptr)
        vh->setSectionResizeMode(QHeaderView::Stretch);
}

void CalendarWidget::showEvent(QShowEvent* e) {
    QCalendarWidget::showEvent(e);
    QTimer::singleShot(0, this, [this] {
        applyTheme(palette().color(QPalette::Window).lightness() > 127);
    });
}

void CalendarWidget::resizeEvent(QResizeEvent* e) {
    QCalendarWidget::resizeEvent(e);
    styleHeader();
}

// ============================================================================
// Event filter for hover tracking
// ============================================================================

bool CalendarWidget::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == m_viewport) {
        switch (ev->type()) {
        case QEvent::MouseMove:
            updateHoveredFromPos(static_cast<QMouseEvent*>(ev)->position().toPoint());
            return false;
        case QEvent::HoverMove:
            updateHoveredFromPos(static_cast<QHoverEvent*>(ev)->position().toPoint());
            return false;
        case QEvent::Enter:
            updateHoveredFromPos(m_viewport->mapFromGlobal(QCursor::pos()));
            return false;
        case QEvent::Leave:
        case QEvent::HoverLeave:
            if (m_hovered.isValid()) {
                updateCell(m_hovered);
                m_hovered = QDate();
            }
            return false;
        default: break;
        }
    }
    return QCalendarWidget::eventFilter(obj, ev);
}

// ============================================================================
// Grid helpers
// ============================================================================

QDate CalendarWidget::gridStartDate() const {
    const QDate first(yearShown(), monthShown(), 1);
    const int fdow = static_cast<int>(firstDayOfWeek());
    const int off  = (first.dayOfWeek() - fdow + 7) % 7;
    return first.addDays(-off);
}

void CalendarWidget::updateHoveredFromPos(const QPoint& pos) {
    setupInternals();
    if (!m_view) return;
    const QModelIndex idx = m_view->indexAt(pos);
    QDate d;
    if (idx.isValid())
        d = dateForIndex(m_view, idx, yearShown(), monthShown());
    if (d == m_hovered) return;
    if (m_hovered.isValid()) updateCell(m_hovered);
    m_hovered = d;
    if (m_hovered.isValid()) updateCell(m_hovered);
}

// ============================================================================
// Public API
// ============================================================================

void CalendarWidget::setEvents(const QList<Event>& events) {
    m_events = events;
    update();
}

void CalendarWidget::setCurrentMonth(const QDate& anyDayInMonth) {
    m_month = QDate(anyDayInMonth.year(), anyDayInMonth.month(), 1);
    setCurrentPage(m_month.year(), m_month.month());
    update();
    emit monthChanged(m_month);
}

// ============================================================================
// Input handling
// ============================================================================

void CalendarWidget::mousePressEvent(QMouseEvent* e) {
    QCalendarWidget::mousePressEvent(e);
    m_selected = selectedDate();
    update();
}

void CalendarWidget::wheelEvent(QWheelEvent* e) {
    if (!m_month.isValid()) m_month = QDate(yearShown(), monthShown(), 1);
    const auto delta = e->angleDelta();
    const int dy = qAbs(delta.y()) >= qAbs(delta.x()) ? delta.y() : 0;
    if (dy > 0)      setCurrentMonth(m_month.addMonths(-1));
    else if (dy < 0) setCurrentMonth(m_month.addMonths(+1));
    e->accept();
}

void CalendarWidget::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Left:     setSelectedDate(selectedDate().addDays(-1));  break;
    case Qt::Key_Right:    setSelectedDate(selectedDate().addDays(+1));  break;
    case Qt::Key_Up:       setSelectedDate(selectedDate().addDays(-7));  break;
    case Qt::Key_Down:     setSelectedDate(selectedDate().addDays(+7));  break;
    case Qt::Key_PageUp:   setCurrentMonth(QDate(yearShown(), monthShown(), 1).addMonths(-1)); break;
    case Qt::Key_PageDown: setCurrentMonth(QDate(yearShown(), monthShown(), 1).addMonths(+1)); break;
    default:
        QCalendarWidget::keyPressEvent(e);
        return;
    }
    m_selected = selectedDate();
    update();
}

// ============================================================================
// Custom cell painting (the core visual)
// ============================================================================

void CalendarWidget::paintCell(QPainter* p, const QRect& rect, QDate date) const {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    p->setBrush(Qt::NoBrush);

    const auto c = m_light ? Theme::light() : Theme::dark();
    const bool inMonth = (date.month() == monthShown() && date.year() == yearShown());
    const bool isSelected = (date == m_selected);
    const bool isHovered  = (date == m_hovered);
    const bool isToday    = (date == QDate::currentDate());
    const bool darkMode   = !m_light;

    const QRect circle = dayCircleRect(rect);
    const QRect cellBg = rect.adjusted(1, 1, -1, -1);
    const QRectF glassRect = cellBg.adjusted(2, 2, -2, -2);

    if (darkMode) {
        QLinearGradient cellGlass(glassRect.topLeft(), glassRect.bottomLeft());
        cellGlass.setColorAt(0.0, QColor(255, 255, 255, isSelected ? 10 : 6));
        cellGlass.setColorAt(1.0, QColor(0, 0, 0, 0));
        p->setBrush(cellGlass);
        p->drawRoundedRect(glassRect, Theme::Radius::M, Theme::Radius::M);
    }

    // Hover: subtle full-cell background
    if (isHovered && !isSelected) {
        QColor hov = darkMode ? QColor(83, 214, 255, 18) : QColor(19, 34, 77, 10);
        p->setBrush(hov);
        p->drawRoundedRect(cellBg, Theme::Radius::M, Theme::Radius::M);
    }

    // Selected: ambient neon glow + glassy cell
    if (isSelected) {
        QColor tint = darkMode ? QColor(63, 179, 255, 26) : QColor(c.accent.red(), c.accent.green(), c.accent.blue(), 18);
        p->setBrush(tint);
        p->drawRoundedRect(cellBg, Theme::Radius::M, Theme::Radius::M);

        if (darkMode) {
            QRadialGradient halo(circle.center(), circle.width() * 0.95);
            halo.setColorAt(0.0, QColor(112, 233, 255, 165));
            halo.setColorAt(0.35, QColor(56, 171, 255, 84));
            halo.setColorAt(1.0, QColor(56, 171, 255, 0));
            p->setBrush(halo);
            p->drawEllipse(circle.adjusted(-9, -9, 9, 9));
        }

        QLinearGradient selectedFill(circle.topLeft(), circle.bottomRight());
        selectedFill.setColorAt(0.0, darkMode ? QColor("#74e3ff") : QColor("#3b82f6"));
        selectedFill.setColorAt(1.0, darkMode ? QColor("#2f79ff") : QColor("#1d4ed8"));
        p->setBrush(selectedFill);
        p->drawEllipse(circle);
        p->setPen(QPen(darkMode ? QColor(219, 248, 255, 170) : QColor(255, 255, 255, 180), 1.1));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(circle.adjusted(1, 1, -1, -1));
        p->setPen(Qt::NoPen);
    }
    // Today: softer luminous ring
    else if (isToday && inMonth) {
        QColor todayWash = darkMode ? QColor(68, 196, 255, 22) : QColor(c.todayBg.red(), c.todayBg.green(), c.todayBg.blue(), 220);
        p->setBrush(todayWash);
        p->drawEllipse(circle.adjusted(-3, -3, 3, 3));
        p->setPen(QPen(darkMode ? QColor(108, 225, 255, 150) : c.todayText, darkMode ? 1.5 : 1.2));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(circle.adjusted(1, 1, -1, -1));
        p->setPen(Qt::NoPen);
    }

    // Day number — centered in circle
    QColor dayFg;
    if (isSelected)               dayFg = c.accentText;
    else if (isToday && inMonth)  dayFg = c.todayText;
    else if (inMonth)             dayFg = c.text;
    else                          dayFg = c.spillover;

    p->setPen(dayFg);
    QFont f = p->font();
    f.setPointSizeF(11.0);
    f.setWeight(inMonth ? QFont::DemiBold : QFont::Normal);
    p->setFont(f);
    p->drawText(circle, Qt::AlignCenter, QString::number(date.day()));

    // Event dots below number (current month only, using each event's color)
    if (inMonth)
        drawEventDots(*p, rect, date);

    p->restore();
}

// ============================================================================
// Event decorations
// ============================================================================

void CalendarWidget::drawEventDots(QPainter& p, const QRect& cell, const QDate& d) const {
    const auto fallback = m_light ? Theme::light().accent : Theme::dark().accent;
    const bool darkMode = !m_light;

    QList<QColor> colors;
    for (const Event& e : m_events) {
        if (!e.isOnDate(d)) continue;
        const QColor col = e.getColor().isValid() ? e.getColor() : fallback;
        colors.append(col);
        if (colors.size() >= 3) break;
    }
    if (colors.isEmpty()) return;

    const QRect circle = dayCircleRect(cell);
    const int dotR = 2;
    const int gap  = darkMode ? 6 : 4;
    const int n    = colors.size();
    const int totalW = n * dotR * 2 + (n - 1) * gap;
    const int startX = circle.center().x() - totalW / 2 + dotR;
    const int cy = circle.bottom() + (darkMode ? 7 : 5);

    p.setPen(Qt::NoPen);
    for (int i = 0; i < n; ++i) {
        const QPoint center(startX + i * (dotR * 2 + gap), cy);
        if (darkMode) {
            QRadialGradient glow(center, 5.5);
            QColor glowColor = colors[i];
            glowColor.setAlpha(120);
            glow.setColorAt(0.0, glowColor);
            glow.setColorAt(1.0, QColor(glowColor.red(), glowColor.green(), glowColor.blue(), 0));
            p.setBrush(glow);
            p.drawEllipse(QRectF(center.x() - 5.5, center.y() - 5.5, 11, 11));
        }
        p.setBrush(colors[i].lighter(darkMode ? 135 : 100));
        p.drawEllipse(center, dotR, dotR);
    }
}

void CalendarWidget::drawEventChips(QPainter&, const QRect&, const QDate&) const {
    // Chips removed for cleaner minimal calendar design.
    // Event presence is indicated by dots below the day number.
}
