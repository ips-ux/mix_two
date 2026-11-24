#include "library/export/devicelibraryplusexportjob.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QSharedPointer>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>

#include "library/dao/analysisdao.h"
#include "library/dao/playlistdao.h"
#include "library/export/dlgdeviceexport.h"
#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/trackset/crate/crate.h"
#include "sources/soundsourceproxy.h"
#include "test/librarytest.h"
#include "track/track.h"
#include "waveform/waveform.h"
#include "waveform/waveformfactory.h"

namespace mixxx {

namespace {

using rekordbox::DeviceLibraryPlusDb;
using rekordbox::DlpContentRef;

constexpr qint64 kMasterDbId = 4242;
const QString kDeviceName = QStringLiteral("JOB TEST");

/// What a run of the job sent.
struct Outcome {
    bool finished = false;
    int completedCount = 0;
    int failedCount = 0;
    DeviceLibraryPlusExportSummary summary;
    QString failure;
};

/// Ruling R21: the export job with a real Mix Two library (LibraryTest) and
/// a stick in a temporary folder.
class DeviceLibraryPlusExportJobTest : public LibraryTest {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_music.isValid());
        ASSERT_TRUE(m_stick.isValid());
        ASSERT_TRUE(m_backups.isValid());
    }

    /// Adds a copy of a file of src/test/id3-test-data to the library.
    TrackPointer addTrack(const QString& testFile, const QString& folder) {
        const QString source =
                getOrInitTestDir().filePath(QStringLiteral("id3-test-data/") + testFile);
        const QString targetFolder = m_music.filePath(folder);
        EXPECT_TRUE(QDir().mkpath(targetFolder));
        const QString target = QDir(targetFolder).filePath(testFile);
        EXPECT_TRUE(QFile::copy(source, target)) << qPrintable(target);
        QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
        TrackPointer pTrack = getOrAddTrackByLocation(target);
        EXPECT_TRUE(pTrack);
        if (pTrack && !pTrack->getSampleRate().isValid()) {
            SoundSourceProxy(pTrack).updateTrackFromSource(
                    SoundSourceProxy::UpdateTrackFromSourceMode::Always,
                    SyncTrackMetadataParams{});
        }
        EXPECT_TRUE(pTrack && pTrack->getId().isValid());
        EXPECT_TRUE(pTrack && pTrack->getSampleRate().isValid());
        return pTrack;
    }

    /// Stores a complete waveform for the track in the analysis database, as
    /// the analyser does.
    void storeWaveform(const TrackPointer& pTrack) {
        const int sampleRate = static_cast<int>(pTrack->getSampleRate().value());
        const auto frames = static_cast<SINT>(pTrack->getDuration() * sampleRate);
        Waveform waveform(sampleRate, frames, 441, -1, 0);
        WaveformData* pData = waveform.data();
        for (int i = 0; i < waveform.getDataSize(); ++i) {
            pData[i].filtered.low = 100;
            pData[i].filtered.mid = 100;
            pData[i].filtered.high = 100;
            pData[i].filtered.all = 100;
        }
        waveform.setCompletion(waveform.getDataSize());
        AnalysisDao::AnalysisInfo analysis;
        analysis.trackId = pTrack->getId();
        analysis.type = AnalysisDao::TYPE_WAVEFORM;
        analysis.description = WaveformFactory::currentWaveformDescription();
        analysis.version = WaveformFactory::currentWaveformVersion();
        analysis.data = waveform.toByteArray();
        ASSERT_TRUE(internalCollection()->getAnalysisDAO().saveAnalysis(&analysis));
    }

    DeviceLibraryPlusExportRequest request() const {
        DeviceLibraryPlusExportRequest result;
        result.stickRoot = m_stick.path();
        result.deviceName = kDeviceName;
        result.backupBaseDir = m_backups.path();
        result.masterDbId = kMasterDbId;
        return result;
    }

