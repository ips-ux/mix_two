#pragma once

#include "controllers/midi/midienumerator.h"
#include "preferences/usersettings.h"

/// This class handles discovery and enumeration of DJ controllers that appear under the PortMIDI cross-platform API.
class PortMidiEnumerator : public MidiEnumerator {
    Q_OBJECT
  public:
    PortMidiEnumerator(UserSettingsPointer pConfig);
    ~PortMidiEnumerator() override;

    QList<Controller*> queryDevices() override;

    /// Hands the current devices to the caller and restarts PortMidi, whose
    /// device list is otherwise fixed at initialization, so that the next
    /// queryDevices() sees devices connected since then. The returned
    /// controllers are closed and no longer usable; the caller deletes them.
    QList<Controller*> releaseDevicesForRescan();

  private:
    QList<Controller*> m_devices;
    UserSettingsPointer m_pConfig;
};

// For testing.
bool shouldLinkInputToOutput(const QString& input_name,
        const QString& output_name);
