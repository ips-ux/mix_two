#pragma once
// AI-generated (Claude Code). Start of AI-generated text.
// Small pure functions behind the library display preferences: sidebar
// labels, library font, artwork crop and "centre the selected track".
// They need no running application, so unit tests can reach them.

#include <QFont>
#include <QModelIndex>
#include <QRect>
#include <QSize>
#include <QString>

class QAbstractItemView;

namespace mixxx {
namespace library {

/// The text of a playlist, crate or history entry in the sidebar.
/// With `showCount` off, the label is the plain name. With it on, the label is
/// "name (count)", followed by the duration text when that text is not empty.
QString sidebarLabel(const QString& name,
        int count,
        const QString& durationText,
        bool showCount);

/// The library font from the saved font text. `bold` sets or clears the bold
/// weight. An empty or unreadable text gives `fallback` (with `bold` applied).
QFont libraryFontFromConfig(const QString& fontString,
        bool bold,
        const QFont& fallback);

struct CoverArtPaintRects {
    // Where to draw, in cell coordinates of the view.
    QRect target;
    // Which part of the picture to draw, in picture coordinates.
    QRect source;
};

/// Where a cover picture goes in a cell of the artwork column.
/// `imageSize` is the size of the picture after it is scaled to the cell width.
/// Upper part (`showWhole` false): the picture keeps that width and its top;
/// the part that is below the cell is cut off. Whole picture: the picture is
/// scaled down until its height fits the cell; nothing is cut off.
CoverArtPaintRects coverArtPaintRects(bool showWhole,
        const QSize& imageSize,
        const QRect& cell);

/// Scrolls `pView` so that the row of `index` is in the middle of the view.
void scrollRowToCenter(QAbstractItemView* pView, const QModelIndex& index);

} // namespace library
} // namespace mixxx
// End of AI-generated text.
