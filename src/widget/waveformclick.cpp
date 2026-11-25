// AI-generated (Claude Code). Start of AI-generated text.
#include "widget/waveformclick.h"

namespace waveformclick {

ConfigKey clickPlayCueConfigKey() {
    return ConfigKey(QStringLiteral("[Waveform]"), QStringLiteral("ClickPlayCue"));
}

bool isClickPlayCueEnabled(const UserSettingsPointer& pConfig) {
    if (!pConfig) {
        return false;
    }
    return pConfig->getValue<bool>(clickPlayCueConfigKey(), false);
}

bool isClick(const QPoint& pressPos, const QPoint& releasePos) {
    const QPoint d = releasePos - pressPos;
    const int distanceSquared = d.x() * d.x() + d.y() * d.y();
    return distanceSquared <= kMaxClickDragPixels * kMaxClickDragPixels;
}

Action actionForButton(Qt::MouseButton button) {
    switch (button) {
    case Qt::LeftButton:
        return Action::TogglePlayOnClick;
    case Qt::RightButton:
        return Action::CueDefaultWhileHeld;
    default:
        return Action::None;
    }
}

} // namespace waveformclick

// End of AI-generated text.
