#pragma once

#include <QPointer>
#include <QSharedPointer>
#include <QWidget>

#include "library/trackset/crate/crateid.h"
#include "preferences/usersettings.h"
#include "util/optional.h"
#include "util/parented_ptr.h"

class TrackCollectionManager;

namespace mixxx {

class DlgDeviceExport;
class DeviceLibraryPlusExportJob;
struct DeviceLibraryPlusExportRequest;

/// Mix Two: starts the export of Mix Two tracks to a stick (the Device Library
/// Plus of rekordbox). It shows the dialog, runs the job in a background
/// thread, shows the progress and shows the result. It is the counterpart of
/// LibraryExporter (the Engine DJ export), and it exists in every build.
class DeviceExporter : public QWidget {
    Q_OBJECT
  public:
    DeviceExporter(QWidget* parent,
            UserSettingsPointer pConfig,
            TrackCollectionManager* pTrackCollectionManager);

    /// Stops a job that still runs. The destructor asks the job to stop and
    /// then runs a local event loop until the job has finished, for 10 seconds
    /// at most (the job needs the event loop of this thread for its blocking
    /// calls). It never calls QThread::wait(). The job deletes itself after
    /// `finished`.
    ~DeviceExporter() override;

  public slots:
    /// Shows the dialog of the export.
    void slotRequestExport() {
        requestExportWithOptionalInitialSelection(std::nullopt, std::nullopt);
    }

    /// Shows the dialog of the export with a crate selected.
    void slotRequestExportWithInitialCrate(CrateId initialSelectedCrateId) {
        requestExportWithOptionalInitialSelection(
                std::make_optional(initialSelectedCrateId), std::nullopt);
    }

    /// Shows the dialog of the export with a playlist selected.
    void slotRequestExportWithInitialPlaylist(int initialSelectedPlaylistId) {
        requestExportWithOptionalInitialSelection(
                std::nullopt, std::make_optional(initialSelectedPlaylistId));
    }

  private slots:
    void beginExport(QSharedPointer<mixxx::DeviceLibraryPlusExportRequest> pRequest);

  private:
    void requestExportWithOptionalInitialSelection(
            std::optional<CrateId> initialSelectedCrateId,
            std::optional<int> initialSelectedPlaylistId);

    /// The library ID of Mix Two for the rows on the stick. The function makes
    /// it at the first export and keeps it in the settings.
    qint64 masterDbId();

    UserSettingsPointer m_pConfig;
    TrackCollectionManager* m_pTrackCollectionManager;
    parented_ptr<DlgDeviceExport> m_pDialog;
    /// The running job, else null. The job has no parent: it deletes itself
    /// after its `finished` signal.
    QPointer<DeviceLibraryPlusExportJob> m_pJob;
    /// True in the destructor. The result boxes are not shown then.
    bool m_isClosing = false;
};

} // namespace mixxx
