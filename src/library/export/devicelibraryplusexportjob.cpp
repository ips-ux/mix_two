#include "library/export/devicelibraryplusexportjob.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QMetaObject>
#include <QSet>
#include <QSqlQuery>
#include <QtDebug>
#include <algorithm>
#include <memory>
#include <utility>

#include "library/coverart.h"
#include "library/dao/analysisdao.h"
#include "library/queryutil.h"
#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/devicelibraryplusstick.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/trackset/crate/crate.h"
#include "moc_devicelibraryplusexportjob.cpp"
#include "track/track.h"
#include "util/thread_affinity.h"
#include "waveform/waveform.h"
#include "waveform/waveformfactory.h"

namespace mixxx {

namespace {

using rekordbox::DeviceLibraryPlusDb;
using rekordbox::DeviceLibraryPlusExporter;
using rekordbox::DlpCopyCandidate;
using rekordbox::DlpExportPlaylist;
using rekordbox::DlpExportTrack;
using rekordbox::DlpStickInfo;
using rekordbox::DlpTrackResult;

constexpr qint64 kBytesPerMegabyte = 1024 * 1024;

/// The name of a stick without a volume label.
const QString kDefaultDeviceName = QStringLiteral("Mix Two");

/// "<artist> - <title>" for the lines of the summary. A track without both
/// names gets its file name.
QString trackLabel(const Track& track) {
    const QString artist = track.getArtist().trimmed();
    const QString title = track.getTitle().trimmed();
    if (artist.isEmpty() && title.isEmpty()) {
        return track.getFileInfo().fileName();
    }
    if (artist.isEmpty()) {
        return title;
    }
    if (title.isEmpty()) {
        return artist;
    }
    return artist + QStringLiteral(" - ") + title;
}

QString messageOrDefault(const QString& message) {
    if (!message.isEmpty()) {
        return message;
    }
    return QCoreApplication::translate("DeviceLibraryPlus",
            "Mix Two cannot use the library on the stick.");
}

} // namespace

DeviceLibraryPlusExportJob::DeviceLibraryPlusExportJob(
        QObject* parent,
        TrackCollectionManager* pTrackCollectionManager,
        const DeviceLibraryPlusExportRequest& request)
        : QThread{parent},
          m_request(request),
          m_pTrackCollectionManager(pTrackCollectionManager) {
    qRegisterMetaType<mixxx::DeviceLibraryPlusExportSummary>();
    // Must be collocated with the TrackCollectionManager.
    if (parent != nullptr) {
        DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(m_pTrackCollectionManager);
    } else {
        DEBUG_ASSERT(m_pTrackCollectionManager);
        moveToThread(m_pTrackCollectionManager->thread());
    }
}

DeviceLibraryPlusExportJob::~DeviceLibraryPlusExportJob() = default;

void DeviceLibraryPlusExportJob::slotCancel() {
    m_cancellationRequested = 1;
}

bool DeviceLibraryPlusExportJob::callInMainThread(const char* slotName, int index) {
    // The object lives in the main thread, so the call waits for it. Called
    // from the thread of the object, the call would wait for itself.
    DEBUG_ASSERT(QThread::currentThread() != thread());
    bool invoked;
    if (index < 0) {
        invoked = QMetaObject::invokeMethod(this, slotName, Qt::BlockingQueuedConnection);
    } else {
        invoked = QMetaObject::invokeMethod(
                this, slotName, Qt::BlockingQueuedConnection, Q_ARG(int, index));
    }
    DEBUG_ASSERT(invoked);
    if (!invoked) {
        qWarning() << "Device Library Plus export: cannot call the slot" << slotName;
    }
    return invoked;
}

void DeviceLibraryPlusExportJob::loadIds() {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(m_pTrackCollectionManager);

    m_trackEntries.clear();
    m_crateIds.clear();
    m_playlistIdsAndNames.clear();
    m_loadError.clear();

    TrackCollection* pCollection = m_pTrackCollectionManager->internalCollection();

    // The data of all tracks in one query: the location, and the artist and
    // album that the exporter needs for the place of the file on the stick.
    struct Details {
        TrackEntry entry;
        bool isHidden = false;
    };
    QHash<TrackId, Details> detailsById;
    QList<TrackId> orderedIds;
    QSqlQuery query(pCollection->database());
    query.prepare(QStringLiteral(
            "SELECT library.id, track_locations.location, library.artist, "
            "library.album, library.mixxx_deleted "
            "FROM library INNER JOIN track_locations "
            "ON library.location=track_locations.id "
            "ORDER BY library.id"));
    if (!query.exec()) {
        LOG_FAILED_QUERY(query) << "could not get the tracks of the library";
        m_loadError = QCoreApplication::translate(
                "DeviceLibraryPlus", "Mix Two cannot read the library.");
        return;
    }
    while (query.next()) {
        const TrackId trackId(query.value(0));
        if (!trackId.isValid()) {
            continue;
        }
        Details details;
        details.entry = TrackEntry{trackId,
                query.value(1).toString(),
                query.value(2).toString(),
                query.value(3).toString()};
        details.isHidden = query.value(4).toBool();
        detailsById.insert(trackId, details);
        orderedIds.append(trackId);
    }

    QSet<TrackId> knownTrackIds;
    const auto addTrack = [&](TrackId trackId) {
        if (!trackId.isValid() || knownTrackIds.contains(trackId)) {
            return;
        }
        knownTrackIds.insert(trackId);
        // A track that is not in the library stays in the list without data;
        // the job lists it as skipped.
        const auto it = detailsById.constFind(trackId);
        m_trackEntries.append(it != detailsById.constEnd() ? it->entry
                                                           : TrackEntry{trackId, {}, {}, {}});
    };

    if (m_request.crateIds.isEmpty() && m_request.playlistIds.isEmpty()) {
        // The whole library: the tracks that are not hidden, the crates that
        // contain them, and all playlists (as the Engine DJ export does).
        for (const TrackId& trackId : std::as_const(orderedIds)) {
            if (!detailsById.value(trackId).isHidden) {
                addTrack(trackId);
            }
        }
        QList<TrackId> trackIds;
        trackIds.reserve(m_trackEntries.size());
        for (const TrackEntry& entry : std::as_const(m_trackEntries)) {
            trackIds.append(entry.id);
        }
        const QSet<CrateId> crateIdsOfTracks =
                pCollection->crates().collectCrateIdsOfTracks(trackIds);
        m_crateIds = QList<CrateId>(crateIdsOfTracks.begin(), crateIdsOfTracks.end());
        m_playlistIdsAndNames =
                pCollection->getPlaylistDAO().getPlaylists(PlaylistDAO::PLHT_NOT_HIDDEN);
    } else {
        // The crates and playlists of the request, and their tracks.
        m_crateIds = QList<CrateId>(m_request.crateIds.begin(), m_request.crateIds.end());
        for (const auto& idAndName :
                pCollection->getPlaylistDAO().getPlaylists(PlaylistDAO::PLHT_NOT_HIDDEN)) {
            if (m_request.playlistIds.contains(idAndName.first)) {
                m_playlistIdsAndNames.append(idAndName);
            }
        }
        std::sort(m_crateIds.begin(), m_crateIds.end());
        for (const CrateId& crateId : std::as_const(m_crateIds)) {
            auto result = pCollection->crates().selectCrateTracksSorted(crateId);
            while (result.next()) {
                addTrack(result.trackId());
            }
        }
        for (const auto& idAndName : std::as_const(m_playlistIdsAndNames)) {
            const QList<TrackId> trackIds =
                    pCollection->getPlaylistDAO().getTrackIdsInPlaylistOrder(idAndName.first);
            for (const TrackId& trackId : trackIds) {
                addTrack(trackId);
            }
        }
    }
    std::sort(m_crateIds.begin(), m_crateIds.end());
    qDebug() << "Device Library Plus export: identified" << m_trackEntries.size()
             << "tracks," << m_crateIds.size() << "crates and"
             << m_playlistIdsAndNames.size() << "playlists";
}

void DeviceLibraryPlusExportJob::loadTrack(int index) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(m_pTrackCollectionManager);

