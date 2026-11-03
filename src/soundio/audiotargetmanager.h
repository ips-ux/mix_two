#pragma once

#include <QObject>
#include <memory>

#include "preferences/usersettings.h"
#include "soundio/soundmanagerconfig.h"

class ControlObject;
class ControlPushButton;
class SoundManager;

/// Mix Two: the HARDWARE / PC audio target, [Master],audio_target
/// (1 = HARDWARE, 0 = PC).
///
/// HARDWARE plays through the DJ unit (the DDJ-800: Main on 1/2, Headphones
/// on 3/4, Booth on 5/6, as rekordbox sends them). PC plays through another
/// audio device. With both, the controller's MASTER, BOOTH and HEADPHONES
/// knobs drive Mix Two's levels: the DDJ-800 does not apply them to the
/// sound it gets from Mix Two (owner's test, 2026-10-31).
///
/// One target plays at a time. Each keeps its own sound configuration file,
/// so a change in Preferences > Sound Hardware belongs to the active target.
/// Both targets use the active sound API, which the Audio Routing setting
/// selects (ASIO or WASAPI on Windows).
class AudioTargetManager : public QObject {
    Q_OBJECT
  public:
    /// `pSoundManager` must outlive this object.
    AudioTargetManager(UserSettingsPointer pConfig, SoundManager* pSoundManager);
    ~AudioTargetManager() override;

    /// Call before the first SoundManager::setupDevices(). With HARDWARE
    /// selected and no hardware device present, starts on PC instead.
    void prepareStartup();

    /// True if a DJ unit for the HARDWARE target is present for `api`.
    bool hasHardwareDevice(const QString& api) const;

  private slots:
    void slotTargetChanged(double value);
    void slotRoutingChanged(double value);
    void slotSelectHardware(double value);
    void slotSelectPc(double value);

  private:
    enum class Target {
        Pc = 0,
        Hardware = 1,
    };

    QString configFileName(Target target) const;
    SoundManagerConfig buildConfig(Target target, const SoundManagerConfig& current) const;
    bool switchTo(Target target);
    void select(Target target);

    UserSettingsPointer m_pConfig;
    SoundManager* m_pSoundManager;
    std::unique_ptr<ControlPushButton> m_pTarget;
    // [Master],audio_target_hardware / audio_target_pc: push buttons for
    // the toolbar's HARDWARE | PC toggle. Selecting the active side does
    // nothing (audio_target itself toggles).
    std::unique_ptr<ControlPushButton> m_pSelectHardware;
    std::unique_ptr<ControlPushButton> m_pSelectPc;
    // [Soundcard],audio_routing: 0 = ASIO, 1 = WASAPI (Windows). Set by
    // Preferences > Sound Hardware > Audio Routing.
    std::unique_ptr<ControlObject> m_pRouting;
    Target m_target;
};
