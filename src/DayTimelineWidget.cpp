#include "DayTimelineWidget.h"
#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QTime>
#include <QTimer>
#include <QLinearGradient>
#include <QRadialGradient>
#include <algorithm>

// ============================================================================
// Internal canvas that does the actual painting
// ============================================================================

class DayTimelineWidget::TimelineCanvas : public QWidget {
public:
    static constexpr int START_HOUR = 0;
    static constexpr int END_HOUR   = 24;
    static constexpr int HOUR_HEIGHT = 64;
    static constexpr int TIME_COL_WIDTH = 56;
    static constexpr int LEFT_MARGIN = 8;

    struct VisualBlock {
        QRectF rect;
        QColor color;
        QString title;
        QString timeLabel;
        int eventIndex = -1;
        QDateTime originalStart;
        QDateTime originalEnd;
    };

    TimelineCanvas(DayTimelineWidget* owner, QWidget* parent)
        : QWidget(parent), m_owner(owner) {
        setMinimumHeight((END_HOUR - START_HOUR) * HOUR_HEIGHT + 40);
        setMouseTracking(true);
        setCursor(Qt::ArrowCursor);
    }

    void setDate(const QDate& d) { m_date = d; rebuild(); }
    void setEvents(const QVector<Event>& all) { m_allEvents = all; rebuild(); }
    void setSelectedIndex(int idx) { m_selectedIndex = idx; update(); }
    int  selectedIndex() const { return m_selectedIndex; }

    // Drag state
    bool    m_dragging = false;
    int     m_dragBlockIndex = -1;
    double  m_dragOffsetY = 0;      // click Y within block
    double  m_dragCurrentY = 0;     // current drag Y (top of block)

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const bool light = palette().color(QPalette::Window).lightness() > 127;
        const auto c = light ? Theme::light() : Theme::dark();
        const bool darkMode = !light;

        const int w = width();
        const int contentLeft = TIME_COL_WIDTH + LEFT_MARGIN;
        const QRectF contentRect(contentLeft, 0, w - contentLeft, height());

        // Background
        if (darkMode) {
            QLinearGradient bg(rect().topLeft(), rect().bottomLeft());
            bg.setColorAt(0.0, QColor("#0b0f17"));
            bg.setColorAt(0.45, QColor("#111725"));
            bg.setColorAt(1.0, QColor("#090d14"));
            p.fillRect(rect(), bg);

            QLinearGradient columnGlow(contentRect.topLeft(), contentRect.topRight());
            columnGlow.setColorAt(0.0, QColor(58, 170, 255, 20));
            columnGlow.setColorAt(0.2, QColor(18, 30, 46, 0));
            columnGlow.setColorAt(1.0, QColor(18, 30, 46, 0));
            p.fillRect(contentRect, columnGlow);
        } else {
            p.fillRect(rect(), c.bg);
        }

        // Hour lines and labels
        QFont timeFont = Theme::Font::base();
        timeFont.setPointSize(10);
        p.setFont(timeFont);

        for (int h = START_HOUR; h <= END_HOUR; ++h) {
            double y = (h - START_HOUR) * HOUR_HEIGHT + 20;

            p.setPen(darkMode ? QColor("#6a7487") : c.textTertiary);
            QString label = QTime(h, 0).toString("h ap");
            p.drawText(QRectF(0, y - 8, TIME_COL_WIDTH - 8, 16),
                       Qt::AlignRight | Qt::AlignVCenter, label);

            QColor lineColor = darkMode ? QColor(94, 217, 255, h % 6 == 0 ? 34 : 18) : c.borderSubtle;
            p.setPen(QPen(lineColor, darkMode ? 0.8 : 0.5));
            p.drawLine(QPointF(contentLeft, y), QPointF(w, y));
        }

        // Current time indicator
        if (m_date == QDate::currentDate()) {
            QTime now = QTime::currentTime();
            if (now.hour() >= START_HOUR && now.hour() < END_HOUR) {
                double nowY = timeToY(now);
                QColor nowColor = darkMode ? QColor("#74e4ff") : c.danger;
                nowColor.setAlpha(220);
                if (darkMode) {
                    QLinearGradient beam(contentLeft - 8, nowY, w, nowY);
                    beam.setColorAt(0.0, QColor(116, 228, 255, 180));
                    beam.setColorAt(0.5, QColor(116, 228, 255, 34));
                    beam.setColorAt(1.0, QColor(116, 228, 255, 0));
                    p.setPen(QPen(QBrush(beam), 1.4));
                } else {
                    p.setPen(QPen(nowColor, 1.2));
                }
                p.drawLine(QPointF(contentLeft - 6, nowY), QPointF(w, nowY));
                p.setBrush(nowColor);
                p.setPen(Qt::NoPen);
                if (darkMode) {
                    QRadialGradient beacon(QPointF(contentLeft - 6, nowY), 9.0);
                    beacon.setColorAt(0.0, QColor(201, 247, 255, 255));
                    beacon.setColorAt(0.28, QColor(116, 228, 255, 200));
                    beacon.setColorAt(1.0, QColor(116, 228, 255, 0));
                    p.setBrush(beacon);
                    p.drawEllipse(QRectF(contentLeft - 15, nowY - 9, 18, 18));
                }
                p.setBrush(nowColor);
                p.drawEllipse(QPointF(contentLeft - 6, nowY), darkMode ? 4.5 : 3.5, darkMode ? 4.5 : 3.5);
            }
        }

