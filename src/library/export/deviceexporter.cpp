#include "library/export/deviceexporter.h"

#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QRandomGenerator>
#include <QStorageInfo>
#include <QTimer>
#include <QtDebug>

#include "library/export/devicelibraryplusexportjob.h"
#include "library/export/devicelibraryplusexportrequest.h"
#include "library/export/dlgdeviceexport.h"
#include "library/export/stickeject.h"
#include "library/rekordbox/devicelibraryplusstick.h"
#include "mixer/playerinfo.h"
#include "moc_deviceexporter.cpp"
#include "track/track.h"

namespace mixxx {

namespace {

const ConfigKey kMasterDbIdConfigKey(
        QStringLiteral("[Library]"), QStringLiteral("DeviceLibraryPlusMasterDbId"));

/// The name of a stick without a volume label.
const QString kDefaultDeviceName = QStringLiteral("Mix Two");

/// The folder for the backups of the libraries of sticks, below the settings
/// folder.
const QString kBackupFolderName = QStringLiteral("/device-backups");

/// How long the destructor waits for a running job to stop.
constexpr int kJobStopTimeoutMs = 10000;

} // namespace

DeviceExporter::DeviceExporter(QWidget* parent,
        UserSettingsPointer pConfig,
        TrackCollectionManager* pTrackCollectionManager)
        : QWidget{parent},
          m_pConfig{std::move(pConfig)},
          m_pTrackCollectionManager{pTrackCollectionManager} {
}

DeviceExporter::~DeviceExporter() {
    // The job uses the track collection, so it must stop before the library
    // shuts down. The job has no parent, so it is not deleted here. QThread::wait()
    // is not possible: the job waits for this thread when it loads data
    // (blocking queued calls). A local event loop runs these calls, and it
    // ends when the job sends `finished` or after the time-out.
    m_isClosing = true;
    if (m_pJob && m_pJob->isRunning()) {
        qDebug() << "Device Library Plus export: stopping the running job";
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        connect(m_pJob.data(), &QThread::finished, &loop, &QEventLoop::quit);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        m_pJob->slotCancel();
        // The job can have ended between the check and the connection.
        if (m_pJob->isRunning()) {
            timeout.start(kJobStopTimeoutMs);
            loop.exec();
            if (m_pJob && m_pJob->isRunning()) {
                qWarning() << "Device Library Plus export: the job did not stop in"
                           << kJobStopTimeoutMs << "ms";
            }
        }
    }
}

void DeviceExporter::slotRequestExportWithInitialTracks(const QList<TrackId>& trackIds) {
    requestExportWithOptionalInitialSelection(std::nullopt, std::nullopt, trackIds);
}

void DeviceExporter::requestExportWithOptionalInitialSelection(
        std::optional<CrateId> initialSelectedCrateId,
        std::optional<int> initialSelectedPlaylistId,
        const QList<TrackId>& initialTrackIds) {
    if (m_pJob && m_pJob->isRunning()) {
        // The progress dialog blocks the main window, so this is rare.
        qDebug() << "Device Library Plus export: an export runs already";
        return;
    }

    if (!m_pDialog) {
        m_pDialog = make_parented<DlgDeviceExport>(
                this, m_pConfig, m_pTrackCollectionManager);
        connect(m_pDialog.get(),
                &DlgDeviceExport::startExport,
                this,
                &DeviceExporter::beginExport);
    }

    m_pDialog->refresh();
    m_pDialog->setInitialSelection(initialSelectedCrateId, initialSelectedPlaylistId);
    // AI-generated (Claude Code). Start of AI-generated text.
    // Tracks replace the choice of the line above: the mode "Selected tracks".
    // End of AI-generated text.
    if (!initialTrackIds.isEmpty()) {
        m_pDialog->setInitialTracks(initialTrackIds);
    }
    m_pDialog->show();
    m_pDialog->raise();
    m_pDialog->setWindowState(
            (m_pDialog->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    m_pDialog->activateWindow();
}

qint64 DeviceExporter::masterDbId() {
    qint64 id = m_pConfig->getValue<int>(kMasterDbIdConfigKey, 0);
    if (id <= 0) {
        id = QRandomGenerator::global()->bounded(1, 0x7fffffff);
        m_pConfig->setValue<int>(kMasterDbIdConfigKey, static_cast<int>(id));
        // Keep the ID at once: a crash before the exit must not lose it.
        // Without it a later export would add the rows again.
        if (!m_pConfig->save()) {
            qWarning() << "Device Library Plus export: cannot save the library ID";
        }
    }
    return id;
}

// AI-generated (Claude Code). Start of AI-generated text.
namespace {

/// The root of the volume that holds `stickRoot` (the user can have chosen a
/// folder of the stick). An empty path would give the current drive: it gives
/// no volume here.
QString volumeRootOf(const QString& stickRoot) {
    return stickRoot.trimmed().isEmpty() ? QString() : QStorageInfo(stickRoot).rootPath();
}

/// Shows a busy cursor while it lives. The eject blocks the window.
class BusyCursor {
  public:
    BusyCursor() {
        QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    }
    ~BusyCursor() {
        QGuiApplication::restoreOverrideCursor();
    }
};

} // namespace

void DeviceExporter::offerEject(const QString& stickRoot,
        const QString& exportedVolumeRoot,
        const QString& exportedIdentity) {
    const std::unique_ptr<StickEjector> pEjector = makeSystemStickEjector();
    while (true) {
        // The files that Mix Two has open: the tracks in the decks, the
        // samplers and the preview deck. Read them again at each try, because
        // the user can unload a track while the message shows.
        QStringList openFiles;
        const auto loadedTracks = PlayerInfo::instance().getLoadedTracks();
        for (const TrackPointer& pTrack : loadedTracks) {
            if (!pTrack) {
                continue;
            }
            const QString location = pTrack->getLocation();
            openFiles.append(location);
            openFiles.append(QFileInfo(location).canonicalFilePath());
        }
        StickEjectResult result;
        {
            BusyCursor busy;
            // The volume that is there now: the guard compares it with the
            // volume and the identity of the export.
            result = ejectExportedStick(*pEjector,
                    exportedVolumeRoot,
                    exportedIdentity,
                    volumeRootOf(stickRoot),
                    openFiles);
        }
        if (result.ok) {
            QMessageBox::information(window(),
                    tr("Stick ejected"),
                    tr("You can remove the stick now."));
            return;
        }
        qWarning() << "Device Library Plus export: the eject did not work:" << result.message;
        QMessageBox box(QMessageBox::Warning,
                tr("Eject failed"),
                result.message,
                QMessageBox::Retry | QMessageBox::Close,
                window());
        box.setDefaultButton(QMessageBox::Close);
        if (box.exec() != QMessageBox::Retry) {
            return;
        }
    }
}
// End of AI-generated text.

void DeviceExporter::beginExport(QSharedPointer<DeviceLibraryPlusExportRequest> pRequest) {
    if (m_pJob && m_pJob->isRunning()) {
        return;
    }

    // The dialog gives the stick and the tracks. The rest comes from here.
    const QString volumeLabel = rekordbox::dlpStickInfo(pRequest->stickRoot).volumeLabel;
    pRequest->deviceName = volumeLabel.isEmpty() ? kDefaultDeviceName : volumeLabel;
    pRequest->backupBaseDir = m_pConfig->getSettingsPath() + kBackupFolderName;
    pRequest->masterDbId = masterDbId();

    // The job runs in a background thread. It has no parent: the destruction
    // of this object must not delete a thread that runs. The job deletes
    // itself after `finished`.
    auto* pJob = new DeviceLibraryPlusExportJob(
            nullptr, m_pTrackCollectionManager, *pRequest);
    m_pJob = pJob;
    connect(pJob, &QThread::finished, pJob, &QObject::deleteLater);

    // The progress dialog is window-modal. It blocks the main window while the
    // job runs. The cancel button asks the job to stop. The job then ends
    // without a result.
    auto pProgressDlg = make_parented<QProgressDialog>(this);
    pProgressDlg->setWindowModality(Qt::WindowModal);
    pProgressDlg->setLabelText(tr("Exporting to the stick..."));
    pProgressDlg->setMinimumDuration(0);
    connect(pJob,
            &DeviceLibraryPlusExportJob::jobMaximum,
            pProgressDlg,
            &QProgressDialog::setMaximum);
    connect(pJob,
            &DeviceLibraryPlusExportJob::jobProgress,
            pProgressDlg,
            &QProgressDialog::setValue);
    connect(pJob, &QThread::finished, pProgressDlg, &QObject::deleteLater);
    const QPointer<QProgressDialog> pProgressGuard(pProgressDlg.get());
    connect(pProgressDlg,
            &QProgressDialog::canceled,
            pJob,
            [pJob, pProgressGuard]() {
                // Late progress signals must not show the dialog again.
                if (pProgressGuard) {
                    QObject::disconnect(pJob,
                            &DeviceLibraryPlusExportJob::jobMaximum,
                            pProgressGuard,
                            nullptr);
                    QObject::disconnect(pJob,
                            &DeviceLibraryPlusExportJob::jobProgress,
                            pProgressGuard,
                            nullptr);
                }
                pJob->slotCancel();
            });

    // The result. The progress dialog closes first, because it is modal.
    // The eject button gets the stick of this request: no other stick.
    // The volume and its identity are read now, before the export writes.
    const QString exportedStickRoot = pRequest->stickRoot;
    const QString exportedVolumeRoot = volumeRootOf(exportedStickRoot);
    const QString exportedIdentity =
            makeSystemStickEjector()->volumeIdentity(exportedVolumeRoot);
    connect(pJob,
            &DeviceLibraryPlusExportJob::completed,
            this,
            [this, pProgressGuard, exportedStickRoot, exportedVolumeRoot, exportedIdentity](const mixxx::DeviceLibraryPlusExportSummary& summary) {
                if (m_isClosing) {
                    return;
                }
                if (pProgressGuard) {
                    pProgressGuard->reset();
                    pProgressGuard->hide();
                }
                QString text = tr("%1 tracks and %2 playlists are on the stick. "
                                  "Eject the stick before you remove it.")
                                       .arg(summary.tracksExported)
                                       .arg(summary.playlistsExported);
                if (!summary.legacyLibraryError.isEmpty()) {
                    text += QStringLiteral("\n\n") +
                            tr("Mix Two did not write the Device Library. Reason: %1")
                                    .arg(summary.legacyLibraryError);
                }
                if (summary.keptReplacedFiles) {
                    text += QStringLiteral("\n\n") + tr("Mix Two keeps the replaced files on the stick.");
                }
                if (summary.legacyLibraryError.isEmpty() && summary.legacyClampedTracks > 0) {
                    text += QStringLiteral("\n\n") +
                            tr("Mix Two adjusts values for %n tracks to fit the Device Library.",
                                    nullptr,
                                    summary.legacyClampedTracks);
                }
                if (!summary.skipped.isEmpty()) {
                    text += QStringLiteral("\n\n") +
                            tr("Mix Two did not export %n tracks.", nullptr, summary.skipped.size());
                }
                QMessageBox box(QMessageBox::Information,
                        tr("Export completed"),
                        text,
                        QMessageBox::Ok,
                        window());
                const QStringList details = summary.skipped + summary.warnings;
                if (!details.isEmpty()) {
                    box.setDetailedText(details.join(QLatin1Char('\n')));
                }
                // AI-generated (Claude Code). Start of AI-generated text.
                QPushButton* pEjectButton = box.addButton(
                        tr("Eject the stick"), QMessageBox::ActionRole);
                // End of AI-generated text.
                box.exec();
                if (box.clickedButton() == pEjectButton) {
                    offerEject(exportedStickRoot, exportedVolumeRoot, exportedIdentity);
                }
            });
    connect(pJob,
            &DeviceLibraryPlusExportJob::failed,
            this,
            [this, pProgressGuard](const QString& message) {
                if (m_isClosing) {
                    return;
                }
                if (pProgressGuard) {
                    pProgressGuard->reset();
                    pProgressGuard->hide();
                }
                QMessageBox::critical(window(), tr("Export failed"), message);
            });

    pJob->start();
}

} // namespace mixxx
