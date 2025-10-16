#include "ModernCalendarWidget.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QTextCharFormat>
#include <QShowEvent>
#include <QHeaderView>
#include <QTableView>
#include <QHoverEvent>
#include <QModelIndex>
#include <QCursor>

/*
 * Helper: keep weekend cell text consistent with weekdays (no special colors).
 * We also take a header pointer for symmetry with callers, but it’s not needed.
 */
static void applyWeekendAndHeaderColors(QCalendarWidget* cal,
                                        QHeaderView* hHeader,
                                        bool /*light*/)
{
    Q_UNUSED(hHeader);

    // Make Saturday/Sunday look like any other weekday in the grid.
    QTextCharFormat fmt;
    fmt.clearBackground();
    fmt.setForeground(cal->palette().color(QPalette::Text));
    cal->setWeekdayTextFormat(Qt::Saturday, fmt);
    cal->setWeekdayTextFormat(Qt::Sunday,   fmt);
}

/* ========================================================================== */
/*  Header styling (weekday row)                                              */
/* ========================================================================== */

void ModernCalendarWidget::ensureHeaderStyled() {
    if (!m_view)    m_view    = findChild<QTableView*>();
    if (m_view && !m_hHeader) m_hHeader = m_view->horizontalHeader();
    if (!m_hHeader) return;

    // Interactions/behavior
    m_hHeader->setSectionsClickable(false);
    m_hHeader->setHighlightSections(false);
    m_hHeader->setFocusPolicy(Qt::NoFocus);
    if (m_view) m_view->setFocusPolicy(Qt::StrongFocus);

    // Object names + structure hints (used by the stylesheet)
    m_hHeader->setObjectName("CalHeader");
    m_hHeader->setAttribute(Qt::WA_StyledBackground, true);
    if (auto *vp = m_hHeader->viewport()) vp->setAttribute(Qt::WA_StyledBackground, true);
    m_hHeader->setSectionResizeMode(QHeaderView::Stretch);
    m_hHeader->setDefaultAlignment(Qt::AlignCenter);

    // Fixed gray palette (works in both light/dark themes)
    const QColor bg  = m_light ? QColor("#f3f4f6") : QColor("#2a3036");
    const QColor fg  = m_light ? QColor("#6b7280") : QColor("#9aa3ab");
    const QColor brd = m_light ? QColor("#e5e7eb") : QColor("#2f3540");

    // (1) Model-level header roles — high priority over generic styles
    if (auto *model = m_view ? m_view->model() : nullptr) {
        for (int c = 0; c < model->columnCount(); ++c) {
            model->setHeaderData(c, Qt::Horizontal, QBrush(bg), Qt::BackgroundRole);
            model->setHeaderData(c, Qt::Horizontal, QBrush(fg), Qt::ForegroundRole);
            model->setHeaderData(c, Qt::Horizontal, Qt::AlignCenter, Qt::TextAlignmentRole);
        }
    }

    // (2) Palette enforcement for the header widget/viewport
    QPalette hp = m_hHeader->palette();
    hp.setColor(QPalette::Window,     bg);
    hp.setColor(QPalette::Base,       bg);
    hp.setColor(QPalette::Button,     bg);
    hp.setColor(QPalette::WindowText, fg);
    hp.setColor(QPalette::ButtonText, fg);
    hp.setColor(QPalette::Text,       fg);
    m_hHeader->setPalette(hp);
    m_hHeader->setAutoFillBackground(true);

    // (3) High-specificity stylesheet that wins against broad app styles
    m_hHeader->setProperty("weekdayHeader", true); // extra selector hook
    m_hHeader->setStyleSheet(QString(R"(
        /* Header container */
        QHeaderView#CalHeader { background:%1; border:0; }

        /* Ensure the painted viewport matches */
        QHeaderView#CalHeader QWidget { background:%1; }

        /* Section painting (use a chain to increase specificity) */
        QCalendarWidget QTableView QHeaderView#CalHeader::section,
        QHeaderView#CalHeader::section,
        QHeaderView[weekdayHeader="true"]::section {
            background:%1; color:%2; border:0;
            border-bottom:1px solid %3;
            padding:6px 0;
            font-weight:600;
            text-transform:uppercase;
            letter-spacing:.04em;
        }

        /* Neutralize hover/pressed/selected: always the same gray */
        QHeaderView#CalHeader::section:hover,
        QHeaderView#CalHeader::section:pressed,
        QHeaderView#CalHeader::section:selected {
            background:%1; color:%2; border:0; border-bottom:1px solid %3;
        }
    )").arg(bg.name(), fg.name(), brd.name()));

    // Keep weekends un-special in the grid
    QTextCharFormat fmt; fmt.clearBackground();
    fmt.setForeground(palette().color(QPalette::Text));
    setWeekdayTextFormat(Qt::Saturday, fmt);
    setWeekdayTextFormat(Qt::Sunday,   fmt);

    // Force repaint
    m_hHeader->update();
    if (auto *vp = m_hHeader->viewport()) vp->update();
    if (m_view) m_view->viewport()->update();
}

