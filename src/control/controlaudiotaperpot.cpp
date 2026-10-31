#include "control/controlaudiotaperpot.h"

#include "moc_controlaudiotaperpot.cpp"

ControlAudioTaperPot::ControlAudioTaperPot(const ConfigKey& key,
        double minDB,
        double maxDB,
        double neutralParameter,
        bool bPersist)
        : ControlPotmeter(key, 0.0, 1.0, false, true, false, bPersist, 1.0) {
    // Override ControlPotmeters default value of 0.5
    setDefaultValue(1.0);
    if (!bPersist) {
        set(1.0);
    }

    if (m_pControl) {
        m_pControl->setBehavior(
                new ControlAudioTaperPotBehavior(minDB, maxDB,
                        neutralParameter));
    }
}
