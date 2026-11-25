#include "library/trackcollection.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <algorithm>

#include "library/basetrackcache.h"
#include "library/libraryprotection.h"
#include "library/trackset/crate/crate.h"
#include "moc_trackcollection.cpp"
#include "sources/soundsourceproxy.h"
#include "track/globaltrackcache.h"
#include "util/assert.h"
#include "util/db/sqltransaction.h"
#include "util/dnd.h"
#include "util/logger.h"

namespace {

mixxx::Logger kLogger("TrackCollection");

} // anonymous namespace

TrackCollection::TrackCollection(
        QObject* parent,
        const UserSettingsPointer& pConfig)
        : QObject(parent),
          m_analysisDao(pConfig),
          m_trackDao(m_cueDao, m_playlistDao,
                     m_analysisDao, m_libraryHashDao, pConfig) {
    // Forward signals from TrackDAO
    connect(&m_trackDao,
            &TrackDAO::trackDirty,
            this,
            &TrackCollection::trackDirty,
            /*signal-to-signal*/ Qt::DirectConnection);
    connect(&m_trackDao,
            &TrackDAO::tracksAdded,
            this,
            &TrackCollection::tracksAdded,
            /*signal-to-signal*/ Qt::DirectConnection);
    connect(&m_trackDao,
            &TrackDAO::tracksChanged,
            this,
            &TrackCollection::tracksChanged,
            /*signal-to-signal*/ Qt::DirectConnection);
    connect(&m_trackDao,
            &TrackDAO::tracksRemoved,
            this,
            &TrackCollection::tracksRemoved, // unused
            /*signal-to-signal*/ Qt::DirectConnection);
    connect(&m_trackDao,
            &TrackDAO::forceModelUpdate,
            this,
            &TrackCollection::multipleTracksChanged,
            /*signal-to-signal*/ Qt::DirectConnection);
}

TrackCollection::~TrackCollection() {
    if (kLogger.debugEnabled()) {
        kLogger.debug() << "~TrackCollection()";
    }
    // The database should have been detached earlier
    DEBUG_ASSERT(!m_database.isOpen());
}

void TrackCollection::repairDatabase(const QSqlDatabase& database) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    kLogger.info() << "Repairing database";
    m_crates.repairDatabase(database);
    m_directoryDao.repairDatabase(database);
}

void TrackCollection::connectDatabase(const QSqlDatabase& database) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    kLogger.info() << "Connecting database";
    DEBUG_ASSERT(database.isOpen());
    m_database = database;
    m_trackDao.initialize(database);
    m_playlistDao.initialize(database);
    m_cueDao.initialize(database);
    m_directoryDao.initialize(database);
    m_analysisDao.initialize(database);
    m_libraryHashDao.initialize(database);
    m_crates.connectDatabase(database);
}

void TrackCollection::disconnectDatabase() {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    kLogger.info() << "Disconnecting database";
    m_database = QSqlDatabase();
    m_trackDao.finish();
    m_crates.disconnectDatabase();
}

void TrackCollection::connectTrackSource(QSharedPointer<BaseTrackCache> pTrackSource) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    VERIFY_OR_DEBUG_ASSERT(m_pTrackSource.isNull()) {
        kLogger.warning() << "Track source has already been connected";
        return;
    }
    kLogger.info() << "Connecting track source";
    m_pTrackSource = pTrackSource;
    connect(this,
            &TrackCollection::scanTrackAdded,
            m_pTrackSource.data(),
            &BaseTrackCache::slotScanTrackAdded);
    connect(&m_trackDao,
            &TrackDAO::trackDirty,
            m_pTrackSource.data(),
            &BaseTrackCache::slotTrackDirty);
    connect(&m_trackDao,
            &TrackDAO::trackClean,
            m_pTrackSource.data(),
            &BaseTrackCache::slotTrackClean);
    connect(&m_trackDao,
            &TrackDAO::tracksAdded,
            m_pTrackSource.data(),
            &BaseTrackCache::slotTracksAddedOrChanged);
    connect(&m_trackDao,
            &TrackDAO::tracksChanged,
            m_pTrackSource.data(),
            &BaseTrackCache::slotTracksAddedOrChanged);
    connect(&m_trackDao,
            &TrackDAO::tracksRemoved,
            m_pTrackSource.data(),
            &BaseTrackCache::slotTracksRemoved);
}

