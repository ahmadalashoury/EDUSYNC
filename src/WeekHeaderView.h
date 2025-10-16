#pragma once
#include <QHeaderView>

class WeekHeaderView : public QHeaderView {
    Q_OBJECT
public:
    explicit WeekHeaderView(Qt::Orientation o, QWidget* parent = nullptr);
    QSize sizeHint() const override;
    void setLightTheme(bool on);

protected:
    void paintSection(QPainter* p, const QRect& rect, int logicalIndex) const override;

private:
    bool m_light = false;
};
