#pragma once

#include <QMetaType>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include "library/trackset/crate/crateid.h"

namespace mixxx {

/// Mix Two: a request to export Mix Two tracks to the Device Library Plus of
/// a stick (the rekordbox USB export).
struct DeviceLibraryPlusExportRequest {
    /// The root folder of the stick.
    QString stickRoot;

    /// The name of the stick for the backup folder: its volume label. The job
    /// uses "Mix Two" when the name is empty.
    QString deviceName;

    /// The folder for the backups of the library of the stick:
    /// `<settings path>/device-backups`.
    QString backupBaseDir;

    /// The library ID of Mix Two (the `masterDbId` of its rows). It must be
    /// larger than 0.
    qint64 masterDbId = 0;

    /// The crates to export.
    ///
    /// An empty set of crates AND playlists to export means that the whole
    /// library is exported.
    QSet<CrateId> crateIds;

    /// The playlists to export.
    ///
    /// An empty set of crates AND playlists to export means that the whole
    /// library is exported.
    QSet<int> playlistIds;

    // AI-generated (Claude Code). Start of AI-generated text.
    /// The tracks to export, as library track IDs (the "Export Track" item of
    /// the track menu). Mix Two exports these tracks only, with no crate and
    /// no playlist. The set counts only when `crateIds` and `playlistIds` are
    /// empty. With all three sets empty, the whole library is exported.
    QSet<int> trackIds;

    /// Also write the Device Library from the updated Device Library Plus.
    bool writeLegacyLibrary = false;
    // End of AI-generated text.
};

/// Mix Two: what an export did. The job sends it with `completed`.
struct DeviceLibraryPlusExportSummary {
    int tracksExported = 0;
    int playlistsExported = 0;

    /// One line for each track that the export skipped:
    /// "<artist> - <title>: <reason>".
    QStringList skipped;

    /// One line for each warning of a track: "<artist> - <title>: <warning>".
    QStringList warnings;

    // AI-generated (Claude Code). Start of AI-generated text.
    /// Empty unless Device Library Plus succeeds and Device Library fails.
    QString legacyLibraryError;
    // End of AI-generated text.
};

} // namespace mixxx

Q_DECLARE_METATYPE(mixxx::DeviceLibraryPlusExportSummary)
