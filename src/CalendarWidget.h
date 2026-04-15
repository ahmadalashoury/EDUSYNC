#pragma once

#include <QCalendarWidget>
#include <QDate>
#include <QList>
#include <QPointer>
#include "Event.h"

class QTableView;
class QHeaderView;

// ============================================================================
// CalendarWidget — clean calendar with event dots and chips
//
// Simplified from ModernCalendarWidget:
//  - Removed unnecessary paintEvent override (just called super)
//  - Removed complex multi-layer header styling (uses Theme tokens)
//  - Kept: custom paintCell, hover tracking, keyboard nav, wheel nav
// ============================================================================

class CalendarWidget : public QCalendarWidget {
    Q_OBJECT
public:
    explicit CalendarWidget(QWidget* parent = nullptr);

    void setEvents(const QList<Event>& events);
    void setCurrentMonth(const QDate& anyDayInMonth);
    void applyTheme(bool light);

signals:
    void dateSelected(const QDate& date);
    void monthChanged(const QDate& firstOfMonth);

protected:
    void paintCell(QPainter* p, const QRect& rect, QDate date) const override;
    void mousePressEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    void setupInternals();
    void styleHeader();
    QDate gridStartDate() const;
    void updateHoveredFromPos(const QPoint& pos);

    void drawEventDots(QPainter& p, const QRect& cell, const QDate& d) const;
    void drawEventChips(QPainter& p, const QRect& cell, const QDate& d) const;

    QPointer<QTableView>  m_view;
    QPointer<QHeaderView> m_hHeader;
    QPointer<QWidget>     m_viewport;

    bool  m_light = false;
    QDate m_month;
    QDate m_selected;
    QDate m_hovered;
    QList<Event> m_events;
};
