#pragma once

#include <QScrollArea>
#include <QDate>
#include <QDateTime>
#include <QVector>
#include "Event.h"

// ============================================================================
// DayTimelineWidget — vertical day timeline (Apple Calendar style)
//
// CENTER panel: vertical time ruler 6am–11pm with event blocks.
// Supports single-event selection with visual highlight.
// ============================================================================

class DayTimelineWidget : public QScrollArea {
    Q_OBJECT
public:
    explicit DayTimelineWidget(QWidget* parent = nullptr);

    void setDate(const QDate& date);
    void setEvents(const QVector<Event>& events);

    void setSelectedIndex(int index);
    int  selectedIndex() const { return m_selectedIndex; }
    void clearSelection();

signals:
    void eventSelected(int eventIndex);   // single click — selects event
    void eventActivated(int eventIndex);  // double click — open edit
    void emptySlotClicked(const QTime& time);
    void selectionCleared();              // clicked empty space
    void eventRescheduled(int eventIndex, const QDateTime& newStart, const QDateTime& newEnd);

private:
    class TimelineCanvas;
    TimelineCanvas* m_canvas = nullptr;
    QDate m_date;
    QVector<Event> m_allEvents;
    int m_selectedIndex = -1;
};
