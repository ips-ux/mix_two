// AI-generated (Claude Code). Start of AI-generated text.
#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTimer>
#include <algorithm>
#include <cstdio>
#include <utility>

#include "library/coverart.h"
#include "library/dao/analysisdao.h"
#include "library/dao/playlistdao.h"
#include "library/export/devicelibraryplusexportjob.h"
#include "library/export/dlgdeviceexport.h"
#include "library/rekordbox/anlztrackdata.h"
#include "library/rekordbox/anlzwriter.h"
#include "library/rekordbox/devicelibraryplusmapping.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "sources/soundsourceproxy.h"
#include "test/librarytest.h"
#include "track/track.h"
#include "waveform/waveform.h"
#include "waveform/waveformfactory.h"

// An opt-in speed test of the export job. It writes to a folder that the
// user gives, so that the folder can be a real stick:
//   MIXTWO_EXPORT_BENCH_DIR=<empty folder> mixxx-test
//       --gtest_filter=DeviceLibraryPlusExportJobBenchTest.*
// MIXTWO_EXPORT_BENCH_TRACKS gives the number of tracks (default 200).
// The test only reports the ratio of the export time to the time of a plain
// copy of the same files. When MIXTWO_EXPORT_BENCH_RATIO is set (for example
// 1.25), the test also fails if the ratio is higher. The test always fails
// if the second export copies audio files again.

namespace mixxx {

namespace {

constexpr qint64 kMasterDbId = 4343;
const QString kDeviceName = QStringLiteral("BENCH");
constexpr int kDefaultTrackCount = 200;
/// The size of each audio file, about the size of a short MP3 track.
constexpr qint64 kTargetFileSize = 5 * 1024 * 1024;
/// A stick that is slow can need much time; the test must not stop it.
constexpr int kJobTimeoutMs = 60 * 60 * 1000;
/// The interval of the timer that measures how long the main thread is busy.
constexpr int kTickMs = 5;
/// The modification time that marks the audio files of the stick before the
/// second export. A copy gets the time of its source file.
const QDateTime kMarkTime = QDateTime(QDate(2001, 1, 1), QTime(0, 0), QTimeZone::utc());

/// The MP3 test files, with covers. Each audio file of the test is made of
/// one of them: its tag and then its frames again and again.
const QStringList kSourceFiles = {
        QString::fromUtf8("cover-test-\xc3\xb8\xc3\xa9~\xc5\x82\xe2\x82\xac\xcb\x9a-jpg.mp3"),
        QString::fromUtf8("cover-test-\xc3\xb8\xc3\xa9~\xc5\x82\xe2\x82\xac\xcb\x9a-png.mp3"),
        QStringLiteral("artist.mp3"),
};

double seconds(qint64 nanoseconds) {
    return static_cast<double>(nanoseconds) / 1e9;
}

/// The bytes of an MP3 file of about kTargetFileSize: the ID3v2 tag of the
/// source once, then the MPEG frames of the source repeated.
QByteArray largeMp3(const QByteArray& source) {
    qsizetype tagSize = 0;
    if (source.size() >= 10 && source.startsWith("ID3")) {
        tagSize = 10 +
                ((static_cast<quint8>(source[6]) & 0x7f) << 21 |
                        (static_cast<quint8>(source[7]) & 0x7f) << 14 |
                        (static_cast<quint8>(source[8]) & 0x7f) << 7 |
                        (static_cast<quint8>(source[9]) & 0x7f));
    }
    qsizetype framesEnd = source.size();
    if (framesEnd - 128 > tagSize && source.mid(framesEnd - 128, 3) == "TAG") {
        framesEnd -= 128;
    }
    const QByteArray frames = source.mid(tagSize, framesEnd - tagSize);
    QByteArray result = source.left(tagSize);
    if (frames.isEmpty()) {
        return result;
    }
    while (result.size() < kTargetFileSize) {
        result.append(frames);
    }
    return result;
}

/// The audio files below /Contents of a stick, with their modification time.
QHash<QString, QDateTime> audioFilesOf(const QString& stickRoot) {
    QHash<QString, QDateTime> result;
    QDirIterator it(QDir(stickRoot).filePath(QStringLiteral("Contents")),
            QDir::Files,
            QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        result.insert(it.filePath(), it.fileInfo().lastModified().toUTC());
    }
    return result;
}

/// The times of one run of the job.
struct BenchRun {
    bool finished = false;
    int completedCount = 0;
    QString failure;
    DeviceLibraryPlusExportSummary summary;
    qint64 totalNs = 0;
    /// The time at which each progress value came, from the start.
    QList<QPair<int, qint64>> progressNs;
    int progressMaximum = 0;
    /// The longest time in which the main thread did not run its timer.
    qint64 longestStallNs = 0;
};

class DeviceLibraryPlusExportJobBenchTest : public LibraryTest {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_music.isValid());
        ASSERT_TRUE(m_backups.isValid());
    }