    /// Runs the job to its end. The main thread runs an event loop, because
    /// the job loads its data there.
    Outcome runJob(const DeviceLibraryPlusExportRequest& jobRequest, bool cancelFirst = false) {
        Outcome outcome;
        DeviceLibraryPlusExportJob job(nullptr, trackCollectionManager(), jobRequest);
        QObject context;
        QEventLoop loop;
        QObject::connect(
                &job,
                &DeviceLibraryPlusExportJob::completed,
                &context,
                [&outcome](const DeviceLibraryPlusExportSummary& summary) {
                    ++outcome.completedCount;
                    outcome.summary = summary;
                },
                Qt::QueuedConnection);
        QObject::connect(
                &job,
                &DeviceLibraryPlusExportJob::failed,
                &context,
                [&outcome](const QString& message) {
                    ++outcome.failedCount;
                    outcome.failure = message;
                },
                Qt::QueuedConnection);
        QObject::connect(
                &job,
                &QThread::finished,
                &loop,
                [&outcome, &loop]() {
                    outcome.finished = true;
                    loop.quit();
                },
                Qt::QueuedConnection);
        QTimer::singleShot(120000, &loop, &QEventLoop::quit);
        if (cancelFirst) {
            job.slotCancel();
        }
        job.start();
        loop.exec();
        if (!outcome.finished) {
            // A time-out: stop the job. It can wait for the main thread, so
            // the events must go on.
            job.slotCancel();
        }
        while (!job.wait(10)) {
            QCoreApplication::processEvents();
        }
        QCoreApplication::processEvents();
        return outcome;
    }

    QString libraryFile() const {
        return QDir(m_stick.path()).filePath(DeviceLibraryPlusDb::kRelativePath);
    }

    QTemporaryDir m_music;
    QTemporaryDir m_stick;
    QTemporaryDir m_backups;
};

} // namespace

