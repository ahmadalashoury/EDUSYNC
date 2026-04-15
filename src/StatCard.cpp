#include "StatCard.h"
#include "Theme.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QtMath>

// ============================================================================
// StatCard
// ============================================================================

StatCard::StatCard(const QString& title, QWidget* parent)
    : QFrame(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(Theme::Space::L, Theme::Space::L,
                             Theme::Space::L, Theme::Space::L);
    root->setSpacing(Theme::Space::M);

    m_title = new QLabel(title, this);
    m_title->setFont(Theme::Font::caption());
    m_title->setObjectName("StatCardTitle");
    root->addWidget(m_title);

    m_contentLayout = new QVBoxLayout;
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(Theme::Space::S);
    root->addLayout(m_contentLayout);

    root->addStretch();

    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_StyledBackground, true);
}

void StatCard::setTitle(const QString& title) {
    if (m_title) m_title->setText(title);
}

void StatCard::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const auto c = palette().color(QPalette::Window).lightness() > 127
                       ? Theme::light() : Theme::dark();
    const bool isDark = palette().color(QPalette::Window).lightness() <= 127;

    QPainterPath path;
    const QRectF cardRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    path.addRoundedRect(cardRect,
                        Theme::Radius::L, Theme::Radius::L);

    QColor shadow = isDark ? QColor(0, 0, 0, 34) : QColor(0, 0, 0, 14);
    p.setPen(Qt::NoPen);
    p.setBrush(shadow);
    p.drawRoundedRect(cardRect.translated(0, 3), Theme::Radius::L, Theme::Radius::L);

    p.setPen(Qt::NoPen);
    if (isDark) {
        QLinearGradient glass(cardRect.topLeft(), cardRect.bottomLeft());
        glass.setColorAt(0.0, QColor(255, 255, 255, 16));
        glass.setColorAt(0.14, QColor(31, 39, 53, 232));
        glass.setColorAt(1.0, QColor(17, 22, 32, 236));
        p.setBrush(glass);
    } else {
        p.setBrush(c.surfaceRaised);
    }
    p.drawPath(path);

    p.setPen(QPen(isDark ? QColor(104, 216, 255, 34) : c.borderSubtle, isDark ? 0.8 : 0.5));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
}

void StatCard::clearRows() {
    QLayoutItem* item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (QLayout* childLayout = item->layout()) {
            QLayoutItem* child;
            while ((child = childLayout->takeAt(0)) != nullptr) {
                delete child->widget();
                delete child;
            }
            delete childLayout;
        } else {
            delete item->widget();
            delete item;
        }
    }
}

void StatCard::addRow(const QString& label, const QString& value) {
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(Theme::Space::S);

    auto* lbl = new QLabel(label, this);
    lbl->setFont(Theme::Font::base());
    auto pal = lbl->palette();
    pal.setColor(QPalette::WindowText, palette().color(QPalette::PlaceholderText));
    lbl->setPalette(pal);

    auto* val = new QLabel(value, this);
    auto f = Theme::Font::base();
    f.setWeight(QFont::DemiBold);
    val->setFont(f);

    row->addWidget(lbl);
    row->addStretch();
    row->addWidget(val);

    m_contentLayout->addLayout(row);
}

void StatCard::addMetricRow(const QString& label, const QString& value,
                            int percent, const QColor& barColor) {
    // Use inline arc gauge for premium look
    auto* gauge = new ArcGaugeWidget(label, value, percent, barColor, this);
    m_contentLayout->addWidget(gauge);
}

void StatCard::addBullet(const QString& text) {
    auto* lbl = new QLabel(QString::fromUtf8("\u2022 ") + text, this);
    lbl->setFont(Theme::Font::base());
    lbl->setWordWrap(true);
    m_contentLayout->addWidget(lbl);
}

void StatCard::setRows(const QVector<QPair<QString, QString>>& rows) {
    clearRows();
    for (const auto& [label, value] : rows)
        addRow(label, value);
}

// ============================================================================
// ArcGaugeWidget — custom-painted arc gauge
// ============================================================================

ArcGaugeWidget::ArcGaugeWidget(const QString& label, const QString& value,
                               int percent, const QColor& color, QWidget* parent)
    : QFrame(parent)
    , m_label(label)
    , m_value(value)
    , m_percent(qBound(0, percent, 100))
    , m_color(color)
{
    setFixedHeight(80);
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_TranslucentBackground);
}

void ArcGaugeWidget::setValue(const QString& value, int percent) {
    m_value = value;
    m_percent = qBound(0, percent, 100);
    update();
}

void ArcGaugeWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool isDark = palette().color(QPalette::Window).lightness() <= 127;
    const auto c = isDark ? Theme::dark() : Theme::light();

    const int gaugeSize = 56;
    const int penWidth = 5;
    const int leftMargin = 4;

    // Arc geometry — centered vertically, left-aligned
    QRect arcRect(leftMargin, (height() - gaugeSize) / 2, gaugeSize, gaugeSize);
    const int startAngle = 225 * 16;    // 7 o'clock position
    const int fullSpan   = -270 * 16;   // sweep 270 degrees clockwise
    const int valueSpan  = fullSpan * m_percent / 100;

    // Track (subtle background arc)
    QColor trackColor = isDark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 12);
    QPen trackPen(trackColor, penWidth, Qt::SolidLine, Qt::RoundCap);
    p.setPen(trackPen);
    p.setBrush(Qt::NoBrush);
    p.drawArc(arcRect.adjusted(penWidth/2, penWidth/2, -penWidth/2, -penWidth/2),
              startAngle, fullSpan);

    // Value arc (colored)
    if (m_percent > 0) {
        QPen valuePen(m_color, penWidth, Qt::SolidLine, Qt::RoundCap);
        p.setPen(valuePen);
        p.drawArc(arcRect.adjusted(penWidth/2, penWidth/2, -penWidth/2, -penWidth/2),
                  startAngle, valueSpan);
    }

    // Value text inside arc
    QFont valueFont("Helvetica Neue", 12);
    valueFont.setWeight(QFont::Bold);
    valueFont.setStyleStrategy(QFont::PreferAntialias);
    p.setFont(valueFont);
    p.setPen(c.text);
    p.drawText(arcRect, Qt::AlignCenter, m_value);

    // Label text to the right of the arc
    const int textX = arcRect.right() + Theme::Space::L;
    const int textW = width() - textX - Theme::Space::S;

    QFont labelFont = Theme::Font::base();
    p.setFont(labelFont);
    p.setPen(c.textSecondary);
    p.drawText(QRect(textX, arcRect.top() + 10, textW, 20),
               Qt::AlignLeft | Qt::AlignVCenter, m_label);

    // Percent below label
    QFont pctFont("Helvetica Neue", 10);
    pctFont.setStyleStrategy(QFont::PreferAntialias);
    p.setFont(pctFont);
    p.setPen(c.textTertiary);
    p.drawText(QRect(textX, arcRect.top() + 32, textW, 18),
               Qt::AlignLeft | Qt::AlignVCenter,
               QString("%1% of ideal").arg(m_percent));
}
