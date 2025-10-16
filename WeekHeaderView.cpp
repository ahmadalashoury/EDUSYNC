#include "WeekHeaderView.h"

#include <QPainter>
#include <QPen>
#include <QFont>
#include <QLocale>

WeekHeaderView::WeekHeaderView(Qt::Orientation o, QWidget* parent)
    : QHeaderView(o, parent)
{
    setSectionsClickable(false);
    setHighlightSections(false);
    setDefaultAlignment(Qt::AlignCenter);
    setSectionResizeMode(QHeaderView::Stretch);
    setFixedHeight(28);
    setObjectName("CalHeader");
}

QSize WeekHeaderView::sizeHint() const {
    QSize s = QHeaderView::sizeHint();
    s.setHeight(28);
    return s;
}

void WeekHeaderView::setLightTheme(bool on) {
    if (m_light == on) return;
    m_light = on;
    if (auto *vp = viewport()) vp->update();
}

void WeekHeaderView::paintSection(QPainter* p, const QRect& rect, int logicalIndex) const {
    if (!p || !rect.isValid()) return;

    const QColor bg  = m_light ? QColor("#f3f4f6") : QColor("#2a3036");
    const QColor fg  = m_light ? QColor("#6b7280") : QColor("#9aa3ab");
    const QColor brd = m_light ? QColor("#e5e7eb") : QColor("#2f3540");

    p->save();
    p->setRenderHint(QPainter::Antialiasing, false);

    p->setPen(Qt::NoPen);
    p->setBrush(bg);
    p->drawRect(rect);

    p->setPen(QPen(brd, 1));
    p->drawLine(rect.bottomLeft(), rect.bottomRight());

    // Prefer whatever the model provides (Qt’s calendar model already sets localized names)
    QString label;
    if (model() && logicalIndex >= 0 && logicalIndex < model()->columnCount())
        label = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();

    // Fallback: compute a safe weekday name
    if (label.isEmpty()) {
        if (logicalIndex < 0) logicalIndex = 0;
        int idx = logicalIndex % 7;                  // 0..6
        int localeDay = ((idx + 1 - 1) % 7) + 1;     // 1..7 (Mon..Sun)
        label = QLocale().standaloneDayName(localeDay, QLocale::ShortFormat);
    }
    label = label.toUpper();

    QFont f = p->font();
    f.setBold(true);
    p->setFont(f);
    p->setPen(fg);
    p->drawText(rect.adjusted(0,0,0,-1), Qt::AlignCenter, label);

    p->restore();
}