    TrackPointer addTrack(const QString& target) {
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

    /// Stores a complete waveform for the track, as the analyser does.
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

    /// Runs the job to its end and records the times.
    BenchRun runJob(const DeviceLibraryPlusExportRequest& jobRequest) {
        BenchRun run;
        DeviceLibraryPlusExportJob job(nullptr, trackCollectionManager(), jobRequest);
        QObject context;
        QEventLoop loop;
        QElapsedTimer clock;
        QObject::connect(&job,
                &DeviceLibraryPlusExportJob::jobMaximum,
                &context,
                [&run](int maximum) { run.progressMaximum = maximum; });
        QObject::connect(&job,
                &DeviceLibraryPlusExportJob::jobProgress,
                &context,
                [&run, &clock](int value) { run.progressNs.append({value, clock.nsecsElapsed()}); });
        QObject::connect(&job,
                &DeviceLibraryPlusExportJob::completed,
                &context,
                [&run](const DeviceLibraryPlusExportSummary& summary) {
                    ++run.completedCount;
                    run.summary = summary;
                },
                Qt::QueuedConnection);
        QObject::connect(&job,
                &DeviceLibraryPlusExportJob::failed,
                &context,
                [&run](const QString& message) { run.failure = message; },
                Qt::QueuedConnection);
        QObject::connect(&job,
                &QThread::finished,
                &loop,
                [&run, &loop, &clock]() {
                    run.finished = true;
                    run.totalNs = clock.nsecsElapsed();
                    loop.quit();
                },
                Qt::QueuedConnection);
        // The stall timer: a GUI would not paint in a long gap.
        QTimer tick;
        qint64 lastTickNs = 0;
        QObject::connect(&tick, &QTimer::timeout, &context, [&]() {
            const qint64 nowNs = clock.nsecsElapsed();
            run.longestStallNs = std::max(run.longestStallNs, nowNs - lastTickNs);
            lastTickNs = nowNs;
        });
        QTimer::singleShot(kJobTimeoutMs, &loop, &QEventLoop::quit);
        clock.start();
        tick.start(kTickMs);
        job.start();
        loop.exec();
        tick.stop();
        if (!run.finished) {
            job.slotCancel();
        }
        while (!job.wait(10)) {
            QCoreApplication::processEvents();
        }
        QCoreApplication::processEvents();
        return run;
    }

    /// Prints where the time of a run went, by the progress values of the job.
    static void printPhases(const char* name, const BenchRun& run, int trackCount) {
        // The job sends 0 at the start, then one value for the checks, one for
        // the backup, one per track, one per list, one after the save of the
        // Plus library, and one after the legacy library.
        QHash<int, qint64> at;
        for (const auto& [value, ns] : run.progressNs) {
            at.insert(value, ns);
        }
        const int tracksEnd = 2 + trackCount;
        const int plusSaved = run.progressMaximum - 1;
        const int legacyWritten = run.progressMaximum;
        const auto span = [&at](int from, int to) {
            return at.contains(from) && at.contains(to) ? seconds(at[to] - at[from]) : -1.0;
        };
        std::printf("%s phases (s): start %.2f, checks %.2f, backup %.2f, tracks %.2f "
                    "(%.1f ms/track), lists %.2f, plus save %.2f, legacy %.2f, end %.2f\n",
                name,
                at.contains(0) ? seconds(at[0]) : -1.0,
                span(0, 1),
                span(1, 2),
                span(2, tracksEnd),
                trackCount > 0 ? span(2, tracksEnd) * 1000.0 / trackCount : 0.0,
                span(tracksEnd, plusSaved - 1),
                span(plusSaved - 1, plusSaved),
                span(plusSaved, legacyWritten),
                at.contains(legacyWritten) ? seconds(run.totalNs - at[legacyWritten]) : -1.0);
        std::printf("%s: longest stall of the main thread %.1f ms\n",
                name,
                static_cast<double>(run.longestStallNs) / 1e6);
    }

    /// Times the steps of the export of one track outside the job, for the
    /// first tracks: where the time of a track goes.
    void printTrackSteps(const QList<TrackId>& trackIds, const QString& stickRoot) {
        const int count = std::min(static_cast<int>(trackIds.size()), 20);
        const QString anlzRoot = QDir(stickRoot).filePath(QStringLiteral("bench-anlz"));
        qint64 loadNs = 0, waveformNs = 0, coverNs = 0, offsetNs = 0, framesNs = 0,
               dataNs = 0, writeNs = 0;
        QElapsedTimer clock;
        auto& analysisDao = internalCollection()->getAnalysisDAO();
        for (int i = 0; i < count; ++i) {
            clock.start();
            const TrackPointer pTrack = trackCollectionManager()->getTrackById(trackIds.at(i));
            loadNs += clock.nsecsElapsed();
            ASSERT_TRUE(pTrack);
            clock.start();
            const auto analyses = analysisDao.getAnalysesForTrackByType(
                    trackIds.at(i), AnalysisDao::TYPE_WAVEFORM);
            ASSERT_FALSE(analyses.isEmpty());
            const ConstWaveformPointer pWaveform(
                    WaveformFactory::loadWaveformFromAnalysis(analyses.first()));
            waveformNs += clock.nsecsElapsed();
            clock.start();
            CoverInfo coverInfo = pTrack->getCoverInfoWithLocation();
            if (coverInfo.hasImage()) {
                const CoverInfo::LoadedImage loaded = coverInfo.loadImage(pTrack);
                if (!coverInfo.hasCacheKey()) {
                    coverInfo.setImageDigest(loaded.image);
                }
            }
            coverNs += clock.nsecsElapsed();
            const QString location = pTrack->getLocation();
            clock.start();
            const int offsetMs = rekordbox::rekordboxTimingOffsetMs(location);
            offsetNs += clock.nsecsElapsed();
            clock.start();
            const auto frames = rekordbox::dlpMpegPcmFrames(location);
            framesNs += clock.nsecsElapsed();
            clock.start();
            QStringList warnings;
            QString error;
            auto data = rekordbox::anlzTrackDataFrom(*pTrack,
                    pWaveform,
                    QStringLiteral("/Contents/a/b/%1.mp3").arg(i),
                    offsetMs,
                    &warnings,
                    &error);
            dataNs += clock.nsecsElapsed();
            ASSERT_TRUE(data) << qPrintable(error);
            data->mp3PcmFrames = frames.value_or(0);
            clock.start();
            ASSERT_TRUE(rekordbox::writeAnlzFiles(
                    *data, QDir(anlzRoot).filePath(QString::number(i)), &error))
                    << qPrintable(error);
            writeNs += clock.nsecsElapsed();
        }
        QDir(anlzRoot).removeRecursively();
        const auto ms = [count](qint64 ns) {
            return static_cast<double>(ns) / 1e6 / count;
        };
        std::printf("steps per track (ms, mean of %d): track load %.2f, waveform load %.2f, "
                    "cover load %.2f, timing offset %.2f, MP3 frame count %.2f, ANLZ data %.2f, "
                    "ANLZ write %.2f\n",
                count,
                ms(loadNs),
                ms(waveformNs),
                ms(coverNs),
                ms(offsetNs),
                ms(framesNs),
                ms(dataNs),
                ms(writeNs));
    }

    QTemporaryDir m_music;
    QTemporaryDir m_backups;
};

} // namespace