    m_loadedTrack = DlpExportTrack();
    DEBUG_ASSERT(index >= 0 && index < m_trackEntries.size());
    const TrackId trackId = m_trackEntries.at(index).id;
    const TrackPointer pTrack = m_pTrackCollectionManager->getTrackById(trackId);
    if (!pTrack) {
        return;
    }
    m_loadedTrack.pTrack = pTrack;

    // The waveform of the analysis database. A track without one stays
    // without; the exporter skips it.
    auto& analysisDao = m_pTrackCollectionManager->internalCollection()->getAnalysisDAO();
    const auto waveformAnalyses =
            analysisDao.getAnalysesForTrackByType(trackId, AnalysisDao::TYPE_WAVEFORM);
    if (!waveformAnalyses.isEmpty()) {
        m_loadedTrack.pWaveform = ConstWaveformPointer(
                WaveformFactory::loadWaveformFromAnalysis(waveformAnalyses.first()));
    }

    // The cover image and its hash. The copy of the cover info gets a digest
    // when the library has none; the track does not change.
    CoverInfo coverInfo = pTrack->getCoverInfoWithLocation();
    if (coverInfo.hasImage()) {
        const CoverInfo::LoadedImage loaded = coverInfo.loadImage(pTrack);
        if (loaded.result == CoverInfo::LoadedImage::Result::Ok && !loaded.image.isNull()) {
            if (!coverInfo.hasCacheKey()) {
                coverInfo.setImageDigest(loaded.image);
            }
            m_loadedTrack.cover = loaded.image;
            m_loadedTrack.coverHash = coverInfo.cacheKey();
        }
    }
}