        // Event blocks
        for (int bi = 0; bi < m_blocks.size(); ++bi) {
            const auto& block = m_blocks[bi];
            const bool isSelected = (block.eventIndex >= 0 && block.eventIndex == m_selectedIndex);
            const bool isHovered = (block.eventIndex >= 0 && block.eventIndex == m_hoveredIndex);
            // Skip the block being dragged (we'll draw it separately at its drag position)
            if (m_dragging && bi == m_dragBlockIndex) continue;

            QPainterPath path;
            path.addRoundedRect(block.rect, Theme::Radius::M, Theme::Radius::M);

            QColor shadow = light ? QColor(0, 0, 0, 10) : QColor(0, 0, 0, 34);
            p.setPen(Qt::NoPen);
            p.setBrush(shadow);
            p.drawRoundedRect(block.rect.translated(0, 2), Theme::Radius::L, Theme::Radius::L);

            // Block background
            QColor fill = darkMode ? QColor(17, 24, 37, 220) : c.surfaceRaised;
            QColor tint = block.color;
            tint.setAlpha(light ? 26 : 36);
            if (isHovered && !isSelected) tint.setAlpha(light ? 42 : 50);
            if (isSelected) tint.setAlpha(light ? 60 : 68);
            p.setPen(Qt::NoPen);
            if (darkMode) {
                QLinearGradient panel(block.rect.topLeft(), block.rect.bottomLeft());
                panel.setColorAt(0.0, QColor(255, 255, 255, 14));
                panel.setColorAt(0.15, fill);
                panel.setColorAt(1.0, QColor(10, 15, 23, 228));
                p.setBrush(panel);
            } else {
                p.setBrush(fill);
            }
            p.drawPath(path);
            p.setBrush(tint);
            p.drawPath(path);

            p.setPen(QPen(isSelected ? block.color.lighter(darkMode ? 130 : 100)
                                     : (darkMode ? QColor(111, 226, 255, 42) : c.borderSubtle),
                          isSelected ? 1.4 : 0.8));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);

            if (darkMode) {
                QColor edgeGlow = block.color;
                edgeGlow.setAlpha(isSelected ? 72 : 38);
                p.setPen(QPen(edgeGlow, isSelected ? 1.2 : 0.8));
                p.drawLine(QPointF(block.rect.left() + 12, block.rect.top() + 1),
                           QPointF(block.rect.right() - 12, block.rect.top() + 1));
            }

            // Left accent bar
            QRectF accent(block.rect.left() + 1, block.rect.top() + 6,
                          4, block.rect.height() - 12);
            p.setPen(Qt::NoPen);
            if (darkMode) {
                QLinearGradient accentFill(accent.topLeft(), accent.bottomLeft());
                accentFill.setColorAt(0.0, block.color.lighter(140));
                accentFill.setColorAt(1.0, block.color);
                p.setBrush(accentFill);
            } else {
                p.setBrush(block.color);
            }
            p.drawRoundedRect(accent, 2.0, 2.0);

            // Title
            p.setPen(darkMode ? QColor("#edf7ff") : c.text);
            QFont titleFont = Theme::Font::base();
            titleFont.setWeight(QFont::DemiBold);
            p.setFont(titleFont);
            QRectF textRect = block.rect.adjusted(16, 8, -10, 0);
            p.drawText(textRect, Qt::AlignLeft | Qt::AlignTop,
                       p.fontMetrics().elidedText(block.title, Qt::ElideRight,
                                                  int(textRect.width())));

