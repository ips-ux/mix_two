#include "library/trackset/playlistfeature.h"

#include <QApplication>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QSqlTableModel>
#include <QStyle>
#include <QtDebug>
#include <algorithm>

#include "library/library.h"
#include "library/parser.h"
#include "library/playlisttablemodel.h"
#include "library/queryutil.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/treeitem.h"
#include "moc_playlistfeature.cpp"
#include "sources/soundsourceproxy.h"
#include "util/db/dbconnection.h"
#include "util/dnd.h"
#include "util/duration.h"
#include "widget/wlibrary.h"
#include "widget/wlibrarysidebar.h"
#include "widget/wtracktableview.h"

PlaylistFeature::PlaylistFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : BasePlaylistFeature(pLibrary,
                  pConfig,
                  new PlaylistTableModel(nullptr,
                          pLibrary->trackCollectionManager(),
                          "mixxx.db.model.playlist"),
                  QStringLiteral("PLAYLISTHOME"),
                  QStringLiteral("playlist"),
                  QStringLiteral("PlaylistsCountsDurations")) {
    // construct child model
    std::unique_ptr<TreeItem> pRootItem = TreeItem::newRoot(this);
    m_pSidebarModel->setRootItem(std::move(pRootItem));
    constructChildModel(kInvalidPlaylistId);

    m_pShufflePlaylistAction = make_parented<QAction>(tr("Shuffle Playlist"), this);
    connect(m_pShufflePlaylistAction,
            &QAction::triggered,
            this,
            &PlaylistFeature::slotShufflePlaylist);

    m_pOrderByCurrentPosAction = make_parented<QAction>(tr("Adopt current order"), this);
    connect(m_pOrderByCurrentPosAction,
            &QAction::triggered,
            this,
            &PlaylistFeature::slotOrderTracksByCurrentPosition);

    m_pUnlockPlaylistsAction =
            make_parented<QAction>(tr("Unlock all playlists"), this);
    connect(m_pUnlockPlaylistsAction,
            &QAction::triggered,
            this,
            &PlaylistFeature::slotUnlockAllPlaylists);

    m_pDeleteAllUnlockedPlaylistsAction =
            make_parented<QAction>(tr("Delete all unlocked playlists"), this);
    connect(m_pDeleteAllUnlockedPlaylistsAction,
            &QAction::triggered,
            this,
            &PlaylistFeature::slotDeleteAllUnlockedPlaylists);

    // Folders, with rekordbox's wording.
    m_pCreateFolderAction = make_parented<QAction>(tr("Create New Folder"), this);
    connect(m_pCreateFolderAction,
            &QAction::triggered,
            this,
            &PlaylistFeature::slotCreateFolder);
    m_pRenameFolderAction = make_parented<QAction>(tr("Rename Folder"), this);
    connect(m_pRenameFolderAction,
            &QAction::triggered,
            this,
            &PlaylistFeature::slotRenameFolder);
    m_pDeleteFolderAction = make_parented<QAction>(tr("Delete Folder"), this);
    connect(m_pDeleteFolderAction,
            &QAction::triggered,
            this,
            &PlaylistFeature::slotDeleteFolder);
    connect(&m_playlistDao,
            &PlaylistDAO::foldersChanged,
            this,
            &PlaylistFeature::slotFoldersChanged);
}

QVariant PlaylistFeature::title() {
    return tr("Playlists");
}

void PlaylistFeature::onRightClick(const QPoint& globalPos) {
    m_lastRightClickedIndex = QModelIndex();
    QMenu menu(m_pSidebarWidget);
    menu.addAction(m_pCreatePlaylistAction);
    menu.addAction(m_pCreateFolderAction);
    menu.addSeparator();
    menu.addAction(m_pUnlockPlaylistsAction);
    menu.addAction(m_pDeleteAllUnlockedPlaylistsAction);
    menu.addSeparator();
    menu.addAction(m_pCreateImportPlaylistAction);
#ifdef __ENGINEPRIME__
    menu.addSeparator();
    menu.addAction(m_pExportAllPlaylistsToEngineAction);
#endif
    menu.exec(globalPos);
}

