// AI-generated (Claude Code). Start of AI-generated text.
#include "preferences/dialog/prefraildelegate.h"

#include <QPainter>
#include <QPainterPath>

#include "moc_prefraildelegate.cpp"

namespace mixxx::preferences {

namespace {
constexpr int kRowHeight = 30;
constexpr int kHeadingHeight = 26;
constexpr int kCollapsedHeadingHeight = 11;
constexpr int kIconSize = 20;
constexpr int kIconLeft = 9;
constexpr int kChildIndent = 12;
constexpr int kIconTextGap = 10;
constexpr int kPillRadius = 9;
// Below this width the row has no space for a label. The width changes during
// the animation of the rail.
constexpr int kMinWidthForLabel = 110;
} // namespace

QPixmap tintedIconPixmap(const QIcon& icon, QSize size, qreal devicePixelRatio, QColor color) {
    QPixmap pixmap = icon.pixmap(size, devicePixelRatio);
    if (pixmap.isNull()) {
        return pixmap;
    }
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    return pixmap;
}

void PrefRailDelegate::paint(QPainter* pPainter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const {
    pPainter->save();
    pPainter->setRenderHint(QPainter::Antialiasing);
    const QRect rect = option.rect;
    const QString text = index.data(Qt::DisplayRole).toString();
    const QColor dim = QColor::fromRgb(glass::kTextDim);

    if (index.data(kRailHeadingRole).toBool()) {
        if (m_collapsed) {
            const int y = rect.center().y();
            pPainter->setPen(QColor::fromRgb(glass::kLine));
            pPainter->drawLine(rect.left() + 8, y, rect.right() - 8, y);
        } else {
            QFont font = option.font;
            font.setPointSizeF(font.pointSizeF() * 0.86);
            pPainter->setFont(font);
            pPainter->setPen(dim);
            pPainter->drawText(rect.adjusted(kIconLeft, 4, -6, 0),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    text);
        }
        pPainter->restore();
        return;
    }

    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);

    if (selected || hovered) {
        // The right side is square: there the selected row joins the pane.
        QPainterPath path;
        const QRectF pill = rect.adjusted(0, 1, 0, -1);
        path.addRoundedRect(pill.adjusted(0, 0, kPillRadius, 0), kPillRadius, kPillRadius);
        pPainter->setClipRect(pill);
        pPainter->fillPath(path, selected ? QColor::fromRgb(glass::kPane) : QColor(255, 255, 255, 18));
        pPainter->setClipping(false);
    }

    if (option.state.testFlag(QStyle::State_HasFocus) &&
            option.state.testFlag(QStyle::State_KeyboardFocusChange)) {
        pPainter->setPen(QColor::fromRgb(glass::kAccent));
        pPainter->drawRoundedRect(QRectF(rect).adjusted(1.5, 1.5, -1.5, -1.5),
                kPillRadius - 1,
                kPillRadius - 1);
    }

    const bool isChild = index.parent().isValid() &&
            !index.parent().data(kRailHeadingRole).toBool();
    const bool showLabel = !m_collapsed && rect.width() >= kMinWidthForLabel;
    const int iconLeft = rect.left() + kIconLeft + ((isChild && showLabel) ? kChildIndent : 0);
    const QColor foreground = selected
            ? QColor::fromRgb(glass::kAccent)
            : (hovered ? QColor::fromRgb(glass::kText) : dim);

    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    const qreal ratio = pPainter->device()->devicePixelRatioF();
    const QPixmap pixmap = tintedIconPixmap(icon, QSize(kIconSize, kIconSize), ratio, foreground);
    pPainter->drawPixmap(QPoint(iconLeft, rect.top() + (rect.height() - kIconSize) / 2), pixmap);

    if (showLabel) {
        QFont font = index.data(Qt::FontRole).isValid()
                ? index.data(Qt::FontRole).value<QFont>()
                : option.font;
        if (selected) {
            font.setWeight(QFont::DemiBold);
        }
        pPainter->setFont(font);
        pPainter->setPen(foreground);
        const QRect textRect = rect.adjusted(iconLeft - rect.left() + kIconSize + kIconTextGap, 0, -6, 0);
        const QString elided = QFontMetrics(font).elidedText(
                text, Qt::ElideRight, textRect.width());
        pPainter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elided);
    }
    pPainter->restore();
}

QSize PrefRailDelegate::sizeHint(const QStyleOptionViewItem& option,
        const QModelIndex& index) const {
    int height = kRowHeight;
    if (index.data(kRailHeadingRole).toBool()) {
        height = m_collapsed ? kCollapsedHeadingHeight : kHeadingHeight;
    }
    return QSize(option.rect.width(), height);
}

} // namespace mixxx::preferences
// End of AI-generated text.