QWeakPointer<BaseTrackCache> TrackCollection::disconnectTrackSource() {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    auto pWeakPtr = m_pTrackSource.toWeakRef();
    if (m_pTrackSource) {
        kLogger.info() << "Disconnecting track source";
        m_trackDao.disconnect(m_pTrackSource.data());
        m_pTrackSource.reset();
    }
    return pWeakPtr;
}

QList<mixxx::FileInfo> TrackCollection::loadRootDirs(bool skipInvalidOrMissing) const {
    return m_directoryDao.loadAllDirectories(skipInvalidOrMissing);
}

QStringList TrackCollection::getRootDirStrings() const {
    QStringList rootDirStrings;
    for (auto& rootDirectory : m_directoryDao.getRootDirectories()) {
        rootDirStrings.append(rootDirectory.path);
    }
    return rootDirStrings;
}

QList<DirectoryDAO::RootDirectoryInfo> TrackCollection::getRootDirectories() const {
    return m_directoryDao.getRootDirectories();
}

DirectoryDAO::AddResult TrackCollection::addDirectory(const mixxx::FileInfo& rootDir) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    SqlTransaction transaction(m_database);
    DirectoryDAO::AddResult result = m_directoryDao.addDirectory(rootDir);
    switch (result) {
    case DirectoryDAO::AddResult::Ok:
        transaction.commit();
        break;
    case DirectoryDAO::AddResult::AlreadyWatching:
    case DirectoryDAO::AddResult::InvalidOrMissingDirectory:
    case DirectoryDAO::AddResult::UnreadableDirectory:
    case DirectoryDAO::AddResult::SqlError:
        break;
    default:
        DEBUG_ASSERT("unreachable");
        return DirectoryDAO::AddResult::SqlError;
    }
    return result;
}

DirectoryDAO::RemoveResult TrackCollection::removeDirectory(const mixxx::FileInfo& rootDir) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    SqlTransaction transaction(m_database);
    DirectoryDAO::RemoveResult result = m_directoryDao.removeDirectory(rootDir);
    switch (result) {
    case DirectoryDAO::RemoveResult::Ok:
        transaction.commit();
        break;
    case DirectoryDAO::RemoveResult::NotFound:
    case DirectoryDAO::RemoveResult::SqlError:
        break;
    default:
        DEBUG_ASSERT("unreachable");
        return DirectoryDAO::RemoveResult::SqlError;
    }
    return result;
}

DirectoryDAO::RelocateResult TrackCollection::relocateDirectory(
        const QString& oldDir, const QString& newDir) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    // We only call this method if the user has picked a relocated directory via
    // a file dialog. This means the system sandboxer (if we are sandboxed) has
    // granted us permission to this folder. Create a security bookmark while we
    // have permission so that we can access the folder on future runs. We need
    // to canonicalize the path so we first wrap the directory string with a
    // QDir.
    Sandbox::createSecurityTokenForDir(QDir(newDir));

    SqlTransaction transaction(m_database);
    DirectoryDAO::RelocateResult result;
    QList<RelocatedTrack> relocatedTracks;
    std::tie(result, relocatedTracks) = m_directoryDao.relocateDirectory(oldDir, newDir);
    transaction.commit();

    if (result != DirectoryDAO::RelocateResult::Ok || relocatedTracks.isEmpty()) {
        // Error or no tracks moved
        return result;
    }

    // Inform the TrackDAO about the changes
    m_trackDao.slotDatabaseTracksRelocated(std::move(relocatedTracks));

    GlobalTrackCacheLocker().relocateCachedTracks(&m_trackDao);

    return result;
}

