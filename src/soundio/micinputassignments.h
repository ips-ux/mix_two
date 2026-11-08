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
    /// The device was missing when the input was kept, so the input is not
    /// in the live config. If the device returns, it must be applied again
    /// (it was not cleared by the user).
    bool pending = false;

    bool operator==(const Assignment& other) const {
        return deviceName == other.deviceName &&
                channelBase == other.channelBase &&
                channelCount == other.channelCount &&
                pending == other.pending;
    }
};

/// Key: mic index 0..3.
using Assignments = QMap<int, Assignment>;

/// True for a device that must never be a mic input: a DDJ-800 (it carries no
/// programme audio) or a WASAPI loop-back device (it records the output, so it
/// would feed the mix back into itself).
bool isExcludedDevice(const QString& deviceName);

/// Starts from `live` (pending false). A saved mic that has no live input:
/// - device not in `availableDevices`: kept, pending true (the user did not
///   clear it, the device is gone);
/// - device present and pending: kept, still pending (the device returned and
///   the input is not applied yet);
/// - device present and not pending: dropped (the user cleared it).
/// A mic on an excluded device (isExcludedDevice()) is dropped, live or saved.
Assignments merge(const Assignments& saved,
        const Assignments& live,
        const QStringList& availableDevices);

/// For a return to WASAPI that did not restore the saved mics (for example
/// from the sound API box in Preferences): each saved mic that has no live
/// input is marked pending, so that merge() keeps it and a restore applies it.
Assignments keepOnReturn(const Assignments& saved, const Assignments& live);

/// True if a pending mic in `assignments` has its device in
/// `availableDevices`, so a restore can apply it.
bool needsRestore(const Assignments& assignments, const QStringList& availableDevices);

/// 1 input channel gives 1 (mono), all other counts give 2.
int channelCountFor(int deviceInputChannels);

/// Reads [Soundcard],mic_input_<n>_device, _channel and _channels, n = index + 1,
/// and _pending (a missing or bad value means false).
/// A mic with a damaged or missing key (not _pending) is left out.
Assignments load(const UserSettingsPointer& pConfig);

/// Writes the keys of each assignment and removes the keys of each mic
/// that is not in `assignments`.
void save(const UserSettingsPointer& pConfig, const Assignments& assignments);

} // namespace micinputs
// End of AI-generated text.
