#pragma once
#include "control/controlpushbutton.h"

/// Skin controls that can be use in controller mappings.
class SkinControls {
  public:
    SkinControls();

  private:
    ControlPushButton m_showEffectRack;
    ControlPushButton m_showLibraryCoverArt;
    ControlPushButton m_showMicrophones;
    ControlPushButton m_showPreviewDecks;
    ControlPushButton m_showSamplers;
    ControlPushButton m_show4EffectUnits;
    ControlPushButton m_showCoverArt;
    ControlPushButton m_showMaximizedLibrary;
    ControlPushButton m_showMixer;
    ControlPushButton m_showSettings;
    ControlPushButton m_showSpinnies;
    ControlPushButton m_showVinylControl;
    // Mix Two: 1 while the DDJ-800 mapping runs (it sets this); the skin
    // then makes the BEAT FX channel segments display-only.
    ControlPushButton m_ddj800Connected;
    // Mix Two: which deck each side window shows. 0 = deck 1 (left) / deck 2
    // (right), 1 = deck 3 / deck 4. Not persisted.
    ControlPushButton m_leftDeckAlt;
    ControlPushButton m_rightDeckAlt;
};
