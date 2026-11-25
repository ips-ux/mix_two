#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <Qt>
#include <QPoint>

#include "preferences/configobject.h"
#include "preferences/usersettings.h"

// Logic of the preference "Click on the waveform for PLAY and CUE". The
// functions have no state, so that a unit test can reach them without a
// waveform widget.
namespace waveformclick {

// The largest distance in pixels that the mouse may move between press and
// release so that the gesture still counts as a click. Choice: 4 pixels.
constexpr int kMaxClickDragPixels = 4;

// The preference. Off by default: the waveform then behaves as before.
ConfigKey clickPlayCueConfigKey();
bool isClickPlayCueEnabled(const UserSettingsPointer& pConfig);

// True when the mouse moved so little between press and release that the
// gesture is a click and not a drag.
bool isClick(const QPoint& pressPos, const QPoint& releasePos);

// True when the mouse moved beyond the click distance from the press.
inline bool isDrag(const QPoint& pressPos, const QPoint& currentPos) {
    return !isClick(pressPos, currentPos);
}

enum class Action {
    None,
    // Play or pause the deck when the button is released after a click.
    TogglePlayOnClick,
    // Act as the CUE button: down at the press, up at the release.
    CueDefaultWhileHeld,
};

// Which control a mouse button drives when the preference is on.
Action actionForButton(Qt::MouseButton button);

// Events after which a held CUE button must go up.
enum class CueReleaseEvent {
    RightButtonReleased,
    Hidden,
    Destroyed,
    WindowDeactivated,
    MouseGrabLost,
    // The mouse moved while the button is held: the setting may have changed.
    Moved,
};

// True when a held CUE must go up now. Without a held CUE, it is false. A
// moved mouse releases the CUE only when the setting is off.
bool mustReleaseCueNow(bool cueHeld, CueReleaseEvent event, bool settingOn);

} // namespace waveformclick

// End of AI-generated text.
