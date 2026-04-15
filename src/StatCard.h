#pragma once

#include <QFrame>
#include <QString>
#include <QVector>
#include <QPair>

class QLabel;
class QVBoxLayout;

// ============================================================================
// StatCard — premium card component
//
// A titled card with label/value rows, arc gauges, and bullet text.
// Paints its own rounded background using Theme tokens.
// ============================================================================

class StatCard : public QFrame {
    Q_OBJECT
public:
    explicit StatCard(const QString& title, QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void clearRows();
    void addRow(const QString& label, const QString& value);
    void addMetricRow(const QString& label, const QString& value,
                      int percent, const QColor& barColor);
    void addBullet(const QString& text);
    void setRows(const QVector<QPair<QString, QString>>& rows);

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QLabel* m_title = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
};

// ============================================================================
// ArcGaugeWidget — custom-painted arc gauge for premium metrics
//
// Displays a value as a colored arc on a subtle track, with the value
// centered inside. Much more premium than a QProgressBar.
// ============================================================================

class ArcGaugeWidget : public QFrame {
    Q_OBJECT
public:
    ArcGaugeWidget(const QString& label, const QString& value,
                   int percent, const QColor& color, QWidget* parent = nullptr);

    void setValue(const QString& value, int percent);

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QString m_label;
    QString m_value;
    int m_percent = 0;
    QColor m_color;
};