// AI-generated (Claude Code). Start of AI-generated text.
bool TrackCollection::relocateTrackLocation(TrackId trackId, const QString& newLocation) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);
    if (!trackId.isValid()) {
        return false;
    }
    // Library Protection: the file place of a track does not change. Relocate,
    // Auto Relocate and the library folder consolidation all end here.
    if (!mixxx::library::isActionAllowed(mixxx::library::ProtectedAction::RelocateTracks)) {
        return false;
    }
    const mixxx::FileInfo newFile(newLocation);
    if (!newFile.checkFileExists() || !newFile.hasLocation()) {
        return false;
    }
    const QString oldLocation = m_trackDao.getTrackLocation(trackId);
    if (oldLocation.isEmpty()) {
        return false;
    }

    {
        SqlTransaction transaction(m_database);
        QSqlQuery query(m_database);
        // The `location` column is unique: when another track already uses
        // the new location, the query fails and nothing changes.
        query.prepare(QStringLiteral(
                "UPDATE track_locations SET location=:newloc, directory=:newdir, "
                "filename=:newname, filesize=:newsize, fs_deleted=0, "
                "needs_verification=0 "
                "WHERE id=(SELECT location FROM library WHERE id=:id)"));
        query.bindValue(QStringLiteral(":newloc"), newFile.location());
        query.bindValue(QStringLiteral(":newdir"), newFile.locationPath());
        query.bindValue(QStringLiteral(":newname"), newFile.fileName());
        // The stored size follows the new file: Auto Relocate compares it.
        query.bindValue(QStringLiteral(":newsize"), newFile.sizeInBytes());
        query.bindValue(QStringLiteral(":id"), trackId.toVariant());
        if (!query.exec() || query.numRowsAffected() != 1) {
            return false;
        }
        // The track object in the track cache (for example a track in a
        // deck) gets the new file too, so that only one track object exists
        // for this id and it never uses the old path again.
        GlobalTrackCacheLocker cacheLocker;
        if (!cacheLocker.relocateCachedTrack(trackId, mixxx::FileAccess(newFile))) {
            return false;
        }
        if (!transaction.commit()) {
            cacheLocker.relocateCachedTrack(
                    trackId, mixxx::FileAccess(mixxx::FileInfo(oldLocation)));
            return false;
        }
    }

    // Tell the track lists about the new place.
    QList<RelocatedTrack> relocatedTracks;
    relocatedTracks.append(RelocatedTrack(
            TrackRef::fromFilePath(oldLocation, trackId),
            TrackRef::fromFilePath(newFile.location())));
    m_trackDao.slotDatabaseTracksRelocated(std::move(relocatedTracks));
    return true;
}
// End of AI-generated text.

// AI-generated (Claude Code). Start of AI-generated text.
namespace mixxx {
namespace library {

QList<RelocateCandidate> findRelocateCandidates(
        const QStringList& folders, const std::atomic_bool* pCancel) {
    QList<RelocateCandidate> candidates;
    QSet<QString> seen;
    for (const QString& folder : folders) {
        if (folder.isEmpty() || !QFileInfo(folder).isDir()) {
            continue;
        }
        QDirIterator it(folder, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (pCancel && pCancel->load()) {
                return candidates;
            }
            it.next();
            const QFileInfo info = it.fileInfo();
            if (!SoundSourceProxy::isFileNameSupported(info.fileName())) {
                continue;
            }
            RelocateCandidate candidate;
            candidate.location = FileInfo(info.absoluteFilePath()).location();
            if (candidate.location.isEmpty() || seen.contains(candidate.location)) {
                continue;
            }
            seen.insert(candidate.location);
            candidate.fileName = info.fileName();
            candidate.sizeInBytes = info.size();
            candidates.append(std::move(candidate));
        }
    }
    return candidates;
}

QStringList relocateMatchLines(const QList<RelocateMatch>& matches) {
    QStringList lines;
    lines.reserve(matches.size());
    for (const RelocateMatch& match : matches) {
        lines.append(match.oldLocation + QStringLiteral(" -> ") + match.newLocation);
    }
    return lines;
}

QString encodeRelocateFolders(const QStringList& folders) {
    QStringList clean;
    for (const QString& folder : folders) {
        if (!folder.isEmpty() && !clean.contains(folder)) {
            clean.append(folder);
        }
    }
    return QString::fromUtf8(
            QJsonDocument(QJsonArray::fromStringList(clean)).toJson(QJsonDocument::Compact));
}

QStringList decodeRelocateFolders(const QString& text) {
    QStringList folders;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (!doc.isArray()) {
        return folders;
    }
    const QJsonArray array = doc.array();
    for (const QJsonValue& value : array) {
        const QString folder = value.toString();
        if (!folder.isEmpty() && !folders.contains(folder)) {
            folders.append(folder);
        }
    }
    return folders;
}

} // namespace library
} // namespace mixxx

