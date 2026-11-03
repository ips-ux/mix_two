#pragma once

#include <QHash>
#include <QImage>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <limits>
#include <optional>

#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/devicelibraryplusexportcleanup.h"
#include "track/track_decl.h"
#include "track/trackid.h"
#include "util/cache.h"
#include "waveform/waveform.h"

namespace mixxx {
namespace rekordbox {

/// Mix Two: one track to export, with the data that the export job loads on
/// the main thread.
struct DlpExportTrack {
    /// The track, loaded on the main thread. It must have a valid ID: the ID
    /// is the `masterContentId` of its row.
    TrackPointer pTrack;
    /// The waveform of the analysis database. May be null; then the track is
    /// skipped.
    ConstWaveformPointer pWaveform;
    /// The cover image. May be null; then the track has no image.
    QImage cover;
    /// The Mix Two cover hash. Tracks of one export with the same hash share
    /// one image row.
    std::optional<mixxx::cache_key_t> coverHash;
};

/// Mix Two: a Mix Two playlist or crate to export.
struct DlpExportPlaylist {
    QString name;
    /// In Mix Two order.
    QList<TrackId> trackIds;
};

/// Mix Two: a track for the estimate of bytesToCopy(). The data is what the
/// export job reads from the Mix Two library; it needs no loaded track.
struct DlpCopyCandidate {
    TrackId trackId;
    /// The location of the audio file.
    QString location;
    QString artist;
    QString album;
};

/// Mix Two: what exportTrack() did with a track.
struct DlpTrackResult {
    /// The ID of the row of the track. Empty when the track was skipped (a
    /// skipped track can still be in the playlists with an older row; then
    /// a warning says so).
    std::optional<qint64> contentId;
    /// Why the track was skipped (ASD-STE100). Empty when it was exported.
    QString skipReason;
    /// The warnings of the ANLZ builder and of the exporter (ASD-STE100).
    QStringList warnings;
};

/// Mix Two: exports Mix Two tracks, playlists and crates to the Device
/// Library Plus of one stick (spec sections 6, 7 and 8).
///
/// The object does all its work on one thread (the worker thread of the
/// export job) and reads no Mix Two library data: all data comes in
/// DlpExportTrack and DlpExportPlaylist. It reads the audio files of the
/// tracks.
///
/// Order of use: prepare(), exportTrack() for each track, exportPlaylists(),
/// finish(). For each track it writes the audio, then the ANLZ files and the
/// artwork to the stick. The rows go into one batch of the database, and only
/// finish() writes the database to the stick (DeviceLibraryPlusDb::save()).
/// Without finish(), the library on the stick does not change; the files that
/// the export wrote stay, and the next export can use the audio files again.
///
/// The export never writes over a file that the library on the stick uses
/// (Rulings R17 and R18): a re-exported track gets a new ANLZ folder, a new
/// image when the cover bytes changed, and a new audio copy (" (2)", ...)
/// when its copy in /Contents has another size than its file. finish()
/// removes the replaced files only after a good save, and only when no row
/// uses them (dlpRemoveReplacedFiles()). A file outside /Contents is never
/// copied over or removed. Each path of the database must pass
/// dlpIsSafeStickPath(); a row of Mix Two with a path that does not is used
/// as a row without that path, and the files of the bad path are never
/// touched.
///
/// Rows of other programs (another `masterDbId`) are never changed. The row
/// with the `masterDbId` of the options and the ID of the track as
/// `masterContentId` belongs to the track: an export updates it and does not
/// add a second row. A track without a row of its own uses the row of
/// another program, writes nothing and gives a warning, when its file is on
/// the stick and that row has the path of the file (Ruling R8), or when that
/// row has the /Contents path of the track and the file there has the size
/// of the track (Ruling R19). All stick paths are compared through
/// dlpPathKey().
class DeviceLibraryPlusExporter {
  public:
    struct Options {
        /// The root folder of the stick.
        QString stickRoot;
        /// The library ID of Mix Two (the `masterDbId` of its rows).
        qint64 masterDbId = 0;
        /// A larger audio file is skipped: kFat32MaxFileSize on FAT32.
        qint64 maxFileSize = std::numeric_limits<qint64>::max();
    };

    /// The database must stay valid while the object lives.
    DeviceLibraryPlusExporter(DeviceLibraryPlusDb* pDb, Options options);

    /// Reads the tracks and images of the library and opens the batch.
    /// Returns false and sets *pErrorMessage (if it is not null) on an error
    /// of the database, when `masterDbId` is not larger than 0, and when the
    /// stick root is not an absolute path to an existing folder.
    bool prepare(QString* pErrorMessage);

    /// The number of bytes of audio files that exportTrack() would copy to the
    /// stick for these tracks now. It decides with the same rules as
    /// exportTrack(): a file that is on the stick already, a track that has a
    /// row of its own whose file exists (with the size of the track, or
    /// outside /Contents), a file of the same size at the target, a track
    /// that uses the row of another program, and a track that exportTrack()
    /// skips (missing file, file type, size) count 0. Call it after prepare().
    /// It writes nothing.
    qint64 bytesToCopy(const QList<DlpCopyCandidate>& tracks) const;

