#pragma once

#include <QDir>
#include <QList>
#include <QSet>
#include <atomic>
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
    // Files that the library already had.
    int alreadyInLibrary = 0;
    // Files that are missing, that are no sound files, or that the library
    // could not add.
    int skipped = 0;

    ImportFilesResult& operator+=(const ImportFilesResult& other) {
        added += other.added;
        alreadyInLibrary += other.alreadyInLibrary;
        skipped += other.skipped;
        return *this;
    }
};

/// All sound files in the folder and below it, sorted by path. It reads the
/// file system only, so it is safe to call from a worker thread.
QStringList soundFilesInFolder(const QString& folder);

/// Cuts a list of files into chunks of at most chunkSize files, so that a
/// caller can import a long list in steps and keep the window responsive.
QList<QStringList> splitImportList(const QStringList& files, int chunkSize);

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
    // already stays as it is. Call it in the main thread.
    mixxx::library::ImportFilesResult importFiles(const QStringList& locations) const;
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
    std::atomic_bool m_libraryScanActive{false};
    std::mutex m_libraryScanSummaryMutex;
    std::optional<LibraryScanResultSummary> m_pendingLibraryScanSummary;
};
