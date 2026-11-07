// AI-generated (Claude Code). Start of AI-generated text.
#include "soundio/micinputassignments.h"

#include "preferences/configobject.h"

namespace micinputs {

namespace {

const QString kGroup = QStringLiteral("[Soundcard]");

ConfigKey keyFor(int index, const char* suffix) {
    return ConfigKey(kGroup,
            QStringLiteral("mic_input_%1_%2").arg(index + 1).arg(QLatin1String(suffix)));
}

} // namespace

Assignments merge(const Assignments& saved,
        const Assignments& live,
        const QStringList& availableDevices) {
    Assignments result = live;
    for (auto it = saved.constBegin(); it != saved.constEnd(); ++it) {
        if (result.contains(it.key())) {
            continue;
        }
        if (!availableDevices.contains(it.value().deviceName)) {
            result.insert(it.key(), it.value());
        }
    }
    return result;
}

int channelCountFor(int deviceInputChannels) {
    return deviceInputChannels == 1 ? 1 : 2;
}

Assignments load(const UserSettingsPointer& pConfig) {
    Assignments result;
    if (!pConfig) {
        return result;
    }
    for (int index = 0; index < kNumMics; ++index) {
        const ConfigKey deviceKey = keyFor(index, "device");
        const ConfigKey baseKey = keyFor(index, "channel");
        const ConfigKey countKey = keyFor(index, "channels");
        if (!pConfig->exists(deviceKey) ||
                !pConfig->exists(baseKey) ||
                !pConfig->exists(countKey)) {
            continue;
        }
        Assignment assignment;
        assignment.deviceName = pConfig->getValueString(deviceKey);
        bool baseOk = false;
        bool countOk = false;
        assignment.channelBase = pConfig->getValueString(baseKey).toInt(&baseOk);
        assignment.channelCount = pConfig->getValueString(countKey).toInt(&countOk);
        if (assignment.deviceName.isEmpty() ||
                !baseOk || assignment.channelBase < 0 ||
                !countOk ||
                (assignment.channelCount != 1 && assignment.channelCount != 2)) {
            continue;
        }
        result.insert(index, assignment);
    }
    return result;
}

void save(const UserSettingsPointer& pConfig, const Assignments& assignments) {
    if (!pConfig) {
        return;
    }
    for (int index = 0; index < kNumMics; ++index) {
        const ConfigKey deviceKey = keyFor(index, "device");
        const ConfigKey baseKey = keyFor(index, "channel");
        const ConfigKey countKey = keyFor(index, "channels");
        const auto it = assignments.constFind(index);
        if (it == assignments.constEnd()) {
            pConfig->remove(deviceKey);
            pConfig->remove(baseKey);
            pConfig->remove(countKey);
            continue;
        }
        pConfig->setValue(deviceKey, it.value().deviceName);
        pConfig->setValue(baseKey, it.value().channelBase);
        pConfig->setValue(countKey, it.value().channelCount);
    }
}

} // namespace micinputs
// End of AI-generated text.
