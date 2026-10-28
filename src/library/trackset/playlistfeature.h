#pragma once

#include <QModelIndex>
#include <QObject>
#include <QUrl>
#include <QVariant>

#include "library/trackset/baseplaylistfeature.h"
#include "preferences/usersettings.h"
#include "util/parented_ptr.h"

class TreeItem;
class QMenu;
class QPoint;
class QWidget;

class PlaylistFeature : public BasePlaylistFeature {
    Q_OBJECT

  public:
    PlaylistFeature(
            Library* pLibrary,
            UserSettingsPointer pConfig);
    ~PlaylistFeature() override = default;

    QVariant title() override;

    bool dropAcceptChild(const QModelIndex& index,
            const QList<QUrl>& urls,
            QObject* pSource) override;
    bool dragMoveAcceptChild(const QModelIndex& index, const QList<QUrl>& urls) override;

  public slots:
    void onRightClick(const QPoint& globalPos) override;
    void onRightClickChild(const QPoint& globalPos, const QModelIndex& index) override;
    void renameItem(const QModelIndex& index) override;
    void deleteItem(const QModelIndex& index) override;

  private slots:
    void slotPlaylistTableChanged(int playlistId) override;
    void slotPlaylistContentOrLockChanged(const QSet<int>& playlistIds) override;
    void slotPlaylistTableRenamed(int playlistId, const QString& newName) override;
    void slotShufflePlaylist();
    void slotOrderTracksByCurrentPosition();
    void slotUnlockAllPlaylists();
    void slotDeleteAllUnlockedPlaylists();
    void slotCreateFolder();
    void slotRenameFolder();
    void slotDeleteFolder();
    void slotFoldersChanged();

  protected:
    void decorateChild(TreeItem* pChild, int playlistId) override;
    int folderIdForNewPlaylist() const override;
    QList<IdAndLabel> createPlaylistLabels();
    QModelIndex constructChildModel(int selectedId);

  private:
    QString getRootViewHtml() const override;

    /// Sidebar items of folders carry "folder:<id>" so that they are never
    /// mistaken for playlists, whose items carry the playlist id.
    static int folderIdFromIndex(const QModelIndex& index);
    /// The folder an index is in, or is itself.
    int targetFolderIdFromIndex(const QModelIndex& index) const;
    QModelIndex indexFromFolderId(int folderId) const;
    /// Rebuilds the tree, keeping the selection and expanded folders.
    void rebuildChildModel(int selectedPlaylistId);
    QSet<int> expandedFolderIds() const;
    void expandFolders(const QSet<int>& folderIds);
    /// "Move to Folder" submenu for a playlist (playlistId) or a folder.
    QMenu* createMoveToFolderMenu(QWidget* pParent, int playlistId, int folderId);

    parented_ptr<QAction> m_pShufflePlaylistAction;
    parented_ptr<QAction> m_pOrderByCurrentPosAction;
    parented_ptr<QAction> m_pUnlockPlaylistsAction;
    parented_ptr<QAction> m_pDeleteAllUnlockedPlaylistsAction;
    parented_ptr<QAction> m_pCreateFolderAction;
    parented_ptr<QAction> m_pRenameFolderAction;
    parented_ptr<QAction> m_pDeleteFolderAction;
};
