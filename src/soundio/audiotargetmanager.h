#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "preferences/usersettings.h"
#include "soundio/sounddevice.h"
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

    /// WASAPI input devices that can carry a microphone: input channels, and
    /// no DJ unit (a DJ unit carries no programme audio) and no loop-back
    /// device (it records the output, so it would feed the mix back).
    QList<SoundDevicePointer> micInputDevices() const;

    /// The device name of Microphone `micIndex` (0..3). On WASAPI it reflects
    /// the live setup as well as the saved one, and a saved device that is not
    /// connected still shows its name. Empty means no input.
    QString micInputDeviceName(int micIndex) const;

    // AI-generated (Claude Code). Start of AI-generated text.
    /// The result of assignMicInput().
    enum class MicAssignResult {
        /// The new input is applied (for None on a mic with no live input,
        /// only its saved keys are removed).
        Applied,
        /// Nothing changed: the sound API is not WASAPI, the index is bad, or
        /// the device is not a usable input device. The old setup is active.
        Rejected,
        /// The device did not open. The old setup is active again.
        PreviousActive,
        /// The device did not open, and the old setup came back only without
        /// its microphone inputs. The main output is active, the mics are off.
        MicrophonesOff,
        /// The device did not open, and no setup came back.
        AudioOff,
    };

    /// Sets the WASAPI input of Microphone `micIndex` (0..3) to channels 1-2
    /// of `device` (channel 1 for a mono device). An empty device id means
    /// None. Call from the GUI thread.
    MicAssignResult assignMicInput(int micIndex, const SoundDeviceId& device);

    /// The text that tells the user that the microphone inputs were removed
    /// to get the audio output back.
    static QString microphonesOffText();
    // End of AI-generated text.

    // AI-generated (Claude Code). Start of AI-generated text.
    /// The two audio targets, for the choice of the buffer size.
    enum class BufferTarget {
        Pc,
        Hardware,
    };

    /// The buffer size index (see SoundManagerConfig::setAudioBufferSizeIndex())
    /// for a new setup of `target` at `deviceRate`. The DDJ-800 (Hardware)
    /// gets 256 frames, as rekordbox uses with it. The PC target keeps
    /// `currentIndex`. Pure, so it needs no sound hardware.
    static unsigned int bufferSizeIndexFor(BufferTarget target,
            mixxx::audio::SampleRate deviceRate,
            unsigned int currentIndex);
    // End of AI-generated text.

    // AI-generated (Claude Code). Start of AI-generated text.
    /// True if a saved setup file belongs to the HARDWARE or the PC target.
    /// A setup with Main on the target's device belongs to it. So does an
    /// External Mixer Mode setup, which has no Main output: the device of its
    /// deck outputs decides. Pure, so it needs no sound hardware.
    static bool acceptsSavedSetup(const SoundManagerConfig& saved, bool hardwareTarget);

    /// Decides when to tell the user about decks without an output: once for
    /// each set of decks, and again after the set was empty. Pure.
    class DecksWithoutOutputNotice {
      public:
        bool shouldTell(const QList<int>& missing) {
            if (missing.isEmpty()) {
                m_last.clear();
                return false;
            }
            if (missing == m_last) {
                return false;
            }
            m_last = missing;
            return true;
        }

      private:
        QList<int> m_last;
    };

    /// The text for decks (0-based) that have no output in External mode.
    static QString decksWithoutOutputText(const QList<int>& decks);
    // End of AI-generated text.

  private slots:
    void slotTargetChanged(double value);
    void slotRoutingChanged(double value);
    void slotSelectHardware(double value);
    void slotSelectPc(double value);
    void slotDevicesSetup();
    // AI-generated (Claude Code). Start of AI-generated text.
    void slotDecksWithoutOutput(const QList<int>& decks);
    // End of AI-generated text.

  private:
    enum class Target {
        Pc = 0,
        Hardware = 1,
    };

    QString configFileName(Target target) const;
    SoundManagerConfig buildConfig(Target target, const SoundManagerConfig& current) const;
    bool switchTo(Target target);
    void select(Target target);
    // Keep the WASAPI Microphone inputs in mixxx.cfg across sound changes.
    void snapshotMicInputs(const SoundManagerConfig& config);
    void restoreMicInputs(SoundManagerConfig* pConfig) const;
    // AI-generated (Claude Code). Start of AI-generated text.
    // Saves the PC device name, only from a PC setup on WASAPI.
    void storePcDeviceName(const SoundManagerConfig& config);

    struct ApplyResult {
        // The config (with or without its mic inputs) is active.
        bool ok = false;
        // The mic inputs were removed to open the other devices.
        bool micsDropped = false;
    };
    // SoundManager::setConfig(). If that fails and the config has Microphone
    // inputs, tries once more without them; their saved keys stay, pending.
    ApplyResult applyConfig(const SoundManagerConfig& config);
    // applyConfig(next); if that fails, applyConfig(old). Returns true if
    // `next` is active. Shows microphonesOffText() if mics were removed.
    bool applyOrGoBack(const SoundManagerConfig& next, const SoundManagerConfig& old);
    // Queued from slotDevicesSetup(): puts the saved mics back into the
    // live WASAPI config.
    void restoreMicInputsNow();
    void showMicrophonesOff();
    QStringList availableMicDevices() const;
    // End of AI-generated text.

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
    // The sound API state last applied: true for WASAPI. slotDevicesSetup()
    // compares it, because the routing control can change before the setup.
    bool m_onWasapi;
    // True while applyConfig() runs: a config it applies has its mic inputs
    // restored (or dropped on purpose) already.
    bool m_applying;
    // AI-generated (Claude Code). Start of AI-generated text.
    DecksWithoutOutputNotice m_decksNotice;
    // True while the last applied setup was a complete External one.
    bool m_wasExternal = false;
    // End of AI-generated text.
};
