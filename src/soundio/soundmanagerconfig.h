#pragma once

#ifndef SOUNDMANAGERCONFIG_FILENAME
#define SOUNDMANAGERCONFIG_FILENAME "soundconfig.xml"
#endif

#include <QDomDocument>
#include <QFileInfo>
#include <QList>
#include <QMultiHash>
#include <QString>
#include <QStringList>

#include "audio/types.h"
#include "soundio/soundmanagerutil.h"

class SoundManager;

class SoundManagerConfig {
  public:
    explicit SoundManagerConfig(
            SoundManager* pSoundManager);

    enum Defaults {
        API = (1 << 0),
        DEVICES = (1 << 1),
        OTHER = (1 << 2),
        ALL = (API | DEVICES | OTHER),
    };

    // Size1xms presents the first buffer size of 2^X
    // that results in a buffer time above 1 ms
    // It is 1.45 ms @ 44.1 kHz
    // The other values are representing the following 2^X sizes.
    enum class AudioBufferSizeIndex {
        Size1xms = 1,
        Size2xms = 2,
        Size5xms = 3,
        Size10xms = 4,
        Size20xms = 5,
        Size40xms = 6,
        Size80xms = 7,
    };

    // Represents the sample rate independent frame/period
    // index values in case of Jack
    enum class JackAudioBufferSizeIndex {
        SizeAuto = 5,
        Size2048fpp = 6,
        Size4096fpp = 7,
    };

    static constexpr auto kMaxAudioBufferSizeIndex =
            static_cast<unsigned int>(AudioBufferSizeIndex::Size80xms);
    static constexpr auto kDefaultAudioBufferSizeIndex =
            static_cast<unsigned int>(AudioBufferSizeIndex::Size20xms);

    static const QString kAPINone;
    static const QString kEmptyComboBox;
    static const QString kAPIJack;
    static const QString kAPIAlsa;
    static const QString kAPIOss;
    static const QString kAPIAsio;
    static const QString kAPIDirectSound;
    static const QString kAPIIosAudio;
    static const QString kAPICoreAudio;
    static const QString kAPIPipewire;

    /// The default sample rate that Mixxx uses.
    static constexpr mixxx::audio::SampleRate kMixxxDefaultSampleRate =
            mixxx::audio::SampleRate(44100);
    /// A sample rate that likely every soundcard supports, even cheap ones.
    static constexpr mixxx::audio::SampleRate kFallbackSampleRate = mixxx::audio::SampleRate(48000);
    static const unsigned int kDefaultDeckCount;
    static const int kDefaultSyncBuffers;

    /// Mix Two: read and write `fileName` in the settings folder instead of
    /// soundconfig.xml (the HARDWARE / PC targets keep one file each).
    void setFileName(const QString& fileName);
    bool readFromDisk();
    // NOTE(pri-yan-shu) separate config reading and device validation, to allow
    // asynchronous API's like PipeWire to validate the devices
    // after they are enumerated, while keeping the logic which expects
    // other parts of the config to already be loaded.
    bool validateDevices();
    bool writeToDisk() const;
    QString getAPI() const;
    void setAPI(const QString& api);
    bool checkAPI();
    mixxx::audio::SampleRate getSampleRate() const;
    void setSampleRate(mixxx::audio::SampleRate sampleRate);
    bool checkSampleRate(const SoundManager& soundManager);

    // Record the number of decks configured with this setup so they can
    // be created and configured.
    unsigned int getDeckCount() const;
    void setDeckCount(unsigned int deckCount);
    void setCorrectDeckCount(int configuredDeckCount);
    QSet<SoundDeviceId> getDevices() const;

    unsigned int getAudioBufferSizeIndex() const;
    unsigned int getFramesPerBuffer() const;
    void setAudioBufferSizeIndex(unsigned int latency);
    unsigned int getSyncBuffers() const;
    void setSyncBuffers(unsigned int syncBuffers);
    bool getForceNetworkClock() const;
    void setForceNetworkClock(bool force);
    void addOutput(const SoundDeviceId& device, const AudioOutput& out);
    void addInput(const SoundDeviceId& device, const AudioInput& in);
    QMultiHash<SoundDeviceId, AudioOutput> getOutputs() const;
    QMultiHash<SoundDeviceId, AudioInput> getInputs() const;
    QMultiHash<SoundDeviceId, AudioOutput>& getOutputsRef();
    QMultiHash<SoundDeviceId, AudioInput>& getInputsRef();
    void clearOutputs();
    void clearInputs();
    bool hasMicInputs();
    bool hasExternalRecordBroadcast();
    void loadDefaults(SoundManager* soundManager, unsigned int flags);

