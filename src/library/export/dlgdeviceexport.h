#pragma once

#include <QDialog>
#include <QSharedPointer>
#include <QString>

#include "preferences/usersettings.h"
#include "util/optional.h"
#include "util/parented_ptr.h"

class TrackCollectionManager;
class CrateId;
class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QRadioButton;

namespace mixxx {

struct DeviceLibraryPlusExportRequest;

/// Mix Two: the dialog that asks the user what to export to a stick (the
/// rekordbox USB export): the stick, the tracks (the whole library or crates
/// and playlists) and the format. It is the Device Library Plus counterpart of
/// DlgLibraryExport (the Engine DJ dialog).
class DlgDeviceExport : public QDialog {
    Q_OBJECT

  public:
    DlgDeviceExport(
            QWidget* parent,
            UserSettingsPointer pConfig,
            TrackCollectionManager* pTrackCollectionManager);

    /// Selects the crate or the playlist in the lists and the mode "Selected
    /// crates and playlists". With neither, it selects the mode "Whole
    /// library". An unknown crate or playlist is ignored.
    void setInitialSelection(std::optional<CrateId> crateId, std::optional<int> playlistId);

    /// Reads the sticks, the crates and the playlists again.
    void refresh();

  signals:
    /// The user wants the export. The request has the stick, the crates and
    /// the playlists. The exporter that receives it adds the other fields.
    void startExport(QSharedPointer<mixxx::DeviceLibraryPlusExportRequest> pRequest);

  private slots:
    void stickComboActivated(int index);
    void updateStickInfo();
    void updateModeAndFormat();
    void exportRequested();

  private:
    /// Fills the stick combo and selects `wantedRoot` when it is in the list,
    /// else the first stick.
    void fillStickCombo(const QString& wantedRoot);
    QString currentStickRoot() const;
    void browseForStick();

    UserSettingsPointer m_pConfig;
    TrackCollectionManager* m_pTrackCollectionManager;

    /// The folder that the user chose with "Browse...", else empty.
    QString m_browsedFolder;
    /// The index in the stick combo before the user chose "Browse...".
    int m_previousStickIndex;

    parented_ptr<QComboBox> m_pStickCombo;
    parented_ptr<QLabel> m_pStickInfoLabel;
    parented_ptr<QRadioButton> m_pWholeLibraryRadio;
    parented_ptr<QRadioButton> m_pCratesAndPlaylistsRadio;
    parented_ptr<QListWidget> m_pCratesList;
    parented_ptr<QListWidget> m_pPlaylistsList;
    parented_ptr<QCheckBox> m_pPlusCheckBox;
    parented_ptr<QCheckBox> m_pLegacyCheckBox;
    parented_ptr<QPushButton> m_pExportButton;
};

} // namespace mixxx
