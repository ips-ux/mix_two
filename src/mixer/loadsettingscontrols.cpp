#include "mixer/loadsettingscontrols.h"

#include "control/controlpushbutton.h"
#include "engine/controls/cuecontrol.h"
#include "moc_loadsettingscontrols.cpp"

// AI-generated (Claude Code). Start of AI-generated text.

namespace {

const QString kControlsGroup = QStringLiteral("[Controls]");
const ConfigKey kCueRecallKey(kControlsGroup, QStringLiteral("CueRecall"));

SeekOnLoadMode seekOnLoadPreference(const UserSettingsPointer& pConfig) {
    return pConfig->getValue(kCueRecallKey, SeekOnLoadMode::IntroStart);
}

} // namespace

LoadSettingsControls::LoadSettingsControls(UserSettingsPointer pConfig)
        : m_pConfig(std::move(pConfig)),
          m_pAutoCue(std::make_unique<ControlPushButton>(
                  ConfigKey(kControlsGroup, QStringLiteral("auto_cue")))),
          m_pAutoHotcue(std::make_unique<ControlPushButton>(
                  ConfigKey(kControlsGroup, QStringLiteral("auto_hotcue")),
                  true,
                  1.0)),
          m_pAutoCueToggle(std::make_unique<ControlPushButton>(
                  ConfigKey(kControlsGroup, QStringLiteral("auto_cue_toggle")))),
          m_pAutoHotcueToggle(std::make_unique<ControlPushButton>(
                  ConfigKey(kControlsGroup, QStringLiteral("auto_hotcue_toggle")))) {
    m_pAutoCue->setButtonMode(mixxx::control::ButtonMode::Toggle);
    m_pAutoHotcue->setButtonMode(mixxx::control::ButtonMode::Toggle);

    m_pAutoCue->set(seekOnLoadPreference(m_pConfig) == SeekOnLoadMode::FirstSound ? 1.0 : 0.0);
    connect(m_pAutoCue.get(),
            &ControlObject::valueChanged,
            this,
            &LoadSettingsControls::slotAutoCueChanged);
    connect(m_pAutoCueToggle.get(),
            &ControlObject::valueChanged,
            this,
            &LoadSettingsControls::slotAutoCueToggle);
    connect(m_pAutoHotcueToggle.get(),
            &ControlObject::valueChanged,
            this,
            &LoadSettingsControls::slotAutoHotcueToggle);
}

LoadSettingsControls::~LoadSettingsControls() = default;

void LoadSettingsControls::slotAutoCueChanged(double value) {
    if (value > 0) {
        m_pConfig->setValue(kCueRecallKey, SeekOnLoadMode::FirstSound);
    } else if (seekOnLoadPreference(m_pConfig) == SeekOnLoadMode::FirstSound) {
        // Only replace the value that AUTO CUE sets; keep a different value
        // that the deck preferences page wrote.
        m_pConfig->setValue(kCueRecallKey, SeekOnLoadMode::MainCue);
    }
}

void LoadSettingsControls::slotAutoCueToggle(double value) {
    if (value <= 0) {
        return;
    }
    const double newValue = m_pAutoCue->toBool() ? 0.0 : 1.0;
    m_pAutoCue->set(newValue);
    // set() from the owner does not emit valueChanged(): write CueRecall here.
    slotAutoCueChanged(newValue);
}

void LoadSettingsControls::slotAutoHotcueToggle(double value) {
    if (value <= 0) {
        return;
    }
    m_pAutoHotcue->set(m_pAutoHotcue->toBool() ? 0.0 : 1.0);
}

// End of AI-generated text.