    /// Exports one track (spec section 6): it copies the audio file when the
    /// stick does not have it, writes the ANLZ files and the artwork, and adds
    /// or updates the row of the track. A track that cannot be exported is
    /// skipped; the export goes on. A track without a valid ID is skipped.
    /// A skipped track that has a row on the stick (its own row, or the row
    /// of another program of Ruling R8) keeps that row in the playlists, with
    /// the warning "The stick has an older copy of this track. The playlist
    /// uses it." (Ruling R20).
    DlpTrackResult exportTrack(const DlpExportTrack& item);

    /// Writes the playlists at the top level and the crates as playlists in
    /// the folder `Crates`. A playlist with the same name at the same level
    /// gets the new track list. Only the tracks that exportTrack() exported
    /// are in the lists. Returns false and sets *pErrorMessage (if it is not
    /// null) on an error of the database.
    bool exportPlaylists(const QList<DlpExportPlaylist>& playlists,
            const QList<DlpExportPlaylist>& crates,
            QString* pErrorMessage);

    /// Sets the number of tracks, commits the batch and writes the library to
    /// the stick. After a good save it removes the replaced files that no row
    /// uses (a failure there is logged and does not fail finish()). Returns
    /// false and sets *pErrorMessage (if it is not null) when the database
    /// has an error, when save() fails, or when exportTrack() had tracks and
    /// none of them has a row (usableTrackCount() is 0; Ruling R20): then the
    /// library on the stick does not change.
    bool finish(QString* pErrorMessage);

    /// The number of playlists that exportPlaylists() wrote, crates included
    /// (the folder `Crates` is not counted).
    int playlistCount() const {
        return m_playlistCount;
    }

    /// The number of tracks of this export that the playlists can use: the
    /// exported tracks, the tracks with the row of another program, and the
    /// skipped tracks that keep an older row (Ruling R20).
    int usableTrackCount() const {
        return static_cast<int>(m_exportedContentIds.size());
    }

  private:
    /// The stick path of a track and whether the export copies the file there.
    struct Target {
        QString stickPath;
        bool needsCopy = false;
        /// Ruling R18: the copy in /Contents of the own row that the new
        /// copy replaces.
        std::optional<QString> replacedPath;
        /// Ruling R19: the row of another program at the /Contents path.
        std::optional<qint64> otherRowId;
    };

    /// The path from the stick root of a file below the stick root, else none.
    std::optional<QString> stickPathOf(const QString& location) const;
    /// Where the audio file of a track goes. exportTrack() and bytesToCopy()
    /// both use it, so that the estimate follows the export.
    struct Placement {
        /// The row of the Mix Two track ID, if there is one.
        std::optional<DlpContentRef> ownRow;
        /// Set when the track has no row of its own and uses the row of
        /// another program (Rulings R8 and R19): the ID of that row.
        std::optional<qint64> otherRowId;
        /// Not set when `otherRowId` is set.
        Target target;
    };
    Placement placementOf(qint64 trackId,
            const QString& location,
            const QString& artist,
            const QString& album) const;
    Target targetOf(const QString& artist,
            const QString& album,
            const QString& location,
            const std::optional<QString>& ownStickPath,
            const std::optional<DlpContentRef>& ownRow) const;
    QString absolutePath(const QString& stickPath) const;
    /// The row that the stick has for a track already: its own row, or the
    /// row of another program for its file on the stick (Ruling R20).
    std::optional<qint64> olderRowOf(const Track& track) const;
    /// The path of a new `bN.jpg`: no image row and no file has the number.
    QString freeArtworkPath();
    /// The image row of a track; empty when the track has no cover. When the
    /// artwork cannot be written, the track keeps the image of its row and
    /// a warning is added.
    std::optional<qint64> imageOf(const DlpExportTrack& item,
            const std::optional<DlpContentRef>& ownRow,
            QStringList* pWarnings);
    /// Records a row in the look-ups.
    void remember(const DlpContentRef& row);
    bool writePlaylist(const DlpExportPlaylist& playlist, qint64 parentId);
    bool failWithDbError(QString* pErrorMessage) const;

    DeviceLibraryPlusDb* const m_pDb;
    const Options m_options;
    /// The stick root with `/` and no `/` at the end.
    const QString m_root;

    /// The row of each Mix Two track ID: the first row with the `masterDbId`
    /// of the options. A path of the row that does not pass
    /// dlpIsSafeStickPath() is empty here.
    QHash<qint64, DlpContentRef> m_ownRows;
    /// The ID of the first row of another program (another `masterDbId`) by
    /// the dlpPathKey() of its path.
    QHash<QString, qint64> m_otherRowIdsByPath;
    /// The dlpPathKey() of the paths that rows use (also paths that are not
    /// safe).
    QSet<QString> m_usedPathKeys;
    /// The dlpPathKey() of the ANLZ folders that rows use.
    QSet<QString> m_usedAnlzFolderKeys;
    /// The path of each image row.
    QHash<qint64, QString> m_imagePaths;
    /// The image rows of this export by cover hash.
    QHash<mixxx::cache_key_t, qint64> m_imageIdsByCoverHash;
    /// The N of the next new `bN.jpg`.
    int m_nextImageNumber = 1;
    /// The rows of the tracks of this export.
    QHash<TrackId, qint64> m_exportedContentIds;
    /// The number of exportTrack() calls.
    int m_offeredTrackCount = 0;
    /// The files that this export replaced; finish() removes them.
    DlpReplacedFiles m_replaced;
    int m_playlistCount = 0;
};

} // namespace rekordbox
} // namespace mixxx
