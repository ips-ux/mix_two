// AI-generated (Claude Code). Start of AI-generated text.
#include "library/libraryviewprefs.h"

#include <QAbstractItemView>
#include <algorithm>

namespace mixxx {
namespace library {

QString sidebarLabel(const QString& name,
        int count,
        const QString& durationText,
        bool showCount) {
    if (!showCount) {
        return name;
    }
    if (durationText.isEmpty()) {
        return QStringLiteral("%1 (%2)").arg(name, QString::number(count));
    }
    return QStringLiteral("%1 (%2) %3")
            .arg(name, QString::number(count), durationText);
}

QFont libraryFontFromConfig(const QString& fontString,
        bool bold,
        const QFont& fallback) {
    QFont font = fallback;
    if (!fontString.isEmpty()) {
        QFont parsed;
        if (parsed.fromString(fontString)) {
            font = parsed;
        }
    }
    font.setBold(bold);
    return font;
}

CoverArtPaintRects coverArtPaintRects(bool showWhole,
        const QSize& imageSize,
        const QRect& cell) {
    if (imageSize.isEmpty() || cell.isEmpty()) {
        return {};
    }
    const QRect wholeImage(QPoint(0, 0), imageSize);
    if (imageSize.height() <= cell.height()) {
        // The picture fits: nothing to cut off or to scale.
        return {QRect(cell.topLeft(), imageSize), wholeImage};
    }
    if (showWhole) {
        // Scale the whole picture down until its height fits the cell.
        const int width = std::max(1, imageSize.width() * cell.height() / imageSize.height());
        return {QRect(cell.topLeft(), QSize(width, cell.height())), wholeImage};
    }
    // Choice: "upper part" keeps the width and the top of the picture and
    // cuts off the bottom.
    const QSize shown(imageSize.width(), cell.height());
    return {QRect(cell.topLeft(), shown), QRect(QPoint(0, 0), shown)};
}

void scrollRowToCenter(QAbstractItemView* pView, const QModelIndex& index) {
    if (pView && index.isValid()) {
        pView->scrollTo(index, QAbstractItemView::PositionAtCenter);
    }
}

} // namespace library
} // namespace mixxx
// End of AI-generated text.