TEST_F(DeviceLibraryPlusExportJobBenchTest, ExportIsNearTheSpeedOfACopy) {
    const QString stickRoot = qEnvironmentVariable("MIXTWO_EXPORT_BENCH_DIR");
    if (stickRoot.isEmpty()) {
        GTEST_SKIP() << "MIXTWO_EXPORT_BENCH_DIR is not set.";
    }
    bool ok = false;
    int trackCount = qEnvironmentVariableIntValue("MIXTWO_EXPORT_BENCH_TRACKS", &ok);
    if (!ok || trackCount <= 0) {
        trackCount = kDefaultTrackCount;
    }
    // No limit (report only) unless the variable gives one.
    const QString ratioText = qEnvironmentVariable("MIXTWO_EXPORT_BENCH_RATIO");
    double maxRatio = 0;
    if (!ratioText.isEmpty()) {
        maxRatio = ratioText.toDouble(&ok);
        ASSERT_TRUE(ok && maxRatio > 0)
                << "MIXTWO_EXPORT_BENCH_RATIO is not a positive number: "
                << qPrintable(ratioText);
    }
    ASSERT_TRUE(QDir().mkpath(stickRoot));
    if (!QDir(stickRoot).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
        FAIL() << "MIXTWO_EXPORT_BENCH_DIR is not empty. Use an empty folder.";
    }

    // The audio files of the library.
    QList<QByteArray> sources;
    for (const QString& name : kSourceFiles) {
        QFile file(getOrInitTestDir().filePath(QStringLiteral("id3-test-data/") + name));
        ASSERT_TRUE(file.open(QIODevice::ReadOnly)) << qPrintable(file.fileName());
        sources.append(largeMp3(file.readAll()));
    }
    QStringList files;
    qint64 totalBytes = 0;
    for (int i = 0; i < trackCount; ++i) {
        const QString folder = m_music.filePath(QStringLiteral("album%1").arg(i / 10));
        ASSERT_TRUE(QDir().mkpath(folder));
        const QString path = QDir(folder).filePath(QStringLiteral("bench track %1.mp3").arg(i));
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly)) << qPrintable(path);
        const QByteArray& bytes = sources.at(i % sources.size());
        ASSERT_EQ(bytes.size(), file.write(bytes));
        file.close();
        files.append(path);
        totalBytes += bytes.size();
    }
    QList<TrackId> trackIds;
    for (int i = 0; i < trackCount; ++i) {
        TrackPointer pTrack = addTrack(files.at(i));
        ASSERT_TRUE(pTrack);
        pTrack->setArtist(QStringLiteral("Bench Artist %1").arg(i / 10));
        pTrack->setAlbum(QStringLiteral("Bench Album %1").arg(i / 10));
        pTrack->setTitle(QStringLiteral("Bench Track %1").arg(i));
        storeWaveform(pTrack);
        trackIds.append(pTrack->getId());
    }
    PlaylistDAO& playlists = internalCollection()->getPlaylistDAO();
    const int playlistId = playlists.createPlaylist(QStringLiteral("Bench Playlist"));
    ASSERT_GE(playlistId, 0);
    ASSERT_TRUE(playlists.appendTracksToPlaylist(trackIds, playlistId));

    // (a) A plain copy of the same files to the stick. The copy goes away
    // after the measurement, so that the export has the same free space.
    const QString copyFolder = QDir(stickRoot).filePath(QStringLiteral("bench-plain-copy"));
    ASSERT_TRUE(QDir().mkpath(copyFolder));
    QElapsedTimer copyClock;
    copyClock.start();
    for (int i = 0; i < trackCount; ++i) {
        ASSERT_TRUE(QFile::copy(files.at(i), QDir(copyFolder).filePath(QStringLiteral("%1.mp3").arg(i))));
    }
    const qint64 copyNs = copyClock.nsecsElapsed();
    ASSERT_TRUE(QDir(copyFolder).removeRecursively());

    // (b) The first export, with the request of the dialog.
    DlgDeviceExport dialog(nullptr, config(), trackCollectionManager());
    const auto pRequest = dialog.makeRequest(stickRoot);
    ASSERT_TRUE(pRequest->writeLegacyLibrary);
    pRequest->deviceName = kDeviceName;
    pRequest->backupBaseDir = m_backups.path();
    pRequest->masterDbId = kMasterDbId;
    const BenchRun first = runJob(*pRequest);
    ASSERT_TRUE(first.finished);
    ASSERT_EQ(1, first.completedCount) << qPrintable(first.failure);
    EXPECT_EQ(trackCount, first.summary.tracksExported)
            << qPrintable(first.summary.skipped.join('\n'));
    EXPECT_TRUE(first.summary.legacyLibraryError.isEmpty())
            << qPrintable(first.summary.legacyLibraryError);

    // (c) A second export of the same library. Each audio file of the stick
    // gets a marker time first: a file that is copied again loses it.
    const QHash<QString, QDateTime> before = audioFilesOf(stickRoot);
    EXPECT_EQ(trackCount, before.size());
    for (auto it = before.constBegin(); it != before.constEnd(); ++it) {
        QFile file(it.key());
        ASSERT_TRUE(file.open(QIODevice::ReadWrite)) << qPrintable(it.key());
        ASSERT_TRUE(file.setFileTime(kMarkTime, QFileDevice::FileModificationTime));
    }
    const BenchRun second = runJob(*pRequest);
    ASSERT_TRUE(second.finished);
    ASSERT_EQ(1, second.completedCount) << qPrintable(second.failure);
    EXPECT_EQ(trackCount, second.summary.tracksExported);
    qint64 bytesCopiedAgain = 0;
    int filesCopiedAgain = 0;
    const QHash<QString, QDateTime> after = audioFilesOf(stickRoot);
    for (auto it = after.constBegin(); it != after.constEnd(); ++it) {
        if (!before.contains(it.key()) || it.value() != kMarkTime) {
            ++filesCopiedAgain;
            bytesCopiedAgain += QFileInfo(it.key()).size();
        }
    }

    const double ratio = static_cast<double>(first.totalNs) / static_cast<double>(copyNs);
    std::printf("\n| files | MB | copy s | export s | ratio | second export s | "
                "audio files copied again | bytes copied again |\n");
    std::printf("|---|---|---|---|---|---|---|---|\n");
    std::printf("| %d | %.1f | %.2f | %.2f | %.2f | %.2f | %d | %lld |\n\n",
            trackCount,
            static_cast<double>(totalBytes) / (1024.0 * 1024.0),
            seconds(copyNs),
            seconds(first.totalNs),
            ratio,
            seconds(second.totalNs),
            filesCopiedAgain,
            static_cast<long long>(bytesCopiedAgain));
    printPhases("first export", first, trackCount);
    printPhases("second export", second, trackCount);
    printTrackSteps(trackIds, stickRoot);

    if (maxRatio > 0) {
        EXPECT_LE(ratio, maxRatio) << "The export took " << ratio << " times the copy time.";
    } else {
        std::printf("Report only: set MIXTWO_EXPORT_BENCH_RATIO to make the ratio a limit.\n");
    }
    EXPECT_EQ(0, filesCopiedAgain) << "The second export copied audio files again.";
}

} // namespace mixxx

// End of AI-generated text.