namespace {

/// What the library stores for the file of a track.
struct StoredFile {
    bool known = false;
    QString location;
    QString fileName;
    qint64 sizeInBytes = 0;
};

StoredFile loadStoredFile(const QSqlDatabase& database, TrackId trackId) {
    StoredFile stored;
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
            "SELECT track_locations.location, track_locations.filename, "
            "track_locations.filesize FROM library INNER JOIN track_locations "
            "ON library.location=track_locations.id WHERE library.id=:id"));
    query.bindValue(QStringLiteral(":id"), trackId.toVariant());
    if (query.exec() && query.next()) {
        stored.known = true;
        stored.location = query.value(0).toString();
        stored.fileName = query.value(1).toString();
        stored.sizeInBytes = query.value(2).toLongLong();
    }
    return stored;
}

bool locationIsInLibrary(const QSqlDatabase& database, const QString& location) {
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT 1 FROM track_locations WHERE location=:loc"));
    query.bindValue(QStringLiteral(":loc"), location);
    return query.exec() && query.next();
}

bool fileIsPresent(const QString& location) {
    return QFileInfo(location).isFile();
}

/// The number of folder names that two paths share at their ends. A candidate
/// with a long shared end looks more like the old place of the file.
int sharedEndLength(const QString& a, const QString& b) {
    const QStringList partsA = QFileInfo(a).absolutePath().split('/', Qt::SkipEmptyParts);
    const QStringList partsB = QFileInfo(b).absolutePath().split('/', Qt::SkipEmptyParts);
    int shared = 0;
    while (shared < partsA.size() && shared < partsB.size() &&
            partsA[partsA.size() - 1 - shared].compare(
                    partsB[partsB.size() - 1 - shared], Qt::CaseInsensitive) == 0) {
        ++shared;
    }
    return shared;
}

} // namespace

mixxx::library::RelocateOutcome TrackCollection::relocateTracksToFile(
        const QList<TrackId>& trackIds, TrackId anchorId, const QString& anchorNewLocation) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);
    mixxx::library::RelocateOutcome outcome;
    const QString newFolder = QFileInfo(anchorNewLocation).absolutePath();
    for (const TrackId& trackId : trackIds) {
        const StoredFile stored = loadStoredFile(m_database, trackId);
        if (!stored.known) {
            ++outcome.notFound;
            continue;
        }
        if (fileIsPresent(stored.location)) {
            // A track with a file is not missing: never move it.
            ++outcome.skipped;
            continue;
        }
        // The anchor track gets the file that the user chose. The others are
        // looked up by their own file name in the folder of that file.
        const QString newLocation = trackId == anchorId
                ? anchorNewLocation
                : QDir(newFolder).filePath(stored.fileName);
        if (!fileIsPresent(newLocation)) {
            ++outcome.notFound;
            continue;
        }
        if (relocateTrackLocation(trackId, newLocation)) {
            ++outcome.relocated;
        } else {
            ++outcome.failed;
        }
    }
    return outcome;
}

mixxx::library::RelocatePlan TrackCollection::planAutoRelocate(
        const QList<TrackId>& trackIds,
        const QList<mixxx::library::RelocateCandidate>& candidates) const {
    mixxx::library::RelocatePlan plan;
    QHash<QString, QList<int>> candidatesByName;
    for (int i = 0; i < candidates.size(); ++i) {
        candidatesByName[candidates[i].fileName.toLower()].append(i);
    }
    QSet<int> usedCandidates;
    for (const TrackId& trackId : trackIds) {
        const StoredFile stored = loadStoredFile(m_database, trackId);
        if (!stored.known) {
            ++plan.notFound;
            continue;
        }
        if (fileIsPresent(stored.location)) {
            ++plan.skipped;
            continue;
        }
        // Without a stored size nothing can be compared: no match.
        QList<int> matches;
        if (stored.sizeInBytes > 0) {
            const QList<int> sameName = candidatesByName.value(stored.fileName.toLower());
            for (int index : sameName) {
                if (!usedCandidates.contains(index) &&
                        candidates[index].sizeInBytes == stored.sizeInBytes &&
                        !locationIsInLibrary(m_database, candidates[index].location)) {
                    matches.append(index);
                }
            }
        }
        if (matches.isEmpty()) {
            ++plan.notFound;
            continue;
        }
        // Choice: when several files match, the one whose folder names are
        // most like the old folder names wins.
        const int best = *std::max_element(matches.begin(),
                matches.end(),
                [&](int a, int b) {
                    return sharedEndLength(candidates[a].location, stored.location) <
                            sharedEndLength(candidates[b].location, stored.location);
                });
        usedCandidates.insert(best);
        plan.matches.append({trackId, stored.location, candidates[best].location});
    }
    return plan;
}