void DeviceLibraryPlusExportJob::loadCrate(int index) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(m_pTrackCollectionManager);

    m_loadedList = DlpExportPlaylist();
    DEBUG_ASSERT(index >= 0 && index < m_crateIds.size());
    const CrateId crateId = m_crateIds.at(index);
    TrackCollection* pCollection = m_pTrackCollectionManager->internalCollection();
    Crate crate;
    if (!pCollection->crates().readCrateById(crateId, &crate)) {
        // The crate is gone: no name, and the job skips the list.
        return;
    }
    m_loadedList.name = crate.getName();
    auto result = pCollection->crates().selectCrateTracksSorted(crateId);
    while (result.next()) {
        m_loadedList.trackIds.append(result.trackId());
    }
}

void DeviceLibraryPlusExportJob::loadPlaylist(int index) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(m_pTrackCollectionManager);

    m_loadedList = DlpExportPlaylist();
    DEBUG_ASSERT(index >= 0 && index < m_playlistIdsAndNames.size());
    const auto& idAndName = m_playlistIdsAndNames.at(index);
    m_loadedList.name = idAndName.second;
    m_loadedList.trackIds = m_pTrackCollectionManager->internalCollection()
                                    ->getPlaylistDAO()
                                    .getTrackIdsInPlaylistOrder(idAndName.first);
}