    // AI-generated (Claude Code). Start of AI-generated text.
    /// Mixer Mode: Internal sends the main mix to the Main output. External
    /// gives every deck its own stereo output and turns the main mix off.
    enum class MixerMode {
        Internal,
        External,
    };

    enum class MixerModeStatus {
        Ok,
        /// The sound device has fewer than two channels for each deck.
        TooFewChannels,
        /// No output device is set, so there is nowhere to put the decks.
        NoDevice,
    };

    struct MixerModeChange {
        MixerModeStatus status = MixerModeStatus::Ok;
        /// The outputs that External removed (the user text names them).
        /// Internal brings them back.
        QStringList droppedNames;
        /// Internal found no usable Main output in the saved setup, so it put
        /// Main on channels 1 and 2 of `fallbackDevice`.
        bool mainFallback = false;
        QString fallbackDevice;
    };

    /// What the outputs really are, whatever the stored mode says.
    enum class MixerSetup {
        /// Main output, no deck output.
        Internal,
        /// An output for every deck, and no Main, Headphones or Booth output
        /// (the engine turns the main mix on for each of those).
        External,
        /// Anything else.
        Custom,
    };

    struct MixerSetupState {
        MixerSetup setup = MixerSetup::Internal;
        /// Not empty when the stored mode is External, there is no Main mix
        /// output, and these decks (0-based) have no output.
        QList<int> decksWithoutOutput;
    };

    /// Pure: decides the setup from the outputs.
    static MixerSetupState analyzeMixerSetup(
            const QMultiHash<SoundDeviceId, AudioOutput>& outputs,
            unsigned int deckCount,
            bool storedExternal);
    MixerSetupState mixerSetupState() const;

    /// Sets the stored mode from the real setup: Internal when the outputs
    /// are Internal, External when they are External.
    void syncMixerModeToOutputs();

    /// Matches the saved pre-External outputs to the devices that exist now,
    /// by device name, and drops the ones whose device is gone. Returns the
    /// number dropped.
    int repairSavedOutputs(const QList<SoundDeviceId>& present);

    /// True if the main mix is on in `mode`.
    static bool mainMixEnabledFor(MixerMode mode);

    MixerMode getMixerMode() const;

    /// Changes the outputs to `mode`. `deviceOutputChannels` is the number of
    /// output channels of the device that carries the main output (External)
    /// or the deck outputs (Internal). If the device is too small, nothing
    /// changes and the status says why.
    MixerModeChange setMixerMode(MixerMode mode, int deviceOutputChannels);

    /// The device that carries the main output, or the deck outputs, or else
    /// the first output device. An empty id if there is none.
    SoundDeviceId mixerModeDevice() const;

    /// Back to Internal, and forget the saved outputs.
    void resetMixerMode();

    void writeMixerModeXml(QDomDocument* pDoc, QDomElement* pRoot) const;
    void readMixerModeXml(const QDomElement& root);
    // End of AI-generated text.

  private:
    QFileInfo m_configFile;
    QString m_api;
    // none of our sample rates are actually decimals, this avoids
    // the weirdness using floating point can introduce
    mixxx::audio::SampleRate m_sampleRate;
    unsigned int m_deckCount;
    // m_latency is an index > 0, where 1 is a latency of 1ms and
    // higher indices represent subsequently higher latencies (storing
    // latency as milliseconds or frames per buffer is bad because those
    // values vary with sample rate) -- bkgood
    unsigned int m_audioBufferSizeIndex;
    unsigned int m_syncBuffers;
    bool m_forceNetworkClock;
    QMultiHash<SoundDeviceId, AudioOutput> m_outputs;
    QMultiHash<SoundDeviceId, AudioInput> m_inputs;
    int m_iNumMicInputs;
    // AI-generated (Claude Code). Start of AI-generated text.
    MixerMode m_mixerMode;
    // The outputs from before External was chosen. Internal restores them.
    QMultiHash<SoundDeviceId, AudioOutput> m_internalOutputs;
    // End of AI-generated text.
    bool m_bExternalRecordBroadcastConnected;
    SoundManager* m_pSoundManager;
    QDomDocument m_doc;
};
