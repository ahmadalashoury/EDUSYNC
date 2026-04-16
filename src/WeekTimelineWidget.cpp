#include "WeekTimelineWidget.h"
#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QShowEvent>
#include <QResizeEvent>
#include <QTime>
#include <QTimer>
#include <algorithm>

// ============================================================================
// Constants shared between header and canvas
// ============================================================================
namespace Wk {
    static constexpr int START_HOUR     = 0;
    static constexpr int END_HOUR       = 24;
    static constexpr int HOUR_HEIGHT    = 64;
    static constexpr int TIME_COL_WIDTH = 56;
    static constexpr int HEADER_HEIGHT  = 52;   // sticky header height
    static constexpr int BLOCK_INSET    = 3;
}

// ============================================================================
// WeekHeader — sticky header (never scrolls)
// ============================================================================

class WeekTimelineWidget::WeekHeader : public QWidget {
public:
    explicit WeekHeader(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(Wk::HEADER_HEIGHT);
    }

    void setDays(const QDate days[7]) {
        for (int i = 0; i < 7; ++i) m_days[i] = days[i];
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const bool light = palette().color(QPalette::Window).lightness() > 127;
        const auto c = light ? Theme::light() : Theme::dark();
        const int w = width();
        const double colW = (w - Wk::TIME_COL_WIDTH) / 7.0;

        // Background
        p.fillRect(rect(), light ? QColor("#f0ede8") : QColor("#09111e"));

        // Column vertical dividers
        p.setPen(QPen(c.borderSubtle, 0.5));
        for (int col = 1; col < 7; ++col) {
            const double x = Wk::TIME_COL_WIDTH + col * colW;
            p.drawLine(QPointF(x, 0), QPointF(x, Wk::HEADER_HEIGHT));
        }

        // Today column highlight
        p.setPen(Qt::NoPen);
        for (int col = 0; col < 7; ++col) {
            const QDate& d = m_days[col];
            if (!d.isValid() || d != QDate::currentDate()) continue;
            QColor todayHdr = c.accent;
            todayHdr.setAlpha(light ? 12 : 20);
            p.setBrush(todayHdr);
            p.drawRect(QRectF(Wk::TIME_COL_WIDTH + col * colW, 0, colW, Wk::HEADER_HEIGHT));
        }

        for (int col = 0; col < 7; ++col) {
            const QDate& d = m_days[col];
            if (!d.isValid()) continue;
            const bool isToday = (d == QDate::currentDate());
            const double x0 = Wk::TIME_COL_WIDTH + col * colW;

            // Day name (MON, TUE…)
            QFont nameFont = Theme::Font::base();
            nameFont.setPointSizeF(9.0);
            nameFont.setWeight(QFont::Medium);
            p.setFont(nameFont);
            p.setPen(isToday ? c.accent : c.textTertiary);
            p.drawText(QRectF(x0, 6, colW, 14), Qt::AlignCenter,
                       d.toString("ddd").toUpper());

            // Day number badge
            const double cx = x0 + colW / 2.0;
            const double cy = Wk::HEADER_HEIGHT - 14;

            if (isToday) {
                p.setPen(Qt::NoPen);
                p.setBrush(c.accent);
                p.drawEllipse(QPointF(cx, cy), 13, 13);
            }

            QFont numFont = Theme::Font::base();
            numFont.setPointSizeF(isToday ? 14.0 : 12.0);
            numFont.setWeight(isToday ? QFont::Bold : QFont::DemiBold);
            p.setFont(numFont);
            p.setPen(isToday ? c.accentText : c.text);
            p.drawText(QRectF(x0, cy - 13, colW, 26), Qt::AlignCenter,
                       QString::number(d.day()));
        }

        // Bottom separator
        p.setPen(QPen(c.border, 0.5));
        p.drawLine(QPointF(0, Wk::HEADER_HEIGHT - 0.5),
                   QPointF(w, Wk::HEADER_HEIGHT - 0.5));
    }

private:
    QDate m_days[7] = {};
};

// ============================================================================
// WeekCanvas — scrollable painting surface (no header)
// ============================================================================

class WeekTimelineWidget::WeekCanvas : public QWidget {
public:
    struct VisualBlock {
        QRectF  rect;
        QColor  color;
        QString title;
        QString timeLabel;
        int     eventIndex = -1;
        int     col = 0;
    };

    WeekCanvas(WeekTimelineWidget* owner, QWidget* parent)
        : QWidget(parent), m_owner(owner)
    {
        setMinimumHeight((Wk::END_HOUR - Wk::START_HOUR) * Wk::HOUR_HEIGHT + 20);
        setMouseTracking(true);
    }