mixxx::library::RelocateOutcome TrackCollection::applyRelocateMatches(
        const QList<mixxx::library::RelocateMatch>& matches) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);
    mixxx::library::RelocateOutcome outcome;
    for (const auto& match : matches) {
        if (relocateTrackLocation(match.trackId, match.newLocation)) {
            ++outcome.relocated;
        } else {
            ++outcome.failed;
        }
    }
    return outcome;
}

mixxx::library::RelocateOutcome TrackCollection::autoRelocateTracks(
        const QList<TrackId>& trackIds,
        const QList<mixxx::library::RelocateCandidate>& candidates) {
    const mixxx::library::RelocatePlan plan = planAutoRelocate(trackIds, candidates);
    mixxx::library::RelocateOutcome outcome = applyRelocateMatches(plan.matches);
    outcome.notFound = plan.notFound;
    outcome.skipped = plan.skipped;
    return outcome;
}
// End of AI-generated text.

QList<TrackId> TrackCollection::resolveTrackIds(
        const QList<QUrl>& urls,
        TrackDAO::ResolveTrackIdFlags flags) {
    QList<TrackId> trackIds = m_trackDao.resolveTrackIds(
            urls,
            flags);
    if (flags & TrackDAO::ResolveTrackIdFlag::UnhideHidden) {
        unhideTracks(trackIds);
    }
    return trackIds;
}

QList<TrackId> TrackCollection::resolveTrackIds(
        const QList<mixxx::FileInfo>& trackFiles,
        TrackDAO::ResolveTrackIdFlags flags) {
    QList<TrackId> trackIds = m_trackDao.resolveTrackIds(
            trackFiles,
            flags);
    if (flags & TrackDAO::ResolveTrackIdFlag::UnhideHidden) {
        unhideTracks(trackIds);
    }
    return trackIds;
}

QList<TrackId> TrackCollection::resolveTrackIds(
        const QList<mixxx::FileInfo>& trackFiles,
        QObject* pSource) {
    TrackDAO::ResolveTrackIdFlags flags =
            TrackDAO::ResolveTrackIdFlag::UnhideHidden;
    if (!pSource) {
        flags |= TrackDAO::ResolveTrackIdFlag::AddMissing;
    }
    return resolveTrackIds(trackFiles, flags);
}

QList<TrackId> TrackCollection::resolveTrackIdsFromUrls(
        const QList<QUrl>& urls,
        bool addMissing) {
    if (urls.isEmpty()) {
        return QList<TrackId>();
    }

    TrackDAO::ResolveTrackIdFlags flags =
            TrackDAO::ResolveTrackIdFlag::UnhideHidden;
    if (addMissing) {
        flags |= TrackDAO::ResolveTrackIdFlag::AddMissing;
    }
    return resolveTrackIds(urls, flags);
}

QList<TrackId> TrackCollection::resolveTrackIdsFromLocations(
        const QList<QString>& locations) {
    QList<mixxx::FileInfo> fileInfos;
    fileInfos.reserve(locations.size());
    for (const QString& location : locations) {
        fileInfos.append(mixxx::FileInfo(location));
    }
    return resolveTrackIds(
            fileInfos,
            TrackDAO::ResolveTrackIdFlag::UnhideHidden | TrackDAO::ResolveTrackIdFlag::AddMissing);
}

