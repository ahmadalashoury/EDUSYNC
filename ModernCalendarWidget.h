#pragma once
#include <QCalendarWidget>
#include <QHeaderView>
#include <QDate>
#include <QList>
#include <QColor>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QTimer>
#include <QTableView>

#include "Event.h"

class ModernCalendarWidget : public QCalendarWidget {
    Q_OBJECT
public:
    explicit ModernCalendarWidget(QWidget* parent = nullptr);

    void setEvents(const QList<Event>& evs);
    const QList<Event>& events() const { return m_events; }

    void setCurrentMonth(const QDate& anyDayInMonth);
    void applyHeaderStyleForTheme(bool light);
    void scheduleRestyle();

signals:
    void dateSelected(const QDate& date);
    void monthChanged(const QDate& firstOfMonth);

protected:
    void paintCell(QPainter* p, const QRect& rect, QDate date) const override;
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void showEvent(QShowEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    // header/hover utilities
    void ensureHeaderStyled();
    QDate gridStartDate() const;
    void updateHoveredFromPos(const QPoint& vp);
    QModelIndex indexForDate(const QDate& d) const;  // ← keep it here

    // optional custom draw helpers
    void drawEventsDots(QPainter& p, const QRect& cell, const QDate& d) const;
    void drawEventChips(QPainter& p, const QRect& cell, const QDate& d) const;
    void restyleNow();

    // cached internals
    QTableView*  m_view     = nullptr;
    QHeaderView* m_hHeader  = nullptr;
    QWidget*     m_viewport = nullptr;

    // state
    bool  m_light = false;
    QDate m_month;
    QDate m_selected;
    QDate m_hovered;
    QList<Event> m_events;
    QTimer* m_restyleTimer = nullptr;
};
