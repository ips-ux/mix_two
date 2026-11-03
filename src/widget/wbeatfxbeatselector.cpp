#include "widget/wbeatfxbeatselector.h"

#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <cmath>

#include "control/controlproxy.h"
#include "moc_wbeatfxbeatselector.cpp"
#include "skin/legacy/skincontext.h"

namespace {
// The DDJ-800's BEAT fractions, in the order of its buttons (the same table
// as beatfxcommon.h).
const QStringList kLabels = {QStringLiteral("1/16"),
        QStringLiteral("1/8"),
        QStringLiteral("1/4"),
        QStringLiteral("1/2"),
        QStringLiteral("1"),
        QStringLiteral("2"),
        QStringLiteral("4"),
        QStringLiteral("8"),
        QStringLiteral("16")};
constexpr int kArrowWidth = 16;
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

int WBeatFxBeatSelector::index() const {
    int i = static_cast<int>(std::lround(m_value));
    return std::max(0, std::min(i, static_cast<int>(kLabels.size()) - 1));
}

void WBeatFxBeatSelector::setIndex(int i) {
    i = std::max(0, std::min(i, static_cast<int>(kLabels.size()) - 1));
    if (m_pBeat) {
        m_pBeat->set(i);
    }
}

QString WBeatFxBeatSelector::label(int i) const {
    return kLabels.value(i, QStringLiteral("-"));
}

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
    painter.setPen(i < kLabels.size() - 1 ? palette().color(QPalette::WindowText) : dim);
    painter.drawText(QRect(r.right() - kArrowWidth + 1, r.top(), kArrowWidth, r.height()),
            Qt::AlignCenter,
            QStringLiteral("▶"));
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(QRect(r.left() + kArrowWidth, r.top(), r.width() - 2 * kArrowWidth, r.height()),
            Qt::AlignCenter,
            label(i) + (i == 4 ? QStringLiteral(" beat") : QStringLiteral(" beats")));
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
        for (int i = 0; i < kLabels.size(); ++i) {
            QAction* pAction = menu.addAction(label(i));
            pAction->setCheckable(true);
            pAction->setChecked(i == index());
            connect(pAction, &QAction::triggered, this, [this, i]() { setIndex(i); });
        }
        menu.exec(mapToGlobal(QPoint(kArrowWidth, height())));
    }
    pEvent->accept();
}
