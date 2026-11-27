#pragma once

#include <QAtomicInteger>
#include <QHash>
#include <QImage>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <functional>
#include <limits>
#include <optional>
#include <utility>

#include "library/rekordbox/anlzwriter.h"
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
    /// is the `masterContentId` of its row (a row of another program keeps
    /// its own `masterContentId`).
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

// AI-generated (Claude Code). Start of AI-generated text.
/// Mix Two: what planTrack() found for a track, before anything is written.
struct DlpTrackPlan {
    /// The location of the audio file, with '/'.
    QString location;
    /// The `fileType` of the file; 0 when the checks skip the track.
    int fileType = 0;
    /// The stick path that the track gets when the tracks before it are
    /// exported. Empty when the checks skip the track (commitTrack() gives
    /// the reason).
    QString stickPath;
};

/// Mix Two: what prepareTrack() made for a track. It holds only values: it
/// can come from another thread.
struct DlpPreparedTrack {
    /// True when the job was cancelled before the work; then nothing else is
    /// set.
    bool cancelled = false;
    /// The stick path of the plan. The DAT file holds it, so the files are
    /// good only for this path.
    QString stickPath;
    /// The timing offset and the MP3 frame count of the audio file, when set.
    std::optional<int> timingOffsetMs;
    quint32 mp3PcmFrames = 0;
    /// The ANLZ files; empty when the builder refused the track (then
    /// `anlzError` has the reason) or nothing was prepared.
    std::optional<AnlzFileSet> anlzFiles;
    QString anlzError;
    /// The warnings of the ANLZ builder.
    QStringList anlzWarnings;
    /// The JPEG bytes of bN.jpg and bN_m.jpg of the cover, when the cover
    /// was encoded. Empty bytes: the cover cannot be encoded.
    std::optional<std::pair<QByteArray, QByteArray>> artworkJpegs;
};
// End of AI-generated text.

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
/// (Rulings R17 and R18): a re-exported track gets a new ANLZ folder when
/// the bytes of its ANLZ files changed (else it keeps the folder of its row
/// and nothing is written), a new
/// image when the cover bytes changed, and a new audio copy (" (2)", ...)
/// when its copy in /Contents has another size than its file. finish()
/// removes the replaced files only after a good save, and only when no row
/// uses them (dlpRemoveReplacedFiles()). A file outside /Contents is never
/// copied over or removed. Each path of the database must pass
/// dlpIsSafeStickPath(); a row that the export updates with a path that
/// does not is used as a row without that path, and the files of the bad path are never
/// touched.
///
// AI-generated (Claude Code). Start of AI-generated text.
/// A stick has one library. The row with the `masterDbId` of the options and
/// the ID of the track as `masterContentId` is the row of the track: an
/// export updates it and does not add a second row. A track without such a
/// row uses a row that rekordbox or another program wrote (another
/// `masterDbId`) for the same file, when its file is on the stick and that
/// row has the path of the file (Ruling R8), or when that row has the
/// /Contents path of the track and the file there has the size of the track
/// (Ruling R19). The export updates that row in place with the data of Mix
/// Two, as for its own row; the row keeps its `masterDbId` and
/// `masterContentId`, so that the other program can find it again. Such a
/// row is found by its stick path only: the `masterContentId` of another
/// program is not an ID of the Mix Two library. A row of another program at
/// the target with a file of another size is another track: it does not
/// change. All stick paths are compared through dlpPathKey().
// End of AI-generated text.
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
    /// whose file a row of another program has (Rulings R8 and R19), and a
    /// track that exportTrack() skips (missing file, file type, size) count
    /// 0. Call it after prepare().
    /// It writes nothing.
    qint64 bytesToCopy(const QList<DlpCopyCandidate>& tracks) const;

    /// Exports one track (spec section 6): it copies the audio file when the
    /// stick does not have it, writes the ANLZ files and the artwork, and adds
    /// or updates the row of the track. A track that cannot be exported is
    /// skipped; the export goes on. A track without a valid ID is skipped.
    /// A skipped track that has a row on the stick (its own row, or the row
    /// of another program for its file of Ruling R8) keeps that row in the
    /// playlists, with
    /// the warning "The stick has an older copy of this track. The playlist
    /// uses it." (Ruling R20).
    DlpTrackResult exportTrack(const DlpExportTrack& item);

    // AI-generated (Claude Code). Start of AI-generated text.
    /// exportTrack() in three steps, so that an export job can prepare the
    /// next tracks in other threads while this thread writes to the stick.
    /// exportTrack() is commitTrack(item, prepareTrack(item, planTrack(item))).
    ///
    /// planTrack(): this thread; it writes nothing. The checks of the file and
    /// the stick path of the track. `pendingPathKeys` are the dlpPathKey() of
    /// the stick paths of the planned tracks that are not committed yet: they
    /// count as used, as they will be when those tracks are exported.
    DlpTrackPlan planTrack(const DlpExportTrack& item,
            const QSet<QString>& pendingPathKeys = {}) const;

    /// Any thread: the CPU work of a track (timing offset, MP3 frame count,
    /// the ANLZ files for the stick path of the plan, the JPEG bytes of the
    /// cover). It reads only its arguments, the track getters and the audio
    /// file; it uses no exporter and no database. When `*pCancel` is not 0,
    /// it does nothing and sets `cancelled`.
    static DlpPreparedTrack prepareTrack(const DlpExportTrack& item,
            const DlpTrackPlan& plan,
            const QAtomicInteger<int>* pCancel = nullptr);

    /// This thread, in the order of the tracks: the rest of exportTrack()
    /// with the prepared data. It finds the stick path again; when it is not
    /// the path of the preparation (an earlier track was skipped), or the
    /// preparation was cancelled, it builds the ANLZ files here. So the stick
    /// is the same as with exportTrack() for each track in turn.
    DlpTrackResult commitTrack(const DlpExportTrack& item, DlpPreparedTrack prepared);
    // End of AI-generated text.

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
    // AI-generated (Claude Code). Start of AI-generated text.
    /// If supplied, allowCleanup runs after a good save and before removal.
    /// Returning false keeps the replaced files without failing finish().
    bool finish(QString* pErrorMessage, const std::function<bool()>& allowCleanup = {});

    /// The number of old files that this export replaced. finish() removes
    /// them only when allowCleanup allows it.
    int replacedFileCount() const {
        return static_cast<int>(m_replaced.anlzFolders.size() +
                m_replaced.imagePaths.size() + m_replaced.audioPaths.size());
    }
    // End of AI-generated text.

    // AI-generated (Claude Code). Start of AI-generated text.
    /// The bytes of the audio files that exportTrack() copied to the stick.
    qint64 bytesCopied() const {
        return m_bytesCopied;
    }
    /// The number of audio files that exportTrack() copied to the stick.
    int audioFilesCopied() const {
        return m_audioFilesCopied;
    }
    /// The number of tracks for which exportTrack() wrote new ANLZ files.
    int anlzWrittenCount() const {
        return m_anlzWritten;
    }
    /// The number of tracks that kept the ANLZ folder of their row, because
    /// the files had the same bytes (Ruling R17).
    int anlzKeptCount() const {
        return m_anlzKept;
    }
    // End of AI-generated text.

    /// The number of playlists that exportPlaylists() wrote, crates included
    /// (the folder `Crates` is not counted).
    int playlistCount() const {
        return m_playlistCount;
    }

    /// The number of tracks of this export that the playlists can use: the
    /// exported tracks and the skipped tracks that keep an older row (Ruling
    /// R20).
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
        /// Ruling R19: the row of another program at the /Contents path, with
        /// the file of the track. The export updates it.
        std::optional<DlpContentRef> otherRow;
    };

    /// The path from the stick root of a file below the stick root, else none.
    std::optional<QString> stickPathOf(const QString& location) const;
    /// Where the audio file of a track goes. exportTrack() and bytesToCopy()
    /// both use it, so that the estimate follows the export.
    struct Placement {
        /// The row that the export updates: the row of the Mix Two track ID,
        /// or else the row of another program for the file of the track
        /// (Rulings R8 and R19).
        std::optional<DlpContentRef> ownRow;
        /// True when `ownRow` is the row of another program. The row keeps
        /// its `masterDbId` and `masterContentId`.
        bool otherProgramRow = false;
        Target target;
    };
    // AI-generated (Claude Code). Start of AI-generated text.
    /// `pPendingPathKeys` (if it is not null): more paths that count as used
    /// (planTrack()).
    Placement placementOf(qint64 trackId,
            const QString& location,
            const QString& artist,
            const QString& album,
            const QSet<QString>* pPendingPathKeys = nullptr) const;
    Target targetOf(const QString& artist,
            const QString& album,
            const QString& location,
            const std::optional<QString>& ownStickPath,
            const std::optional<DlpContentRef>& ownRow,
            const QSet<QString>* pPendingPathKeys = nullptr) const;
    /// The checks of step 1 of exportTrack(): the plan without the stick
    /// path, or the reason to skip the track.
    DlpTrackPlan checkTrack(const DlpExportTrack& item, QString* pSkipReason) const;
    // End of AI-generated text.
    QString absolutePath(const QString& stickPath) const;
    /// The row that the stick has for a track already: its own row, or the
    /// row of another program for its file on the stick (Ruling R20).
    std::optional<qint64> olderRowOf(const Track& track) const;
    /// The path of a new `bN.jpg`: no image row and no file has the number.
    QString freeArtworkPath();
    /// The image row of a track; empty when the track has no cover. When the
    /// artwork cannot be written, the track keeps the image of its row and
    /// a warning is added.
    // AI-generated (Claude Code). Start of AI-generated text.
    /// `pJpegs` (if it is not null): the JPEG bytes that prepareTrack() made.
    std::optional<qint64> imageOf(const DlpExportTrack& item,
            const std::optional<DlpContentRef>& ownRow,
            QStringList* pWarnings,
            const std::pair<QByteArray, QByteArray>* pJpegs = nullptr);
    // End of AI-generated text.
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
    /// The first row of another program (another `masterDbId`) by the
    /// dlpPathKey() of its path; a row of this export replaces the one that
    /// it updated. A path of the row that does not pass dlpIsSafeStickPath()
    /// is empty here.
    QHash<QString, DlpContentRef> m_otherRowsByPath;
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
    // AI-generated (Claude Code). Start of AI-generated text.
    /// The dlpPathKey() of the old ANLZ folders that tracks of this export
    /// kept. A folder is kept for one track only, also when two rows use it.
    QSet<QString> m_keptAnlzFolderKeys;
    qint64 m_bytesCopied = 0;
    int m_audioFilesCopied = 0;
    int m_anlzWritten = 0;
    int m_anlzKept = 0;
    // End of AI-generated text.
};

} // namespace rekordbox
} // namespace mixxx