void PlaylistFeature::onRightClickChild(
        const QPoint& globalPos, const QModelIndex& index) {
    //Save the model index so we can get it in the action slots...
    m_lastRightClickedIndex = index;

    const int folderId = folderIdFromIndex(index);
    if (folderId != kInvalidPlaylistFolderId) {
        QMenu menu(m_pSidebarWidget);
        menu.addAction(m_pCreatePlaylistAction);
        menu.addAction(m_pCreateFolderAction);
        menu.addSeparator();
        menu.addAction(m_pRenameFolderAction);
        menu.addAction(m_pDeleteFolderAction);
        menu.addMenu(createMoveToFolderMenu(&menu, kInvalidPlaylistId, folderId));
        menu.exec(globalPos);
        return;
    }

    int playlistId = playlistIdFromIndex(index);

    bool locked = m_playlistDao.isPlaylistLocked(playlistId);
    m_pShufflePlaylistAction->setEnabled(!locked);
    m_pOrderByCurrentPosAction->setEnabled(!locked && isChildIndexSelectedInSidebar(index));
    m_pDeletePlaylistAction->setEnabled(!locked);
    m_pRenamePlaylistAction->setEnabled(!locked);
    m_pShufflePlaylistAction->setEnabled(!locked);
    m_pImportPlaylistAction->setEnabled(!locked);

    m_pLockPlaylistAction->setText(locked ? tr("Unlock") : tr("Lock"));

    QMenu menu(m_pSidebarWidget);
    menu.addAction(m_pCreatePlaylistAction);
    menu.addAction(m_pCreateFolderAction);
    menu.addSeparator();
    // TODO If playlist is selected and has more than one track selected
    // show "Shuffle selected tracks", else show "Shuffle playlist"?
    menu.addAction(m_pShufflePlaylistAction);
    menu.addAction(m_pOrderByCurrentPosAction);
    menu.addSeparator();
    menu.addAction(m_pRenamePlaylistAction);
    menu.addAction(m_pDuplicatePlaylistAction);
    menu.addAction(m_pDeletePlaylistAction);
    menu.addAction(m_pLockPlaylistAction);
    menu.addMenu(createMoveToFolderMenu(&menu, playlistId, kInvalidPlaylistFolderId));
    menu.addSeparator();
    menu.addAction(m_pAddToAutoDJAction);
    menu.addAction(m_pAddToAutoDJTopAction);
    menu.addAction(m_pAddToAutoDJReplaceAction);
    menu.addSeparator();
    menu.addAction(m_pAnalyzePlaylistAction);
    menu.addSeparator();
    menu.addAction(m_pImportPlaylistAction);
    menu.addAction(m_pExportPlaylistAction);
    menu.addAction(m_pExportTrackFilesAction);
#ifdef __ENGINEPRIME__
    menu.addAction(m_pExportPlaylistToEngineAction);
#endif
    menu.exec(globalPos);
}

bool PlaylistFeature::dropAcceptChild(
        const QModelIndex& index, const QList<QUrl>& urls, QObject* pSource) {
    int playlistId = playlistIdFromIndex(index);
    VERIFY_OR_DEBUG_ASSERT(playlistId != kInvalidPlaylistId) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(!m_playlistDao.isPlaylistLocked(playlistId)) {
        return false;
    }
    // If a track is dropped onto a playlist's name, but the track isn't in the
    // library, then add the track to the library before adding it to the
    // playlist.
    // pSource != nullptr it is a drop from inside Mixxx and indicates all
    // tracks already in the DB
    const QList<mixxx::FileInfo> fileInfos =
            // collect all tracks, accept playlist files
            DragAndDropHelper::supportedTracksFromUrls(urls, false, true);
    const QList<TrackId> trackIds =
            m_pLibrary->trackCollectionManager()->resolveTrackIds(fileInfos, pSource);
    if (trackIds.isEmpty()) {
        return false;
    }

    // Return whether appendTracksToPlaylist succeeded.
    return m_playlistDao.appendTracksToPlaylist(trackIds, playlistId);
}

bool PlaylistFeature::dragMoveAcceptChild(const QModelIndex& index, const QList<QUrl>& urls) {
    int playlistId = playlistIdFromIndex(index);
    if (playlistId == kInvalidPlaylistId) {
        return false;
    }
    if (m_playlistDao.isPlaylistLocked(playlistId)) {
        return false;
    }

    return DragAndDropHelper::urlsContainSupportedTrackFiles(urls, true);
}

