#include "library/export/devicelibraryplusexportjob.h"

#include <gtest/gtest.h>
#include <rekordbox_pdb.h>

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSharedPointer>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>
#include <QtConcurrent>
#include <QtEndian>
#include <cstdio>
#include <sstream>
#include <utility>

#include "library/dao/analysisdao.h"
#include "library/dao/playlistdao.h"
#include "library/export/dlgdeviceexport.h"
#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/devicelibraryplusexporter.h"
#include "library/rekordbox/legacyfromdlp.h"
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
    Outcome runJob(const DeviceLibraryPlusExportRequest& jobRequest,
            bool cancelFirst = false,
            DeviceLibraryPlusExportJob::LegacyWriter writer = rekordbox::legacyPdbBytes) {
        DeviceLibraryPlusExportJob job(
                nullptr, trackCollectionManager(), jobRequest, std::move(writer));
        if (cancelFirst) {
            job.slotCancel();
        }
        return runJob(job);
    }

    Outcome runJob(DeviceLibraryPlusExportJob& job) {
        Outcome outcome;
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
    // AI-generated (Claude Code). Start of AI-generated text.
    // The counts of the copied and written files. The skipped track adds
    // nothing.
    EXPECT_EQ(QFileInfo(pAnalysed->getLocation()).size(), first.summary.bytesCopied);
    EXPECT_EQ(1, first.summary.audioFilesCopied);
    EXPECT_EQ(1, first.summary.anlzWritten);
    EXPECT_EQ(0, first.summary.anlzKept);
    // End of AI-generated text.

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
    // AI-generated (Claude Code). Start of AI-generated text.
    // Nothing changed: no copy, and the ANLZ folder stays.
    EXPECT_EQ(0, second.summary.bytesCopied);
    EXPECT_EQ(0, second.summary.audioFilesCopied);
    EXPECT_EQ(0, second.summary.anlzWritten);
    EXPECT_EQ(1, second.summary.anlzKept);
    // End of AI-generated text.
    QString error;
    auto pDb = DeviceLibraryPlusDb::open(m_stick.path(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    const QList<DlpContentRef> again = pDb->listContents();
    ASSERT_EQ(1, again.size());
    EXPECT_EQ(rows[0].id, again[0].id);
    EXPECT_EQ(rows[0].path, again[0].path);
    EXPECT_EQ(rows[0].analysisDataFilePath, again[0].analysisDataFilePath);
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
    trackRequest.writeLegacyLibrary = true;
    const Outcome second = runJob(trackRequest, false,
            [](const rekordbox::LegacyLibrary& library, QString*) {
                EXPECT_EQ(2, library.tracks.size());
                return QByteArray("legacy bytes");
            });
    ASSERT_EQ(1, second.completedCount) << qPrintable(second.failure);
    EXPECT_TRUE(second.summary.legacyLibraryError.isEmpty());
    QFile legacy(m_stick.filePath(QStringLiteral("PIONEER/rekordbox/export.pdb")));
    ASSERT_TRUE(legacy.open(QIODevice::ReadOnly));
    EXPECT_EQ(QByteArray("legacy bytes"), legacy.readAll());
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

// AI-generated (Claude Code). Start of AI-generated text.
TEST_F(DeviceLibraryPlusExportJobTest, LegacyFailureKeepsThePreviousFileAndPlusExport) {
    const QDir root(m_stick.path());
    ASSERT_TRUE(root.mkpath(QStringLiteral("PIONEER/rekordbox")));
    QFile file(root.filePath(QStringLiteral("PIONEER/rekordbox/export.pdb")));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(3, file.write("old"));
    file.close();
    auto jobRequest = request();
    jobRequest.writeLegacyLibrary = true;
    const Outcome outcome = runJob(jobRequest, false,
            [](const rekordbox::LegacyLibrary&, QString* pError) {
                *pError = QStringLiteral("Test writer cannot write the data.");
                return QByteArray();
            });
    ASSERT_EQ(1, outcome.completedCount) << qPrintable(outcome.failure);
    EXPECT_EQ(0, outcome.failedCount);
    EXPECT_EQ(QStringLiteral("Test writer cannot write the data."),
            outcome.summary.legacyLibraryError);
    QString error;
    EXPECT_TRUE(DeviceLibraryPlusDb::open(m_stick.path(), &error)) << qPrintable(error);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(QByteArray("old"), file.readAll());
    const QDir folder = QFileInfo(file).dir();
    EXPECT_EQ((QStringList{QStringLiteral("export.pdb"), QStringLiteral("exportLibrary.db")}),
            folder.entryList(QDir::Files | QDir::Hidden, QDir::Name));
}

class LegacyReplacementTest : public DeviceLibraryPlusExportJobTest,
                              public testing::WithParamInterface<int> {};

TEST_P(LegacyReplacementTest, RemovesOldAnalysisOnlyAfterLegacyReplacement) {
    auto pTrack = addTrack(QStringLiteral("artist.mp3"), QStringLiteral("a"));
    ASSERT_TRUE(pTrack);
    storeWaveform(pTrack);
    auto jobRequest = request();
    jobRequest.writeLegacyLibrary = true;
    const auto initial = runJob(jobRequest, false,
            [](const rekordbox::LegacyLibrary&, QString*) { return QByteArray("old library"); });
    ASSERT_EQ(1, initial.completedCount) << qPrintable(initial.failure);
    ASSERT_TRUE(initial.summary.legacyLibraryError.isEmpty());
    QString error;
    auto pDb = DeviceLibraryPlusDb::open(m_stick.path(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    const auto before = pDb->listContents();
    ASSERT_EQ(1, before.size());
    pDb.reset();
    const QString oldAnalysis = m_stick.path() + before.first().analysisDataFilePath;
    ASSERT_TRUE(QFile::exists(oldAnalysis));
    // AI-generated (Claude Code). Start of AI-generated text.
    // Another hot cue: the ANLZ files change, so the export makes a new
    // folder and replaces the old one (Ruling R17).
    pTrack->createAndAddCue(mixxx::CueType::HotCue,
            1,
            mixxx::audio::FramePos(5000),
            mixxx::audio::kInvalidFramePos);
    // End of AI-generated text.

    const bool legacySucceeds = GetParam() == 0;
    jobRequest.writeLegacyLibrary = GetParam() != 2;
    const auto outcome = runJob(jobRequest, false,
            [&](const rekordbox::LegacyLibrary&, QString* pError) {
                EXPECT_TRUE(QFile::exists(oldAnalysis));
                if (legacySucceeds) {
                    return QByteArray("new library");
                }
                *pError = QStringLiteral("Test writer cannot write the data.");
                return QByteArray();
            });
    ASSERT_EQ(1, outcome.completedCount) << qPrintable(outcome.failure);
    EXPECT_EQ(!legacySucceeds, QFile::exists(oldAnalysis));
    EXPECT_EQ(!legacySucceeds, outcome.summary.keptReplacedFiles);
    pDb = DeviceLibraryPlusDb::open(m_stick.path(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    const auto after = pDb->listContents();
    ASSERT_EQ(1, after.size());
    EXPECT_NE(before.first().analysisDataFilePath, after.first().analysisDataFilePath);
    EXPECT_TRUE(QFile::exists(m_stick.path() + after.first().analysisDataFilePath));
    QFile legacy(m_stick.filePath(QStringLiteral("PIONEER/rekordbox/export.pdb")));
    ASSERT_TRUE(legacy.open(QIODevice::ReadOnly));
    EXPECT_EQ(QByteArray(legacySucceeds ? "new library" : "old library"), legacy.readAll());
}

INSTANTIATE_TEST_SUITE_P(DeviceLibraryPlus, LegacyReplacementTest, testing::Values(0, 1, 2));

TEST_F(DeviceLibraryPlusExportJobTest, WritesTheLegacyBytesToTheExistingFolder) {
    const QDir root(m_stick.path());
    ASSERT_TRUE(root.mkpath(QStringLiteral("pIoNeEr/ReKoRdBoX")));
    QFile file(root.filePath(QStringLiteral("pIoNeEr/ReKoRdBoX/ExPoRt.PdB")));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(3, file.write("old"));
    file.close();

    QString error;
    QTemporaryDir seed;
    ASSERT_TRUE(seed.isValid());
    auto pDb = DeviceLibraryPlusDb::create(seed.path(), kDeviceName, kMasterDbId, &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    rekordbox::DlpContent previous;
    previous.title = QStringLiteral("Previous track");
    previous.path = QStringLiteral("/Contents/previous.mp3");
    previous.playCount = 65536;
    previous.masterDbId = kMasterDbId + 1;
    previous.masterContentId = 1;
    ASSERT_GT(pDb->addContent(previous), 0);
    ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    pDb.reset();
    const QString mixedPlus = root.filePath(QStringLiteral("pIoNeEr/ReKoRdBoX/ExPoRtLiBrArY.Db"));
    ASSERT_TRUE(QFile::copy(seed.filePath(DeviceLibraryPlusDb::kRelativePath), mixedPlus));

    auto jobRequest = request();
    jobRequest.writeLegacyLibrary = true;
    const auto pTrack = addTrack(QStringLiteral("artist.mp3"), QStringLiteral("a"));
    ASSERT_TRUE(pTrack);
    storeWaveform(pTrack);
    const QByteArray bytes("legacy\0" "data", 11);
    const Outcome outcome = runJob(jobRequest, false,
            [&bytes](const rekordbox::LegacyLibrary& library, QString*) {
                EXPECT_EQ(2, library.tracks.size());
                if (!library.tracks.isEmpty()) {
                    EXPECT_EQ(QStringLiteral("Previous track"), library.tracks.first().title);
                    EXPECT_EQ(65535u, library.tracks.first().playCount);
                }
                EXPECT_NE(QThread::currentThread(), QCoreApplication::instance()->thread());
                return bytes;
            });
    ASSERT_EQ(1, outcome.completedCount) << qPrintable(outcome.failure);
    EXPECT_TRUE(outcome.summary.legacyLibraryError.isEmpty());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(bytes, file.readAll());
    EXPECT_EQ(1, outcome.summary.legacyClampedTracks);
    // A disk that keeps the letter case gets a /PIONEER folder for the
    // analysis files. The libraries stay in the folder that they had.
    QFile probe(root.filePath(QStringLiteral("case-probe")));
    ASSERT_TRUE(probe.open(QIODevice::WriteOnly));
    probe.close();
    const bool caseSensitive = !QFileInfo::exists(root.filePath(QStringLiteral("CASE-PROBE")));
    ASSERT_TRUE(probe.remove());
    if (caseSensitive) {
        EXPECT_FALSE(QFileInfo::exists(root.filePath(QStringLiteral("PIONEER/rekordbox"))));
    } else {
        EXPECT_FALSE(root.entryList(QDir::Dirs).contains(QStringLiteral("PIONEER")));
    }
    EXPECT_EQ((QStringList{QStringLiteral("ExPoRt.PdB"), QStringLiteral("ExPoRtLiBrArY.Db")}),
            QFileInfo(mixedPlus).dir().entryList(QDir::Files, QDir::Name));
    pDb = DeviceLibraryPlusDb::open(m_stick.path(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    EXPECT_EQ(2, pDb->listContents().size());
}

TEST_F(DeviceLibraryPlusExportJobTest, WritesTheLegacyBytesToANewStick) {
    auto jobRequest = request();
    jobRequest.writeLegacyLibrary = true;
    const Outcome outcome = runJob(jobRequest, false,
            [](const rekordbox::LegacyLibrary&, QString*) { return QByteArray("new library"); });
    ASSERT_EQ(1, outcome.completedCount) << qPrintable(outcome.failure);
    EXPECT_TRUE(outcome.summary.legacyLibraryError.isEmpty());
    QFile file(m_stick.filePath(QStringLiteral("PIONEER/rekordbox/export.pdb")));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(QByteArray("new library"), file.readAll());
}

TEST_F(DeviceLibraryPlusExportJobTest, KeepsNoFilesWhenTheExportReplacedNone) {
    const QDir root(m_stick.path());
    ASSERT_TRUE(root.mkpath(QStringLiteral("PIONEER/rekordbox")));
    QFile file(root.filePath(QStringLiteral("PIONEER/rekordbox/export.pdb")));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(3, file.write("old"));
    file.close();

    // The legacy write fails, but the export replaced no file.
    auto jobRequest = request();
    jobRequest.writeLegacyLibrary = true;
    const Outcome failedLegacy = runJob(jobRequest, false,
            [](const rekordbox::LegacyLibrary&, QString* pError) {
                *pError = QStringLiteral("Test writer cannot write the data.");
                return QByteArray();
            });
    ASSERT_EQ(1, failedLegacy.completedCount) << qPrintable(failedLegacy.failure);
    EXPECT_FALSE(failedLegacy.summary.legacyLibraryError.isEmpty());
    EXPECT_FALSE(failedLegacy.summary.keptReplacedFiles);

    // A Plus export of a new track to a stick that has a Device Library.
    const auto pTrack = addTrack(QStringLiteral("artist.mp3"), QStringLiteral("a"));
    ASSERT_TRUE(pTrack);
    storeWaveform(pTrack);
    jobRequest.writeLegacyLibrary = false;
    const Outcome plusOnly = runJob(jobRequest);
    ASSERT_EQ(1, plusOnly.completedCount) << qPrintable(plusOnly.failure);
    EXPECT_EQ(1, plusOnly.summary.tracksExported);
    EXPECT_FALSE(plusOnly.summary.keptReplacedFiles);
}

TEST(DeviceLibraryPlusCompletionTextTest, ShowsOnlyTheLinesThatApply) {
    DeviceLibraryPlusExportSummary summary;
    summary.tracksExported = 3;
    summary.playlistsExported = 2;
    // AI-generated (Claude Code). Start of AI-generated text.
    summary.audioFilesCopied = 2;
    summary.bytesCopied = 3 * 1024 * 1024 + 300 * 1024;
    summary.anlzWritten = 2;
    const QString first = QStringLiteral(
            "3 tracks and 2 playlists are on the stick. "
            "Eject the stick before you remove it.\n"
            "Mix Two copied 2 audio files (3.3 MB) and wrote new analysis files for 2 tracks.");
    // End of AI-generated text.
    EXPECT_EQ(first, deviceLibraryPlusCompletionText(summary));

    summary.legacyClampedTracks = 2;
    EXPECT_EQ(first +
                    QStringLiteral("\n\nMix Two adjusts values for 2 tracks "
                                   "to fit the Device Library."),
            deviceLibraryPlusCompletionText(summary));

    // No adjusted values when the Device Library was not written.
    summary.legacyLibraryError = QStringLiteral("The disk is full.");
    summary.keptReplacedFiles = true;
    summary.skipped = QStringList{QStringLiteral("a"), QStringLiteral("b")};
    EXPECT_EQ(first +
                    QStringLiteral(
                            "\n\nMix Two did not write the Device Library. "
                            "Reason: The disk is full."
                            "\n\nMix Two keeps the replaced files on the stick."
                            "\n\nMix Two did not export 2 tracks."),
            deviceLibraryPlusCompletionText(summary));
}

TEST_F(DeviceLibraryPlusExportJobTest, UsesTheDefaultLegacyWriter) {
    auto jobRequest = request();
    jobRequest.writeLegacyLibrary = true;
    DeviceLibraryPlusExportJob job(nullptr, trackCollectionManager(), jobRequest);
    const Outcome outcome = runJob(job);
    ASSERT_EQ(1, outcome.completedCount) << qPrintable(outcome.failure);
    EXPECT_EQ(0, outcome.failedCount);
    QString error;
    const auto pDb = DeviceLibraryPlusDb::open(m_stick.path(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    const auto library = rekordbox::legacyFromDlp(*pDb, &error);
    ASSERT_TRUE(library) << qPrintable(error);
    const QByteArray expected = rekordbox::legacyPdbBytes(*library, &error);
    QFile file(m_stick.filePath(QStringLiteral("PIONEER/rekordbox/export.pdb")));
    if (expected.isEmpty()) {
        EXPECT_FALSE(error.isEmpty());
        EXPECT_EQ(error, outcome.summary.legacyLibraryError);
        EXPECT_FALSE(file.exists());
    } else {
        EXPECT_TRUE(outcome.summary.legacyLibraryError.isEmpty());
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        EXPECT_EQ(expected, file.readAll());
    }
}

TEST_F(DeviceLibraryPlusExportJobTest, DialogSelectsBothFormatsByDefault) {
    DlgDeviceExport dialog(nullptr, config(), trackCollectionManager());
    QCheckBox* pLegacy = nullptr;
    for (auto* pBox : dialog.findChildren<QCheckBox*>()) {
        if (pBox->text() == QStringLiteral("Device Library")) {
            pLegacy = pBox;
            break;
        }
    }
    ASSERT_TRUE(pLegacy);
    EXPECT_TRUE(pLegacy->isEnabled());
    EXPECT_TRUE(pLegacy->isChecked());
    EXPECT_TRUE(dialog.makeRequest(m_stick.path())->writeLegacyLibrary);
    pLegacy->setChecked(false);
    EXPECT_FALSE(dialog.makeRequest(m_stick.path())->writeLegacyLibrary);
}

namespace {

using Pdb = rekordbox_pdb_t;

QString pdbText(Pdb::device_sql_string_t* pString) {
    switch (pString->length_and_kind()) {
    case 0x40:
        return QString::fromStdString(
                static_cast<Pdb::device_sql_long_ascii_t*>(pString->body())->text());
    case 0x90: {
        // Kaitai gives the UTF-16LE bytes without conversion.
        const std::string payload =
                static_cast<Pdb::device_sql_long_utf16le_t*>(pString->body())->text();
        QString result;
        for (size_t i = 0; i + 1 < payload.size(); i += 2) {
            result.append(QChar(qFromLittleEndian<quint16>(payload.data() + i)));
        }
        return result;
    }
    default:
        return QString::fromStdString(
                static_cast<Pdb::device_sql_short_ascii_t*>(pString->body())->text());
    }
}

/// Calls the visitor for each present row of a table of the Kaitai model,
/// along the page chain from the first page to the last page.
template<typename Visitor>
bool visitPdbRows(Pdb& db, unsigned type, qint64 fileSize, Visitor visitor) {
    Pdb::table_t* pTable = db.tables()->at(type);
    Pdb::page_ref_t* pRef = pTable->first_page();
    const qint64 pageCount = fileSize / db.len_page();
    for (qint64 steps = 0; steps < pageCount; ++steps) {
        if (pRef->index() == 0 || pRef->index() >= pageCount) {
            return false;
        }
        Pdb::page_t* pPage = pRef->body();
        if (pPage->is_data_page()) {
            for (auto* pGroup : *pPage->row_groups()) {
                for (auto* pRow : *pGroup->rows()) {
                    if (pRow->present()) {
                        visitor(pRow);
                    }
                }
            }
        }
        if (pRef->index() == pTable->last_page()->index()) {
            return true;
        }
        pRef = pPage->next_page();
    }
    return false;
}

} // namespace

TEST_F(DeviceLibraryPlusExportJobTest, WritesBothFormatsToSampleStick) {
    // For the independent readers of both libraries:
    //   MIXTWO_DLP_EXPORT_SAMPLE_DIR=<empty folder> mixxx-test
    //       --gtest_filter=DeviceLibraryPlusExportJobTest.WritesBothFormatsToSampleStick
    // The folder must be empty or missing. The job writes the stick with the
    // request that the export dialog makes by default.
    const QString sampleDir = qEnvironmentVariable("MIXTWO_DLP_EXPORT_SAMPLE_DIR");
    if (sampleDir.isEmpty()) {
        GTEST_SKIP() << "MIXTWO_DLP_EXPORT_SAMPLE_DIR is not set.";
    }
    ASSERT_TRUE(QDir().mkpath(sampleDir));
    if (!QDir(sampleDir).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
        FAIL() << "MIXTWO_DLP_EXPORT_SAMPLE_DIR is not empty. Use an empty folder.";
    }

    const QList<QPair<QString, QString>> files = {
            {QStringLiteral("artist.mp3"), QStringLiteral("Sample One")},
            {QStringLiteral("all.mp3"), QStringLiteral("Sample Two")},
            {QString::fromUtf8("cover-test-\xc3\xb8\xc3\xa9~\xc5\x82\xe2\x82\xac\xcb\x9a.flac"),
                    QString::fromUtf8("Sample Three \xe6\x97\xa5")},
            {QString::fromUtf8(
                     "cover-test-\xc3\xb8\xc3\xa9~\xc5\x82\xe2\x82\xac\xcb\x9a-jpg.mp3"),
                    QStringLiteral("Sample Four")},
    };
    QList<TrackPointer> tracks;
    for (int i = 0; i < files.size(); ++i) {
        TrackPointer pTrack = addTrack(files[i].first, QStringLiteral("f%1").arg(i));
        ASSERT_TRUE(pTrack);
        pTrack->setTitle(files[i].second);
        pTrack->setArtist(QStringLiteral("Sample Artist"));
        storeWaveform(pTrack);
        tracks.append(pTrack);
    }
    PlaylistDAO& playlists = internalCollection()->getPlaylistDAO();
    const int playlistId = playlists.createPlaylist(QStringLiteral("Sample Playlist"));
    ASSERT_GE(playlistId, 0);
    ASSERT_TRUE(playlists.appendTracksToPlaylist(
            {tracks[3]->getId(), tracks[0]->getId(), tracks[2]->getId()}, playlistId));
    Crate crate;
    crate.setName(QStringLiteral("Sample Crate"));
    CrateId crateId;
    ASSERT_TRUE(internalCollection()->insertCrate(crate, &crateId));
    ASSERT_TRUE(internalCollection()->addCrateTracks(
            crateId, {tracks[1]->getId(), tracks[2]->getId()}));

    // The request of the dialog with its default choices, completed as the
    // device exporter does.
    DlgDeviceExport dialog(nullptr, config(), trackCollectionManager());
    const auto pRequest = dialog.makeRequest(sampleDir);
    ASSERT_TRUE(pRequest->writeLegacyLibrary);
    pRequest->deviceName = kDeviceName;
    pRequest->backupBaseDir = m_backups.path();
    pRequest->masterDbId = kMasterDbId;
    DeviceLibraryPlusExportJob job(nullptr, trackCollectionManager(), *pRequest);
    const Outcome outcome = runJob(job);
    ASSERT_TRUE(outcome.finished);
    ASSERT_EQ(1, outcome.completedCount) << qPrintable(outcome.failure);
    EXPECT_TRUE(outcome.summary.legacyLibraryError.isEmpty())
            << qPrintable(outcome.summary.legacyLibraryError);
    EXPECT_EQ(4, outcome.summary.tracksExported)
            << qPrintable(outcome.summary.skipped.join('\n'));
    std::printf("job: %d tracks, %d playlists, %d skipped\n",
            outcome.summary.tracksExported,
            outcome.summary.playlistsExported,
            static_cast<int>(outcome.summary.skipped.size()));

    // Device Library Plus, through Mix Two's SQLCipher reader.
    QString error;
    auto pDb = DeviceLibraryPlusDb::open(sampleDir, &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    // The two libraries are compared by track ID and file path on the stick.
    QMap<qint64, QString> plusPaths;
    for (const DlpContentRef& row : pDb->listContents()) {
        plusPaths.insert(row.id, row.path);
    }
    QMap<QString, QStringList> plusPlaylists;
    QStringList plusFolders;
    for (const auto& playlist : pDb->listPlaylists()) {
        if (playlist.isFolder) {
            plusFolders.append(playlist.name);
            continue;
        }
        QStringList titles;
        for (const qint64 contentId : pDb->listPlaylistContentIds(playlist.id)) {
            titles.append(plusPaths.value(contentId, QStringLiteral("?")));
        }
        plusPlaylists.insert(playlist.name, titles);
    }
    pDb.reset();

    // Device Library, through the bundled Kaitai reader.
    QFile pdbFile(QDir(sampleDir).filePath(QStringLiteral("PIONEER/rekordbox/export.pdb")));
    ASSERT_TRUE(pdbFile.open(QIODevice::ReadOnly));
    const QByteArray bytes = pdbFile.readAll();
    std::istringstream input(bytes.toStdString());
    kaitai::kstream stream(&input);
    Pdb pdb(&stream);
    QMap<qint64, QString> legacyPaths;
    QMap<QString, QString> titleOfPath;
    ASSERT_TRUE(visitPdbRows(pdb, Pdb::PAGE_TYPE_TRACKS, bytes.size(), [&](Pdb::row_ref_t* pRow) {
        auto* pTrack = static_cast<Pdb::track_row_t*>(pRow->body());
        const QString path = pdbText(pTrack->file_path());
        legacyPaths.insert(pTrack->id(), path);
        titleOfPath.insert(path, pdbText(pTrack->title()));
        std::printf("legacy title %u: %s\n",
                pTrack->id(),
                pdbText(pTrack->title()).toUtf8().constData());
    }));
    QMap<qint64, QString> legacyNames;
    QStringList legacyFolders;
    ASSERT_TRUE(visitPdbRows(pdb,
            Pdb::PAGE_TYPE_PLAYLIST_TREE,
            bytes.size(),
            [&](Pdb::row_ref_t* pRow) {
                auto* pNode = static_cast<Pdb::playlist_tree_row_t*>(pRow->body());
                if (pNode->is_folder()) {
                    legacyFolders.append(pdbText(pNode->name()));
                } else {
                    legacyNames.insert(pNode->id(), pdbText(pNode->name()));
                }
            }));
    QMap<qint64, QMap<quint32, qint64>> legacyEntries;
    ASSERT_TRUE(visitPdbRows(pdb,
            Pdb::PAGE_TYPE_PLAYLIST_ENTRIES,
            bytes.size(),
            [&](Pdb::row_ref_t* pRow) {
                auto* pEntry = static_cast<Pdb::playlist_entry_row_t*>(pRow->body());
                legacyEntries[pEntry->playlist_id()].insert(
                        pEntry->entry_index(), pEntry->track_id());
            }));
    QMap<QString, QStringList> legacyPlaylists;
    for (auto it = legacyNames.cbegin(); it != legacyNames.cend(); ++it) {
        QStringList titles;
        for (const qint64 trackId : legacyEntries.value(it.key())) {
            titles.append(legacyPaths.value(trackId, QStringLiteral("?")));
        }
        legacyPlaylists.insert(it.value(), titles);
    }

    for (auto it = plusPaths.cbegin(); it != plusPaths.cend(); ++it) {
        std::printf("plus track %lld: %s\n",
                static_cast<long long>(it.key()),
                it.value().toUtf8().constData());
    }
    for (auto it = legacyPaths.cbegin(); it != legacyPaths.cend(); ++it) {
        std::printf("legacy track %lld: %s\n",
                static_cast<long long>(it.key()),
                it.value().toUtf8().constData());
    }
    std::printf("plus folders: %s\nlegacy folders: %s\n",
            plusFolders.join(", ").toUtf8().constData(),
            legacyFolders.join(", ").toUtf8().constData());
    for (auto it = plusPlaylists.cbegin(); it != plusPlaylists.cend(); ++it) {
        std::printf("plus playlist %s: %s\n",
                it.key().toUtf8().constData(),
                it.value().join(" | ").toUtf8().constData());
    }
    for (auto it = legacyPlaylists.cbegin(); it != legacyPlaylists.cend(); ++it) {
        std::printf("legacy playlist %s: %s\n",
                it.key().toUtf8().constData(),
                it.value().join(" | ").toUtf8().constData());
    }
    std::fflush(stdout);

    EXPECT_EQ(4, plusPaths.size());
    EXPECT_EQ(plusPaths, legacyPaths);
    plusFolders.sort();
    legacyFolders.sort();
    EXPECT_EQ(plusFolders, legacyFolders);
    EXPECT_EQ(plusPlaylists, legacyPlaylists);
    const auto titles = [&titleOfPath](const QStringList& paths) {
        QStringList result;
        for (const QString& path : paths) {
            result.append(titleOfPath.value(path));
        }
        return result;
    };
    EXPECT_EQ((QStringList{QStringLiteral("Sample Four"),
                      QStringLiteral("Sample One"),
                      QString::fromUtf8("Sample Three \xe6\x97\xa5")}),
            titles(plusPlaylists.value(QStringLiteral("Sample Playlist"))));
    EXPECT_EQ((QStringList{QStringLiteral("Sample Two"),
                      QString::fromUtf8("Sample Three \xe6\x97\xa5")}),
            titles(plusPlaylists.value(QStringLiteral("Sample Crate"))));
}
// End of AI-generated text.

// AI-generated (Claude Code). Start of AI-generated text.
namespace {

/// The bytes of the three ANLZ files of each row, by masterContentId.
QMap<qint64, QList<QByteArray>> anlzBytesByTrack(const QString& stickRoot) {
    QMap<qint64, QList<QByteArray>> result;
    QString error;
    auto pDb = DeviceLibraryPlusDb::open(stickRoot, &error);
    EXPECT_TRUE(pDb) << qPrintable(error);
    if (!pDb) {
        return result;
    }
    for (const DlpContentRef& row : pDb->listContents()) {
        const QFileInfo dat(stickRoot + row.analysisDataFilePath);
        QList<QByteArray> files;
        for (const QString& name : {QStringLiteral("ANLZ0000.DAT"),
                     QStringLiteral("ANLZ0000.EXT"),
                     QStringLiteral("ANLZ0000.2EX")}) {
            QFile file(dat.dir().filePath(name));
            EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << qPrintable(file.fileName());
            files.append(file.readAll());
        }
        result.insert(row.masterContentId, files);
    }
    return result;
}

/// The masterContentIds of the rows, in the order of the rows.
QList<qint64> contentIdsInRowOrder(const QString& stickRoot) {
    QList<qint64> result;
    QString error;
    auto pDb = DeviceLibraryPlusDb::open(stickRoot, &error);
    EXPECT_TRUE(pDb) << qPrintable(error);
    if (pDb) {
        for (const DlpContentRef& row : pDb->listContents()) {
            result.append(row.masterContentId);
        }
    }
    return result;
}

} // namespace

// The job reads and decodes the waveform and the cover in its own thread. The
// ANLZ files are the same as from the waveform and the cover that the main
// thread loaded before this change.
TEST_F(DeviceLibraryPlusExportJobTest, WorkerLoadGivesTheSameAnlzBytes) {
    TrackPointer pPlain = addTrack(QStringLiteral("artist.mp3"), QStringLiteral("a"));
    TrackPointer pCover = addTrack(
            QString::fromUtf8("cover-test-\xc3\xb8\xc3\xa9~\xc5\x82\xe2\x82\xac\xcb\x9a-jpg.mp3"),
            QStringLiteral("b"));
    ASSERT_TRUE(pPlain && pCover);
    storeWaveform(pPlain);
    storeWaveform(pCover);

    const Outcome outcome = runJob(request());
    ASSERT_TRUE(outcome.finished);
    ASSERT_EQ(1, outcome.completedCount) << qPrintable(outcome.failure);
    ASSERT_EQ(2, outcome.summary.tracksExported)
            << qPrintable(outcome.summary.skipped.join('\n'));

    // The reference: the exporter with the data loaded in this (main) thread,
    // as the job did before.
    QTemporaryDir reference;
    ASSERT_TRUE(reference.isValid());
    QString error;
    auto pDb = DeviceLibraryPlusDb::create(reference.path(), kDeviceName, kMasterDbId, &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    int referenceImages = 0;
    {
        rekordbox::DeviceLibraryPlusExporter::Options options;
        options.stickRoot = reference.path();
        options.masterDbId = kMasterDbId;
        rekordbox::DeviceLibraryPlusExporter exporter(pDb.get(), options);
        ASSERT_TRUE(exporter.prepare(&error)) << qPrintable(error);
        auto& analysisDao = internalCollection()->getAnalysisDAO();
        for (const TrackPointer& pTrack : {pPlain, pCover}) {
            rekordbox::DlpExportTrack item;
            item.pTrack = pTrack;
            const auto analyses = analysisDao.getAnalysesForTrackByType(
                    pTrack->getId(), AnalysisDao::TYPE_WAVEFORM);
            ASSERT_EQ(1, analyses.size());
            item.pWaveform = ConstWaveformPointer(
                    WaveformFactory::loadWaveformFromAnalysis(analyses.first()));
            CoverInfo coverInfo = pTrack->getCoverInfoWithLocation();
            if (coverInfo.hasImage()) {
                const CoverInfo::LoadedImage loaded = coverInfo.loadImage(pTrack);
                if (loaded.result == CoverInfo::LoadedImage::Result::Ok &&
                        !loaded.image.isNull()) {
                    if (!coverInfo.hasCacheKey()) {
                        coverInfo.setImageDigest(loaded.image);
                    }
                    item.cover = loaded.image;
                    item.coverHash = coverInfo.cacheKey();
                }
            }
            ASSERT_TRUE(exporter.exportTrack(item).contentId);
        }
        ASSERT_TRUE(exporter.finish(&error)) << qPrintable(error);
        referenceImages = static_cast<int>(pDb->listImages().size());
    }
    pDb.reset();

    const auto expected = anlzBytesByTrack(reference.path());
    const auto actual = anlzBytesByTrack(m_stick.path());
    ASSERT_EQ(2, expected.size());
    EXPECT_EQ(expected.keys(), actual.keys());
    for (auto it = expected.cbegin(); it != expected.cend(); ++it) {
        const QList<QByteArray> files = actual.value(it.key());
        ASSERT_EQ(3, files.size());
        for (int i = 0; i < 3; ++i) {
            EXPECT_FALSE(files[i].isEmpty());
            EXPECT_TRUE(files[i] == it.value()[i]) << "track " << it.key() << " file " << i;
        }
    }
    auto pJobDb = DeviceLibraryPlusDb::open(m_stick.path(), &error);
    ASSERT_TRUE(pJobDb) << qPrintable(error);
    EXPECT_EQ(referenceImages, pJobDb->listImages().size());
}

/// The group size of the main-thread loads, and the track count.
class DeviceLibraryPlusExportJobGroupTest
        : public DeviceLibraryPlusExportJobTest,
          public ::testing::WithParamInterface<std::pair<int, int>> {};

// The main thread loads up to the group size of tracks in one call. Every
// track is exported in the order of the library, at each group boundary. The
// track without a waveform is skipped with the same reason as before.
TEST_P(DeviceLibraryPlusExportJobGroupTest, ExportsEveryTrackInOrder) {
    const int groupSize = GetParam().first;
    const int trackCount = GetParam().second;
    const int noWaveformIndex = trackCount > 8 ? 8 : -1;
    QList<qint64> expectedIds;
    for (int i = 0; i < trackCount; ++i) {
        TrackPointer pTrack = addTrack(QStringLiteral("artist.mp3"), QString::number(i));
        ASSERT_TRUE(pTrack);
        pTrack->setTitle(QStringLiteral("Group %1").arg(i));
        if (i != noWaveformIndex) {
            storeWaveform(pTrack);
            expectedIds.append(pTrack->getId().toVariant().toLongLong());
        }
    }

    DeviceLibraryPlusExportJob job(nullptr, trackCollectionManager(), request());
    job.setTrackLoadGroupSize(groupSize);
    const Outcome outcome = runJob(job);
    ASSERT_TRUE(outcome.finished);
    ASSERT_EQ(1, outcome.completedCount) << qPrintable(outcome.failure);
    EXPECT_EQ(expectedIds.size(), outcome.summary.tracksExported);
    EXPECT_EQ((trackCount + groupSize - 1) / groupSize, job.trackLoadCallCount());
    if (noWaveformIndex >= 0) {
        ASSERT_EQ(1, outcome.summary.skipped.size());
        EXPECT_TRUE(outcome.summary.skipped.first().endsWith(
                QStringLiteral(": The track has no waveform. Analyse the track first.")))
                << qPrintable(outcome.summary.skipped.first());
    } else {
        EXPECT_TRUE(outcome.summary.skipped.isEmpty());
    }
    EXPECT_EQ(expectedIds, contentIdsInRowOrder(m_stick.path()));
}

INSTANTIATE_TEST_SUITE_P(DeviceLibraryPlusExportJobGroups,
        DeviceLibraryPlusExportJobGroupTest,
        ::testing::Values(std::make_pair(8, 1),
                std::make_pair(8, 8),
                std::make_pair(8, 9),
                std::make_pair(8, 17),
                std::make_pair(1, 9)));

/// The analysis file read without a database: AnalysisDao::loadAnalysisData.
class AnalysisDaoFileReadTest : public DeviceLibraryPlusExportJobTest {};

// The file list of the query and the read in another thread give the bytes
// that getAnalysesForTrackByType() gives. A corrupt file gives no bytes.
TEST_F(AnalysisDaoFileReadTest, ReadInAWorkerGivesTheSameBytes) {
    TrackPointer pTrack = addTrack(QStringLiteral("artist.mp3"), QStringLiteral("a"));
    ASSERT_TRUE(pTrack);
    storeWaveform(pTrack);
    auto& analysisDao = internalCollection()->getAnalysisDAO();
    const auto analyses =
            analysisDao.getAnalysesForTrackByType(pTrack->getId(), AnalysisDao::TYPE_WAVEFORM);
    ASSERT_EQ(1, analyses.size());
    const auto files = analysisDao.getAnalysisFilesForTrackByType(
            pTrack->getId(), AnalysisDao::TYPE_WAVEFORM);
    ASSERT_EQ(1, files.size());
    EXPECT_EQ(analyses.first().analysisId, files.first().analysisId);
    EXPECT_EQ(analyses.first().version, files.first().version);
    EXPECT_EQ(analyses.first().description, files.first().description);
    EXPECT_TRUE(files.first().data.isEmpty());
    ASSERT_TRUE(QFileInfo(files.first().dataPath).isFile());

    const QString path = files.first().dataPath;
    const int checksum = files.first().dataChecksum;
    const std::optional<QByteArray> data =
            QtConcurrent::run([path, checksum]() {
                return AnalysisDao::loadAnalysisData(path, checksum);
            }).result();
    ASSERT_TRUE(data);
    EXPECT_TRUE(*data == analyses.first().data);
    const ConstWaveformPointer pWaveform(
            WaveformFactory::loadWaveformFromAnalysis(analyses.first()));
    AnalysisDao::AnalysisInfo read = files.first();
    read.data = *data;
    const ConstWaveformPointer pRead(WaveformFactory::loadWaveformFromAnalysis(read));
    ASSERT_TRUE(pWaveform && pRead);
    EXPECT_TRUE(pWaveform->toByteArray() == pRead->toByteArray());

    // A corrupt file: no bytes, and the old function leaves the analysis out.
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::ReadWrite));
        file.seek(file.size() / 2);
        char byte = 0;
        file.peek(&byte, 1);
        byte = static_cast<char>(byte ^ 0x5a);
        file.write(&byte, 1);
    }
    EXPECT_FALSE(AnalysisDao::loadAnalysisData(path, checksum));
    EXPECT_TRUE(analysisDao.getAnalysesForTrackByType(pTrack->getId(), AnalysisDao::TYPE_WAVEFORM)
                        .isEmpty());
    // A missing file: no bytes.
    ASSERT_TRUE(QFile::remove(path));
    EXPECT_FALSE(AnalysisDao::loadAnalysisData(path, checksum));
}

// A track with a corrupt waveform file is skipped as a track without a
// waveform, as before.
TEST_F(DeviceLibraryPlusExportJobTest, CorruptWaveformFileSkipsTheTrack) {
    TrackPointer pGood = addTrack(QStringLiteral("all.mp3"), QStringLiteral("a"));
    TrackPointer pTrack = addTrack(QStringLiteral("artist.mp3"), QStringLiteral("b"));
    ASSERT_TRUE(pGood && pTrack);
    storeWaveform(pGood);
    storeWaveform(pTrack);
    const auto files = internalCollection()->getAnalysisDAO().getAnalysisFilesForTrackByType(
            pTrack->getId(), AnalysisDao::TYPE_WAVEFORM);
    ASSERT_EQ(1, files.size());
    {
        QFile file(files.first().dataPath);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("broken");
    }
    const Outcome outcome = runJob(request());
    ASSERT_TRUE(outcome.finished);
    ASSERT_EQ(1, outcome.completedCount) << qPrintable(outcome.failure);
    EXPECT_EQ(1, outcome.summary.tracksExported);
    ASSERT_EQ(1, outcome.summary.skipped.size());
    EXPECT_TRUE(outcome.summary.skipped.first().endsWith(
            QStringLiteral(": The track has no waveform. Analyse the track first.")))
            << qPrintable(outcome.summary.skipped.first());
    EXPECT_EQ(QList<qint64>{pGood->getId().toVariant().toLongLong()},
            contentIdsInRowOrder(m_stick.path()));
}
// End of AI-generated text.

// AI-generated (Claude Code). Start of AI-generated text.
namespace {

/// The bytes of every file on a stick by its relative path, without the
/// encrypted library file (it differs from save to save).
QMap<QString, QByteArray> stickFileBytes(const QString& stickRoot) {
    QMap<QString, QByteArray> result;
    QDirIterator it(stickRoot,
            QDir::Files | QDir::Hidden | QDir::System,
            QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (QFileInfo(path).fileName() == QStringLiteral("exportLibrary.db")) {
            continue;
        }
        QFile file(path);
        EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << qPrintable(path);
        result.insert(QDir(stickRoot).relativeFilePath(path), file.readAll());
    }
    return result;
}

/// The rows of the Plus library as text: the content rows with their stick
/// paths, the images and the playlists with their tracks in order.
QStringList plusRows(const QString& stickRoot) {
    QStringList result;
    QString error;
    auto pDb = DeviceLibraryPlusDb::open(stickRoot, &error);
    EXPECT_TRUE(pDb) << qPrintable(error);
    if (!pDb) {
        return result;
    }
    for (const DlpContentRef& row : pDb->listContents()) {
        result.append(QStringLiteral("content %1|%2|%3|%4|%5|%6")
                        .arg(QString::number(row.id),
                                row.path,
                                QString::number(row.masterDbId),
                                QString::number(row.masterContentId),
                                row.analysisDataFilePath,
                                row.imageId ? QString::number(*row.imageId)
                                            : QStringLiteral("-")));
    }
    for (const auto& [imageId, path] : pDb->listImages()) {
        result.append(QStringLiteral("image %1|%2").arg(QString::number(imageId), path));
    }
    for (const auto& playlist : pDb->listPlaylists()) {
        QStringList ids;
        for (const qint64 id : pDb->listPlaylistContentIds(playlist.id)) {
            ids.append(QString::number(id));
        }
        result.append(QStringLiteral("playlist %1|%2|%3")
                        .arg(QString::number(playlist.id), playlist.name, ids.join(',')));
    }
    return result;
}

/// The stick path that the export gives to artist.mp3 of the blocked track.
const QString kBlockedStickPath =
        QStringLiteral("/Contents/Blocked Artist/Blocked Album/artist.mp3");

} // namespace

/// The prepare threads and the tracks ahead of the pipelined export.
class DeviceLibraryPlusExportJobPipelineTest
        : public DeviceLibraryPlusExportJobTest,
          public ::testing::WithParamInterface<std::pair<int, int>> {
  protected:
    /// A library with tracks that get the same file name on the stick, two
    /// tracks with the same cover, a track without a waveform, a track whose
    /// file is missing and a track whose copy fails, and one playlist with
    /// all tracks.
    void makeLibrary() {
        QList<TrackId> playlistTracks;
        for (int i = 0; i < 12; ++i) {
            TrackPointer pTrack = addTrack(QStringLiteral("artist.mp3"), QString::number(i));
            ASSERT_TRUE(pTrack);
            pTrack->setTitle(QStringLiteral("Pipeline %1").arg(i));
            storeWaveform(pTrack);
            playlistTracks.append(pTrack->getId());
            m_tracks.append(pTrack);
            if (i == 4) {
                const QString coverFile = QString::fromUtf8(
                        "cover-test-\xc3\xb8\xc3\xa9~\xc5\x82\xe2\x82\xac\xcb\x9a-jpg.mp3");
                for (int cover = 0; cover < 2; ++cover) {
                    TrackPointer pCover =
                            addTrack(coverFile, QStringLiteral("cover%1").arg(cover));
                    ASSERT_TRUE(pCover);
                    storeWaveform(pCover);
                    playlistTracks.append(pCover->getId());
                    m_tracks.append(pCover);
                }
            }
            if (i == 6) {
                TrackPointer pNoWaveform =
                        addTrack(QStringLiteral("all.mp3"), QStringLiteral("nw"));
                ASSERT_TRUE(pNoWaveform);
                playlistTracks.append(pNoWaveform->getId());
                m_tracks.append(pNoWaveform);
                TrackPointer pMissing =
                        addTrack(QStringLiteral("artist.mp3"), QStringLiteral("gone"));
                ASSERT_TRUE(pMissing);
                storeWaveform(pMissing);
                ASSERT_TRUE(QFile::remove(pMissing->getLocation()));
                playlistTracks.append(pMissing->getId());
                m_tracks.append(pMissing);
            }
            if (i == 8) {
                TrackPointer pBlocked =
                        addTrack(QStringLiteral("artist.mp3"), QStringLiteral("blk"));
                ASSERT_TRUE(pBlocked);
                pBlocked->setArtist(QStringLiteral("Blocked Artist"));
                pBlocked->setAlbum(QStringLiteral("Blocked Album"));
                storeWaveform(pBlocked);
                playlistTracks.append(pBlocked->getId());
                m_tracks.append(pBlocked);
            }
        }
        PlaylistDAO& playlists = internalCollection()->getPlaylistDAO();
        const int playlistId = playlists.createPlaylist(QStringLiteral("Pipeline Playlist"));
        ASSERT_GE(playlistId, 0);
        ASSERT_TRUE(playlists.appendTracksToPlaylist(playlistTracks, playlistId));
    }

    /// The copy to the blocked path fails: a folder has the name of the
    /// temporary file of the copy.
    static void blockCopy(const QString& stickRoot) {
        ASSERT_TRUE(QDir().mkpath(stickRoot + kBlockedStickPath + QStringLiteral(".mixtwo-part")));
    }

    /// Runs the job to a stick with the prepare threads and tracks ahead.
    /// When `cancelAtProgress` is larger than 0, the job is cancelled when it
    /// sends this progress.
    Outcome runTo(const QString& stickRoot,
            int threads,
            int tracksAhead,
            int cancelAtProgress = 0) {
        DeviceLibraryPlusExportRequest jobRequest = request();
        jobRequest.stickRoot = stickRoot;
        DeviceLibraryPlusExportJob job(nullptr, trackCollectionManager(), jobRequest);
        job.setPrepareThreads(threads, tracksAhead);
        if (cancelAtProgress > 0) {
            // Direct: the cancel comes in the worker thread when it sends
            // the progress, so the point of the cancel is the same each time.
            QObject::connect(&job,
                    &DeviceLibraryPlusExportJob::jobProgress,
                    [&job, cancelAtProgress](int progress) {
                        if (progress == cancelAtProgress) {
                            job.slotCancel();
                        }
                    });
        }
        return runJob(job);
    }

    QList<TrackPointer> m_tracks;
};

// The pipelined export gives the same stick as the export in one thread: the
// same files with the same bytes (audio, ANLZ, artwork, export.pdb), the same
// rows and playlists, and the same summary in the order of the tracks. The
// failed tracks do not stop the others.
TEST_P(DeviceLibraryPlusExportJobPipelineTest, GivesTheSameStickAsOneThread) {
    makeLibrary();
    QTemporaryDir serialStick;
    ASSERT_TRUE(serialStick.isValid());
    blockCopy(serialStick.path());
    blockCopy(m_stick.path());

    const Outcome serial = runTo(serialStick.path(), 0, 1);
    ASSERT_TRUE(serial.finished);
    ASSERT_EQ(1, serial.completedCount) << qPrintable(serial.failure);
    EXPECT_EQ(14, serial.summary.tracksExported);
    ASSERT_EQ(3, serial.summary.skipped.size()) << qPrintable(serial.summary.skipped.join('\n'));
    EXPECT_TRUE(serial.summary.skipped[0].endsWith(
            QStringLiteral(": The track has no waveform. Analyse the track first.")));
    EXPECT_TRUE(serial.summary.skipped[1].endsWith(QStringLiteral(": The track file is missing.")));
    EXPECT_TRUE(serial.summary.skipped[2].contains(
            QStringLiteral(": Mix Two cannot copy the track to the stick: ")))
            << qPrintable(serial.summary.skipped[2]);

    const Outcome pipelined = runTo(m_stick.path(), GetParam().first, GetParam().second);
    ASSERT_TRUE(pipelined.finished);
    ASSERT_EQ(1, pipelined.completedCount) << qPrintable(pipelined.failure);
    EXPECT_EQ(serial.summary.tracksExported, pipelined.summary.tracksExported);
    EXPECT_EQ(serial.summary.playlistsExported, pipelined.summary.playlistsExported);
    EXPECT_EQ(serial.summary.skipped, pipelined.summary.skipped);
    EXPECT_EQ(serial.summary.warnings, pipelined.summary.warnings);
    EXPECT_EQ(serial.summary.bytesCopied, pipelined.summary.bytesCopied);
    EXPECT_EQ(serial.summary.audioFilesCopied, pipelined.summary.audioFilesCopied);
    EXPECT_EQ(serial.summary.anlzWritten, pipelined.summary.anlzWritten);
    EXPECT_EQ(serial.summary.anlzKept, pipelined.summary.anlzKept);

    const auto serialFiles = stickFileBytes(serialStick.path());
    const auto pipelinedFiles = stickFileBytes(m_stick.path());
    EXPECT_EQ(serialFiles.keys(), pipelinedFiles.keys());
    for (auto it = serialFiles.cbegin(); it != serialFiles.cend(); ++it) {
        EXPECT_TRUE(pipelinedFiles.value(it.key()) == it.value()) << qPrintable(it.key());
    }
    const QStringList serialRows = plusRows(serialStick.path());
    EXPECT_EQ(serialRows, plusRows(m_stick.path()));
    // Two tracks share the cover image; the names of the same file are
    // numbered in the order of the tracks.
    EXPECT_EQ(1, serialRows.filter(QStringLiteral("image ")).size());
    EXPECT_EQ(1, QStringList(serialFiles.keys()).filter(QStringLiteral("/artist (12).mp3")).size())
            << qPrintable(QStringList(serialFiles.keys()).join('\n'));

    // A second export of each: no copy, every ANLZ folder kept, the same rows.
    const Outcome serialAgain = runTo(serialStick.path(), 0, 1);
    const Outcome pipelinedAgain = runTo(m_stick.path(), GetParam().first, GetParam().second);
    ASSERT_EQ(1, serialAgain.completedCount) << qPrintable(serialAgain.failure);
    ASSERT_EQ(1, pipelinedAgain.completedCount) << qPrintable(pipelinedAgain.failure);
    EXPECT_EQ(0, pipelinedAgain.summary.bytesCopied);
    EXPECT_EQ(0, pipelinedAgain.summary.anlzWritten);
    EXPECT_EQ(serialAgain.summary.anlzKept, pipelinedAgain.summary.anlzKept);
    EXPECT_EQ(serialAgain.summary.skipped, pipelinedAgain.summary.skipped);
    EXPECT_EQ(plusRows(serialStick.path()), plusRows(m_stick.path()));
}

// A cancel after some tracks stops the job and the prepare threads: no
// `completed`, no `failed`, no library on a new stick, no half-copied file,
// and the same files on the stick as a cancel at the same point in one
// thread. On a stick with a library, the library does not change.
TEST_P(DeviceLibraryPlusExportJobPipelineTest, CancelLeavesTheSameStickAsOneThread) {
    makeLibrary();
    // Progress 2 is the end of the checks and the backup; then one per track.
    constexpr int kCancelAt = 2 + 3;
    QTemporaryDir serialStick;
    ASSERT_TRUE(serialStick.isValid());

    const Outcome serial = runTo(serialStick.path(), 0, 1, kCancelAt);
    ASSERT_TRUE(serial.finished);
    EXPECT_EQ(0, serial.completedCount);
    EXPECT_EQ(0, serial.failedCount);
    const Outcome pipelined =
            runTo(m_stick.path(), GetParam().first, GetParam().second, kCancelAt);
    ASSERT_TRUE(pipelined.finished);
    EXPECT_EQ(0, pipelined.completedCount);
    EXPECT_EQ(0, pipelined.failedCount);
    EXPECT_FALSE(QFileInfo::exists(libraryFile()));

    const auto serialFiles = stickFileBytes(serialStick.path());
    const auto pipelinedFiles = stickFileBytes(m_stick.path());
    // The three tracks before the cancel: their audio and ANLZ files.
    EXPECT_EQ(3, QStringList(serialFiles.keys()).filter(QStringLiteral(".mp3")).size());
    EXPECT_EQ(serialFiles.keys(), pipelinedFiles.keys());
    for (auto it = serialFiles.cbegin(); it != serialFiles.cend(); ++it) {
        EXPECT_TRUE(pipelinedFiles.value(it.key()) == it.value()) << qPrintable(it.key());
        EXPECT_FALSE(it.key().endsWith(QStringLiteral(".mixtwo-part"))) << qPrintable(it.key());
    }

    // A full export, then a cancel of the next one: the library on the stick
    // stays as it was.
    const Outcome full = runTo(m_stick.path(), GetParam().first, GetParam().second);
    ASSERT_EQ(1, full.completedCount) << qPrintable(full.failure);
    const QByteArray libraryBefore = [this] {
        QFile file(libraryFile());
        EXPECT_TRUE(file.open(QIODevice::ReadOnly));
        return file.readAll();
    }();
    const QStringList rowsBefore = plusRows(m_stick.path());
    const auto filesBefore = stickFileBytes(m_stick.path());
    const Outcome cancelled =
            runTo(m_stick.path(), GetParam().first, GetParam().second, kCancelAt);
    EXPECT_EQ(0, cancelled.completedCount);
    EXPECT_EQ(0, cancelled.failedCount);
    QFile library(libraryFile());
    ASSERT_TRUE(library.open(QIODevice::ReadOnly));
    EXPECT_TRUE(library.readAll() == libraryBefore);
    EXPECT_EQ(rowsBefore, plusRows(m_stick.path()));
    EXPECT_EQ(filesBefore.keys(), stickFileBytes(m_stick.path()).keys());
}

INSTANTIATE_TEST_SUITE_P(DeviceLibraryPlusExportJobPipeline,
        DeviceLibraryPlusExportJobPipelineTest,
        ::testing::Values(std::make_pair(1, 1),
                std::make_pair(2, 3),
                std::make_pair(3, 8),
                std::make_pair(4, 20)));
// End of AI-generated text.

} // namespace mixxx
