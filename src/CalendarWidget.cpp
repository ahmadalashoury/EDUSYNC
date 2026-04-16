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

// Circle rect for the day number badge — centered in cell (iOS pill style)
static QRect dayCircleRect(const QRect& cell) {
    const int sz = qMin(cell.width() - 4, cell.height() - 6);
    const int cx = cell.center().x();
    const int cy = cell.center().y() - 2; // slightly above center to leave bar room
    return QRect(cx - sz / 2, cy - sz / 2, sz, sz);
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
    const QColor headerText = m_light ? c.headerText : QColor("#3d5780");

    m_hHeader->setSectionsClickable(false);
    m_hHeader->setHighlightSections(false);
    m_hHeader->setSectionResizeMode(QHeaderView::Stretch);
    m_hHeader->setDefaultAlignment(Qt::AlignCenter);
    m_hHeader->setFixedHeight(22);

    m_hHeader->setStyleSheet(QString(
        "QHeaderView { background: transparent; border: 0; }"
        "QHeaderView::section {"
        "  background: transparent; color: %1; border: 0;"
        "  padding: 3px 0; font-size: 9px; font-weight: 700;"
        "  letter-spacing: 0.10em;"
        "}"
        "QHeaderView::section:hover { background: transparent; }")
        .arg(headerText.name()));

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

    // ── Dark mode only: dark navy disc on every in-month day ────────────
    if (inMonth && !isToday && !isSelected && darkMode) {
        p->setBrush(QColor(18, 26, 42));
        p->drawEllipse(circle);
    }

    // ── Hover: brighten the disc slightly ────────────────────────────────
    if (isHovered && inMonth && !isSelected && !isToday) {
        QColor hov = darkMode ? QColor(79, 140, 255, 22) : QColor(0, 0, 0, 10);
        p->setBrush(hov);
        p->drawEllipse(circle);
    }

    // ── Selected: bright accent fill with outer glow ─────────────────────
    if (isSelected && !isToday) {
        if (darkMode) {
            // soft glow halo
            QColor glow = c.accent; glow.setAlpha(30);
            p->setBrush(glow);
            p->drawEllipse(circle.adjusted(-4, -4, 4, 4));
        }
        p->setBrush(c.accent);
        p->drawEllipse(circle);
    }

    // ── Today: neon bloom glow rings + bright solid core ─────────────────
    if (isToday && inMonth) {
        if (darkMode) {
            struct Ring { int ex; int alpha; };
            constexpr Ring rings[] = {{12,3},{8,7},{5,14},{3,22}};
            for (const auto& r : rings) {
                QColor glow = c.todayBg; glow.setAlpha(r.alpha);
                p->setBrush(glow);
                p->drawEllipse(circle.adjusted(-r.ex, -r.ex, r.ex, r.ex));
            }
        }
        p->setBrush(isSelected ? c.accent.lighter(115) : c.todayBg);
        p->drawEllipse(circle);
    }

    // ── Day number ────────────────────────────────────────────────────────
    QColor dayFg;
    if      (isSelected || (isSelected && isToday)) dayFg = c.accentText;
    else if (isToday && inMonth)                    dayFg = c.todayText;
    else if (inMonth)                               dayFg = darkMode ? QColor("#c8d8f0") : c.text;
    else                                            dayFg = c.spillover;

    p->setPen(dayFg);
    QFont f = p->font();
    f.setPointSizeF(10.5);
    f.setWeight((isToday || isSelected) ? QFont::Bold
                : (inMonth ? QFont::Medium : QFont::Normal));
    p->setFont(f);
    p->drawText(circle, Qt::AlignCenter, QString::number(date.day()));

    // ── Event bars (tiny, at very bottom of cell) ─────────────────────────
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

    const int n      = colors.size();
    const int barH   = 2;
    const int gap    = 2;
    const int padX   = 5;
    const int totalW = cell.width() - padX * 2;
    const int segW   = qMax(3, (totalW - gap * (n - 1)) / n);
    const int startX = cell.left() + padX;
    const int y      = cell.bottom() - 3;  // at very bottom edge of cell

    p.setPen(Qt::NoPen);
    for (int i = 0; i < n; ++i) {
        QColor bar = colors[i];
        bar.setAlpha(darkMode ? 210 : 180);
        p.setBrush(bar);
        p.drawRoundedRect(QRectF(startX + i * (segW + gap), y, segW, barH), 1.0, 1.0);
    }
}

void CalendarWidget::drawEventChips(QPainter&, const QRect&, const QDate&) const {
    // Chips removed for cleaner minimal calendar design.
    // Event presence is indicated by dots below the day number.
}
