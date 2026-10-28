#pragma once

#include <QString>
#include <QStringList>
#include <optional>

namespace mixxx {

namespace midi {

/// Returns the sorted names of the MIDI ports the operating system currently
/// knows about, read live from the platform MIDI API rather than PortMidi's
/// cached device list. Comparing two results tells whether a MIDI device was
/// connected or disconnected. Returns std::nullopt if the platform is not
/// supported.
std::optional<QStringList> currentPortNames();

/// Extracts the client and port lines from the contents of
/// /proc/asound/seq/clients, dropping the counters that change while MIDI
/// data flows. Exposed for testing.
QStringList parseAlsaSeqClients(const QString& procContents);

} // namespace midi

} // namespace mixxx