QList<BasePlaylistFeature::IdAndLabel> PlaylistFeature::createPlaylistLabels() {
    QSqlDatabase database =
            m_pLibrary->trackCollectionManager()->internalCollection()->database();

    QList<BasePlaylistFeature::IdAndLabel> playlistLabels;
    QString queryString = QStringLiteral(
            "CREATE TEMPORARY VIEW IF NOT EXISTS %1 "
            "AS SELECT "
            "  Playlists.id AS id, "
            "  Playlists.name AS name, "
            "  LOWER(Playlists.name) AS sort_name, "
            "  COUNT(case library.mixxx_deleted when 0 then 1 else null end) "
            "    AS count, "
            "  SUM(case library.mixxx_deleted "
            "    when 0 then library.duration else 0 end) AS durationSeconds "
            "FROM Playlists "
            "LEFT JOIN PlaylistTracks "
            "  ON PlaylistTracks.playlist_id = Playlists.id "
            "LEFT JOIN library "
            "  ON PlaylistTracks.track_id = library.id "
            "  WHERE Playlists.hidden = %2 "
            "  GROUP BY Playlists.id")
                                  .arg(m_countsDurationTableName,
                                          QString::number(
                                                  PlaylistDAO::PLHT_NOT_HIDDEN));
    queryString.append(
            mixxx::DbConnection::collateLexicographically(
                    " ORDER BY sort_name"));
    QSqlQuery query(database);
    if (!query.exec(queryString)) {
        LOG_FAILED_QUERY(query);
    }

    // Setup the sidebar playlist model
    QSqlTableModel playlistTableModel(this, database);
    playlistTableModel.setTable(m_countsDurationTableName);
    playlistTableModel.select();
    while (playlistTableModel.canFetchMore()) {
        playlistTableModel.fetchMore();
    }
    QSqlRecord record = playlistTableModel.record();
    int nameColumn = record.indexOf("name");
    int idColumn = record.indexOf("id");
    int countColumn = record.indexOf("count");
    int durationColumn = record.indexOf("durationSeconds");

    for (int row = 0; row < playlistTableModel.rowCount(); ++row) {
        int id =
                playlistTableModel
                        .data(playlistTableModel.index(row, idColumn))
                        .toInt();
        QString name =
                playlistTableModel
                        .data(playlistTableModel.index(row, nameColumn))
                        .toString();
        int count =
                playlistTableModel
                        .data(playlistTableModel.index(row, countColumn))
                        .toInt();
        int duration =
                playlistTableModel
                        .data(playlistTableModel.index(row, durationColumn))
                        .toInt();
        BasePlaylistFeature::IdAndLabel idAndLabel;
        idAndLabel.id = id;
        idAndLabel.label = createPlaylistLabel(name, count, duration);
        playlistLabels.append(idAndLabel);
    }
    return playlistLabels;
}

void PlaylistFeature::slotShufflePlaylist() {
    int playlistId = playlistIdFromIndex(m_lastRightClickedIndex);
    if (playlistId == kInvalidPlaylistId) {
        return;
    }

    if (m_playlistDao.isPlaylistLocked(playlistId)) {
        qDebug() << "Can't shuffle locked playlist" << playlistId
                 << m_playlistDao.getPlaylistName(playlistId);
        return;
    }

    // Shuffle all tracks
    // If the playlist is loaded/visible shuffle only selected tracks
    if (isChildIndexSelectedInSidebar(m_lastRightClickedIndex) &&
            m_pPlaylistTableModel->getPlaylist() == playlistId) {
        QModelIndexList selection;
        if (m_pLibraryWidget) {
            WTrackTableView* view = dynamic_cast<WTrackTableView*>(
                    m_pLibraryWidget->getActiveView());
            if (view != nullptr) {
                selection = view->selectionModel()->selectedIndexes();
            }
        }
        m_pPlaylistTableModel->shuffleTracks(selection);
    } else {
        // Create a temp model so we don't need to select the playlist
        // in the persistent model in order to shuffle it
        std::unique_ptr<PlaylistTableModel> pPlaylistTableModel =
                std::make_unique<PlaylistTableModel>(this,
                        m_pLibrary->trackCollectionManager(),
                        "mixxx.db.model.playlist_shuffle");
        pPlaylistTableModel->selectPlaylist(playlistId);
        pPlaylistTableModel->setSort(
                pPlaylistTableModel->fieldIndex(
                        ColumnCache::COLUMN_PLAYLISTTRACKSTABLE_POSITION),
                Qt::AscendingOrder);
        pPlaylistTableModel->select();

        pPlaylistTableModel->shuffleTracks();
    }
}