    void setWeek(const QDate& monday, const QVector<Event>& events) {
        for (int i = 0; i < 7; ++i) m_days[i] = monday.addDays(i);
        m_allEvents = events;
        rebuild();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const bool light = palette().color(QPalette::Window).lightness() > 127;
        const auto c = light ? Theme::light() : Theme::dark();
        const int w = width();
        const double colW = (w - Wk::TIME_COL_WIDTH) / 7.0;

        p.fillRect(rect(), c.bg);

        // Today column tint
        p.setPen(Qt::NoPen);
        for (int col = 0; col < 7; ++col) {
            if (!m_days[col].isValid() || m_days[col] != QDate::currentDate()) continue;
            QColor todayBg = c.accent;
            todayBg.setAlpha(light ? 6 : 10);
            p.setBrush(todayBg);
            p.drawRect(QRectF(Wk::TIME_COL_WIDTH + col * colW, 0, colW, height()));
        }

        // Column vertical dividers
        p.setPen(QPen(c.borderSubtle, 0.5));
        for (int col = 1; col < 7; ++col) {
            const double x = Wk::TIME_COL_WIDTH + col * colW;
            p.drawLine(QPointF(x, 0), QPointF(x, height()));
        }

        // Hour lines + time labels
        QFont timeFont = Theme::Font::base();
        timeFont.setPointSize(10);
        p.setFont(timeFont);

        const double yOff = 20.0;
        for (int h = Wk::START_HOUR; h <= Wk::END_HOUR; ++h) {
            const double y = yOff + (h - Wk::START_HOUR) * Wk::HOUR_HEIGHT;
            p.setPen(c.textTertiary);
            p.drawText(QRectF(0, y - 8, Wk::TIME_COL_WIDTH - 8, 16),
                       Qt::AlignRight | Qt::AlignVCenter,
                       h == 0 ? "" : QTime(h, 0).toString("h ap"));
            p.setPen(QPen(c.borderSubtle, 0.5));
            p.drawLine(QPointF(Wk::TIME_COL_WIDTH, y), QPointF(w, y));
        }

        // Current-time line in today's column
        {
            const QDate today = QDate::currentDate();
            int todayCol = -1;
            for (int i = 0; i < 7; ++i)
                if (m_days[i] == today) { todayCol = i; break; }

            if (todayCol >= 0) {
                const QTime now = QTime::currentTime();
                const double nowY = yOff + now.hour() * Wk::HOUR_HEIGHT
                                    + now.minute() * Wk::HOUR_HEIGHT / 60.0;
                const double x0 = Wk::TIME_COL_WIDTH + todayCol * colW;
                QColor nowColor = c.danger;
                nowColor.setAlpha(220);
                p.setPen(QPen(nowColor, 1.5));
                p.drawLine(QPointF(x0, nowY), QPointF(x0 + colW, nowY));
                p.setPen(Qt::NoPen);
                p.setBrush(nowColor);
                p.drawEllipse(QPointF(x0 + 2, nowY), 3.5, 3.5);
            }
        }

        // Event blocks
        for (const auto& block : m_blocks) {
            const bool isHovered = (block.eventIndex == m_hoveredIndex);

            QPainterPath path;
            path.addRoundedRect(block.rect, Theme::Radius::M, Theme::Radius::M);

            // Shadow
            p.setPen(Qt::NoPen);
            p.setBrush(light ? QColor(0,0,0,8) : QColor(0,0,0,24));
            p.drawRoundedRect(block.rect.translated(0,1.5), Theme::Radius::M, Theme::Radius::M);

            // Surface fill + color tint
            p.setBrush(c.surfaceRaised);
            p.setPen(Qt::NoPen);
            p.drawPath(path);
            QColor tint = block.color;
            tint.setAlpha(light ? (isHovered ? 40 : 24) : (isHovered ? 52 : 36));
            p.setBrush(tint);
            p.drawPath(path);

            // Border
            p.setPen(QPen(c.borderSubtle, 0.7));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);

            // Left accent bar
            QRectF accent(block.rect.left()+1, block.rect.top()+5, 3, block.rect.height()-10);
            p.setPen(Qt::NoPen);
            p.setBrush(block.color);
            p.drawRoundedRect(accent, 1.5, 1.5);

            // Title
            if (block.rect.height() >= 18) {
                p.setPen(c.text);
                QFont tf = Theme::Font::base();
                tf.setWeight(QFont::DemiBold);
                tf.setPointSizeF(10.0);
                p.setFont(tf);
                QRectF textRect = block.rect.adjusted(10, 5, -4, -4);
                p.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                           p.fontMetrics().elidedText(block.title, Qt::ElideRight,
                                                      int(textRect.width())));
            }

            // Time sublabel
            if (block.rect.height() > 44) {
                p.setPen(c.textSecondary);
                QFont sf = Theme::Font::base();
                sf.setPointSizeF(9.0);
                p.setFont(sf);
                p.drawText(block.rect.adjusted(10, 22, -4, -4),
                           Qt::AlignLeft | Qt::AlignTop, block.timeLabel);
            }
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (!m_owner || e->pos().x() < Wk::TIME_COL_WIDTH) return;
        const double colW = (width() - Wk::TIME_COL_WIDTH) / 7.0;
        const int col = qBound(0, int((e->pos().x() - Wk::TIME_COL_WIDTH) / colW), 6);

