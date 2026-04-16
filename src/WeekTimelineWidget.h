#pragma once

#include <QWidget>
#include <QDate>
#include <QVector>
#include "Event.h"

class QScrollArea;

// ============================================================================
// WeekTimelineWidget — 7-column week timeline view
//
// Layout: fixed sticky header row (day names + date badges) on top, with a
// QScrollArea beneath for the hourly grid and event blocks.  The header never
// scrolls away.
// ============================================================================

class WeekTimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit WeekTimelineWidget(QWidget* parent = nullptr);

    void setWeekContaining(const QDate& anyDay);
    void setEvents(const QVector<Event>& events);

signals:
    void eventActivated(int eventIndex);
    void emptySlotClicked(QDate date, QTime time);

private:
    class WeekHeader;
    class WeekCanvas;

    WeekHeader*  m_header     = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    WeekCanvas*  m_canvas     = nullptr;
    QDate        m_weekStart;
    QVector<Event> m_allEvents;
};
