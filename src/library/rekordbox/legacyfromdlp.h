#pragma once

// AI-generated (Claude Code). Start of AI-generated text.

#include <QString>
#include <functional>
#include <optional>

#include "library/rekordbox/legacylibrary.h"

namespace mixxx::rekordbox {

class DeviceLibraryPlusDb;

/// Reads the database on its owning thread. Returns no model on a read error
/// or a missing list reference, and sets pError if it is not null.
/// On success, pClampedTracks receives the number of tracks with adjusted values.
std::optional<LegacyLibrary> legacyFromDlp(
        const DeviceLibraryPlusDb& db, QString* pError, int* pClampedTracks = nullptr);

/// Makes the bytes of `export.pdb` from a legacy model; legacyPdbBytes() by
/// default. Tests give another function.
using LegacyPdbWriter = std::function<QByteArray(const LegacyLibrary&, QString*)>;

/// Writes the legacy library (`export.pdb`) at `pdbPath` from the Device
/// Library Plus `db`, on the thread that owns `db`. `stickRoot` is the root of
/// the stick; the function makes the `PIONEER` and `rekordbox` folders below
/// it when they are missing, and refuses links. The old file stays until the
/// new file is complete (QSaveFile). Returns an empty string on success, else
/// the reason (ASD-STE100).
QString writeLegacyLibraryFromDlp(const DeviceLibraryPlusDb& db,
        const QString& stickRoot,
        const QString& pdbPath,
        const LegacyPdbWriter& writer = legacyPdbBytes,
        int* pClampedTracks = nullptr);

} // namespace mixxx::rekordbox

// End of AI-generated text.
