#include "WeekTimelineWidget.h"
#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QResizeEvent>
#include <QTime>
#include <QTimer>
#include <algorithm>

// ============================================================================
// WeekCanvas — the actual painting surface inside the scroll area
// ============================================================================

class WeekTimelineWidget::WeekCanvas : public QWidget {
public:
    static constexpr int START_HOUR    = 0;
    static constexpr int END_HOUR      = 24;
    static constexpr int HOUR_HEIGHT   = 64;
    static constexpr int TIME_COL_WIDTH= 56;
    static constexpr int HEADER_HEIGHT = 40;
    static constexpr int BLOCK_INSET   = 3;

    struct VisualBlock {
        QRectF   rect;
        QColor   color;
        QString  title;
        QString  timeLabel;
        int      eventIndex = -1;
        int      col = 0;       // 0-6
    };

    WeekCanvas(WeekTimelineWidget* owner, QWidget* parent)
        : QWidget(parent), m_owner(owner)
    {
        setMinimumHeight(HEADER_HEIGHT + (END_HOUR - START_HOUR) * HOUR_HEIGHT + 20);
        setMouseTracking(true);
    }

    void setWeek(const QDate& monday, const QVector<Event>& events) {
        for (int i = 0; i < 7; ++i)
            m_days[i] = monday.addDays(i);
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
        const int totalCols = 7;
        const double colW = (w - TIME_COL_WIDTH) / double(totalCols);

        // Background
        p.fillRect(rect(), c.bg);

        // ── Column header row ─────────────────────────────────────────────
        p.setPen(Qt::NoPen);
        for (int col = 0; col < 7; ++col) {
            const QDate& d = m_days[col];
            if (!d.isValid()) continue;
            const bool isToday = (d == QDate::currentDate());
            const double x0 = TIME_COL_WIDTH + col * colW;

            if (isToday) {
                QColor todayHdr = c.accent;
                todayHdr.setAlpha(light ? 14 : 22);
                p.setBrush(todayHdr);
                p.drawRect(QRectF(x0, 0, colW, HEADER_HEIGHT));
            }
        }

        // Header separator line
        p.setPen(QPen(c.border, 0.5));
        p.drawLine(QPointF(0, HEADER_HEIGHT), QPointF(w, HEADER_HEIGHT));

        // Column vertical dividers (subtle)
        p.setPen(QPen(c.borderSubtle, 0.5));
        for (int col = 1; col < 7; ++col) {
            const double x = TIME_COL_WIDTH + col * colW;
            p.drawLine(QPointF(x, 0), QPointF(x, height()));
        }

        // Day name labels in header
        for (int col = 0; col < 7; ++col) {
            const QDate& d = m_days[col];
            if (!d.isValid()) continue;
            const bool isToday = (d == QDate::currentDate());
            const double x0 = TIME_COL_WIDTH + col * colW;

            // Day name
            QFont nameFont = Theme::Font::base();
            nameFont.setPointSizeF(9.5);
            nameFont.setWeight(QFont::Medium);
            p.setFont(nameFont);
            p.setPen(isToday ? c.accent : c.textTertiary);
            const QString dayAbbr = d.toString("ddd").toUpper();
            p.drawText(QRectF(x0, 4, colW, 16), Qt::AlignCenter, dayAbbr);

            // Day number
            QFont numFont = Theme::Font::base();
            numFont.setPointSizeF(isToday ? 14 : 12);
            numFont.setWeight(isToday ? QFont::Bold : QFont::DemiBold);
            p.setFont(numFont);
            if (isToday) {
                // Circle behind today's number
                const double cx = x0 + colW / 2.0;
                const double cy = HEADER_HEIGHT - 12;
                p.setPen(Qt::NoPen);
                p.setBrush(c.accent);
                p.drawEllipse(QPointF(cx, cy), 12, 12);
                p.setPen(c.accentText);
            } else {
                p.setPen(c.text);
            }
            p.drawText(QRectF(x0, 18, colW, 20), Qt::AlignCenter,
                       QString::number(d.day()));
        }

        // ── Hour lines + time labels ───────────────────────────────────────
        QFont timeFont = Theme::Font::base();
        timeFont.setPointSize(10);
        p.setFont(timeFont);

        const double yBase = HEADER_HEIGHT + 20;
        for (int h = START_HOUR; h <= END_HOUR; ++h) {
            const double y = yBase + (h - START_HOUR) * HOUR_HEIGHT;
            p.setPen(c.textTertiary);
            p.drawText(QRectF(0, y - 8, TIME_COL_WIDTH - 8, 16),
                       Qt::AlignRight | Qt::AlignVCenter,
                       h == 0 ? "" : QTime(h, 0).toString("h ap"));
            p.setPen(QPen(c.borderSubtle, 0.5));
            p.drawLine(QPointF(TIME_COL_WIDTH, y), QPointF(w, y));
        }

        // ── Current time indicator in today's column ───────────────────────
        {
            const QDate today = QDate::currentDate();
            int todayCol = -1;
            for (int i = 0; i < 7; ++i) {
                if (m_days[i] == today) { todayCol = i; break; }
            }
            if (todayCol >= 0) {
                QTime now = QTime::currentTime();
                const double nowY = yBase + now.hour() * HOUR_HEIGHT
                                    + now.minute() * HOUR_HEIGHT / 60.0;
                const double x0 = TIME_COL_WIDTH + todayCol * colW;
                QColor nowColor = c.danger;
                nowColor.setAlpha(220);
                p.setPen(QPen(nowColor, 1.5));
                p.drawLine(QPointF(x0, nowY), QPointF(x0 + colW, nowY));
                p.setPen(Qt::NoPen);
                p.setBrush(nowColor);
                p.drawEllipse(QPointF(x0 + 2, nowY), 3.5, 3.5);
            }
        }

        // ── Event blocks ──────────────────────────────────────────────────
        for (const auto& block : m_blocks) {
            const bool isHovered = (block.eventIndex == m_hoveredIndex);

            QPainterPath path;
            path.addRoundedRect(block.rect, Theme::Radius::M, Theme::Radius::M);

            // Shadow
            p.setPen(Qt::NoPen);
            QColor shadow = light ? QColor(0, 0, 0, 8) : QColor(0, 0, 0, 24);
            p.setBrush(shadow);
            p.drawRoundedRect(block.rect.translated(0, 1.5), Theme::Radius::M, Theme::Radius::M);

            // Fill
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
            QRectF accent(block.rect.left() + 1, block.rect.top() + 5,
                          3, block.rect.height() - 10);
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

            // Time sublabel for taller blocks
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
        if (!m_owner || e->pos().x() < TIME_COL_WIDTH) return;
        const double colW = (width() - TIME_COL_WIDTH) / 7.0;
        const int col = qBound(0, int((e->pos().x() - TIME_COL_WIDTH) / colW), 6);

        for (const auto& block : m_blocks) {
            if (block.rect.contains(e->pos()) && block.eventIndex >= 0) {
                emit m_owner->eventActivated(block.eventIndex);
                return;
            }
        }
        QTime t = yToTime(e->pos().y());
        if (t.isValid() && m_days[col].isValid())
            emit m_owner->emptySlotClicked(m_days[col], t);
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        int newHovered = -1;
        if (e->pos().x() >= TIME_COL_WIDTH) {
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
        if (m_hoveredIndex >= 0) {
            m_hoveredIndex = -1;
            setCursor(Qt::ArrowCursor);
            update();
        }
    }

    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        if (e->oldSize().width() != e->size().width())
            rebuild();
    }

    void showEvent(QShowEvent* e) override {
        QWidget::showEvent(e);
        QTimer::singleShot(0, this, [this] { rebuild(); });
    }

private:
    double yBase() const { return HEADER_HEIGHT + 20; }

    double timeToY(const QTime& t) const {
        return yBase() + t.hour() * HOUR_HEIGHT + t.minute() * HOUR_HEIGHT / 60.0;
    }

    QTime yToTime(int y) const {
        double hours = (y - yBase()) / HOUR_HEIGHT + START_HOUR;
        int h = qBound(0, int(hours), 23);
        int m = qBound(0, int((hours - h) * 60), 59);
        m = (m / 15) * 15;
        return QTime(h, m);
    }

    int colWidth() const {
        return width() > TIME_COL_WIDTH
               ? (width() - TIME_COL_WIDTH) / 7
               : 80;
    }

    void rebuild() {
        m_blocks.clear();
        m_hoveredIndex = -1;

        const double cw = (width() > TIME_COL_WIDTH)
                          ? (width() - TIME_COL_WIDTH) / 7.0
                          : 80.0;

        for (int i = 0; i < m_allEvents.size(); ++i) {
            const Event& ev = m_allEvents[i];
            for (int col = 0; col < 7; ++col) {
                if (!m_days[col].isValid() || !ev.isOnDate(m_days[col]))
                    continue;

                QTime startT = ev.getStartTime().time();
                QTime endT   = ev.getEndTime().time();
                if (endT == QTime(0, 0) || endT > QTime(23, 59)) endT = QTime(23, 59);
                if (startT >= endT) continue;

                const double y1 = timeToY(startT);
                const double y2 = timeToY(endT);
                const double bh = qMax(y2 - y1, 20.0);
                const double x  = TIME_COL_WIDTH + col * cw + BLOCK_INSET;

                QString cat = ev.getDescription();
                int sep = cat.indexOf("::");
                if (sep >= 0) cat = cat.left(sep);

                VisualBlock block;
                block.rect = QRectF(x, y1, cw - 2 * BLOCK_INSET, bh);
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
// WeekTimelineWidget (scroll area wrapper)
// ============================================================================

WeekTimelineWidget::WeekTimelineWidget(QWidget* parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_canvas = new WeekCanvas(this, this);
    setWidget(m_canvas);

    QTimer::singleShot(0, this, [this] {
        const int scrollTo = (7 - WeekCanvas::START_HOUR) * WeekCanvas::HOUR_HEIGHT;
        verticalScrollBar()->setValue(scrollTo);
    });
}

void WeekTimelineWidget::setWeekContaining(const QDate& anyDay) {
    const int dow = anyDay.dayOfWeek(); // 1=Mon
    m_weekStart = anyDay.addDays(1 - dow);
    m_canvas->setWeek(m_weekStart, m_allEvents);
}

void WeekTimelineWidget::setEvents(const QVector<Event>& events) {
    m_allEvents = events;
    if (m_weekStart.isValid())
        m_canvas->setWeek(m_weekStart, events);
}
