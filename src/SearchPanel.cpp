#include "SearchPanel.h"
#include "Theme.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

// ============================================================================
// ResultCanvas — custom-painted row list
// ============================================================================

class SearchPanel::ResultList : public QScrollArea {
public:
    class ResultCanvas : public QWidget {
    public:
        static constexpr int ROW_H = 60;

        ResultCanvas(QWidget* parent) : QWidget(parent) {
            setMouseTracking(true);
        }

        void setMatches(const QVector<SearchPanel::Match>& matches) {
            m_matches = matches;
            m_hovered = -1;
            setMinimumHeight(qMax(50, matches.size() * ROW_H));
            update();
        }

        std::function<void(int)> onActivate;

    protected:
        void paintEvent(QPaintEvent*) override {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);

            const bool light = palette().color(QPalette::Window).lightness() > 127;
            const auto c = light ? Theme::light() : Theme::dark();

            if (m_matches.isEmpty()) {
                p.setPen(c.textTertiary);
                p.setFont(Theme::Font::base());
                p.drawText(rect(), Qt::AlignCenter, "No events found");
                return;
            }

            for (int i = 0; i < m_matches.size(); ++i) {
                const auto& m = m_matches[i];
                const QRect row(0, i * ROW_H, width(), ROW_H);

                // Hover bg
                if (i == m_hovered) {
                    p.setPen(Qt::NoPen);
                    QColor hov = light ? QColor(0,0,0,10) : QColor(255,255,255,12);
                    p.setBrush(hov);
                    p.drawRoundedRect(row.adjusted(4, 2, -4, -2), Theme::Radius::M, Theme::Radius::M);
                }

                // Color dot
                p.setPen(Qt::NoPen);
                QColor dot = m.ev->getColor().isValid() ? m.ev->getColor() : c.accent;
                p.setBrush(dot);
                p.drawEllipse(QPoint(row.left() + 16, row.center().y()), 5, 5);

                // Title
                p.setPen(c.text);
                QFont tf = Theme::Font::base();
                tf.setWeight(QFont::DemiBold);
                p.setFont(tf);
                const QRect titleR(row.left() + 30, row.top() + 10, row.width() - 40, 20);
                p.drawText(titleR, Qt::AlignLeft | Qt::AlignVCenter,
                           p.fontMetrics().elidedText(m.ev->getTitle(), Qt::ElideRight, titleR.width()));

                // Date + time subtitle
                p.setPen(c.textSecondary);
                QFont sf = Theme::Font::base();
                sf.setPointSize(10);
                p.setFont(sf);
                const QString sub = m.ev->getStartTime().toString("ddd, MMM d · h:mm ap")
                                    + " – "
                                    + m.ev->getEndTime().time().toString("h:mm ap");
                p.drawText(QRect(row.left() + 30, row.top() + 32, row.width() - 40, 18),
                           Qt::AlignLeft | Qt::AlignVCenter, sub);

                // Row separator
                p.setPen(QPen(c.borderSubtle, 0.5));
                if (i < m_matches.size() - 1)
                    p.drawLine(row.left() + 30, row.bottom(), row.right() - 8, row.bottom());
            }
        }

        void mouseMoveEvent(QMouseEvent* e) override {
            const int row = e->pos().y() / ROW_H;
            if (row != m_hovered) {
                m_hovered = (row >= 0 && row < m_matches.size()) ? row : -1;
                setCursor(m_hovered >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
                update();
            }
        }

        void mousePressEvent(QMouseEvent* e) override {
            const int row = e->pos().y() / ROW_H;
            if (row >= 0 && row < m_matches.size() && onActivate)
                onActivate(row);
        }

        void leaveEvent(QEvent*) override {
            m_hovered = -1;
            update();
        }

    private:
        QVector<SearchPanel::Match> m_matches;
        int m_hovered = -1;
    };

    ResultList(QWidget* parent) : QScrollArea(parent) {
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setWidgetResizable(true);
        m_canvas = new ResultCanvas(this);
        setWidget(m_canvas);
    }

    void setMatches(const QVector<SearchPanel::Match>& matches) {
        m_canvas->setMatches(matches);
    }

    std::function<void(int)>& onActivate() { return m_canvas->onActivate; }

private:
    ResultCanvas* m_canvas = nullptr;
};

