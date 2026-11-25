#pragma once

#include <QDir>
#include <QHash>
#include <QList>
#include <QSet>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

#include "library/dao/directorydao.h"
#include "library/library_decl.h"
#include "preferences/usersettings.h"
#include "track/globaltrackcache.h"
#include "util/db/dbconnectionpool.h"
#include "util/parented_ptr.h"
#include "util/thread_affinity.h"

class LibraryScanner;
class TrackCollection;
class ExternalTrackCollection;
class RelocatedTrack;

// AI-generated (Claude Code). Start of AI-generated text.
// Mix Two: the import of files into the library (File menu "Import Track" and
// "Import Folder", track menu "Import To Collection"). An import adds tracks
// to the library. It never moves, copies or changes a file, and it adds no
// watched folder.
namespace mixxx::library {

struct ImportFilesResult {
    // Tracks that the call added to the library.
    int added = 0;
    // Files that the library already had and that were visible.
    int alreadyInLibrary = 0;
    // Tracks that the library had but hid; the import shows them again.
    int shownAgain = 0;
    // Files that are missing, that are no sound files, or that the library
    // could not add.
    int skipped = 0;

    ImportFilesResult& operator+=(const ImportFilesResult& other) {
        added += other.added;
        alreadyInLibrary += other.alreadyInLibrary;
        shownAgain += other.shownAgain;
        skipped += other.skipped;
        return *this;
    }
};

/// The canonical path of a file: links are resolved and, where the file
/// system ignores letter case, the letter case is the one of the disk. This is
/// the rule that the track cache of the library uses for "the same file".
QString canonicalImportPath(const QString& path);

/// Two canonical paths name the same file. Windows and macOS ignore the
/// letter case in file names, so the comparison ignores it there.
bool isSameImportPath(const QString& a, const QString& b);

/// Knows the tracks that the library has, to find out if a file is in the
/// library already, also under another spelling of its path (letter case,
/// links). Candidates come from the file name; two files are the same when
/// their resolved paths are equal.
class ImportIndex {
  public:
    using PathResolver = std::function<QString(const QString&)>;

    struct Entry {
        QString location;
        TrackId trackId;
        bool hidden = false;
    };

    explicit ImportIndex(PathResolver resolver = canonicalImportPath)
            : m_resolver(std::move(resolver)) {
    }

    void add(const QString& location, TrackId trackId, bool hidden);
    /// The entry of the same file, or nullptr.
    Entry* find(const QString& location);

  private:
    PathResolver m_resolver;
    QHash<QString, QList<Entry>> m_entriesByName;
};

/// All sound files in the folder and below it, sorted by path. It reads the
/// file system only, so it is safe to call from a worker thread.
QStringList soundFilesInFolder(const QString& folder);

} // namespace mixxx::library
// End of AI-generated text.

// Manages Mixxx's internal database of tracks as well as external track collections.
//
// All modifying operations that might affect external collections
// must be invoked through this class to keep all track collections
// synchronized!
//
// Both crates and playlists are currently only supported by the internal
// collection, which needs to be modified directly.
class TrackCollectionManager : public QObject,
                               public virtual /*implements*/ GlobalTrackCacheSaver {
    Q_OBJECT

  public:
    TrackCollectionManager(
            QObject* parent,
            UserSettingsPointer pConfig,
            mixxx::DbConnectionPoolPtr pDbConnectionPool,
            deleteTrackFn_t deleteTrackForTestingFn = nullptr);
    ~TrackCollectionManager() override;

    TrackCollection* internalCollection() const {
        DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);
        return m_pInternalCollection;
    }

    const QList<ExternalTrackCollection*>& externalCollections() const {
        DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);
        return m_externalCollections;
    }

    TrackPointer getTrackById(
            TrackId trackId) const;
    TrackPointer getTrackByRef(
            const TrackRef& trackRef) const;
    QList<TrackId> resolveTrackIds(
            const QList<mixxx::FileInfo>& fileInfos,
            QObject* pSource) const;
    QList<TrackId> resolveTrackIdsFromUrls(
            const QList<QUrl>& urls,
            bool addMissing) const;
    QList<TrackId> resolveTrackIdsFromLocations(
            const QList<QString>& locations) const;

    bool updateTrackGenre(
            Track* pTrack,
            const QString& genre) const;
#if defined(__EXTRA_METADATA__)
    bool updateTrackMood(
            Track* pTrack,
            const QString& mood) const;