TEST_F(DeviceLibraryPlusExportJobTest, ExportsLibraryToStick) {
    TrackPointer pAnalysed = addTrack(QStringLiteral("artist.mp3"), QStringLiteral("a"));
    TrackPointer pNotAnalysed = addTrack(QStringLiteral("all.mp3"), QStringLiteral("b"));
    ASSERT_TRUE(pAnalysed && pNotAnalysed);
    pAnalysed->setArtist(QStringLiteral("Job Artist"));
    pAnalysed->setTitle(QStringLiteral("Job Title"));
    storeWaveform(pAnalysed);

    PlaylistDAO& playlists = internalCollection()->getPlaylistDAO();
    const int playlistId = playlists.createPlaylist(QStringLiteral("Job Playlist"));
    ASSERT_GE(playlistId, 0);
    ASSERT_TRUE(playlists.appendTracksToPlaylist(
            {pNotAnalysed->getId(), pAnalysed->getId()}, playlistId));
    Crate crate;
    crate.setName(QStringLiteral("Job Crate"));
    CrateId crateId;
    ASSERT_TRUE(internalCollection()->insertCrate(crate, &crateId));
    ASSERT_TRUE(internalCollection()->addCrateTracks(
            crateId, {pAnalysed->getId(), pNotAnalysed->getId()}));

    const Outcome first = runJob(request());
    ASSERT_TRUE(first.finished);
    ASSERT_EQ(1, first.completedCount) << qPrintable(first.failure);
    EXPECT_EQ(0, first.failedCount);
    EXPECT_EQ(1, first.summary.tracksExported);
    EXPECT_EQ(2, first.summary.playlistsExported);
    ASSERT_EQ(1, first.summary.skipped.size()) << qPrintable(first.summary.skipped.join('\n'));
    EXPECT_TRUE(first.summary.skipped.first().endsWith(
            QStringLiteral(": The track has no waveform. Analyse the track first.")))
            << qPrintable(first.summary.skipped.first());

    QList<DlpContentRef> rows;
    {
        QString error;
        auto pDb = DeviceLibraryPlusDb::open(m_stick.path(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        rows = pDb->listContents();
        ASSERT_EQ(1, rows.size());
        EXPECT_EQ(kMasterDbId, rows[0].masterDbId);
        EXPECT_EQ(pAnalysed->getId().toVariant().toLongLong(), rows[0].masterContentId);
        EXPECT_TRUE(QFileInfo(m_stick.path() + rows[0].path).isFile());
        EXPECT_TRUE(QFileInfo(m_stick.path() + rows[0].analysisDataFilePath).isFile());
        const auto playlistId = pDb->findPlaylistOnly(QStringLiteral("Job Playlist"), 0);
        EXPECT_TRUE(playlistId);
        const auto folderId = pDb->findFolder(QStringLiteral("Crates"), 0);
        ASSERT_TRUE(folderId);
        EXPECT_TRUE(pDb->findPlaylistOnly(QStringLiteral("Job Crate"), *folderId));
    }
    // The first export made no backup: the stick had no library.
    EXPECT_FALSE(QFileInfo::exists(QDir(m_backups.path()).filePath(kDeviceName)));

    // A second export to the same stick adds no rows and makes a backup.
    const Outcome second = runJob(request());
    ASSERT_TRUE(second.finished);
    ASSERT_EQ(1, second.completedCount) << qPrintable(second.failure);
    EXPECT_EQ(1, second.summary.tracksExported);
    QString error;
    auto pDb = DeviceLibraryPlusDb::open(m_stick.path(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    const QList<DlpContentRef> again = pDb->listContents();
    ASSERT_EQ(1, again.size());
    EXPECT_EQ(rows[0].id, again[0].id);
    EXPECT_EQ(rows[0].path, again[0].path);
    const QStringList backups = QDir(QDir(m_backups.path()).filePath(kDeviceName))
                                        .entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    EXPECT_EQ(1, backups.size());
}

TEST_F(DeviceLibraryPlusExportJobTest, NoTrackExportedFails) {
    // Ruling R20: no track has a waveform, and the stick has no library.
    TrackPointer pTrack = addTrack(QStringLiteral("artist.mp3"), QStringLiteral("a"));
    ASSERT_TRUE(pTrack);
    PlaylistDAO& playlists = internalCollection()->getPlaylistDAO();
    const int playlistId = playlists.createPlaylist(QStringLiteral("Job Playlist"));
    ASSERT_TRUE(playlists.appendTracksToPlaylist({pTrack->getId()}, playlistId));

    const Outcome outcome = runJob(request());
    ASSERT_TRUE(outcome.finished);
    EXPECT_EQ(0, outcome.completedCount);
    ASSERT_EQ(1, outcome.failedCount);
    EXPECT_EQ(QStringLiteral("No track was exported. The stick did not change."), outcome.failure);
    EXPECT_FALSE(QFileInfo::exists(libraryFile()));
}

TEST_F(DeviceLibraryPlusExportJobTest, CancelBeforeStartWritesNothing) {
    TrackPointer pTrack = addTrack(QStringLiteral("artist.mp3"), QStringLiteral("a"));
    ASSERT_TRUE(pTrack);
    storeWaveform(pTrack);
    const Outcome outcome = runJob(request(), true);
    ASSERT_TRUE(outcome.finished);
    EXPECT_EQ(0, outcome.completedCount);
    EXPECT_EQ(0, outcome.failedCount);
    EXPECT_FALSE(QFileInfo::exists(libraryFile()));
    EXPECT_TRUE(QDir(m_stick.path()).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden));
}

// AI-generated (Claude Code). Start of AI-generated text.
TEST_F(DeviceLibraryPlusExportJobTest, ExportsOnlyTheSelectedTracks) {
    // The "Export Track" item of the track menu: the request has track IDs
    // and no crates or playlists. The job exports these tracks only, with no
    // playlist.
    TrackPointer pFirst = addTrack(QStringLiteral("artist.mp3"), QStringLiteral("a"));
    TrackPointer pSecond = addTrack(QStringLiteral("all.mp3"), QStringLiteral("b"));
    ASSERT_TRUE(pFirst && pSecond);
    storeWaveform(pFirst);
    storeWaveform(pSecond);

    // A playlist with both tracks must not enter the export.
    PlaylistDAO& playlists = internalCollection()->getPlaylistDAO();
    const int playlistId = playlists.createPlaylist(QStringLiteral("Not Selected"));
    ASSERT_TRUE(playlists.appendTracksToPlaylist({pFirst->getId(), pSecond->getId()}, playlistId));

    DeviceLibraryPlusExportRequest jobRequest = request();
    jobRequest.trackIds.insert(pSecond->getId().toVariant().toInt());
    const Outcome outcome = runJob(jobRequest);
    ASSERT_TRUE(outcome.finished);
    ASSERT_EQ(1, outcome.completedCount) << qPrintable(outcome.failure);
    EXPECT_EQ(1, outcome.summary.tracksExported);
    EXPECT_EQ(0, outcome.summary.playlistsExported);

    QString error;
    auto pDb = DeviceLibraryPlusDb::open(m_stick.path(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    const QList<DlpContentRef> rows = pDb->listContents();
    ASSERT_EQ(1, rows.size());
    EXPECT_EQ(pSecond->getId().toVariant().toLongLong(), rows[0].masterContentId);
    EXPECT_FALSE(pDb->findPlaylistOnly(QStringLiteral("Not Selected"), 0));
}

TEST_F(DeviceLibraryPlusExportJobTest, DialogGivesTheSelectedTrackIdsToTheRequest) {
    DlgDeviceExport dialog(nullptr, config(), trackCollectionManager());

    // Without a track selection the request has no track IDs.
    EXPECT_TRUE(dialog.makeRequest(m_stick.path())->trackIds.isEmpty());

    dialog.setInitialTracks({TrackId(QVariant(7)), TrackId(QVariant(9)), TrackId(QVariant(7))});
    const auto pRequest = dialog.makeRequest(m_stick.path());
    EXPECT_EQ(m_stick.path(), pRequest->stickRoot);
    EXPECT_EQ((QSet<int>{7, 9}), pRequest->trackIds);
    EXPECT_TRUE(pRequest->crateIds.isEmpty());
    EXPECT_TRUE(pRequest->playlistIds.isEmpty());

    // A choice of a crate or a playlist replaces the tracks.
    dialog.setInitialSelection(std::nullopt, std::nullopt);
    EXPECT_TRUE(dialog.makeRequest(m_stick.path())->trackIds.isEmpty());
}
// End of AI-generated text.

// AI-generated (Claude Code). Start of AI-generated text.
TEST_F(DeviceLibraryPlusExportJobTest, SelectedTracksKeepTheOtherRowsOfAStick) {
    // The stick holds the export of a playlist. A later export of one other
    // track adds its row and changes no playlist and no other track.
    TrackPointer pFirst = addTrack(QStringLiteral("artist.mp3"), QStringLiteral("a"));
    TrackPointer pSecond = addTrack(QStringLiteral("all.mp3"), QStringLiteral("b"));
    ASSERT_TRUE(pFirst && pSecond);
    storeWaveform(pFirst);
    storeWaveform(pSecond);
    PlaylistDAO& playlists = internalCollection()->getPlaylistDAO();
    const int playlistId = playlists.createPlaylist(QStringLiteral("On Stick"));
    ASSERT_TRUE(playlists.appendTracksToPlaylist({pFirst->getId()}, playlistId));

    DeviceLibraryPlusExportRequest playlistRequest = request();
    playlistRequest.playlistIds.insert(playlistId);
    const Outcome first = runJob(playlistRequest);
    ASSERT_EQ(1, first.completedCount) << qPrintable(first.failure);

    QList<DlpContentRef> before;
    {
        QString error;
        auto pDb = DeviceLibraryPlusDb::open(m_stick.path(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        before = pDb->listContents();
        ASSERT_EQ(1, before.size());
    }

    DeviceLibraryPlusExportRequest trackRequest = request();
    trackRequest.trackIds.insert(pSecond->getId().toVariant().toInt());
    const Outcome second = runJob(trackRequest);
    ASSERT_EQ(1, second.completedCount) << qPrintable(second.failure);
    EXPECT_EQ(1, second.summary.tracksExported);
    EXPECT_EQ(0, second.summary.playlistsExported);

    QString error;
    auto pDb = DeviceLibraryPlusDb::open(m_stick.path(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    const QList<DlpContentRef> after = pDb->listContents();
    ASSERT_EQ(2, after.size());
    bool firstKept = false;
    for (const DlpContentRef& row : after) {
        if (row.masterContentId == before[0].masterContentId) {
            firstKept = true;
            EXPECT_EQ(before[0].id, row.id);
            EXPECT_EQ(before[0].path, row.path);
        }
    }
    EXPECT_TRUE(firstKept);
    EXPECT_TRUE(pDb->findPlaylistOnly(QStringLiteral("On Stick"), 0));
}
// End of AI-generated text.

} // namespace mixxx
