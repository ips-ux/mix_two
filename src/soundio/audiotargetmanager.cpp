#include "soundio/audiotargetmanager.h"

#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "mixer/playermanager.h"
#include "moc_audiotargetmanager.cpp"
#include "soundio/micinputassignments.h"
#include "soundio/sounddevice.h"
#include "soundio/soundmanager.h"

namespace {

const QString kMainGroup = QStringLiteral("[Master]");
const QString kSoundcardGroup = QStringLiteral("[Soundcard]");
const ConfigKey kTargetKey(kSoundcardGroup, QStringLiteral("audio_target"));
// The PC device's name, to find the same interface again under another
// sound API (for example "Audient USB Audio ASIO Driver" under ASIO and
// "Main Output 1/2 (Audient EVO4)" under WASAPI).
const ConfigKey kPcDeviceKey(kSoundcardGroup, QStringLiteral("audio_target_pc_device"));

// DJ units that apply MASTER, BOOTH and HEADPHONES in hardware on their own
// outputs: Main 1/2, Headphones 3/4, Booth 5/6.
const QStringList kHardwareUnits = {QStringLiteral("DDJ-800")};

const QString kApiWasapi = QStringLiteral("Windows WASAPI");

bool isHardwareUnit(const QString& deviceName) {
    for (const auto& unit : kHardwareUnits) {
        if (deviceName.contains(unit, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool isLoopback(const QString& deviceName) {
    return deviceName.contains(QStringLiteral("loop-back"), Qt::CaseInsensitive) ||
            deviceName.contains(QStringLiteral("loopback"), Qt::CaseInsensitive);
}

QString mainDeviceName(const SoundManagerConfig& config) {
    const auto outputs = config.getOutputs();
    for (auto it = outputs.constBegin(); it != outputs.constEnd(); ++it) {
        if (it.value().getType() == AudioPathType::Main) {
            return it.key().name;
        }
    }
    return QString();
}

bool usesHardwareUnit(const SoundManagerConfig& config) {
    return isHardwareUnit(mainDeviceName(config));
}

bool hasMainOutput(const SoundManagerConfig& config) {
    return !mainDeviceName(config).isEmpty();
}

/// The Microphone inputs of `config`, as assignments.
micinputs::Assignments liveMicInputs(const SoundManagerConfig& config) {
    micinputs::Assignments live;
    const auto inputs = config.getInputs();
    for (auto it = inputs.constBegin(); it != inputs.constEnd(); ++it) {
        const AudioInput& input = it.value();
        const int micIndex = input.getIndex();
        if (input.getType() != AudioPathType::Microphone ||
                micIndex < 0 || micIndex >= micinputs::kNumMics) {
            continue;
        }
        micinputs::Assignment assignment;
        assignment.deviceName = it.key().name;
        assignment.channelBase = input.getChannelGroup().getChannelBase();
        assignment.channelCount = input.getChannelGroup().getChannelCount().value();
        live.insert(micIndex, assignment);
    }
    return live;
}

} // anonymous namespace

AudioTargetManager::AudioTargetManager(UserSettingsPointer pConfig,
        SoundManager* pSoundManager)
        : m_pConfig(pConfig),
          m_pSoundManager(pSoundManager),
          m_pTarget(std::make_unique<ControlPushButton>(
                  ConfigKey(kMainGroup, QStringLiteral("audio_target")))),
          m_pSelectHardware(std::make_unique<ControlPushButton>(
                  ConfigKey(kMainGroup, QStringLiteral("audio_target_hardware")))),
          m_pSelectPc(std::make_unique<ControlPushButton>(
                  ConfigKey(kMainGroup, QStringLiteral("audio_target_pc")))),
          m_pRouting(std::make_unique<ControlObject>(
                  ConfigKey(kSoundcardGroup, QStringLiteral("audio_routing")))),
          m_target(Target::Hardware) {
    m_pRouting->set(m_pSoundManager->getConfig().getAPI() == kApiWasapi ? 1.0 : 0.0);
    connect(m_pRouting.get(),
            &ControlObject::valueChanged,
            this,
            &AudioTargetManager::slotRoutingChanged);
    m_pTarget->setButtonMode(mixxx::control::ButtonMode::Toggle);
    // Default: HARDWARE, as rekordbox plays through the DJ unit.
    m_target = m_pConfig->getValue(kTargetKey, 1) > 0 ? Target::Hardware : Target::Pc;
    m_pTarget->set(m_target == Target::Hardware ? 1.0 : 0.0);
    connect(m_pTarget.get(),
            &ControlObject::valueChanged,
            this,
            &AudioTargetManager::slotTargetChanged);
    connect(m_pSelectHardware.get(),
            &ControlObject::valueChanged,
            this,
            &AudioTargetManager::slotSelectHardware);
    connect(m_pSelectPc.get(),
            &ControlObject::valueChanged,
            this,
            &AudioTargetManager::slotSelectPc);
    connect(m_pSoundManager,
            &SoundManager::devicesSetup,
            this,
            &AudioTargetManager::slotDevicesSetup);
}

AudioTargetManager::~AudioTargetManager() = default;

QString AudioTargetManager::configFileName(Target target) const {
    return target == Target::Hardware
            ? QStringLiteral("soundconfig_hardware.xml")
            : QStringLiteral("soundconfig_pc.xml");
}

bool AudioTargetManager::hasHardwareDevice(const QString& api) const {
    const auto devices = m_pSoundManager->getDeviceList(api, true, false);
    for (const auto& pDevice : devices) {
        if (isHardwareUnit(pDevice->getDeviceId().name) &&
                pDevice->getNumOutputChannels() >= 2) {
            return true;
        }
    }
    return false;
}

SoundManagerConfig AudioTargetManager::buildConfig(
        Target target, const SoundManagerConfig& current) const {
    const QString api = current.getAPI();

    // The target's own saved setup, when it is for the active sound API and
    // its device is present.
    SoundManagerConfig saved(m_pSoundManager);
    saved.setFileName(configFileName(target));
    if (saved.readFromDisk() && saved.getAPI() == api) {
        saved.validateDevices();
        if (hasMainOutput(saved) &&
                usesHardwareUnit(saved) == (target == Target::Hardware)) {
            if (api == kApiWasapi) {
                restoreMicInputs(&saved);
            }
            return saved;
        }
    }

    // Otherwise a default setup on the active API, with the current sample
    // rate and buffer.
    SoundManagerConfig config(m_pSoundManager);
    config.setAPI(api);
    config.setSampleRate(current.getSampleRate());
    config.setAudioBufferSizeIndex(current.getAudioBufferSizeIndex());
    config.setSyncBuffers(current.getSyncBuffers());
    config.setDeckCount(current.getDeckCount());
    config.setForceNetworkClock(current.getForceNetworkClock());

    const auto devices = m_pSoundManager->getDeviceList(api, true, false);
    SoundDevicePointer pChosen;
    if (target == Target::Hardware) {
        for (const auto& pDevice : devices) {
            if (isHardwareUnit(pDevice->getDeviceId().name) &&
                    pDevice->getNumOutputChannels() >= 2) {
                pChosen = pDevice;
                break;
            }
        }
    } else {
        // The PC interface used before (same name, or the same maker under
        // another API), else the first non-loop-back interface that is not
        // a hardware DJ unit. "Main" and "Speakers" outputs come first.
        const QString lastName = m_pConfig->getValueString(kPcDeviceKey);
        const QString maker = lastName.section(QChar(' '), 0, 0);
        int bestScore = -1;
        for (const auto& pDevice : devices) {
            const QString name = pDevice->getDeviceId().name;
            if (isHardwareUnit(name) || isLoopback(name) ||
                    pDevice->getNumOutputChannels() < 2) {
                continue;
            }
            int score = 0;
            if (!lastName.isEmpty() && name == lastName) {
                score += 8;
            }
            if (maker.size() > 2 && name.contains(maker, Qt::CaseInsensitive)) {
                score += 4;
            }
            if (name.contains(QStringLiteral("Main"), Qt::CaseInsensitive) ||
                    name.contains(QStringLiteral("Speaker"), Qt::CaseInsensitive)) {
                score += 1;
            }
            if (score > bestScore) {
                bestScore = score;
                pChosen = pDevice;
            }
        }
    }
    if (!pChosen) {
        if (api == kApiWasapi) {
            restoreMicInputs(&config);
        }
        return config;
    }
    // A new device opens at its own rate: the DDJ-800's ASIO driver, for
    // example, refuses 48 kHz when its utility is set to 44.1 kHz.
    if (pChosen->getDefaultSampleRate().isValid()) {
        config.setSampleRate(pChosen->getDefaultSampleRate());
    }
    const auto channels = pChosen->getNumOutputChannels();
    config.addOutput(pChosen->getDeviceId(),
            AudioOutput(AudioPathType::Main, 0, mixxx::audio::ChannelCount::stereo(), 0));
    if (target == Target::Hardware) {
        if (channels >= 4) {
            config.addOutput(pChosen->getDeviceId(),
                    AudioOutput(AudioPathType::Headphones,
                            2,
                            mixxx::audio::ChannelCount::stereo(),
                            0));
        }
        if (channels >= 6) {
            config.addOutput(pChosen->getDeviceId(),
                    AudioOutput(AudioPathType::Booth,
                            4,
                            mixxx::audio::ChannelCount::stereo(),
                            0));
        }
    }
    if (api == kApiWasapi) {
        restoreMicInputs(&config);
    }
    return config;
}

void AudioTargetManager::prepareStartup() {
    const SoundManagerConfig current = m_pSoundManager->getConfig();
    snapshotMicInputs(current);
    Target want = m_target;
    if (want == Target::Hardware && !hasHardwareDevice(current.getAPI())) {
        qInfo() << "AudioTargetManager: no hardware DJ unit found, starting on PC";
        want = Target::Pc;
    }

    const bool currentIsHardware = usesHardwareUnit(current);
    const QString lastPcName = m_pConfig->getValueString(kPcDeviceKey);
    const bool currentIsSavedPc = !currentIsHardware &&
            (lastPcName.isEmpty() || mainDeviceName(current) == lastPcName);
    const bool keepCurrent = (want == Target::Hardware)
            ? currentIsHardware
            : currentIsSavedPc;

    if (!keepCurrent) {
        if (hasMainOutput(current) && (currentIsHardware || currentIsSavedPc)) {
            // Keep the other target's setup for later.
            SoundManagerConfig saved = current;
            saved.setFileName(configFileName(currentIsHardware ? Target::Hardware : Target::Pc));
            saved.writeToDisk();
            if (!currentIsHardware) {
                m_pConfig->setValue(kPcDeviceKey, mainDeviceName(current));
            }
        }
        SoundManagerConfig next = buildConfig(want, current);
        if (hasMainOutput(next)) {
            next.setFileName(QStringLiteral(SOUNDMANAGERCONFIG_FILENAME));
            m_pSoundManager->setConfigBeforeSetup(next);
        } else {
            want = currentIsHardware ? Target::Hardware : Target::Pc;
        }
    }

    m_target = want;
    m_pTarget->set(want == Target::Hardware ? 1.0 : 0.0);
}

bool AudioTargetManager::switchTo(Target target) {
    const SoundManagerConfig current = m_pSoundManager->getConfig();
    snapshotMicInputs(current);
    if (m_target == Target::Pc && hasMainOutput(current)) {
        m_pConfig->setValue(kPcDeviceKey, mainDeviceName(current));
    }
    SoundManagerConfig saved = current;
    saved.setFileName(configFileName(m_target));
    saved.writeToDisk();

    SoundManagerConfig next = buildConfig(target, current);
    if (!hasMainOutput(next)) {
        qWarning() << "AudioTargetManager: no output device for the"
                   << (target == Target::Hardware ? "HARDWARE" : "PC") << "target";
        return false;
    }
    next.setFileName(QStringLiteral(SOUNDMANAGERCONFIG_FILENAME));
    if (m_pSoundManager->setConfig(next) != SoundDeviceStatus::Ok) {
        qWarning() << "AudioTargetManager: could not open the"
                   << (target == Target::Hardware ? "HARDWARE" : "PC")
                   << "device, going back";
        m_pSoundManager->setConfig(current);
        return false;
    }
    m_target = target;
    m_pConfig->setValue(kTargetKey, target == Target::Hardware ? 1 : 0);
    return true;
}

void AudioTargetManager::slotTargetChanged(double value) {
    const Target target = value > 0 ? Target::Hardware : Target::Pc;
    if (target == m_target) {
        return;
    }
    if (!switchTo(target)) {
        m_pTarget->set(m_target == Target::Hardware ? 1.0 : 0.0);
    }
}

void AudioTargetManager::select(Target target) {
    if (target == m_target) {
        return;
    }
    if (switchTo(target)) {
        // Set by its owner, so slotTargetChanged() is not called again.
        m_pTarget->set(target == Target::Hardware ? 1.0 : 0.0);
    }
}

void AudioTargetManager::slotSelectHardware(double value) {
    if (value > 0) {
        select(Target::Hardware);
    }
}

void AudioTargetManager::slotSelectPc(double value) {
    if (value > 0) {
        select(Target::Pc);
    }
}

void AudioTargetManager::slotRoutingChanged(double value) {
    const QString api = value > 0 ? kApiWasapi : SoundManagerConfig::kAPIAsio;
    const SoundManagerConfig current = m_pSoundManager->getConfig();
    if (current.getAPI() == api) {
        return;
    }
    snapshotMicInputs(current);
    if (m_target == Target::Pc && hasMainOutput(current)) {
        m_pConfig->setValue(kPcDeviceKey, mainDeviceName(current));
    }
    SoundManagerConfig saved = current;
    saved.setFileName(configFileName(m_target));
    saved.writeToDisk();

    Target target = m_target;
    SoundManagerConfig onNewApi = current;
    onNewApi.setAPI(api);
    if (target == Target::Hardware && !hasHardwareDevice(api)) {
        target = Target::Pc;
    }
    SoundManagerConfig next = buildConfig(target, onNewApi);
    next.setFileName(QStringLiteral(SOUNDMANAGERCONFIG_FILENAME));
    if (!hasMainOutput(next) || m_pSoundManager->setConfig(next) != SoundDeviceStatus::Ok) {
        qWarning() << "AudioTargetManager: could not open a device on" << api << ", going back";
        m_pSoundManager->setConfig(current);
        m_pRouting->set(current.getAPI() == kApiWasapi ? 1.0 : 0.0);
        return;
    }
    if (target != m_target) {
        m_target = target;
        m_pTarget->set(target == Target::Hardware ? 1.0 : 0.0);
    }
}

void AudioTargetManager::slotDevicesSetup() {
    const bool wasapi = m_pSoundManager->getConfig().getAPI() == kApiWasapi;
    const bool wasWasapi = m_pRouting->get() > 0;
    if (wasapi == wasWasapi) {
        return;
    }
    m_pRouting->set(wasapi ? 1.0 : 0.0);
    if (wasWasapi) {
        // The four squares are gone, so no talk-over may stay on.
        for (int i = 0; i < micinputs::kNumMics; ++i) {
            ControlObject::set(
                    ConfigKey(PlayerManager::groupForMicrophone(i),
                            QStringLiteral("talkover")),
                    0.0);
        }
    }
}

QList<SoundDevicePointer> AudioTargetManager::micInputDevices() const {
    QList<SoundDevicePointer> result;
    const auto devices = m_pSoundManager->getDeviceList(kApiWasapi, false, true);
    for (const auto& pDevice : devices) {
        if (pDevice->getNumInputChannels().isValid() &&
                !isHardwareUnit(pDevice->getDeviceId().name)) {
            result.append(pDevice);
        }
    }
    return result;
}

QString AudioTargetManager::micInputDeviceName(int micIndex) const {
    return micinputs::load(m_pConfig).value(micIndex).deviceName;
}

bool AudioTargetManager::assignMicInput(int micIndex, const SoundDeviceId& device) {
    if (micIndex < 0 || micIndex >= micinputs::kNumMics) {
        return false;
    }
    const SoundManagerConfig old = m_pSoundManager->getConfig();
    if (old.getAPI() != kApiWasapi) {
        return false;
    }

    micinputs::Assignment assignment;
    const bool isNone = device.name.isEmpty();
    if (!isNone) {
        SoundDevicePointer pChosen;
        const auto devices = m_pSoundManager->getDeviceList(kApiWasapi, false, true);
        for (const auto& pDevice : devices) {
            if (pDevice->getDeviceId() == device) {
                pChosen = pDevice;
                break;
            }
        }
        if (!pChosen) {
            return false;
        }
        assignment.deviceName = device.name;
        assignment.channelBase = 0;
        assignment.channelCount = micinputs::channelCountFor(
                pChosen->getNumInputChannels().value());
    }

    SoundManagerConfig next = old;
    auto& inputs = next.getInputsRef();
    for (auto it = inputs.begin(); it != inputs.end();) {
        if (it.value().getType() == AudioPathType::Microphone &&
                it.value().getIndex() == micIndex) {
            it = inputs.erase(it);
        } else {
            ++it;
        }
    }
    if (!isNone) {
        next.addInput(device,
                AudioInput(AudioPathType::Microphone,
                        static_cast<unsigned char>(assignment.channelBase),
                        mixxx::audio::ChannelCount(assignment.channelCount),
                        static_cast<unsigned char>(micIndex)));
    }

    if (m_pSoundManager->setConfig(next) != SoundDeviceStatus::Ok) {
        qWarning() << "AudioTargetManager: could not open the input for MIC"
                   << (micIndex + 1) << ", going back";
        m_pSoundManager->setConfig(old);
        return false;
    }

    // The other mics keep their keys, this one gets what was applied.
    micinputs::Assignments assignments = micinputs::load(m_pConfig);
    if (isNone) {
        assignments.remove(micIndex);
        ControlObject::set(
                ConfigKey(PlayerManager::groupForMicrophone(micIndex),
                        QStringLiteral("talkover")),
                0.0);
    } else {
        assignments.insert(micIndex, assignment);
    }
    micinputs::save(m_pConfig, assignments);
    return true;
}

void AudioTargetManager::snapshotMicInputs(const SoundManagerConfig& config) {
    if (config.getAPI() != kApiWasapi) {
        return;
    }
    QStringList available;
    const auto devices = m_pSoundManager->getDeviceList(kApiWasapi, false, true);
    for (const auto& pDevice : devices) {
        available.append(pDevice->getDeviceId().name);
    }
    micinputs::save(m_pConfig,
            micinputs::merge(micinputs::load(m_pConfig),
                    liveMicInputs(config),
                    available));
}

void AudioTargetManager::restoreMicInputs(SoundManagerConfig* pConfig) const {
    auto& inputs = pConfig->getInputsRef();
    for (auto it = inputs.begin(); it != inputs.end();) {
        if (it.value().getType() == AudioPathType::Microphone) {
            it = inputs.erase(it);
        } else {
            ++it;
        }
    }
    const auto devices = m_pSoundManager->getDeviceList(kApiWasapi, false, true);
    const auto assignments = micinputs::load(m_pConfig);
    for (auto it = assignments.constBegin(); it != assignments.constEnd(); ++it) {
        const micinputs::Assignment& assignment = it.value();
        for (const auto& pDevice : devices) {
            if (pDevice->getDeviceId().name != assignment.deviceName) {
                continue;
            }
            // Too few channels: add nothing, the keys stay.
            if (pDevice->getNumInputChannels().value() >=
                    assignment.channelBase + assignment.channelCount) {
                pConfig->addInput(pDevice->getDeviceId(),
                        AudioInput(AudioPathType::Microphone,
                                static_cast<unsigned char>(assignment.channelBase),
                                mixxx::audio::ChannelCount(assignment.channelCount),
                                static_cast<unsigned char>(it.key())));
            }
            break;
        }
    }
}
