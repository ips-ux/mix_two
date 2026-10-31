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
/// HARDWARE plays through a DJ unit that applies MASTER, BOOTH and the
/// HEADPHONES knobs in its own hardware (the DDJ-800: Main on 1/2,
/// Headphones on 3/4, Booth on 5/6, as rekordbox sends them). Mix Two's own
/// main, booth and headphone levels then stay at 0 dB and the headphone
/// output carries cue only, so nothing is applied twice.
/// PC plays through another audio device. A controller's level knobs then
/// drive Mix Two's levels (the DDJ-800 mapping reads audio_target).
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

  private:
    enum class Target {
        Pc = 0,
        Hardware = 1,
    };

    QString configFileName(Target target) const;
    SoundManagerConfig buildConfig(Target target, const SoundManagerConfig& current) const;
    void applyLevels(Target target) const;
    bool switchTo(Target target);

    UserSettingsPointer m_pConfig;
    SoundManager* m_pSoundManager;
    std::unique_ptr<ControlPushButton> m_pTarget;
    // [Soundcard],audio_routing: 0 = ASIO, 1 = WASAPI (Windows). Set by
    // Preferences > Sound Hardware > Audio Routing.
    std::unique_ptr<ControlObject> m_pRouting;
    Target m_target;
};