        for (const auto& block : m_blocks) {
            if (block.rect.contains(e->pos()) && block.eventIndex >= 0) {
                emit m_owner->eventActivated(block.eventIndex);
                return;
            }
        }
        const QTime t = yToTime(e->pos().y());
        if (t.isValid() && m_days[col].isValid())
            emit m_owner->emptySlotClicked(m_days[col], t);
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        int newHovered = -1;
        if (e->pos().x() >= Wk::TIME_COL_WIDTH) {
            for (const auto& block : m_blocks) {
                if (block.rect.contains(e->pos()) && block.eventIndex >= 0) {
                    newHovered = block.eventIndex;
                    break;
                }
            }
        }
        if (newHovered != m_hoveredIndex) {
            m_hoveredIndex = newHovered;
            setCursor(newHovered >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
    }

    void leaveEvent(QEvent*) override {
        if (m_hoveredIndex >= 0) { m_hoveredIndex = -1; update(); }
        setCursor(Qt::ArrowCursor);
    }

    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        if (e->oldSize().width() != e->size().width()) rebuild();
    }

    void showEvent(QShowEvent* e) override {
        QWidget::showEvent(e);
        QTimer::singleShot(0, this, [this] { rebuild(); });
    }

private:
    double timeToY(const QTime& t) const {
        return 20.0 + t.hour() * Wk::HOUR_HEIGHT + t.minute() * Wk::HOUR_HEIGHT / 60.0;
    }

    QTime yToTime(int y) const {
        double hours = (y - 20.0) / Wk::HOUR_HEIGHT + Wk::START_HOUR;
        int h = qBound(0, int(hours), 23);
        int m = qBound(0, int((hours - h) * 60), 59);
        m = (m / 15) * 15;
        return QTime(h, m);
    }

    void rebuild() {
        m_blocks.clear();
        m_hoveredIndex = -1;

        const double cw = (width() > Wk::TIME_COL_WIDTH)
                          ? (width() - Wk::TIME_COL_WIDTH) / 7.0
                          : 80.0;

        for (int i = 0; i < m_allEvents.size(); ++i) {
            const Event& ev = m_allEvents[i];
            for (int col = 0; col < 7; ++col) {
                if (!m_days[col].isValid() || !ev.isOnDate(m_days[col])) continue;

                QTime startT = ev.getStartTime().time();
                QTime endT   = ev.getEndTime().time();
                if (endT == QTime(0,0) || endT > QTime(23,59)) endT = QTime(23,59);
                if (startT >= endT) continue;

                const double y1 = timeToY(startT);
                const double y2 = timeToY(endT);
                const double bh = qMax(y2 - y1, 20.0);
                const double x  = Wk::TIME_COL_WIDTH + col * cw + Wk::BLOCK_INSET;

                QString cat = ev.getDescription();
                int sep = cat.indexOf("::");
                if (sep >= 0) cat = cat.left(sep);

                VisualBlock block;
                block.rect = QRectF(x, y1, cw - 2*Wk::BLOCK_INSET, bh);
                block.color = ev.getColor().isValid() ? ev.getColor()
                                                      : Theme::Category::color(cat);
                block.title = ev.getTitle();
                block.timeLabel = QString("%1–%2")
                    .arg(ev.getStartTime().time().toString("h:mm"),
                         ev.getEndTime().time().toString("h:mm ap"));
                block.eventIndex = i;
                block.col = col;
                m_blocks.push_back(block);
            }
        }
        update();
    }

    WeekTimelineWidget* m_owner = nullptr;
    QDate m_days[7];
    QVector<Event> m_allEvents;
    QVector<VisualBlock> m_blocks;
    int m_hoveredIndex = -1;
};

// ============================================================================
// WeekTimelineWidget — sticky header + scrollable canvas
// ============================================================================

WeekTimelineWidget::WeekTimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Sticky header (never scrolls)
    m_header = new WeekHeader(this);
    layout->addWidget(m_header);

    // Scrollable canvas below the header
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setWidgetResizable(true);

    m_canvas = new WeekCanvas(this, m_scrollArea->viewport());
    m_scrollArea->setWidget(m_canvas);

    layout->addWidget(m_scrollArea, 1);

    // Scroll to ~7 am on startup
    QTimer::singleShot(0, this, [this] {
        m_scrollArea->verticalScrollBar()->setValue(7 * Wk::HOUR_HEIGHT);
    });
}

void WeekTimelineWidget::setWeekContaining(const QDate& anyDay) {
    const int dow = anyDay.dayOfWeek(); // 1=Mon
    m_weekStart = anyDay.addDays(1 - dow);

    QDate days[7];
    for (int i = 0; i < 7; ++i) days[i] = m_weekStart.addDays(i);
    m_header->setDays(days);
    m_canvas->setWeek(m_weekStart, m_allEvents);
}

void WeekTimelineWidget::setEvents(const QVector<Event>& events) {
    m_allEvents = events;
    if (m_weekStart.isValid())
        m_canvas->setWeek(m_weekStart, events);
}
