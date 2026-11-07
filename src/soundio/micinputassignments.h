// AI-generated (Claude Code). Start of AI-generated text.
#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

#include "preferences/usersettings.h"

/// Mix Two: rules to keep the WASAPI Microphone input assignments in
/// mixxx.cfg, so a change of sound device or API does not lose them.
/// No sound device is used here, so the rules can be tested alone.
namespace micinputs {

constexpr int kNumMics = 4;

struct Assignment {
    QString deviceName;
    int channelBase = 0;
    int channelCount = 2;

    bool operator==(const Assignment& other) const {
        return deviceName == other.deviceName &&
                channelBase == other.channelBase &&
                channelCount == other.channelCount;
    }
};

/// Key: mic index 0..3.
using Assignments = QMap<int, Assignment>;

/// Starts from `live`. A saved mic that has no live input stays only when its
/// device is not in `availableDevices` (the device is gone, so the user did
/// not clear it). If the device is present, the user cleared it: it is dropped.
Assignments merge(const Assignments& saved,
        const Assignments& live,
        const QStringList& availableDevices);

/// 1 input channel gives 1 (mono), all other counts give 2.
int channelCountFor(int deviceInputChannels);

/// Reads [Soundcard],mic_input_<n>_device, _channel and _channels, n = index + 1.
/// A mic with a damaged or missing key is left out.
Assignments load(const UserSettingsPointer& pConfig);

/// Writes the keys of each assignment and removes the keys of each mic
/// that is not in `assignments`.
void save(const UserSettingsPointer& pConfig, const Assignments& assignments);

} // namespace micinputs
// End of AI-generated text.
