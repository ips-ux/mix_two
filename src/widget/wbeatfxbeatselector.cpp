#include "widget/wbeatfxbeatselector.h"

#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <cmath>

#include "control/controlproxy.h"
#include "effects/backends/builtin/beatfxcommon.h"
#include "moc_wbeatfxbeatselector.cpp"
#include "skin/legacy/skincontext.h"

namespace {
constexpr int kArrowWidth = 16;
// Without an effect the control keeps the nine steps of most Beat FX.
constexpr int kDefaultStepCount = 9;
} // namespace

WBeatFxBeatSelector::WBeatFxBeatSelector(QWidget* pParent)
        : WWidget(pParent),
          m_pBeat(nullptr),
          m_value(4) {
}

void WBeatFxBeatSelector::setup(const QDomNode& node, const SkinContext& context) {
    const QString group = context.selectString(node, QStringLiteral("Group"));
    m_pBeat = new ControlProxy(group, QStringLiteral("parameter1"), this);
    m_pBeat->connectValueChanged(this, &WBeatFxBeatSelector::slotValueChanged);
    m_value = m_pBeat->get();
    setCursor(Qt::PointingHandCursor);
}

// AI-generated (Claude Code). Start of AI-generated text.
// The number of steps of the loaded effect's beat parameter: the parameter
// control runs from value 0 to value (steps - 1), so one step is the
// normalised position of value 1. This is 9, 11 or 22 for the Mix Two BEAT FX.
int WBeatFxBeatSelector::stepCount() const {
    if (m_pBeat) {
        const double step = m_pBeat->getParameterForValue(1.0);
        // An empty slot has the range 0..1 (2 steps): show the default list.
        if (step > 0.0 && step <= 1.0) {
            const int steps = static_cast<int>(std::lround(1.0 / step)) + 1;
            if (steps >= 3) {
                return steps;
            }
        }
    }
    return kDefaultStepCount;
}

int WBeatFxBeatSelector::index() const {
    int i = static_cast<int>(std::lround(m_value));
    return std::max(0, std::min(i, stepCount() - 1));
}

void WBeatFxBeatSelector::setIndex(int i) {
    i = std::max(0, std::min(i, stepCount() - 1));
    if (m_pBeat) {
        m_pBeat->set(i);
    }
}

QString WBeatFxBeatSelector::label(int i) const {
    double beats = 0;
    if (!beatfx::beatStepValue(stepCount(), i, &beats)) {
        return QStringLiteral("-");
    }
    return beatfx::beatLabel(beats);
}
// End of AI-generated text.

void WBeatFxBeatSelector::slotValueChanged(double value) {
    m_value = value;
    update();
}

void WBeatFxBeatSelector::paintEvent(QPaintEvent* pEvent) {
    Q_UNUSED(pEvent);
    QStyleOption option;
    option.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
    painter.setPen(palette().color(QPalette::WindowText));
    painter.setFont(font());
    const QRect r = rect();
    const int i = index();
    const QColor dim = palette().color(QPalette::WindowText).darker(160);
    painter.setPen(i > 0 ? palette().color(QPalette::WindowText) : dim);
    painter.drawText(QRect(r.left(), r.top(), kArrowWidth, r.height()),
            Qt::AlignCenter,
            QStringLiteral("◀"));
    painter.setPen(i < stepCount() - 1 ? palette().color(QPalette::WindowText) : dim);
    painter.drawText(QRect(r.right() - kArrowWidth + 1, r.top(), kArrowWidth, r.height()),
            Qt::AlignCenter,
            QStringLiteral("▶"));
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(QRect(r.left() + kArrowWidth, r.top(), r.width() - 2 * kArrowWidth, r.height()),
            Qt::AlignCenter,
            label(i) + (label(i) == QStringLiteral("1") ? QStringLiteral(" beat") : QStringLiteral(" beats")));
}

void WBeatFxBeatSelector::mousePressEvent(QMouseEvent* pEvent) {
    if (pEvent->button() != Qt::LeftButton) {
        WWidget::mousePressEvent(pEvent);
        return;
    }
    const int x = pEvent->pos().x();
    if (x < kArrowWidth) {
        setIndex(index() - 1);
    } else if (x >= width() - kArrowWidth) {
        setIndex(index() + 1);
    } else {
        QMenu menu(this);
        for (int i = 0; i < stepCount(); ++i) {
            QAction* pAction = menu.addAction(label(i));
            pAction->setCheckable(true);
            pAction->setChecked(i == index());
            connect(pAction, &QAction::triggered, this, [this, i]() { setIndex(i); });
        }
        menu.exec(mapToGlobal(QPoint(kArrowWidth, height())));
    }
    pEvent->accept();
}
