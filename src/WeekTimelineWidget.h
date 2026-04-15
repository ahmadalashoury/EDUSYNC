#pragma once

#include <QScrollArea>
#include <QDate>
#include <QVector>
#include "Event.h"

// ============================================================================
// WeekTimelineWidget — 7-column week timeline view
//
// Mirrors DayTimelineWidget structure: QScrollArea + custom-painted WeekCanvas.
// Shows Mon-Sun for the week containing the given date, with:
//   - Hour labels and horizontal grid lines
//   - Column header per day ("Mon 14")
//   - Event blocks positioned by time within their day column
//   - Current-time indicator in today's column only
// ============================================================================

class WeekTimelineWidget : public QScrollArea {
    Q_OBJECT
public:
    explicit WeekTimelineWidget(QWidget* parent = nullptr);

    void setWeekContaining(const QDate& anyDay);
    void setEvents(const QVector<Event>& events);

signals:
    void eventActivated(int eventIndex);
    void emptySlotClicked(QDate date, QTime time);

private:
    class WeekCanvas;
    WeekCanvas*    m_canvas    = nullptr;
    QDate          m_weekStart;           // always Monday
    QVector<Event> m_allEvents;
};