/* ========================================================================== */
/*  Construction & lifecycle                                                  */
/* ========================================================================== */

ModernCalendarWidget::ModernCalendarWidget(QWidget* parent)
    : QCalendarWidget(parent)
{
    // Get the internal table, header, and viewport; give them sensible defaults.
    m_view = findChild<QTableView*>();
    if (m_view) {
        m_view->setObjectName("CalView");
        m_view->setMouseTracking(true);
        m_view->setSelectionMode(QAbstractItemView::NoSelection); // we draw selection ourselves
        m_view->setStyleSheet(R"(
            QTableView::item:selected            { background: transparent; border: 0; }
            QTableView::item:active:selected     { background: transparent; border: 0; }
            QTableView::item:!active:selected    { background: transparent; border: 0; }
            QTableView::item:focus               { outline: 0; }
            QAbstractItemView::item              { background: transparent; }
        )");

        m_hHeader = m_view->horizontalHeader();
        if (m_hHeader) m_hHeader->setObjectName("CalHeader");

        m_viewport = m_view->viewport();
        if (m_viewport) {
            m_viewport->setMouseTracking(true);
            m_viewport->setAttribute(Qt::WA_Hover, true);
            m_viewport->installEventFilter(this);
        }
    }

    // Track our selected day so paintCell can render a custom selection.
    m_selected = selectedDate();
    connect(this, &QCalendarWidget::selectionChanged, this, [this]{
        m_selected = selectedDate();
        update();
    });

    // When the month page changes, Qt rebuilds internals; re-grab and restyle.
    connect(this, &QCalendarWidget::currentPageChanged, this, [this](int, int){
        m_view = findChild<QTableView*>();
        if (m_view) {
            m_view->setObjectName("CalView");
            m_view->setMouseTracking(true);
            m_view->setSelectionMode(QAbstractItemView::NoSelection);
            m_view->setStyleSheet(R"(
                QTableView::item:selected            { background: transparent; border: 0; }
                QTableView::item:active:selected     { background: transparent; border: 0; }
                QTableView::item:!active:selected    { background: transparent; border: 0; }
                QTableView::item:focus               { outline: 0; }
                QAbstractItemView::item              { background: transparent; }
            )");

            m_hHeader = m_view->horizontalHeader();
            if (m_hHeader) m_hHeader->setObjectName("CalHeader");

            m_viewport = m_view->viewport();
            if (m_viewport) {
                m_viewport->setMouseTracking(true);
                m_viewport->setAttribute(Qt::WA_Hover, true);
                m_viewport->installEventFilter(this);
            }
        }
        ensureHeaderStyled();                                   // keep weekday header gray
        applyWeekendAndHeaderColors(this, m_hHeader, m_light);  // keep weekends neutral
        update();
    });
}

void ModernCalendarWidget::showEvent(QShowEvent* e) {
    QCalendarWidget::showEvent(e);
    // Apply theme-aware header styling after the widget is shown.
    QTimer::singleShot(0, this, [this]{
        const bool light = palette().color(QPalette::Window).lightness() > 127;
        applyHeaderStyleForTheme(light);
    });
}

/* ========================================================================== */
/*  Event filtering: hover, move, enter/leave                                 */
/* ========================================================================== */

bool ModernCalendarWidget::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == m_viewport) {
        switch (ev->type()) {
        case QEvent::MouseMove: {
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
            updateHoveredFromPos(static_cast<QMouseEvent*>(ev)->position().toPoint());
#else
            updateHoveredFromPos(static_cast<QMouseEvent*>(ev)->pos());
#endif
            return false;
        }
        case QEvent::HoverMove: {
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
            updateHoveredFromPos(static_cast<QHoverEvent*>(ev)->position().toPoint());
#else
            updateHoveredFromPos(static_cast<QHoverEvent*>(ev)->pos());
#endif
            return false;
        }
        case QEvent::Enter: {
            updateHoveredFromPos(m_viewport->mapFromGlobal(QCursor::pos()));
            return false;
        }
        case QEvent::Leave:
        case QEvent::HoverLeave: {
            if (m_hovered.isValid()) {
                updateCell(m_hovered);   // repaint only the previously hovered cell
                m_hovered = QDate();
            }
            return false;
        }
        default: break;
        }
    }
    return QCalendarWidget::eventFilter(obj, ev);
}

/* ========================================================================== */
/*  Grid helpers                                                               */
/* ========================================================================== */