            // Time sublabel
            if (block.rect.height() > 36) {
                p.setPen(darkMode ? QColor("#86a9c7") : c.textSecondary);
                QFont subFont = Theme::Font::base();
                subFont.setPointSize(10);
                p.setFont(subFont);
                QRectF subRect = block.rect.adjusted(16, 28, -10, -4);
                p.drawText(subRect, Qt::AlignLeft | Qt::AlignTop, block.timeLabel);
            }
        }

        // Drag preview
        if (m_dragging && m_dragBlockIndex >= 0 && m_dragBlockIndex < m_blocks.size()) {
            const auto& block = m_blocks[m_dragBlockIndex];
            QRectF preview(block.rect.left(), m_dragCurrentY, block.rect.width(), block.rect.height());
            preview.setBottom(qMin(preview.bottom(), (double)((END_HOUR - START_HOUR) * HOUR_HEIGHT + 20)));

            QPainterPath previewPath;
            previewPath.addRoundedRect(preview, Theme::Radius::M, Theme::Radius::M);
            QColor previewFill = block.color;
            previewFill.setAlpha(light ? 50 : 65);
            p.setPen(QPen(block.color.lighter(darkMode ? 130 : 100), 1.5, Qt::DashLine));
            p.setBrush(previewFill);
            p.drawPath(previewPath);

            // Time label showing where it will land
            QTime newTime = yToTime(m_dragCurrentY);
            p.setPen(darkMode ? QColor("#edf7ff") : c.text);
            QFont tf = Theme::Font::base();
            tf.setPointSize(10);
            tf.setWeight(QFont::DemiBold);
            p.setFont(tf);
            p.drawText(preview.adjusted(12, 4, -4, -4), Qt::AlignLeft | Qt::AlignTop,
                       newTime.toString("h:mm ap"));
        }

        // "No events" message
        if (m_blocks.isEmpty() && m_date.isValid()) {
            p.setPen(darkMode ? QColor("#617086") : c.textTertiary);
            p.setFont(Theme::Font::base());
            const int contentWidth = w - contentLeft - Theme::Space::M;
            double centerY = (END_HOUR - START_HOUR) * HOUR_HEIGHT / 2.0 + 20;
            p.drawText(QRectF(contentLeft, centerY - 20, contentWidth, 40),
                       Qt::AlignCenter, "No events scheduled");
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (!m_owner) return;
        const int contentLeft = TIME_COL_WIDTH + LEFT_MARGIN;
        if (e->pos().x() < contentLeft) return;

        for (int bi = 0; bi < m_blocks.size(); ++bi) {
            const auto& block = m_blocks[bi];
            if (block.rect.contains(e->pos()) && block.eventIndex >= 0) {
                m_selectedIndex = block.eventIndex;
                m_owner->m_selectedIndex = block.eventIndex;
                // Begin potential drag
                m_dragging = false;
                m_dragBlockIndex = bi;
                m_dragOffsetY = e->pos().y() - block.rect.top();
                m_dragCurrentY = block.rect.top();
                update();
                emit m_owner->eventSelected(block.eventIndex);
                return;
            }
        }
        // Clicked empty space
        m_dragBlockIndex = -1;
        m_selectedIndex = -1;
        m_owner->m_selectedIndex = -1;
        update();
        emit m_owner->selectionCleared();

        QTime clickedTime = yToTime(e->pos().y());
        if (clickedTime.isValid())
            emit m_owner->emptySlotClicked(clickedTime);
    }

    void mouseDoubleClickEvent(QMouseEvent* e) override {
        if (!m_owner) return;
        const int contentLeft = TIME_COL_WIDTH + LEFT_MARGIN;

        if (e->pos().x() >= contentLeft) {
            for (const auto& block : m_blocks) {
                if (block.rect.contains(e->pos()) && block.eventIndex >= 0) {
                    emit m_owner->eventActivated(block.eventIndex);
                    return;
                }
            }
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        const int contentLeft = TIME_COL_WIDTH + LEFT_MARGIN;

        // Drag handling
        if (m_dragBlockIndex >= 0 && (e->buttons() & Qt::LeftButton)) {
            const double newTop = e->pos().y() - m_dragOffsetY;
            const double minY = 20.0;
            const double maxY = (END_HOUR - START_HOUR) * HOUR_HEIGHT + 20
                                - m_blocks[m_dragBlockIndex].rect.height();
            m_dragCurrentY = qBound(minY, newTop, maxY);
            m_dragging = true;
            setCursor(Qt::ClosedHandCursor);
            update();
            return;
        }

        int newHovered = -1;
        if (e->pos().x() >= contentLeft) {
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

    void mouseReleaseEvent(QMouseEvent* e) override {
        if (m_dragging && m_dragBlockIndex >= 0 && m_dragBlockIndex < m_blocks.size() && m_owner) {
            const auto& block = m_blocks[m_dragBlockIndex];
            QTime newStartTime = yToTime(m_dragCurrentY);
            // Snap to 15-min grid
            int totalMin = newStartTime.hour() * 60 + newStartTime.minute();
            totalMin = (totalMin / 15) * 15;
            newStartTime = QTime(totalMin / 60, totalMin % 60);

            const qint64 durSecs = block.originalStart.secsTo(block.originalEnd);
            QDateTime newStart = QDateTime(m_date, newStartTime);
            QDateTime newEnd   = newStart.addSecs(durSecs);

            emit m_owner->eventRescheduled(block.eventIndex, newStart, newEnd);
        }
        m_dragging = false;
        m_dragBlockIndex = -1;
        setCursor(Qt::ArrowCursor);
        update();
        QWidget::mouseReleaseEvent(e);
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
        // Recompute block rects when width changes (fixes stale geometry on startup)
        if (e->oldSize().width() != e->size().width())
            rebuild();
    }

    void showEvent(QShowEvent* e) override {
        QWidget::showEvent(e);
        // Deferred rebuild after the layout has assigned our final geometry
        QTimer::singleShot(0, this, [this] { rebuild(); });
    }

private:
    double timeToY(const QTime& t) const {
        double hours = t.hour() + t.minute() / 60.0;
        return (hours - START_HOUR) * HOUR_HEIGHT + 20;
    }

    QTime yToTime(int y) const {
        double hours = (y - 20.0) / HOUR_HEIGHT + START_HOUR;
        int h = qBound(0, int(hours), 23);
        int m = qBound(0, int((hours - h) * 60), 59);
        m = (m / 15) * 15;  // snap to 15-min grid
        return QTime(h, m);
    }

    void rebuild() {
        m_blocks.clear();
        m_hoveredIndex = -1;

        if (!m_date.isValid()) { update(); return; }

        const int contentLeft = TIME_COL_WIDTH + LEFT_MARGIN;
        const int contentWidth = width() > 0
            ? width() - contentLeft - Theme::Space::M
            : 400;

        struct TodayEvent {
            int globalIndex;
            const Event* ev;
        };
        QVector<TodayEvent> todays;
        for (int i = 0; i < m_allEvents.size(); ++i) {
            if (m_allEvents[i].isOnDate(m_date))
                todays.push_back({i, &m_allEvents[i]});
        }
        std::sort(todays.begin(), todays.end(), [](const TodayEvent& a, const TodayEvent& b) {
            return a.ev->getStartTime() < b.ev->getStartTime();
        });

        for (const auto& te : todays) {
            const Event& ev = *te.ev;
            QTime startT = ev.getStartTime().time();
            QTime endT   = ev.getEndTime().time();

            // Clamp to visible range
            if (startT < QTime(START_HOUR, 0)) startT = QTime(START_HOUR, 0);
            // Handle midnight-crossing: QTime(0,0) means end-of-day for display
            if (endT == QTime(0, 0) || endT > QTime(23, 59)) endT = QTime(23, 59);
            if (startT >= endT) continue;

            double y1 = timeToY(startT);
            double y2 = timeToY(endT);
            double blockH = qMax(y2 - y1, 28.0);

            // Get category color from Theme
            QString cat = ev.getDescription();
            int sep = cat.indexOf("::");
            if (sep >= 0) cat = cat.left(sep);

            VisualBlock block;
            block.rect = QRectF(contentLeft + 4, y1, contentWidth - 8, blockH);
            block.color = ev.getColor().isValid() ? ev.getColor() : Theme::Category::color(cat);
            block.title = ev.getTitle();
            block.timeLabel = QString("%1 - %2")
                .arg(ev.getStartTime().time().toString("h:mm ap"),
                     ev.getEndTime().time().toString("h:mm ap"));
            block.eventIndex = te.globalIndex;
            block.originalStart = ev.getStartTime();
            block.originalEnd   = ev.getEndTime();
            m_blocks.push_back(block);
        }

        update();
    }

    DayTimelineWidget* m_owner = nullptr;
    QDate m_date;
    QVector<Event> m_allEvents;
    QVector<VisualBlock> m_blocks;
    int m_selectedIndex = -1;
    int m_hoveredIndex = -1;
};

// ============================================================================
// DayTimelineWidget (scroll area wrapper)
// ============================================================================

DayTimelineWidget::DayTimelineWidget(QWidget* parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_canvas = new TimelineCanvas(this, this);
    setWidget(m_canvas);

    // Scroll to ~8am on construction, offset slightly so 7am is visible at top
    QTimer::singleShot(0, this, [this] {
        const int scrollTo = (7 - TimelineCanvas::START_HOUR) * TimelineCanvas::HOUR_HEIGHT;
        verticalScrollBar()->setValue(scrollTo);
    });
}

void DayTimelineWidget::setDate(const QDate& date) {
    m_date = date;
    m_canvas->setDate(date);
    m_canvas->setEvents(m_allEvents);
}

void DayTimelineWidget::setEvents(const QVector<Event>& events) {
    m_allEvents = events;
    m_canvas->setEvents(events);
}

void DayTimelineWidget::setSelectedIndex(int index) {
    m_selectedIndex = index;
    m_canvas->setSelectedIndex(index);
}

void DayTimelineWidget::clearSelection() {
    m_selectedIndex = -1;
    m_canvas->setSelectedIndex(-1);
}
