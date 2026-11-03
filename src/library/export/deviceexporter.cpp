#include "library/export/deviceexporter.h"

#include <QEventLoop>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QRandomGenerator>
#include <QTimer>
#include <QtDebug>

#include "library/export/devicelibraryplusexportjob.h"
#include "library/export/devicelibraryplusexportrequest.h"
#include "library/export/dlgdeviceexport.h"
#include "library/rekordbox/devicelibraryplusstick.h"
#include "moc_deviceexporter.cpp"

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

void DeviceExporter::requestExportWithOptionalInitialSelection(
        std::optional<CrateId> initialSelectedCrateId,
        std::optional<int> initialSelectedPlaylistId) {
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
    connect(pJob,
            &DeviceLibraryPlusExportJob::completed,
            this,
            [this, pProgressGuard](const mixxx::DeviceLibraryPlusExportSummary& summary) {
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
                if (!summary.skipped.isEmpty()) {
                    text += QStringLiteral("\n\n") +
                            tr("%n tracks were not exported.", nullptr, summary.skipped.size());
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
                box.exec();
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
