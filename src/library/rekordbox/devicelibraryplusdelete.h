// AI-generated (Claude Code). Start of AI-generated text.
#pragma once

#include <functional>

#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace mixxx {
namespace rekordbox {

/// Mix Two: deletes a playlist from the Device Library Plus of a stick, with
/// the tracks that no other playlist holds (as "Delete Tracks" of rekordbox
/// does on a device). The functions have no state and no thread rules; the
/// export job can call them on its worker thread.

/// An item that the function did not remove, and why.
struct DlpRefusedItem {
    /// The name of a playlist, or the stick path of a track.
    QString name;
    /// The reason (ASD-STE100).
    QString reason;
};

/// A track that dlpDeletePlaylist() removed from the stick.
struct DlpRemovedTrack {
    /// The ID of its row in the content table (the row is gone now).
    qint64 contentId = 0;
    QString title;
    /// The name of the artist of the row; empty when the row has none.
    QString artist;
    /// The path of its audio file on the stick, from the root of the stick.
    QString stickPath;
    /// The library ID and the track ID that the row had.
    qint64 masterDbId = 0;
    qint64 masterContentId = 0;
    /// What the check of the caller said; false when there is no check.
    bool inMixTwoLibrary = false;
};

/// Tells whether a removed track is in the Mix Two library. It runs on the
/// thread of dlpDeletePlaylist(), once for each removed track.
using DlpLibraryCheck = std::function<bool(const DlpRemovedTrack&)>;

/// What dlpDeletePlaylist() did.
struct DlpDeletePlaylistResult {
    /// True when the playlist is gone from the library on the stick.
    bool ok = false;
    /// A message for the user (ASD-STE100) when `ok` is false.
    QString errorMessage;
    int playlistsRemoved = 0;
    /// The tracks whose row and files the function removed.
    int tracksRemoved = 0;
    /// The tracks that another playlist holds. They stay, with their files.
    int tracksKept = 0;
    /// The items that the function did not remove: the playlist when it is a
    /// folder.
    QList<DlpRefusedItem> refused;
    /// The removed tracks, in the order of the playlist.
    QList<DlpRemovedTrack> removedTracks;
    /// The stick paths of the files of removed tracks that exist and that
    /// Mix Two could not delete. This is not an error: the library on the
    /// stick is already saved.
    QStringList filesNotDeleted;
    /// True when the stick has an `export.pdb` and the function wrote it
    /// again from the changed Device Library Plus.
    bool legacyLibraryWritten = false;
    /// Why the function could not write `export.pdb` again; empty when it
    /// wrote it or when the stick has none. The delete is still done.
    QString legacyLibraryError;
    /// True when the files of the removed tracks stay on the stick, because
    /// the old `export.pdb` still points to them.
    bool keptFilesForLegacyLibrary = false;
};

/// Deletes the playlist `playlistId` (an ID of the playlist table) from the
/// library on the stick at `stickRoot`. `masterDbId` is the library ID of Mix
/// Two (the `masterDbId` of its rows); a value that is not larger than 0
/// gives an error. It selects no rows: a stick has one library, and the rows
/// that rekordbox or another program wrote follow the same rules as the rows
/// of Mix Two.
///
/// For each track of the playlist:
/// - a track that another playlist holds stays, with its files;
/// - any other track is removed: its row, the rows of other tables that
///   belong only to it, its audio file in `/Contents`, and its ANLZ files and
///   folder. The cover image files go too, when no other track, album or
///   playlist uses the image. Rows of the tables that tracks share (artist,
///   album, genre, key, label, colour, image) stay. `removedTracks` lists
///   the track. When `isInLibrary` is set, it runs once for each removed
///   track before the library changes, and its answer goes in
///   `inMixTwoLibrary`. The function only reports; it does not ask the user.
/// A playlist folder, also an empty one, is refused: `ok` is false and the
/// library does not change. An unknown ID gives an error and no change.
///
/// When `backupBaseDir` is not empty, the function first makes a backup of
/// the library on the stick with dlpBackupLibrary() (the device name of the
/// library, or "Mix Two" when it has none). When the backup fails, the
/// function fails and the stick does not change. An empty `backupBaseDir`
/// makes no backup.
///
/// The library changes in one transaction, and the function writes it to the
/// stick (DeviceLibraryPlusDb::save()). When the stick has an `export.pdb`
/// next to the Device Library Plus, the function then writes it again from the
/// changed library (writeLegacyLibraryFromDlp()); when that fails, the files
/// stay and `legacyLibraryError` tells why. The function deletes files only after
/// the save, and only with the rules of dlpRemoveReplacedFiles(): a file that
/// cannot be deleted goes in `filesNotDeleted`. When the function fails
/// before the save, the stick does not change.
DlpDeletePlaylistResult dlpDeletePlaylist(const QString& stickRoot,
        qint64 masterDbId,
        qint64 playlistId,
        const DlpLibraryCheck& isInLibrary = {},
        const QString& backupBaseDir = QString());

/// Like dlpDeletePlaylist(), for the playlist at a path of names: for example
/// {"Crates", "House"} is the playlist "House" in the folder "Crates", and
/// {"Warm up"} is a playlist at the top level.
DlpDeletePlaylistResult dlpDeletePlaylistByPath(const QString& stickRoot,
        qint64 masterDbId,
        const QStringList& namePath,
        const DlpLibraryCheck& isInLibrary = {},
        const QString& backupBaseDir = QString());

/// Finds what dlpDeletePlaylistByPath() removes, and does not change the
/// stick: no backup, no save, no file is deleted. `ok`, `refused`,
/// `tracksKept` and `removedTracks` (with `inMixTwoLibrary`) are as the
/// delete gives them; `playlistsRemoved` and `tracksRemoved` are 0. A folder
/// or an unknown playlist gives the same error as the delete.
DlpDeletePlaylistResult dlpPreviewDeletePlaylistByPath(const QString& stickRoot,
        qint64 masterDbId,
        const QStringList& namePath,
        const DlpLibraryCheck& isInLibrary = {});

} // namespace rekordbox
} // namespace mixxx
// End of AI-generated text.