// ============================================================================
// SearchPanel
// ============================================================================

SearchPanel::SearchPanel(QWidget* centerPanel)
    : QWidget(centerPanel)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::Widget);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(Theme::Space::L, Theme::Space::M,
                             Theme::Space::L, Theme::Space::L);
    root->setSpacing(Theme::Space::S);

    // Search input
    m_lineEdit = new QLineEdit;
    m_lineEdit->setPlaceholderText("Search events…");
    m_lineEdit->setMinimumHeight(40);
    m_lineEdit->setStyleSheet(
        "QLineEdit {"
        "  background: transparent;"
        "  border: none;"
        "  border-bottom: 1px solid rgba(0,0,0,0.12);"
        "  border-radius: 0;"
        "  font-size: 15px;"
        "  padding: 4px 0;"
        "}"
        "QLineEdit:focus { border-bottom-color: rgba(47,111,237,0.7); }");
    root->addWidget(m_lineEdit);

    // Results
    m_resultList = new ResultList(this);
    root->addWidget(m_resultList, 1);

    m_resultList->onActivate() = [this](int idx) { activateResult(idx); };

    connect(m_lineEdit, &QLineEdit::textChanged, this, &SearchPanel::rebuildResults);

    qApp->installEventFilter(this);
    hide();
}

SearchPanel::~SearchPanel() {
    qApp->removeEventFilter(this);
}

void SearchPanel::setEvents(const QVector<Event>& events) {
    m_allEvents = events;
    if (isVisible()) rebuildResults();
}

void SearchPanel::popup() {
    m_lineEdit->clear();
    rebuildResults();
    reposition();
    raise();
    show();
    m_lineEdit->setFocus();
}

void SearchPanel::reposition() {
    QWidget* par = parentWidget();
    if (!par) return;
    const int pw = par->width();
    const int ph = par->height();
    const int panelW = qMin(580, pw - 2 * Theme::Space::XL);
    const int panelH = qMin(460, ph - 80);
    move((pw - panelW) / 2, 60);
    resize(panelW, panelH);
}

void SearchPanel::rebuildResults() {
    const QString q = m_lineEdit->text().trimmed();
    m_matches.clear();
    for (int i = 0; i < m_allEvents.size(); ++i) {
        if (q.isEmpty() || m_allEvents[i].getTitle().contains(q, Qt::CaseInsensitive))
            m_matches.push_back({i, &m_allEvents[i]});
    }
    std::sort(m_matches.begin(), m_matches.end(), [](const Match& a, const Match& b) {
        return a.ev->getStartTime() < b.ev->getStartTime();
    });
    if (m_matches.size() > 80) m_matches.resize(80); // cap for perf
    m_resultList->setMatches(m_matches);
}

void SearchPanel::activateResult(int localIdx) {
    if (localIdx < 0 || localIdx >= m_matches.size()) return;
    const Match& m = m_matches[localIdx];
    emit dateNavigated(m.ev->getStartTime().date());
    emit eventSelected(m.globalIndex);
    hide();
}

void SearchPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const bool light = palette().color(QPalette::Window).lightness() > 127;
    const auto c = light ? Theme::light() : Theme::dark();

    // Drop shadow
    QRectF body = QRectF(rect()).adjusted(6, 6, -6, -6);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 36));
    p.drawRoundedRect(body.translated(0, 6), Theme::Radius::XL, Theme::Radius::XL);

    // Card body
    p.setBrush(c.surfaceRaised);
    p.setPen(QPen(c.border, 1.0));
    p.drawRoundedRect(body, Theme::Radius::XL, Theme::Radius::XL);
}

void SearchPanel::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (!m_matches.isEmpty()) activateResult(0);
        return;
    }
    QWidget::keyPressEvent(e);
}

bool SearchPanel::eventFilter(QObject* obj, QEvent* ev) {
    if (isVisible() && ev->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(ev);
        QWidget* par = parentWidget();
        if (par) {
            const QPoint localPos = par->mapFromGlobal(me->globalPosition().toPoint());
            if (!geometry().contains(localPos))
                hide();
        }
    }
    if (obj == parentWidget() && ev->type() == QEvent::Resize)
        reposition();
    return false;
}

void SearchPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    reposition();
}
