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

bool isExcludedDevice(const QString& deviceName) {
    return deviceName.contains(QStringLiteral("DDJ-800"), Qt::CaseInsensitive) ||
            deviceName.contains(QStringLiteral("loopback"), Qt::CaseInsensitive) ||
            deviceName.contains(QStringLiteral("loop-back"), Qt::CaseInsensitive);
}

Assignments merge(const Assignments& saved,
        const Assignments& live,
        const QStringList& availableDevices) {
    Assignments result;
    for (auto it = live.constBegin(); it != live.constEnd(); ++it) {
        if (!isExcludedDevice(it.value().deviceName)) {
            result.insert(it.key(), it.value());
        }
    }
    for (auto it = saved.constBegin(); it != saved.constEnd(); ++it) {
        if (live.contains(it.key()) || isExcludedDevice(it.value().deviceName)) {
            continue;
        }
        Assignment kept = it.value();
        if (!availableDevices.contains(kept.deviceName)) {
            kept.pending = true;
            result.insert(it.key(), kept);
        } else if (kept.pending) {
            result.insert(it.key(), kept);
        }
    }
    return result;
}

Assignments keepOnReturn(const Assignments& saved, const Assignments& live) {
    Assignments result = saved;
    for (auto it = result.begin(); it != result.end(); ++it) {
        if (!live.contains(it.key())) {
            it.value().pending = true;
        }
    }
    return result;
}

bool needsRestore(const Assignments& assignments, const QStringList& availableDevices) {
    for (const Assignment& assignment : assignments) {
        if (assignment.pending &&
                !isExcludedDevice(assignment.deviceName) &&
                availableDevices.contains(assignment.deviceName)) {
            return true;
        }
    }
    return false;
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
        // A missing or bad value means false.
        assignment.pending = pConfig->getValueString(keyFor(index, "pending")).toInt() != 0;
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
            pConfig->remove(keyFor(index, "pending"));
            continue;
        }
        pConfig->setValue(deviceKey, it.value().deviceName);
        pConfig->setValue(baseKey, it.value().channelBase);
        pConfig->setValue(countKey, it.value().channelCount);
        pConfig->setValue(keyFor(index, "pending"), it.value().pending ? 1 : 0);
    }
}

} // namespace micinputs
// End of AI-generated text.