QDate ModernCalendarWidget::gridStartDate() const {
    const QDate first(yearShown(), monthShown(), 1);
    const int fdow = static_cast<int>(firstDayOfWeek());
    const int off  = (first.dayOfWeek() - fdow + 7) % 7;
    return first.addDays(-off);        // first date shown in the 6x7 grid
}

// Map a table cell to its real date using the cell's displayed day number.
// Works regardless of how Qt laid out the 6×7 grid.
static inline QDate dateForIndex(QTableView* view,
                                 const QModelIndex& idx,
                                 int shownYear,
                                 int shownMonth)
{
    if (!view || !idx.isValid()) return {};

    const QAbstractItemModel *model = view->model();
    const int day = model ? model->data(idx, Qt::DisplayRole).toInt() : 0;
    if (day <= 0) return {};

    const int r = idx.row();

    // Heuristic used by QCalendarWidget:
    // Top row with big day numbers → previous month
    if (r == 0 && day > 7) {
        QDate prev = QDate(shownYear, shownMonth, 1).addMonths(-1);
        return QDate(prev.year(), prev.month(), day);
    }
    // Bottom rows with small day numbers → next month
    if (r >= 4 && day <= 14) {
        QDate next = QDate(shownYear, shownMonth, 1).addMonths(1);
        return QDate(next.year(), next.month(), day);
    }
    // Otherwise it's the current month
    return QDate(shownYear, shownMonth, day);
}

void ModernCalendarWidget::updateHoveredFromPos(const QPoint& vp) {
    if (!m_view) return;

    const QModelIndex idx = m_view->indexAt(vp);
    QDate d;
    if (idx.isValid()) {
        d = dateForIndex(m_view, idx, yearShown(), monthShown());
    }

    if (d == m_hovered) return;

    if (m_hovered.isValid()) updateCell(m_hovered);
    m_hovered = d;
    if (m_hovered.isValid()) updateCell(m_hovered);
}

/* ========================================================================== */
/*  Public API                                                                 */
/* ========================================================================== */

void ModernCalendarWidget::setEvents(const QList<Event>& evs)
{
    m_events = evs;
    update();
}

void ModernCalendarWidget::applyHeaderStyleForTheme(bool light) {
    m_light = light;
    ensureHeaderStyled();
    applyWeekendAndHeaderColors(this, m_hHeader, m_light);
}

/* ========================================================================== */
/*  Base widget overrides                                                      */
/* ========================================================================== */

void ModernCalendarWidget::paintEvent(QPaintEvent* e)
{
    QCalendarWidget::paintEvent(e);
}

void ModernCalendarWidget::mousePressEvent(QMouseEvent* e) {
    QCalendarWidget::mousePressEvent(e);
    m_selected = selectedDate();
    update();
}

void ModernCalendarWidget::wheelEvent(QWheelEvent* e) {
    // Horizontal trackpads sometimes report X, guard by using dominant axis.
    if (!m_month.isValid()) m_month = QDate(yearShown(), monthShown(), 1);

    const auto delta = e->angleDelta();
    const int dy = qAbs(delta.y()) >= qAbs(delta.x()) ? delta.y() : 0;
    if (dy > 0)      setCurrentMonth(m_month.addMonths(-1));
    else if (dy < 0) setCurrentMonth(m_month.addMonths(+1));
    e->accept();
}

void ModernCalendarWidget::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Left:    setSelectedDate(selectedDate().addDays(-1)); break;
    case Qt::Key_Right:   setSelectedDate(selectedDate().addDays(+1)); break;
    case Qt::Key_Up:      setSelectedDate(selectedDate().addDays(-7)); break;
    case Qt::Key_Down:    setSelectedDate(selectedDate().addDays(+7)); break;
    case Qt::Key_PageUp:  setCurrentMonth(QDate(yearShown(), monthShown(), 1).addMonths(-1)); break;
    case Qt::Key_PageDown:setCurrentMonth(QDate(yearShown(), monthShown(), 1).addMonths(+1)); break;
    default:
        QCalendarWidget::keyPressEvent(e);
        return;
    }
    m_selected = selectedDate();
    update();
}

void ModernCalendarWidget::setCurrentMonth(const QDate& anyDayInMonth) {
    m_month = QDate(anyDayInMonth.year(), anyDayInMonth.month(), 1);
    setCurrentPage(m_month.year(), m_month.month());
    update();
    emit monthChanged(m_month);
}

/*  ✅ Correct override signature for Qt 5/6:
 *     void paintCell(QPainter*, const QRect&, const QDate&) const override;
 */
