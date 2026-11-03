#pragma once

#include <QAtomicInteger>
#include <QList>
#include <QPair>
#include <QString>
#include <QThread>

#include "library/export/devicelibraryplusexportrequest.h"
#include "library/rekordbox/devicelibraryplusexporter.h"
#include "library/trackset/crate/crateid.h"
#include "track/trackid.h"

class TrackCollectionManager;

namespace mixxx {

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
///
/// Delete the job after its `finished()` signal. Do not wait for the thread in
/// the main thread: the worker waits for the main thread when it loads data.
class DeviceLibraryPlusExportJob : public QThread {
    Q_OBJECT
  public:
    DeviceLibraryPlusExportJob(
            QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            const DeviceLibraryPlusExportRequest& request);

    ~DeviceLibraryPlusExportJob() override;

    /// Runs the export in the worker thread. Call start(), not run().
    void run() override;

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
    void loadTrack(int index);
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

    bool isCancelled() const {
        return m_cancellationRequested.loadAcquire() != 0;
    }

    /// Runs a private slot in the main thread and waits for it. Returns false
    /// when Qt could not call the slot.
    bool callInMainThread(const char* slotName, int index = -1);

    const DeviceLibraryPlusExportRequest m_request;
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
    rekordbox::DlpExportTrack m_loadedTrack;
    rekordbox::DlpExportPlaylist m_loadedList;
};

} // namespace mixxx