bool TrackCollection::hideTracks(const QList<TrackId>& trackIds) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    // Warn if tracks have a playlist membership
    QSet<int> allPlaylistIds;
    for (const auto& trackId: trackIds) {
        QSet<int> playlistIds;
        m_playlistDao.getPlaylistsTrackIsIn(trackId, &playlistIds);
        for (const auto& playlistId : std::as_const(playlistIds)) {
            if (m_playlistDao.getHiddenType(playlistId) != PlaylistDAO::PLHT_SET_LOG) {
                allPlaylistIds.insert(playlistId);
            }
        }
    }

    if (!allPlaylistIds.isEmpty()) {
         QStringList playlistNames;
         playlistNames.reserve(allPlaylistIds.count());
         for (const auto& playlistId: allPlaylistIds) {
             playlistNames.append(m_playlistDao.getPlaylistName(playlistId));
         }

         QString playlistNamesSection =
                 "\n\n\"" %
                 playlistNames.join("\"\n\"") %
                 "\"\n\n";

         if (QMessageBox::question(
                 nullptr,
                 tr("Hiding tracks"),
                 tr("The selected tracks are in the following playlists:"
                     "%1"
                     "Hiding them will remove them from these playlists. Continue?")
                         .arg(playlistNamesSection),
                 QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok) {
             return false;
         }
     }

    // Transactional
    SqlTransaction transaction(m_database);
    VERIFY_OR_DEBUG_ASSERT(transaction) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(m_trackDao.hideTracks(trackIds)) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(transaction.commit()) {
        return false;
    }

    m_playlistDao.removeTracksFromPlaylists(trackIds);

    // Post-processing
    // TODO(XXX): Move signals from TrackDAO to TrackCollection
    m_trackDao.afterHidingTracks(trackIds);
    QSet<CrateId> modifiedCrateSummaries(
            m_crates.collectCrateIdsOfTracks(trackIds));

    // Emit signal(s)
    // TODO(XXX): Emit signals here instead of from DAOs
    emit crateSummaryChanged(modifiedCrateSummaries);

    return true;
}

void TrackCollection::hideAllTracks(const QDir& rootDir) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    m_trackDao.hideAllTracks(rootDir);
}

bool TrackCollection::unhideTracks(const QList<TrackId>& trackIds) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    VERIFY_OR_DEBUG_ASSERT(m_trackDao.unhideTracks(trackIds)) {
        return false;
    }

    // Post-processing
    // TODO(XXX): Move signals from TrackDAO to TrackCollection
    // to update BaseTrackCache
    m_trackDao.afterUnhidingTracks(trackIds);

    // Emit signal(s)
    // TODO(XXX): Emit signals here instead of from DAOs
    // To update labels of CrateFeature, because unhiding might make a
    // crate track visible again.
    QSet<CrateId> modifiedCrateSummaries =
            m_crates.collectCrateIdsOfTracks(trackIds);
    emit crateSummaryChanged(modifiedCrateSummaries);

    return true;
}

bool TrackCollection::purgeTracks(
        const QList<TrackId>& trackIds) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    // Transactional
    SqlTransaction transaction(m_database);
    VERIFY_OR_DEBUG_ASSERT(transaction) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(m_trackDao.onPurgingTracks(trackIds)) {
        return false;
    }
    // Collect crates of tracks that will be purged before actually purging
    // them within the same transactions. Those tracks will be removed from
    // all crates on purging.
    QSet<CrateId> modifiedCrateSummaries(
            m_crates.collectCrateIdsOfTracks(trackIds));
    VERIFY_OR_DEBUG_ASSERT(m_crates.onPurgingTracks(trackIds)) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(transaction.commit()) {
        return false;
    }
    // TODO(XXX): Move reversible actions inside transaction
    m_cueDao.deleteCuesForTracks(trackIds);
    m_playlistDao.removeTracksFromPlaylists(trackIds, true);
    m_analysisDao.deleteAnalyses(trackIds);

    // Post-processing
    // TODO(XXX): Move signals from TrackDAO to TrackCollection
    m_trackDao.afterPurgingTracks(trackIds);

    // Emit signal(s)
    // TODO(XXX): Emit signals here instead of from DAOs
    emit crateSummaryChanged(modifiedCrateSummaries);

    return true;
}

