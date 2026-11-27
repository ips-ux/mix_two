#pragma once

#include <QAtomicInteger>
#include <QList>
#include <QPair>
#include <QString>
#include <QThread>
#include <functional>

#include "library/dao/analysisdao.h"
#include "library/export/devicelibraryplusexportrequest.h"
#include "library/rekordbox/devicelibraryplusexporter.h"
#include "library/rekordbox/legacylibrary.h"
#include "library/trackset/crate/crateid.h"
#include "track/trackid.h"

class TrackCollectionManager;

namespace mixxx {

// AI-generated (Claude Code). Start of AI-generated text.
/// The text that the user sees when the export is completed.
QString deviceLibraryPlusCompletionText(const DeviceLibraryPlusExportSummary& summary);
// End of AI-generated text.

/// Mix Two: exports Mix Two tracks, crates and playlists to the Device
/// Library Plus of a stick, as described by the request.
///
/// Threads: the object lives in the main thread (the thread of the track
/// collection manager). The Mix Two library (the track collection, its DAOs
/// and the loading of tracks) is used only in the private slots, and Qt runs
/// them in the main thread: run() calls them with a blocking queued
/// connection. run() does the rest of the work in the worker thread: the
/// stick checks, the backup, the move of leftover files to the computer, the
/// DeviceLibraryPlusDb and the exporter. The database and the exporter exist
/// only inside run(). run() checks for a cancel before each call into the
/// main thread.
// AI-generated (Claude Code). Start of AI-generated text.
/// The main thread loads the tracks in groups (kTrackLoadGroupSize per call):
/// only the track and the list of its waveform files. The worker reads and
/// decodes the waveform file and loads the cover image.
///
/// The track export is a pipeline: the job thread plans each track (its
/// stick path) in the order of the tracks, a private thread pool prepares up
/// to kTracksAhead planned tracks (waveform, cover, ANLZ bytes, JPEG bytes),
/// and the job thread writes the audio, the ANLZ files, the artwork and the
/// rows in the order of the tracks. Only the job thread writes to the stick
/// and uses the database. The job thread waits for all prepare tasks before
/// it returns.
// End of AI-generated text.
///
/// Delete the job after its `finished()` signal. Do not wait for the thread in
/// the main thread: the worker waits for the main thread when it loads data.
class DeviceLibraryPlusExportJob : public QThread {
    Q_OBJECT
  public:
    using LegacyWriter = std::function<QByteArray(const rekordbox::LegacyLibrary&, QString*)>;

    DeviceLibraryPlusExportJob(
            QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            const DeviceLibraryPlusExportRequest& request,
            LegacyWriter legacyWriter = rekordbox::legacyPdbBytes);

    ~DeviceLibraryPlusExportJob() override;

    /// Runs the export in the worker thread. Call start(), not run().
    void run() override;

    // AI-generated (Claude Code). Start of AI-generated text.
    /// For tests: the number of tracks that one call into the main thread
    /// loads (default kTrackLoadGroupSize). Call before start().
    void setTrackLoadGroupSize(int groupSize);

    /// For tests: the number of calls into the main thread that loaded tracks.
    /// Read after the job finished.
    int trackLoadCallCount() const {
        return m_trackLoadCallCount;
    }

    /// The default number of tracks that one call into the main thread loads.
    static constexpr int kTrackLoadGroupSize = 8;
    // End of AI-generated text.

    // AI-generated (Claude Code). Start of AI-generated text.
    /// For tests: the number of threads that prepare the next tracks, and the
    /// number of tracks that are planned and prepared before they are written
    /// (at least 1). 0 threads: the job thread prepares each track itself.
    /// Call before start(). The default is defaultPrepareThreadCount() and
    /// kTracksAhead.
    void setPrepareThreads(int threadCount, int tracksAhead);

    /// Half the cores, from 1 to 3.
    static int defaultPrepareThreadCount();

    /// The default number of tracks ahead. It limits the memory of the
    /// decoded waveforms.
    static constexpr int kTracksAhead = 8;
    // End of AI-generated text.

  signals:
    /// The maximum of the numbers that jobProgress() sends.
    void jobMaximum(int maximum);

    /// The progress of the job, up to the maximum.
    void jobProgress(int progress);

    /// The export finished and the library on the stick was written.
    void completed(const mixxx::DeviceLibraryPlusExportSummary& summary);

    /// The export failed. The message is for the user.
    void failed(const QString& message);

  public slots:
    /// Asks the job to stop. A stop before the library is written leaves the
    /// library on the stick unchanged, and the job sends neither `completed`
    /// nor `failed`.
    void slotCancel();

  private slots:
    // These slots load data from the Mix Two library. Qt runs them in the
    // main thread, which is not the thread of run().
    void loadIds();
    // AI-generated (Claude Code). Start of AI-generated text.
    void loadTracks(int first);
    // End of AI-generated text.
    void loadCrate(int index);
    void loadPlaylist(int index);

  private:
    /// A track to export: its ID, and what the exporter needs to estimate the
    /// bytes to copy (the location of its file, the artist and the album).
    struct TrackEntry {
        TrackId id;
        QString location;
        QString artist;
        QString album;
    };

    // AI-generated (Claude Code). Start of AI-generated text.
    /// A track that loadTracks() loaded in the main thread: the track (null
    /// when it is not in the library) and its waveform files, not yet read.
    struct LoadedTrack {
        TrackPointer pTrack;
        QList<AnalysisDao::AnalysisInfo> waveformFiles;
    };
    // End of AI-generated text.

    bool isCancelled() const {
        return m_cancellationRequested.loadAcquire() != 0;
    }

    /// Runs a private slot in the main thread and waits for it. Returns false
    /// when Qt could not call the slot.
    bool callInMainThread(const char* slotName, int index = -1);

    const DeviceLibraryPlusExportRequest m_request;
    const LegacyWriter m_legacyWriter;
    TrackCollectionManager* const m_pTrackCollectionManager;
    QAtomicInteger<int> m_cancellationRequested;

    // Written by loadIds() in the main thread. run() reads them after the
    // blocking call returns.
    QList<TrackEntry> m_trackEntries;
    QList<CrateId> m_crateIds;
    QList<QPair<int, QString>> m_playlistIdsAndNames;
    /// The reason that loadIds() could not read the library, else empty.
    QString m_loadError;

    // Written by the slots in the main thread, one slot at a time. run() reads
    // the result after the blocking call returns.
    // AI-generated (Claude Code). Start of AI-generated text.
    QList<LoadedTrack> m_loadedTracks;
    // End of AI-generated text.
    rekordbox::DlpExportPlaylist m_loadedList;
    // AI-generated (Claude Code). Start of AI-generated text.
    int m_trackLoadGroupSize = kTrackLoadGroupSize;
    int m_trackLoadCallCount = 0;
    /// -1: defaultPrepareThreadCount().
    int m_prepareThreadCount = -1;
    int m_tracksAhead = kTracksAhead;
    // End of AI-generated text.
};

} // namespace mixxx
