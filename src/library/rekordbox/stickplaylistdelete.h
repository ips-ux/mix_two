#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <functional>

#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include "library/rekordbox/devicelibraryplusdelete.h"

namespace mixxx {
namespace rekordbox {

/// Mix Two: the steps of "Delete from stick" for a playlist in the Device
/// Library Plus of a stick, without dialogs. The caller gives the answers of
/// the user through StickDeleteHooks, so a test can give them too.
///
/// The order of the steps:
/// 1. `confirmDelete` asks the user; "no" stops and the stick does not change.
/// 2. dlpPreviewDeletePlaylistByPath() finds the tracks that the delete
///    removes. It does not change the stick.
/// 3. When some of these tracks are not in the Mix Two library,
///    `offerImport` asks the user to add them. For "yes", `importTrack` runs
///    for each of them while its file is still on the stick. When one import
///    fails, the flow stops and the stick does not change.
/// 4. dlpDeletePlaylistByPath() makes the backup and deletes.
///
/// runStickPlaylistDelete() has no thread rules. The app runs it on a worker
/// thread; the hooks that ask the user go to the GUI thread.

/// The answers and the import, from the caller.
struct StickDeleteHooks {
    /// Step 1. True when the user agrees to delete. When it is not set, the
    /// flow does not ask.
    std::function<bool()> confirmDelete;
    /// Step 3. Gets the tracks that are not in the Mix Two library. True when
    /// the user wants to add them.
    std::function<bool(const QList<DlpRemovedTrack>&)> offerImport;
    /// Step 3. Adds one track to the Mix Two library from its file on the
    /// stick (`stickFilePath` is the full path). True when the track is in
    /// the library after the call.
    std::function<bool(const QString& stickFilePath, const DlpRemovedTrack&)> importTrack;
};

/// What to delete.
struct StickDeleteRequest {
    QString stickRoot;
    qint64 masterDbId = 0;
    /// The names from the top of the stick to the playlist.
    QStringList namePath;
    /// Tells whether a track is in the Mix Two library.
    DlpLibraryCheck isInLibrary;
    /// The folder for the backup of the library on the stick.
    QString backupBaseDir;
};

enum class StickDeleteStatus {
    /// The user said no. The stick did not change.
    Canceled,
    /// An error. `message` tells why. The stick did not change, unless
    /// `result.ok` is true.
    Failed,
    /// The playlist is gone from the stick.
    Deleted,
};

struct StickDeleteOutcome {
    StickDeleteStatus status = StickDeleteStatus::Failed;
    /// A message for the user (ASD-STE100) when the status is Failed.
    QString message;
    /// The result of the preview (Canceled or Failed before the delete) or of
    /// the delete.
    DlpDeletePlaylistResult result;
    /// The tracks that were not in the library when the flow started.
    QList<DlpRemovedTrack> notInLibrary;
    /// The number of tracks that importTrack() added.
    int tracksImported = 0;
    /// The stick paths of the tracks that importTrack() could not add.
    QStringList importFailed;
};

/// Runs the steps above. See StickDeleteHooks.
StickDeleteOutcome runStickPlaylistDelete(
        const StickDeleteRequest& request, const StickDeleteHooks& hooks);

/// The text of the result for the user (ASD-STE100).
QString stickDeleteSummary(const StickDeleteOutcome& outcome);

/// A file of the Mix Two library, for dlpFileNameAndSizeCheck().
struct LibraryFileRef {
    /// The full path of the file.
    QString location;
    /// The file name, without the folder.
    QString fileName;
};

/// Gives the library files with this size in bytes (the `filesize` column of
/// the track locations).
using LibraryFilesOfSize = std::function<QList<LibraryFileRef>(qint64 sizeInBytes)>;

/// A DlpLibraryCheck that compares the file of a removed track on the stick
/// with the library files. A track is in the library when a library file has
/// the same size and
/// - the same name after fatSafeName() (the name that the export gives the
///   copy; case is ignored), or
/// - the same bytes (SHA-256), for a file whose name the export changed in a
///   different way.
/// A track whose file is not on the stick is not in the library.
DlpLibraryCheck dlpFileNameAndSizeCheck(
        const QString& stickRoot, const LibraryFilesOfSize& filesOfSize);

} // namespace rekordbox
} // namespace mixxx
// End of AI-generated text.
