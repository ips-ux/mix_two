#pragma once

// AI-generated (Claude Code). Start of AI-generated text.

#include <QObject>
#include <memory>

#include "preferences/usersettings.h"

class ControlPushButton;

/// Global track-load settings for the deck HUD (Pioneer AUTO CUE and
/// A.HOT CUE):
///
/// - `[Controls],auto_cue` (toggle): 1 writes the seek-on-load preference
///   `[Controls],CueRecall` = SeekOnLoadMode::FirstSound, 0 writes
///   SeekOnLoadMode::MainCue (only when the preference is FirstSound, so a
///   value set in the deck preferences stays). At start the control is 1 only
///   when CueRecall is FirstSound. The deck preferences page sets it after it
///   writes CueRecall.
/// - `[Controls],auto_hotcue` (toggle, persisted, default 1): when 0, CueControl
///   keeps the hot cue pads empty after a track loads, until
///   `[ChannelN],hotcues_load` or a hot cue action.
/// - `[Controls],auto_cue_toggle`, `[Controls],auto_hotcue_toggle` (push):
///   a press sets the control on or off. The skin cells use them on the right
///   mouse button, because a WPushButton does not toggle on the right button.
class LoadSettingsControls : public QObject {
    Q_OBJECT
  public:
    explicit LoadSettingsControls(UserSettingsPointer pConfig);
    ~LoadSettingsControls() override;

  private slots:
    void slotAutoCueChanged(double value);
    void slotAutoCueToggle(double value);
    void slotAutoHotcueToggle(double value);

  private:
    UserSettingsPointer m_pConfig;
    std::unique_ptr<ControlPushButton> m_pAutoCue;
    std::unique_ptr<ControlPushButton> m_pAutoHotcue;
    std::unique_ptr<ControlPushButton> m_pAutoCueToggle;
    std::unique_ptr<ControlPushButton> m_pAutoHotcueToggle;
};

// End of AI-generated text.
