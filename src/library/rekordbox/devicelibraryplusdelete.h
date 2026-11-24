// AI-generated (Claude Code). Start of AI-generated text.
#pragma once

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
    /// folder, and the tracks of other programs.
    QList<DlpRefusedItem> refused;
    /// The stick paths of the files of removed tracks that exist and that
    /// Mix Two could not delete. This is not an error: the library on the
    /// stick is already saved.
    QStringList filesNotDeleted;
};

/// Deletes the playlist `playlistId` (an ID of the playlist table) from the
/// library on the stick at `stickRoot`. `masterDbId` is the library ID of Mix
/// Two (the `masterDbId` of its rows).
///
/// For each track of the playlist:
/// - a track that another playlist holds stays, with its files;
/// - a track with another `masterDbId` (a row of another program) stays, and
///   the result lists it as refused (the export never changes such a row);
/// - any other track is removed: its row, the rows of other tables that
///   belong only to it, its audio file in `/Contents`, and its ANLZ files and
///   folder. The cover image files go too, when no other track, album or
///   playlist uses the image. Rows of the tables that tracks share (artist,
///   album, genre, key, label, colour, image) stay.
/// A playlist folder, also an empty one, is refused: `ok` is false and the
/// library does not change. An unknown ID gives an error and no change.
///
/// The library changes in one transaction, and the function writes it to the
/// stick (DeviceLibraryPlusDb::save()). The function deletes files only after
/// the save, and only with the rules of dlpRemoveReplacedFiles(): a file that
/// cannot be deleted goes in `filesNotDeleted`. When the function fails
/// before the save, the stick does not change.
DlpDeletePlaylistResult dlpDeletePlaylist(
        const QString& stickRoot, qint64 masterDbId, qint64 playlistId);

/// Like dlpDeletePlaylist(), for the playlist at a path of names: for example
/// {"Crates", "House"} is the playlist "House" in the folder "Crates", and
/// {"Warm up"} is a playlist at the top level.
DlpDeletePlaylistResult dlpDeletePlaylistByPath(
        const QString& stickRoot, qint64 masterDbId, const QStringList& namePath);

} // namespace rekordbox
} // namespace mixxx
// End of AI-generated text.