void ModernCalendarWidget::paintCell(QPainter* p, const QRect& rect, QDate date) const
{
    p->save();

    p->setPen(Qt::NoPen);
    p->setBrush(Qt::NoBrush);

    const bool inMonth = (date.month() == monthShown() && date.year() == yearShown());
    const bool light   = palette().color(QPalette::Window).lightness() > 127;

    // Custom selected-day background (we disable native selection elsewhere)
    if (date == m_selected) {
        const QColor base = m_view ? m_view->palette().color(QPalette::Base)
                                   : palette().color(QPalette::Base);
        p->fillRect(rect, base);
        QRect sel = rect.adjusted(3,3,-3,-3);
        p->setBrush(light ? QColor(0,0,0,16) : QColor(255,255,255,28));
        p->drawRoundedRect(sel, 6, 6);
    }

    // Hover highlight
    if (date == m_hovered) {
        p->setBrush(light ? QColor(0,0,0,18) : QColor(255,255,255,40));
        p->drawRoundedRect(rect.adjusted(2,2,-2,-2), 6, 6);
    }

    // Thin “today” ring
    if (date == QDate::currentDate()) {
        QPen ring(light ? QColor(0,0,0,35) : QColor(255,255,255,60));
        ring.setWidth(1);
        p->setPen(ring); p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(rect.adjusted(2,2,-2,-2), 6, 6);
        p->setPen(Qt::NoPen);
    }

    // Day number in the top-left; dim spillover days
    QColor dayFg = palette().color(QPalette::Text);
    if (!inMonth) dayFg = QColor(light ? "#c7c9ce" : "#5a6168");
    p->setPen(dayFg);
    QFont f = p->font(); f.setBold(true); p->setFont(f);
    p->drawText(rect.adjusted(10, 6, -10, -6),
                Qt::AlignLeft | Qt::AlignTop,
                QString::number(date.day()));

    // Event glyphs/chips (only for current-month cells to avoid clutter)
    if (inMonth) {
        drawEventsDots(*p, rect, date);
        drawEventChips(*p, rect, date);
    }

    p->restore();
}

/* ========================================================================== */
/*  Per-cell event adornments                                                  */
/* ========================================================================== */

void ModernCalendarWidget::drawEventsDots(QPainter& p, const QRect& cell, const QDate& d) const {
    int count = 0;
    for (const Event& e : m_events)
        if (e.isOnDate(d)) ++count;
    if (!count) return;

    const int dotR = 3;
    const int gap  = 6;
    const int totalW = count*dotR*2 + (count-1)*gap;
    int x = cell.center().x() - totalW/2;
    int y = cell.top() + 26;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(180,170,255));
    for (int i=0;i<count;++i)
        p.drawEllipse(QPoint(x + i*(2*dotR+gap), y), dotR, dotR);
}

void ModernCalendarWidget::drawEventChips(QPainter& p, const QRect& cell, const QDate& d) const {
    QStringList titles;
    for (const Event& e : m_events)
        if (e.isOnDate(d)) titles << e.getTitle();
    if (titles.isEmpty()) return;

    const int maxChips = qMin(2, titles.size());
    const int chipH = 18;
    int y = cell.bottom() - 6 - maxChips * (chipH + 4);

    QFont f = p.font();
    f.setBold(false);
    p.setFont(f);

    for (int i = 0; i < maxChips; ++i) {
        QRect r = cell.adjusted(6, y - cell.top(), -6, 0);
        r.setHeight(chipH);

        QFontMetrics fm(p.font());
        const QString txt = fm.elidedText(titles[i], Qt::ElideRight, r.width() - 12); // 6px padding each side

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(140, 70, 255));
        p.drawRoundedRect(r, 6, 6);

        p.setPen(QColor(250, 250, 255));
        p.drawText(r.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, txt);

        y += chipH + 4;
    }
}

/* ========================================================================== */
/*  Restyle throttle + resize                                                  */
/* ========================================================================== */

void ModernCalendarWidget::scheduleRestyle() {
    if (!m_restyleTimer) {
        m_restyleTimer = new QTimer(this);
        m_restyleTimer->setSingleShot(true);
        connect(m_restyleTimer, &QTimer::timeout, this, &ModernCalendarWidget::restyleNow);
    }
    // Coalesce multiple calls into one tick.
    m_restyleTimer->start(0);
}

void ModernCalendarWidget::restyleNow() {
    ensureHeaderStyled();
    applyWeekendAndHeaderColors(this, m_hHeader, m_light);
    update();
}

void ModernCalendarWidget::resizeEvent(QResizeEvent* e) {
    QCalendarWidget::resizeEvent(e);
    scheduleRestyle();
}

/* ========================================================================== */
/*  Mapping helpers                                                            */
/* ========================================================================== */

QModelIndex ModernCalendarWidget::indexForDate(const QDate& d) const {
    if (!m_view || !m_view->model()) return {};
    const QDate start = gridStartDate();
    const int days = start.daysTo(d);
    if (days < 0 || days >= 42) return {};
    return m_view->model()->index(days / 7, days % 7);
}
