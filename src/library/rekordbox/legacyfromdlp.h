#pragma once

// AI-generated (Claude Code). Start of AI-generated text.

#include <optional>

#include "library/rekordbox/legacylibrary.h"

namespace mixxx::rekordbox {

class DeviceLibraryPlusDb;

/// Reads the database on its owning thread. Returns no model on a read error
/// or a missing list reference, and sets pError if it is not null.
/// On success, pClampedTracks receives the number of tracks with adjusted values.
std::optional<LegacyLibrary> legacyFromDlp(
        const DeviceLibraryPlusDb& db, QString* pError, int* pClampedTracks = nullptr);

} // namespace mixxx::rekordbox

// End of AI-generated text.