#endif // __EXTRA_METADATA__

    bool hideTracks(const QList<TrackId>& trackIds) const;
    bool unhideTracks(const QList<TrackId>& trackIds) const;
    void hideAllTracks(const QDir& rootDir) const;

    void purgeTracks(const QList<TrackRef>& trackRefs) const;
    void purgeAllTracks(const QDir& rootDir) const;

    DirectoryDAO::AddResult addDirectory(const mixxx::FileInfo& newDir) const;
    DirectoryDAO::RemoveResult removeDirectory(const mixxx::FileInfo& oldDir) const;
    DirectoryDAO::RelocateResult relocateDirectory(
            const QString& oldDir, const QString& newDir) const;

    TrackPointer getOrAddTrack(
            const TrackRef& trackRef,
            bool* pAlreadyInLibrary = nullptr) const;

    // AI-generated (Claude Code). Start of AI-generated text.
    // Adds the sound files to the library. A file that is in the library
    // already stays as it is; a hidden track is shown again. The files are
    // never written. Call it in the main thread.
    //
    // The index tells which files the library has. Without one, the call
    // makes its own. With a time budget the call stops after the file that
    // used up the budget (it handles at least one file); *pProcessed tells
    // how many files it handled.
    mixxx::library::ImportFilesResult importFiles(
            const QStringList& locations,
            mixxx::library::ImportIndex* pIndex = nullptr,
            std::optional<std::chrono::milliseconds> budget = std::nullopt,
            int* pProcessed = nullptr) const;
    /// Reads the tracks of the library into an index.
    mixxx::library::ImportIndex makeImportIndex(
            mixxx::library::ImportIndex::PathResolver resolver =
                    mixxx::library::canonicalImportPath) const;
    // End of AI-generated text.

    // Save the track in both the internal database and external collections.
    // Export of metadata is deferred until the track is evicted from the
    // cache to prevent file corruption due to concurrent access.
    enum class SaveTrackResult {
        Saved,
        Skipped, // e.g. unmodified or missing/deleted tracks
        Failed,
    };
    SaveTrackResult saveTrack(const TrackPointer& pTrack) const;
    // Same as startLibraryScan() but don't emit the scan summary.
    void startLibraryAutoScan();

    LibraryScanner* scanner() const {
        return m_pScanner.get();
    }

    bool isLibraryScanActive() const {
        return m_libraryScanActive.load();
    }
    std::optional<LibraryScanResultSummary> takePendingLibraryScanSummary();

  signals:
    void libraryScanStarted();
    void libraryScanFinished();
    void libraryScanSummary(const LibraryScanResultSummary& result);

  public slots:
    void startLibraryScan();
    void stopLibraryScan();

  private:
    void afterTrackAdded(const TrackPointer& pTrack) const;
    void afterTracksUpdated(const QSet<TrackId>& updatedTrackIds) const;
    void afterTracksRelocated(const QList<RelocatedTrack>& relocatedTracks) const;

    // Callback for GlobalTrackCache
    void saveEvictedTrack(Track* pTrack) noexcept override;

    // Might be called from any thread
    enum class TrackMetadataExportMode {
        Immediate,
        Deferred,
    };
    SaveTrackResult saveTrack(
            Track* pTrack,
            TrackMetadataExportMode mode) const;
    ExportTrackMetadataResult exportTrackMetadataBeforeSaving(
            Track* pTrack,
            TrackMetadataExportMode mode) const;

    const UserSettingsPointer m_pConfig;

    const parented_ptr<TrackCollection> m_pInternalCollection;

    QList<ExternalTrackCollection*> m_externalCollections;

    // TODO: Extract and decouple LibraryScanner from TrackCollectionManager
    std::unique_ptr<LibraryScanner> m_pScanner;

    // AI-generated (Claude Code). Start of AI-generated text.
    // Locations of tracks that an import added and that no save has seen
    // yet. The first save of such a track that has changes does not write
    // the tags to the file, also when the preference "write tags to files"
    // is on: an import alone never writes to a file (the background guess of
    // the cover art marks the track as changed). A later save writes as
    // always; it exports only tags that differ from the file.
    mutable std::mutex m_importedUnsavedMutex;
    mutable QSet<QString> m_importedUnsaved;
    // End of AI-generated text.
    std::atomic_bool m_libraryScanActive{false};
    std::mutex m_libraryScanSummaryMutex;
    std::optional<LibraryScanResultSummary> m_pendingLibraryScanSummary;
};
