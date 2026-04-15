#pragma once

#include <QWidget>
#include <QVector>
#include <QDate>
#include "Event.h"

class QLineEdit;

// ============================================================================
// SearchPanel — Cmd+F floating search overlay
//
// Parented to the center panel, positioned as an overlay card.
// Shows all events by default; filters by title as user types.
// Dismissed by Escape, click outside, or selecting a result.
// ============================================================================

class SearchPanel : public QWidget {
    Q_OBJECT
public:
    explicit SearchPanel(QWidget* centerPanel);
    ~SearchPanel() override;

    void setEvents(const QVector<Event>& events);
    void popup();   // clear, reposition, show, focus input

signals:
    void dateNavigated(QDate date);
    void eventSelected(int eventIndex);

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void showEvent(QShowEvent*) override;

private:
    void reposition();
    void rebuildResults();
    void activateResult(int localIdx);

    struct Match {
        int         globalIndex;
        const Event* ev;
    };

    class ResultList;

    QLineEdit*     m_lineEdit   = nullptr;
    ResultList*    m_resultList = nullptr;
    QVector<Event> m_allEvents;
    QVector<Match> m_matches;
};
