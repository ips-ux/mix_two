#pragma once
// AI-generated (Claude Code). Start of AI-generated text.
// Consolidate: copy a track file into the library folder, check the copy,
// point the track at the copy, and send the original to the trash.

#include <QByteArray>
#include <QString>
#include <functional>
#include <optional>

#include "track/track_decl.h"
#include "util/removablevolume.h"

class TrackCollection;

namespace mixxx {
namespace library {

enum class ConsolidateStatus {
    /// The copy is in the library folder and the track points to it. Look at
    /// `ConsolidateOutcome::originalTrash` for what happened to the original.
    Consolidated,
    /// The track has no id or no file location. Nothing happened.
    InvalidTrack,
    /// The track file is already at the place that the pattern gives.
    AlreadyInLibrary,
    /// The track file does not exist.
    SourceMissing,
    /// A file with that name is already at the target place. Mix Two never
    /// overwrites a file.
    TargetExists,
    /// The copy failed (or the target folder could not be made). The original
    /// and the track location are not touched.
    CopyFailed,
    /// The copy differs from the original. Mix Two removed the copy and kept
    /// the original and the track location.
    HashMismatch,
    /// The track location could not change. Mix Two removed the copy and kept
    /// the original.
    RelocateFailed,
};

struct ConsolidateOutcome {
    ConsolidateStatus status = ConsolidateStatus::CopyFailed;
    /// The new file location (set when `status` is `Consolidated`).
    QString newLocation;
    /// What happened to the original (set when `status` is `Consolidated`).
    /// `RefusedUnsafe` means that the original is on a removable or network
    /// drive and stays there.
    std::optional<TrashResult> originalTrash;
};

/// The five system calls of a consolidation. A test gives fake ones.
struct ConsolidateOps {
    /// Copies a file. Must fail when the target exists.
    std::function<bool(const QString& from, const QString& to)> copyFile;
    /// The content hash of a file; an empty value when the file is not readable.
    std::function<QByteArray(const QString& path)> hashFile;
    /// Points the track at a new location. False when that fails.
    std::function<bool(const QString& newLocation)> relocate;
    /// Deletes the copy that this consolidation made. The function never
    /// gets the original.
    std::function<bool(const QString& path)> removeCopy;
    /// Sends the original to the trash (never from a removable drive).
    std::function<TrashResult(const QString& path)> trashOriginal;
};

/// The SHA-256 hash of the content of a file, or an empty value on an error.
QByteArray hashFileContent(const QString& path);

/// The real file calls (QFile, hashFileContent, moveToTrashChecked) with the
/// given `relocate` function.
ConsolidateOps systemConsolidateOps(std::function<bool(const QString&)> relocate);

/// The path of the consolidated file: `libraryRoot`, then `folder`, then the
/// file name of `sourcePath` (the name never changes).
QString consolidateTargetPath(const QString& sourcePath,
        const QString& libraryRoot,
        const QString& folder);

/// Consolidates one file. The steps, in this order:
/// 1. Copy the file to `targetPath` (never over a file that exists).
/// 2. Check that the copy has the same content hash as the original.
///    When it differs, remove the copy and stop: the original stays.
/// 3. Point the track at the copy. When that fails, remove the copy and stop.
/// 4. Send the original to the trash. A failure here is not an error of the
///    consolidation: the track already points to the copy.
ConsolidateOutcome consolidateFile(const QString& sourcePath,
        const QString& targetPath,
        const ConsolidateOps& ops);

/// Consolidates a track: the folder comes from `pattern` (see
/// libraryFolderFor()), the track location changes in the database of
/// `pCollection`, and the original goes to the trash as consolidateFile()
/// says. `trashOriginal` replaces the trash call (a test gives a fake one);
/// when it is empty, the function uses moveToTrashChecked().
ConsolidateOutcome consolidateTrack(const TrackPointer& pTrack,
        const QString& libraryRoot,
        const QString& pattern,
        TrackCollection* pCollection,
        const std::function<TrashResult(const QString&)>& trashOriginal = {});

} // namespace library
} // namespace mixxx

// End of AI-generated text.