void DeviceLibraryPlusExportJob::run() {
    // From here on this runs in the worker thread. The Mix Two library is used
    // only through callInMainThread().
    const QDateTime now = QDateTime::currentDateTime();

    // Old working copies are on the computer, not on the stick.
    rekordbox::dlpRemoveOldWorkingCopies(now.addDays(-1));

    // Important 5: no call into the main thread after a cancel. Quitting Mix
    // Two cancels the job and can destroy the library soon after.
    if (isCancelled()) {
        qInfo() << "Device Library Plus export: cancelled";
        return;
    }
    if (!callInMainThread("loadIds")) {
        // Nothing is written before this point.
        emit failed(QCoreApplication::translate("DeviceLibraryPlus",
                "Mix Two cannot read its library. The stick did not change."));
        return;
    }
    if (!m_loadError.isEmpty()) {
        emit failed(m_loadError);
        return;
    }
    const int trackCount = static_cast<int>(m_trackEntries.size());
    const int crateCount = static_cast<int>(m_crateIds.size());
    const int playlistCount = static_cast<int>(m_playlistIdsAndNames.size());
    // One count for each track, playlist and crate, and one each for the
    // checks, the opening of the library and the writing of the library.
    emit jobMaximum(trackCount + crateCount + playlistCount + 3);
    int progress = 0;
    emit jobProgress(progress);
    if (isCancelled()) {
        return;
    }

    // The checks of the stick.
    const DlpStickInfo stick = rekordbox::dlpStickInfo(m_request.stickRoot);
    if (!stick.isReady || stick.isReadOnly) {
        emit failed(rekordbox::dlpMessageCannotWrite());
        return;
    }

    // The library on the stick. The database and the exporter live only in
    // this function, and so only in this thread. The exporter is declared after
    // the database and so it is destroyed first. Neither writes to the stick
    // before finish().
    QString error;
    const QString deviceName = m_request.deviceName.trimmed().isEmpty()
            ? kDefaultDeviceName
            : m_request.deviceName.trimmed();
    std::unique_ptr<DeviceLibraryPlusDb> pDb;
    if (stick.hasPlusLibrary) {
        pDb = DeviceLibraryPlusDb::open(m_request.stickRoot, &error);
    } else {
        pDb = DeviceLibraryPlusDb::create(
                m_request.stickRoot, deviceName, m_request.masterDbId, &error);
    }
    if (!pDb) {
        qWarning() << "Device Library Plus export: cannot open the library:" << error;
        emit failed(messageOrDefault(error));
        return;
    }
    DeviceLibraryPlusExporter::Options options;
    options.stickRoot = m_request.stickRoot;
    options.masterDbId = m_request.masterDbId;
    if (stick.fileSystem == rekordbox::DlpFileSystem::Fat32) {
        options.maxFileSize = rekordbox::kFat32MaxFileSize;
    }
    DeviceLibraryPlusExporter exporter(pDb.get(), options);
    if (!exporter.prepare(&error)) {
        emit failed(messageOrDefault(error));
        return;
    }

    // The free space (Ruling R12): the exporter says which files it copies. A
    // file that an earlier export copied is not counted.
    QList<DlpCopyCandidate> candidates;
    candidates.reserve(trackCount);
    for (const TrackEntry& entry : std::as_const(m_trackEntries)) {
        candidates.append(DlpCopyCandidate{entry.id, entry.location, entry.artist, entry.album});
    }
    const qint64 bytesNeeded =
            rekordbox::dlpBytesNeeded(exporter.bytesToCopy(candidates), trackCount);
    const qint64 bytesFree = rekordbox::dlpStickInfo(m_request.stickRoot).bytesFree;
    if (bytesFree < bytesNeeded) {
        const qint64 missing = bytesNeeded - bytesFree;
        emit failed(rekordbox::dlpMessageNotEnoughSpace(
                (missing + kBytesPerMegabyte - 1) / kBytesPerMegabyte));
        return;
    }
    emit jobProgress(++progress);
    if (isCancelled()) {
        return;
    }

    // The backup of the library on the stick, before anything is written there.
    if (stick.hasPlusLibrary) {
        // The message of the function is complete.
        if (rekordbox::dlpBackupLibrary(m_request.stickRoot,
                    m_request.backupBaseDir,
                    deviceName,
                    now,
                    &error)
                        .isEmpty()) {
            emit failed(messageOrDefault(error));
            return;
        }
    }
    // Ruling R16: after the checks and the backup, the files that an export
    // that did not finish can leave go to the computer. They are never
    // deleted: the names can also be files of the DJ.
    rekordbox::dlpMoveLeftoverFiles(m_request.stickRoot, m_request.backupBaseDir, deviceName, now);
    emit jobProgress(++progress);

    // The message when Qt cannot call a loading slot after the export began.
    const QString loadFailedMessage = QCoreApplication::translate("DeviceLibraryPlus",
            "Mix Two cannot read its library. The export stopped.");

    // The tracks.
    DeviceLibraryPlusExportSummary summary;
    for (int i = 0; i < trackCount; ++i) {
        if (isCancelled()) {
            qInfo() << "Device Library Plus export: cancelled";
            return;
        }
        if (!callInMainThread("loadTrack", i)) {
            emit failed(loadFailedMessage);
            return;
        }
        DlpExportTrack item = std::move(m_loadedTrack);
        m_loadedTrack = DlpExportTrack();
        if (isCancelled()) {
            qInfo() << "Device Library Plus export: cancelled";
            return;
        }
        if (!item.pTrack) {
            summary.skipped.append(QStringLiteral("%1: %2")
                            .arg(m_trackEntries.at(i).location,
                                    QCoreApplication::translate("DeviceLibraryPlus",
                                            "The track is not in the Mix Two "
                                            "library.")));
        } else {
            const DlpTrackResult result = exporter.exportTrack(item);
            const QString label = trackLabel(*item.pTrack);
            if (result.contentId) {
                ++summary.tracksExported;
            } else {
                summary.skipped.append(QStringLiteral("%1: %2").arg(label, result.skipReason));
            }
            for (const QString& warning : result.warnings) {
                summary.warnings.append(QStringLiteral("%1: %2").arg(label, warning));
            }
        }
        // The track is released in this thread.
        item = DlpExportTrack();
        emit jobProgress(++progress);
    }

    // The crates and the playlists.
    QList<DlpExportPlaylist> crates;
    for (int i = 0; i < crateCount; ++i) {
        if (isCancelled()) {
            qInfo() << "Device Library Plus export: cancelled";
            return;
        }
        if (!callInMainThread("loadCrate", i)) {
            emit failed(loadFailedMessage);
            return;
        }
        DlpExportPlaylist list = std::move(m_loadedList);
        m_loadedList = DlpExportPlaylist();
        if (isCancelled()) {
            qInfo() << "Device Library Plus export: cancelled";
            return;
        }
        if (!list.name.isEmpty()) {
            crates.append(std::move(list));
        }
        emit jobProgress(++progress);
    }
    QList<DlpExportPlaylist> playlists;
    for (int i = 0; i < playlistCount; ++i) {
        if (isCancelled()) {
            qInfo() << "Device Library Plus export: cancelled";
            return;
        }
        if (!callInMainThread("loadPlaylist", i)) {
            emit failed(loadFailedMessage);
            return;
        }
        DlpExportPlaylist list = std::move(m_loadedList);
        m_loadedList = DlpExportPlaylist();
        if (isCancelled()) {
            qInfo() << "Device Library Plus export: cancelled";
            return;
        }
        if (!list.name.isEmpty()) {
            playlists.append(std::move(list));
        }
        emit jobProgress(++progress);
    }
    if (isCancelled()) {
        qInfo() << "Device Library Plus export: cancelled";
        return;
    }
    if (!exporter.exportPlaylists(playlists, crates, &error)) {
        emit failed(messageOrDefault(error));
        return;
    }

    // The last point to stop: finish() writes the library to the stick.
    if (isCancelled()) {
        qInfo() << "Device Library Plus export: cancelled";
        return;
    }
    // Ruling R20: the export had tracks, and none of them has a row on the
    // stick (also when Mix Two could not load them). The stick does not
    // change.
    if (trackCount > 0 && exporter.usableTrackCount() == 0) {
        emit failed(rekordbox::dlpMessageNoTrackExported());
        return;
    }
    if (!exporter.finish(&error)) {
        emit failed(messageOrDefault(error));
        return;
    }
    emit jobProgress(++progress);

    summary.playlistsExported = exporter.playlistCount();
    qInfo() << "Device Library Plus export completed:" << summary.tracksExported
            << "tracks," << summary.playlistsExported << "playlists," << summary.skipped.size()
            << "skipped";
    emit completed(summary);
}

} // namespace mixxx
