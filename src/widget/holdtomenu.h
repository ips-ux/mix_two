#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <QPoint>

namespace holdtomenu {

// The time that the user holds the mouse button on a track row before the
// track menu opens (rekordbox "Drop-down menu": hold for one second).
constexpr int kHoldMillis = 1000;

// The preference is off by default, so the right-click menu stays the only way
// to open the track menu unless the user turns it on.
constexpr bool kEnabledDefault = false;

/// True when the pointer moved so far from the press point that the press is a
/// drag or a selection change and not a hold.
inline bool movedTooFar(const QPoint& pressPos, const QPoint& nowPos, int dragDistancePx) {
    return (nowPos - pressPos).manhattanLength() > dragDistancePx;
}

} // namespace holdtomenu

// End of AI-generated text.