void PlaylistFeature::slotOrderTracksByCurrentPosition() {
    int playlistId = playlistIdFromIndex(m_lastRightClickedIndex);
    if (playlistId == kInvalidPlaylistId) {
        return;
    }

    if (m_playlistDao.isPlaylistLocked(playlistId)) {
        qDebug() << "Can't adopt current sorting for locked playlist" << playlistId
                 << m_playlistDao.getPlaylistName(playlistId);
        return;
    }
    // Note(ronso0) I propose to proceed only if the playlist is selected and loaded.
    // without playlist content visible we don't have a preview.
    if (!isChildIndexSelectedInSidebar(m_lastRightClickedIndex)) {
        return;
    }
    m_pPlaylistTableModel->orderTracksByCurrPos();
}

void PlaylistFeature::slotUnlockAllPlaylists() {
    m_playlistDao.setPlaylistsLockedByType(PlaylistDAO::PLHT_NOT_HIDDEN, false);
}

void PlaylistFeature::slotDeleteAllUnlockedPlaylists() {
    // Collect playlists to display the count
    const QList<QPair<int, QString>> playlists =
            m_playlistDao.getUnlockedPlaylists(PlaylistDAO::PLHT_NOT_HIDDEN);
    if (playlists.size() <= 0) {
        return;
    }

    QMessageBox::StandardButton btn = QMessageBox::question(nullptr,
            tr("Confirm Deletion"),
            tr("Do you really want to delete all unlocked playlists?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
    if (btn != QMessageBox::Yes) {
        return;
    }

    btn = QMessageBox::question(nullptr,
            tr("Confirm Deletion"),
            tr("Deleting %1 unlocked playlists.<br>"
               "This operation can not be undone!")
                    .arg(playlists.size()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
    if (btn != QMessageBox::Yes) {
        return;
    }

    QStringList ids;
    for (const auto& playlist : std::as_const(playlists)) {
        ids << QString::number(playlist.first);
    }

    m_playlistDao.deleteUnlockedPlaylists(std::move(ids));
}

/// Purpose: When inserting or removing playlists,
/// we require the sidebar model not to reset.
/// This method queries the database and does dynamic insertion
/// @param selectedId entry which should be selected
QModelIndex PlaylistFeature::constructChildModel(int selectedId) {
    // qDebug() << "PlaylistFeature::constructChildModel() id:" << selectedId;
    const QList<IdAndLabel> playlistLabels = createPlaylistLabels(); // sorted
    QList<PlaylistDAO::PlaylistFolder> folders = m_playlistDao.getFolders();
    const QHash<int, int> folderIdOfPlaylist =
            m_playlistDao.getPlaylistFolderIds(PlaylistDAO::PLHT_NOT_HIDDEN);

    QSet<int> folderIds;
    for (const auto& folder : std::as_const(folders)) {
        folderIds.insert(folder.id);
    }
    // Folders before playlists, both sorted by name, as in rekordbox.
    std::sort(folders.begin(),
            folders.end(),
            [](const PlaylistDAO::PlaylistFolder& a, const PlaylistDAO::PlaylistFolder& b) {
                return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
            });
    // Items whose folder no longer exists are shown at the top level.
    QHash<int, QList<PlaylistDAO::PlaylistFolder>> subfoldersOf;
    for (const auto& folder : std::as_const(folders)) {
        const int parentId = folderIds.contains(folder.parentId)
                ? folder.parentId
                : kInvalidPlaylistFolderId;
        subfoldersOf[parentId].append(folder);
    }
    QHash<int, QList<IdAndLabel>> playlistsIn;
    for (const auto& idAndLabel : playlistLabels) {
        const int folderId = folderIdOfPlaylist.value(idAndLabel.id, kInvalidPlaylistFolderId);
        playlistsIn[folderIds.contains(folderId) ? folderId : kInvalidPlaylistFolderId]
                .append(idAndLabel);
    }

    const QIcon folderIcon = QApplication::style()->standardIcon(QStyle::SP_DirIcon);
    const auto decoratePlaylist = [this](TreeItem* pItem, int playlistId) {
        pItem->setBold(m_playlistIdsOfSelectedTrack.contains(playlistId));
        decorateChild(pItem, playlistId);
    };
    // Builds the children of a folder item; guards against folder cycles.
    QSet<int> builtFolderIds;
    const auto addChildren = [&](const auto& self, TreeItem* pParent, int folderId) -> void {
        for (const auto& subfolder : subfoldersOf.value(folderId)) {
            if (builtFolderIds.contains(subfolder.id)) {
                continue;
            }
            builtFolderIds.insert(subfolder.id);
            TreeItem* pFolderItem = pParent->appendChild(
                    subfolder.name, QStringLiteral("folder:%1").arg(subfolder.id));
            pFolderItem->setIcon(folderIcon);
            self(self, pFolderItem, subfolder.id);
        }
        for (const auto& idAndLabel : playlistsIn.value(folderId)) {
            TreeItem* pItem = pParent->appendChild(idAndLabel.label, idAndLabel.id);
            decoratePlaylist(pItem, idAndLabel.id);
        }
    };

    // The top-level items, whose parent is the invisible root item.
    std::vector<std::unique_ptr<TreeItem>> childrenToAdd;
    for (const auto& folder : subfoldersOf.value(kInvalidPlaylistFolderId)) {
        builtFolderIds.insert(folder.id);
        auto pFolderItem = std::make_unique<TreeItem>(
                folder.name, QStringLiteral("folder:%1").arg(folder.id));
        pFolderItem->setIcon(folderIcon);
        addChildren(addChildren, pFolderItem.get(), folder.id);
        childrenToAdd.push_back(std::move(pFolderItem));
    }
    for (const auto& idAndLabel : playlistsIn.value(kInvalidPlaylistFolderId)) {
        auto pItem = std::make_unique<TreeItem>(idAndLabel.label, idAndLabel.id);
        decoratePlaylist(pItem.get(), idAndLabel.id);
        childrenToAdd.push_back(std::move(pItem));
    }

    // Append all the newly created TreeItems in a dynamic way to the childmodel
    m_pSidebarModel->insertTreeItemRows(std::move(childrenToAdd), 0);
    if (selectedId == kInvalidPlaylistId) {
        return QModelIndex();
    }
    return indexFromPlaylistId(selectedId);
}

int PlaylistFeature::folderIdFromIndex(const QModelIndex& index) {
    const TreeItem* pItem = static_cast<TreeItem*>(index.internalPointer());
    if (!pItem) {
        return kInvalidPlaylistFolderId;
    }
    const QString data = pItem->getData().toString();
    if (!data.startsWith(QLatin1String("folder:"))) {
        return kInvalidPlaylistFolderId;
    }
    bool ok = false;
    const int folderId = data.mid(7).toInt(&ok);
    return ok ? folderId : kInvalidPlaylistFolderId;
}

int PlaylistFeature::targetFolderIdFromIndex(const QModelIndex& index) const {
    if (!index.isValid()) {
        return kInvalidPlaylistFolderId;
    }
    const int folderId = folderIdFromIndex(index);
    if (folderId != kInvalidPlaylistFolderId) {
        return folderId;
    }
    const int playlistId = playlistIdFromIndex(index);
    return playlistId != kInvalidPlaylistId
            ? m_playlistDao.getPlaylistFolderId(playlistId)
            : kInvalidPlaylistFolderId;
}

int PlaylistFeature::folderIdForNewPlaylist() const {
    return targetFolderIdFromIndex(m_lastRightClickedIndex);
}

QModelIndex PlaylistFeature::indexFromFolderId(int folderId) const {
    const QModelIndexList results = m_pSidebarModel->match(
            m_pSidebarModel->getRootIndex(),
            TreeItemModel::kDataRole,
            QVariant(QStringLiteral("folder:%1").arg(folderId)),
            1,
            Qt::MatchWrap | Qt::MatchExactly | Qt::MatchRecursive);
    return results.isEmpty() ? QModelIndex() : results.front();
}

QSet<int> PlaylistFeature::expandedFolderIds() const {
    QSet<int> folderIds;
    if (!m_pSidebarWidget) {
        return folderIds;
    }
    const auto collect = [&](const auto& self, const QModelIndex& parent) -> void {
        for (int row = 0; row < m_pSidebarModel->rowCount(parent); ++row) {
            const QModelIndex index = m_pSidebarModel->index(row, 0, parent);
            const int folderId = folderIdFromIndex(index);
            if (folderId == kInvalidPlaylistFolderId) {
                continue;
            }
            if (m_pSidebarWidget->isChildIndexExpanded(index)) {
                folderIds.insert(folderId);
            }
            self(self, index);
        }
    };
    collect(collect, QModelIndex());
    return folderIds;
}

void PlaylistFeature::expandFolders(const QSet<int>& folderIds) {
    if (!m_pSidebarWidget) {
        return;
    }
    for (const int folderId : folderIds) {
        const QModelIndex index = indexFromFolderId(folderId);
        if (index.isValid()) {
            m_pSidebarWidget->setChildIndexExpanded(index, true);
        }
    }
}

void PlaylistFeature::rebuildChildModel(int selectedPlaylistId) {
    const QSet<int> expanded = expandedFolderIds();
    clearChildModel();
    const QModelIndex newIndex = constructChildModel(selectedPlaylistId);
    expandFolders(expanded);
    if (selectedPlaylistId != kInvalidPlaylistId && newIndex.isValid()) {
        // If a child index was selected and we got a new valid index select that.
        // Else (root item was selected or for some reason no index could be created)
        // there's nothing to do: either no child was selected earlier, or the root
        // was selected and will remain selected after the child model was rebuilt.
        selectAndActivate(newIndex);
    }
}

void PlaylistFeature::slotFoldersChanged() {
    int selectedPlaylistId = kInvalidPlaylistId;
    if (isChildIndexSelectedInSidebar(m_lastClickedIndex)) {
        selectedPlaylistId = playlistIdFromIndex(m_lastClickedIndex);
    }
    rebuildChildModel(selectedPlaylistId);
}

QMenu* PlaylistFeature::createMoveToFolderMenu(QWidget* pParent, int playlistId, int folderId) {
    auto* pMenu = new QMenu(tr("Move to Folder"), pParent);
    const int currentParentId = playlistId != kInvalidPlaylistId
            ? m_playlistDao.getPlaylistFolderId(playlistId)
            : [&] {
                  const QList<PlaylistDAO::PlaylistFolder> folders = m_playlistDao.getFolders();
                  for (const auto& folder : folders) {
                      if (folder.id == folderId) {
                          return folder.parentId;
                      }
                  }
                  return kInvalidPlaylistFolderId;
              }();
    // A folder cannot move into itself or one of its subfolders.
    const QSet<int> excluded = folderId != kInvalidPlaylistFolderId
            ? m_playlistDao.getFolderAndSubfolderIds(folderId)
            : QSet<int>();

    const auto moveTo = [this, playlistId, folderId](int targetFolderId) {
        if (playlistId != kInvalidPlaylistId) {
            m_playlistDao.setPlaylistFolder(playlistId, targetFolderId);
        } else if (!m_playlistDao.moveFolder(folderId, targetFolderId)) {
            QMessageBox::warning(nullptr,
                    tr("Moving Folder Failed"),
                    tr("A folder by that name already exists there."));
        }
    };

    QAction* pTopLevel = pMenu->addAction(tr("Top Level"));
    pTopLevel->setEnabled(currentParentId != kInvalidPlaylistFolderId);
    connect(pTopLevel, &QAction::triggered, this, [moveTo] {
        moveTo(kInvalidPlaylistFolderId);
    });

    // Every folder, listed by its path.
    QList<PlaylistDAO::PlaylistFolder> folders = m_playlistDao.getFolders();
    QHash<int, PlaylistDAO::PlaylistFolder> folderById;
    for (const auto& folder : std::as_const(folders)) {
        folderById.insert(folder.id, folder);
    }
    QList<QPair<QString, int>> paths;
    for (const auto& folder : std::as_const(folders)) {
        if (excluded.contains(folder.id)) {
            continue;
        }
        QStringList names{folder.name};
        QSet<int> visited{folder.id};
        int parentId = folder.parentId;
        while (folderById.contains(parentId) && !visited.contains(parentId)) {
            visited.insert(parentId);
            names.prepend(folderById.value(parentId).name);
            parentId = folderById.value(parentId).parentId;
        }
        paths.append({names.join(QStringLiteral(" / ")), folder.id});
    }
    std::sort(paths.begin(), paths.end(), [](const auto& a, const auto& b) {
        return a.first.compare(b.first, Qt::CaseInsensitive) < 0;
    });
    if (!paths.isEmpty()) {
        pMenu->addSeparator();
    }
    for (const auto& [path, targetFolderId] : std::as_const(paths)) {
        QAction* pAction = pMenu->addAction(path);
        pAction->setEnabled(targetFolderId != currentParentId);
        connect(pAction, &QAction::triggered, this, [moveTo, id = targetFolderId] {
            moveTo(id);
        });
    }
    return pMenu;
}

void PlaylistFeature::slotCreateFolder() {
    const int parentFolderId = targetFolderIdFromIndex(m_lastRightClickedIndex);
    QString name;
    while (true) {
        bool ok = false;
        name = QInputDialog::getText(nullptr,
                tr("Create New Folder"),
                tr("Enter name for new folder:"),
                QLineEdit::Normal,
                tr("Untitled Folder"),
                &ok)
                       .trimmed();
        if (!ok) {
            return;
        }
        if (name.isEmpty()) {
            QMessageBox::warning(nullptr,
                    tr("Folder Creation Failed"),
                    tr("A folder cannot have a blank name."));
        } else if (m_playlistDao.getFolderIdFromName(name, parentFolderId) !=
                kInvalidPlaylistFolderId) {
            QMessageBox::warning(nullptr,
                    tr("Folder Creation Failed"),
                    tr("A folder by that name already exists here."));
        } else {
            break;
        }
    }
    const int folderId = m_playlistDao.createFolder(name, parentFolderId);
    if (folderId == kInvalidPlaylistFolderId) {
        QMessageBox::warning(nullptr,
                tr("Folder Creation Failed"),
                tr("An unknown error occurred while creating folder: ") + name);
        return;
    }
    // Show the new folder.
    const QModelIndex index = indexFromFolderId(folderId);
    if (m_pSidebarWidget && index.isValid()) {
        m_pSidebarWidget->selectChildIndex(index, false);
    }
}

void PlaylistFeature::slotRenameFolder() {
    const int folderId = folderIdFromIndex(m_lastRightClickedIndex);
    if (folderId == kInvalidPlaylistFolderId) {
        return;
    }
    const QString oldName = m_playlistDao.getFolderName(folderId);
    while (true) {
        bool ok = false;
        const QString newName = QInputDialog::getText(nullptr,
                tr("Rename Folder"),
                tr("Enter new name for folder:"),
                QLineEdit::Normal,
                oldName,
                &ok)
                                        .trimmed();
        if (!ok || newName == oldName) {
            return;
        }
        if (newName.isEmpty()) {
            QMessageBox::warning(nullptr,
                    tr("Renaming Folder Failed"),
                    tr("A folder cannot have a blank name."));
        } else if (!m_playlistDao.renameFolder(folderId, newName)) {
            QMessageBox::warning(nullptr,
                    tr("Renaming Folder Failed"),
                    tr("A folder by that name already exists here."));
        } else {
            return;
        }
    }
}

void PlaylistFeature::slotDeleteFolder() {
    const int folderId = folderIdFromIndex(m_lastRightClickedIndex);
    if (folderId == kInvalidPlaylistFolderId) {
        return;
    }
    const QString name = m_playlistDao.getFolderName(folderId);
    const QList<int> playlistIds = m_playlistDao.getPlaylistIdsInFolder(folderId);
    const bool hasLockedPlaylist = std::any_of(playlistIds.cbegin(),
            playlistIds.cend(),
            [this](int playlistId) { return m_playlistDao.isPlaylistLocked(playlistId); });
    if (hasLockedPlaylist) {
        QMessageBox::warning(nullptr,
                tr("Deleting Folder Failed"),
                tr("The folder \"%1\" contains locked playlists. "
                   "Unlock them before deleting the folder.")
                        .arg(name));
        return;
    }

    const QString question = playlistIds.isEmpty()
            ? tr("Do you really want to delete the folder \"%1\"?").arg(name)
            : tr("Do you really want to delete the folder \"%1\" and the %2 "
                 "playlist(s) in it?<br>This operation can not be undone!")
                      .arg(name, QString::number(playlistIds.size()));
    const QMessageBox::StandardButton btn = QMessageBox::question(nullptr,
            tr("Confirm Deletion"),
            question,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
    if (btn != QMessageBox::Yes) {
        return;
    }
    m_playlistDao.deleteFolder(folderId);
}

void PlaylistFeature::renameItem(const QModelIndex& index) {
    if (folderIdFromIndex(index) != kInvalidPlaylistFolderId) {
        m_lastRightClickedIndex = index;
        slotRenameFolder();
        return;
    }
    BasePlaylistFeature::renameItem(index);
}

void PlaylistFeature::deleteItem(const QModelIndex& index) {
    if (folderIdFromIndex(index) != kInvalidPlaylistFolderId) {
        m_lastRightClickedIndex = index;
        slotDeleteFolder();
        return;
    }
    BasePlaylistFeature::deleteItem(index);
}

void PlaylistFeature::decorateChild(TreeItem* item, int playlistId) {
    if (m_playlistDao.isPlaylistLocked(playlistId)) {
        item->setIcon(
                QIcon(":/images/library/ic_library_locked_tracklist.svg"));
    } else {
        item->setIcon(QIcon());
    }
}

void PlaylistFeature::slotPlaylistTableChanged(int playlistId) {
    // qDebug() << "PlaylistFeature::slotPlaylistTableChanged() playlistId:" << playlistId;
    enum PlaylistDAO::HiddenType type = m_playlistDao.getHiddenType(playlistId);
    if (type != PlaylistDAO::PLHT_NOT_HIDDEN &&  // not a regular playlist
            type != PlaylistDAO::PLHT_UNKNOWN) { // not a deleted playlist
        return;
    }

    // Store current selection
    int selectedPlaylistId = kInvalidPlaylistId;
    if (isChildIndexSelectedInSidebar(m_lastClickedIndex)) {
        if (playlistId == playlistIdFromIndex(m_lastClickedIndex) &&
                type == PlaylistDAO::PLHT_UNKNOWN) {
            // if the selected playlist was deleted, find a sibling to select
            selectedPlaylistId = getSiblingPlaylistIdOf(m_lastClickedIndex);
        } else {
            // just restore the current selection
            selectedPlaylistId = playlistIdFromIndex(m_lastClickedIndex);
        }
    }

    rebuildChildModel(selectedPlaylistId);
}

void PlaylistFeature::slotPlaylistContentOrLockChanged(const QSet<int>& playlistIds) {
    // qDebug() << "PlaylistFeature::slotPlaylistContentOrLockChanged() playlistId:" << playlistIds;
    QSet<int> idsToBeUpdated;
    for (const auto playlistId : std::as_const(playlistIds)) {
        if (m_playlistDao.getHiddenType(playlistId) == PlaylistDAO::PLHT_NOT_HIDDEN) {
            idsToBeUpdated.insert(playlistId);
        }
    }
    // Update the playlists set to allow toggling bold correctly after
    // tracks have been dropped on sidebar items
    m_playlistDao.getPlaylistsTrackIsIn(m_selectedTrackId, &m_playlistIdsOfSelectedTrack);
    updateChildModel(idsToBeUpdated);
}

void PlaylistFeature::slotPlaylistTableRenamed(int playlistId, const QString& newName) {
    Q_UNUSED(newName);
    // qDebug() << "PlaylistFeature::slotPlaylistTableRenamed() playlistId:" << playlistId;
    if (m_playlistDao.getHiddenType(playlistId) == PlaylistDAO::PLHT_NOT_HIDDEN) {
        // Maybe we need to re-sort the sidebar items, so call slotPlaylistTableChanged()
        // in order to rebuild the model, not just updateChildModel()
        slotPlaylistTableChanged(playlistId);
    }
}

QString PlaylistFeature::getRootViewHtml() const {
    QString playlistsTitle = tr("Playlists");
    QString playlistsSummary =
            tr("Playlists are ordered lists of tracks that allow you to plan "
               "your DJ sets.");
    QString playlistsSummary2 =
            tr("Some DJs construct playlists before they perform live, but "
               "others prefer to build them on-the-fly.");
    QString playlistsSummary3 =
            tr("When using a playlist during a live DJ set, remember to always "
               "pay close attention to how your audience reacts to the music "
               "you've chosen to play.");
    QString playlistsSummary4 =
            tr("It may be necessary to skip some tracks in your prepared "
               "playlist or add some different tracks in order to maintain the "
               "energy of your audience.");
    QString createPlaylistLink = tr("Create New Playlist");

    QString html;
    html.append(QStringLiteral("<h2>%1</h2>").arg(playlistsTitle));
    html.append(QStringLiteral("<p>%1</p>").arg(playlistsSummary));
    html.append(QStringLiteral("<p>%1</p>").arg(playlistsSummary2));
    html.append(QStringLiteral("<p>%1<br>%2</p>").arg(playlistsSummary3, playlistsSummary4));
    html.append(QStringLiteral("<a style=\"color:#0496FF;\" href=\"create\">%1</a>")
                        .arg(createPlaylistLink));
    return html;
}