bool TrackCollection::purgeAllTracks(
        const QDir& rootDir) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    const QList<TrackRef> trackRefs = m_trackDao.getAllTrackRefs(rootDir);
    QList<TrackId> trackIds;
    trackIds.reserve(trackRefs.size());
    for (const auto& trackRef : trackRefs) {
        DEBUG_ASSERT(trackRef.hasId());
        trackIds.append(trackRef.getId());
    }
    return purgeTracks(trackIds);
}

bool TrackCollection::insertCrate(
        const Crate& crate,
        CrateId* pCrateId) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    // Transactional
    SqlTransaction transaction(m_database);
    VERIFY_OR_DEBUG_ASSERT(transaction) {
        return false;
    }
    CrateId crateId;
    VERIFY_OR_DEBUG_ASSERT(m_crates.onInsertingCrate(crate, &crateId)) {
        return false;
    }
    DEBUG_ASSERT(crateId.isValid());
    VERIFY_OR_DEBUG_ASSERT(transaction.commit()) {
        return false;
    }

    // Emit signals
    emit crateInserted(crateId);

    if (pCrateId != nullptr) {
        *pCrateId = crateId;
    }
    return true;
}

bool TrackCollection::updateCrate(
        const Crate& crate) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    // Transactional
    SqlTransaction transaction(m_database);
    VERIFY_OR_DEBUG_ASSERT(transaction) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(m_crates.onUpdatingCrate(crate)) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(transaction.commit()) {
        return false;
    }

    // Emit signals
    emit crateUpdated(crate.getId());

    return true;
}

bool TrackCollection::deleteCrate(
        CrateId crateId) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    // Transactional
    SqlTransaction transaction(m_database);
    VERIFY_OR_DEBUG_ASSERT(transaction) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(m_crates.onDeletingCrate(crateId)) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(transaction.commit()) {
        return false;
    }

    // Emit signals
    emit crateDeleted(crateId);

    return true;
}

bool TrackCollection::addCrateTracks(
        CrateId crateId,
        const QList<TrackId>& trackIds) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    // Transactional
    SqlTransaction transaction(m_database);
    VERIFY_OR_DEBUG_ASSERT(transaction) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(m_crates.onAddingCrateTracks(crateId, trackIds)) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(transaction.commit()) {
        return false;
    }

    // Emit signals
    emit crateTracksChanged(crateId, trackIds, QList<TrackId>());

    return true;
}

bool TrackCollection::removeCrateTracks(
        CrateId crateId,
        const QList<TrackId>& trackIds) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    // Transactional
    SqlTransaction transaction(m_database);
    VERIFY_OR_DEBUG_ASSERT(transaction) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(m_crates.onRemovingCrateTracks(crateId, trackIds)) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(transaction.commit()) {
        return false;
    }

    // Emit signals
    emit crateTracksChanged(crateId, QList<TrackId>(), trackIds);

    return true;
}

bool TrackCollection::updateAutoDjCrate(
        CrateId crateId,
        bool isAutoDjSource) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    Crate crate;
    VERIFY_OR_DEBUG_ASSERT(crates().readCrateById(crateId, &crate)) {
        return false; // inexistent or failure
    }
    if (crate.isAutoDjSource() == isAutoDjSource) {
        return false; // nothing to do
    }
    crate.setAutoDjSource(isAutoDjSource);
    return updateCrate(crate);
}

bool TrackCollection::saveTrack(Track* pTrack) const {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    return m_trackDao.saveTrack(pTrack);
}

TrackPointer TrackCollection::getTrackById(
        TrackId trackId) const {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    return m_trackDao.getTrackById(trackId);
}

TrackPointer TrackCollection::getTrackByRef(
        const TrackRef& trackRef) const {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    return m_trackDao.getTrackByRef(trackRef);
}

TrackPointer TrackCollection::getOrAddTrack(
        const TrackRef& trackRef,
        bool* pAlreadyInLibrary) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    return m_trackDao.getOrAddTrack(trackRef, pAlreadyInLibrary);
}

TrackId TrackCollection::addTrack(
        const TrackPointer& pTrack,
        bool unremove) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(this);

    m_trackDao.addTracksPrepare();
    const auto trackId = m_trackDao.addTracksAddTrack(pTrack, unremove);
    m_trackDao.addTracksFinish(!trackId.isValid());
    return trackId;
}
