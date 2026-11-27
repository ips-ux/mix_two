// This feature reads tracks, playlists and folders from removable Recordbox
// prepared devices (USB drives, etc), by parsing the binary *.PDB files
// stored on each removable device. It does not read the locally stored
// Rekordbox database (Collection).

// It draws heavily from the hard work completed here:

//      https://github.com/Deep-Symmetry/crate-digger

// And uses the C++ Kaitai Struct binary parsing libraries:

//      http://kaitai.io
//      https://github.com/kaitai-io/kaitai_struct
//      https://github.com/kaitai-io/kaitai_struct_cpp_stl_runtime

// The *.PDB C++ files:

//      rekordbox_pdb.h
//      rekordbox_pdb.cpp

// Were generated from the following structure definition file:

//      https://github.com/Deep-Symmetry/crate-digger/blob/master/src/main/kaitai/rekordbox_pdb.ksy

#pragma once

#include <QAction>
#include <QHash>
#include <QSet>
#include <QFutureWatcher>
#include <QPointer>
#include <QWidget>
#include <atomic>

#include "library/baseexternallibraryfeature.h"
#include "library/baseexternalplaylistmodel.h"
#include "library/baseexternaltrackmodel.h"
#include "library/treeitemmodel.h"
#include "library/rekordbox/stickplaylistdelete.h"
#include "library/rekordbox/stickwatcher.h"
#include "util/parented_ptr.h"

class TrackCollectionManager;
class BaseExternalPlaylistModel;
namespace mixxx::rekordbox {
class StickWatcherFeature;
}

class RekordboxPlaylistModel : public BaseExternalPlaylistModel {
    Q_OBJECT
  public:
    RekordboxPlaylistModel(QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            QSharedPointer<BaseTrackCache> trackSource);
    TrackPointer getTrack(const QModelIndex& index) const override;
    bool isColumnHiddenByDefault(int column) override;
    bool isColumnInternal(int column) override;

  protected:
    void initSortColumnMapping() override;
};

class RekordboxFeature : public BaseExternalLibraryFeature {
    Q_OBJECT
  public:
    RekordboxFeature(Library* pLibrary, UserSettingsPointer pConfig);
    ~RekordboxFeature() override;

    QVariant title() override;
    static bool isSupported();
    void bindLibraryWidget(WLibrary* libraryWidget,
            KeyboardEventFilter* keyboard) override;

    TreeItemModel* sidebarModel() const override;

  public slots:
    void activate() override;
    void activateChild(const QModelIndex& index) override;
    void refreshLibraryModels();

  private slots:
    void htmlLinkClicked(const QUrl& link);
    // AI-generated (Claude Code). Start of AI-generated text.
    /// "Delete from stick" of the sidebar menu.
    void slotDeleteFromStick();
    // End of AI-generated text.

  protected:
    // AI-generated (Claude Code). Start of AI-generated text.
    void addMenuActions(QMenu* pMenu, const QModelIndex& index) override;
    // End of AI-generated text.

  private:
    friend class mixxx::rekordbox::StickWatcherFeature;

    QString formatRootViewHtml() const;
    std::unique_ptr<BaseSqlTableModel> createPlaylistModelForPlaylist(
            const QVariant& data) override;

    parented_ptr<TreeItemModel> m_pSidebarModel;
    parented_ptr<RekordboxPlaylistModel> m_pRekordboxPlaylistModel;

    void probeVolume(const QString& root, int retriesLeft);
    void removeVolume(const QString& root);
    QModelIndex deviceIndex(const QString& root) const;
    void removeDevice(const QString& root);
    void showDevicePlaylist(const QString& root, const QString& playlist);
    void startNextDatabaseJob();
    void onDatabaseJobFinished();
    // AI-generated (Claude Code). Start of AI-generated text.
    /// Tells the user once for each stick that its Device Library Plus cannot
    /// be read.
    void showPlusReadError(const QString& root, const QString& reason);
    /// The state of the "Delete from stick" item of the sidebar menu.
    struct DeleteFromStickMenuState {
        /// False when the item is not a playlist of a stick.
        bool visible = false;
        bool enabled = false;
        /// Tells why the item is disabled.
        QString toolTip;
    };
    DeleteFromStickMenuState deleteFromStickMenuState(const QModelIndex& index) const;
    /// The second question and the delete, after the preview of the worker.
    void continueStickDelete(const mixxx::rekordbox::StickDeleteRequest& request,
            const mixxx::rekordbox::DlpDeletePlaylistResult& preview,
            const QString& label);
    /// Reads the stick at `root` again, after it changed.
    void reloadStick(const QString& root);
    // End of AI-generated text.

    mixxx::rekordbox::StickWatcher m_stickWatcher;
    QHash<QString, QFutureWatcher<mixxx::rekordbox::StickFormats>*> m_probes;
    QHash<QString, mixxx::rekordbox::StickFormats> m_formats;
    QSet<QString> m_pendingCleanup;
    QString m_pendingOpen;
    QString m_importRoot;
    QString m_modelRoot;
    bool m_modelVisible = false;
    bool m_databaseBusy = false;
    std::shared_ptr<std::atomic_bool> m_importCanceled;
    QFutureWatcher<std::shared_ptr<TreeItem>> m_databaseJob;
    // AI-generated (Claude Code). Start of AI-generated text.
    /// The worker writes the reason why the Device Library Plus cannot be read.
    std::shared_ptr<QString> m_importPlusError;
    /// For each connected stick: the reason that showPlusReadError() showed.
    QHash<QString, QString> m_plusReadErrors;
    QPointer<QWidget> m_pLibraryWidget;
    parented_ptr<QAction> m_pDeleteFromStickAction;
    /// True while a delete from a stick runs. Only one runs at a time.
    bool m_stickDeleteBusy = false;
    // End of AI-generated text.

    QSharedPointer<BaseTrackCache> m_trackSource;
};
