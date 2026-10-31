#include "soundio/audiotargetmanager.h"

#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "moc_audiotargetmanager.cpp"
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

} // anonymous namespace

AudioTargetManager::AudioTargetManager(UserSettingsPointer pConfig,
        SoundManager* pSoundManager)
        : m_pConfig(pConfig),
          m_pSoundManager(pSoundManager),
          m_pTarget(std::make_unique<ControlPushButton>(
                  ConfigKey(kMainGroup, QStringLiteral("audio_target")))),
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
    return config;
}

void AudioTargetManager::applyLevels(Target target) const {
    // On HARDWARE the DJ unit applies MASTER, BOOTH and HEADPHONES LEVEL /
    // MIXING, so Mix Two sends them at 0 dB and the headphone output carries
    // cue only (the unit blends in the master). On PC the controller's knobs
    // set these levels when they move.
    ControlObject::set(ConfigKey(kMainGroup, QStringLiteral("gain")), 1.0);
    ControlObject::set(ConfigKey(kMainGroup, QStringLiteral("booth_gain")), 1.0);
    ControlObject::set(ConfigKey(kMainGroup, QStringLiteral("headGain")), 1.0);
    ControlObject::set(ConfigKey(kMainGroup, QStringLiteral("headMix")),
            target == Target::Hardware ? -1.0 : 0.0);
}

void AudioTargetManager::prepareStartup() {
    const SoundManagerConfig current = m_pSoundManager->getConfig();
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
    applyLevels(want);
}

bool AudioTargetManager::switchTo(Target target) {
    const SoundManagerConfig current = m_pSoundManager->getConfig();
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
    applyLevels(target);
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

void AudioTargetManager::slotRoutingChanged(double value) {
    const QString api = value > 0 ? kApiWasapi : SoundManagerConfig::kAPIAsio;
    const SoundManagerConfig current = m_pSoundManager->getConfig();
    if (current.getAPI() == api) {
        return;
    }
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
    applyLevels(m_target);
}
