// AI-generated (Claude Code). Start of AI-generated text.
#include "library/trackset/playlistdrop.h"

#include <QDir>
#include <QFileInfo>
#include <algorithm>

#include "library/libraryprotection.h"
#include "library/trackcollectionmanager.h"
#include "util/dnd.h"
#include "util/fileinfo.h"

namespace playlistdrop {

namespace {

const QString kPlaylistPrefix = QStringLiteral("playlist:");
const QString kFolderPrefix = QStringLiteral("folder:");

int parseId(const QString& text) {
    bool ok = false;
    const int id = text.toInt(&ok);
    return ok && id >= 0 ? id : -1;
}

/// The folder a playlist or a folder is in, or kInvalidPlaylistFolderId.
int currentFolderOf(const PlaylistDAO& dao, const Node& node) {
    if (node.kind == NodeKind::Playlist) {
        return dao.getPlaylistFolderId(node.id);
    }
    const QList<PlaylistDAO::PlaylistFolder> folders = dao.getFolders();
    for (const auto& folder : folders) {
        if (folder.id == node.id) {
            return folder.parentId;
        }
    }
    return kInvalidPlaylistFolderId;
}

bool nodeExists(const PlaylistDAO& dao, const Node& node) {
    switch (node.kind) {
    case NodeKind::Playlist:
        return dao.playlistExists(node.id);
    case NodeKind::Folder:
        return !dao.getFolderName(node.id).isEmpty();
    case NodeKind::None:
        break;
    }
    return false;
}

} // namespace

QString nodeIdForPlaylist(int playlistId) {
    return kPlaylistPrefix + QString::number(playlistId);
}

QString nodeIdForFolder(int folderId) {
    return kFolderPrefix + QString::number(folderId);
}

Node parseNodeId(const QString& text) {
    if (text.startsWith(kPlaylistPrefix)) {
        const int id = parseId(text.mid(kPlaylistPrefix.size()));
        if (id >= 0) {
            return Node{NodeKind::Playlist, id};
        }
    } else if (text.startsWith(kFolderPrefix)) {
        const int id = parseId(text.mid(kFolderPrefix.size()));
        if (id >= 0) {
            return Node{NodeKind::Folder, id};
        }
    }
    return Node();
}

QString nodeName(const PlaylistDAO& dao, const Node& node) {
    switch (node.kind) {
    case NodeKind::Playlist:
        return dao.getPlaylistName(node.id);
    case NodeKind::Folder:
        return dao.getFolderName(node.id);
    case NodeKind::None:
        break;
    }
    return QString();
}

bool canDropNode(const PlaylistDAO& dao, const Node& node, int targetFolderId) {
    // Library Protection: playlists and folders stay where they are.
    if (!mixxx::library::isActionAllowed(mixxx::library::ProtectedAction::MoveTrackSet)) {
        return false;
    }
    if (!nodeExists(dao, node)) {
        return false;
    }
    if (currentFolderOf(dao, node) == targetFolderId) {
        return false;
    }
    if (node.kind == NodeKind::Folder &&
            dao.getFolderAndSubfolderIds(node.id).contains(targetFolderId)) {
        // A folder cannot go into itself or into a folder below it.
        return false;
    }
    return true;
}

bool dropPlaylistOnFolder(PlaylistDAO& dao, int playlistId, int targetFolderId) {
    if (!canDropNode(dao, Node{NodeKind::Playlist, playlistId}, targetFolderId)) {
        return false;
    }
    return dao.setPlaylistFolder(playlistId, targetFolderId);
}

bool dropFolderOnFolder(PlaylistDAO& dao, int folderId, int targetFolderId) {
    if (!canDropNode(dao, Node{NodeKind::Folder, folderId}, targetFolderId)) {
        return false;
    }
    return dao.moveFolder(folderId, targetFolderId);
}

RenameResult renameNode(PlaylistDAO& dao, const Node& node, const QString& newName) {
    // Library Protection: playlists and folders keep their names.
    if (!mixxx::library::isActionAllowed(mixxx::library::ProtectedAction::RenameTrackSet)) {
        return RenameResult::Protected;
    }
    const QString name = newName.trimmed();
    if (name.isEmpty()) {
        return RenameResult::Blank;
    }
    if (node.kind == NodeKind::Playlist) {
        if (dao.isPlaylistLocked(node.id)) {
            return RenameResult::Locked;
        }
        if (dao.getPlaylistName(node.id) == name) {
            return RenameResult::Ok;
        }
        const int existingId = dao.getPlaylistIdFromName(name);
        // Another playlist with the name; the playlist itself is no clash, so
        // a change of the letter case is fine.
        if (existingId != kInvalidPlaylistId && existingId != node.id) {
            return RenameResult::NameTaken;
        }
        dao.renamePlaylist(node.id, name);
        return RenameResult::Ok;
    }
    if (node.kind == NodeKind::Folder) {
        if (dao.getFolderName(node.id) == name) {
            return RenameResult::Ok;
        }
        return dao.renameFolder(node.id, name) ? RenameResult::Ok
                                               : RenameResult::NameTaken;
    }
    return RenameResult::Blank;
}

QStringList existingFolderPathsFromUrls(const QList<QUrl>& urls) {
    QStringList paths;
    for (const QUrl& url : urls) {
        const QString path = url.toLocalFile();
        if (!path.isEmpty() && QFileInfo(path).isDir()) {
            paths.append(path);
        }
    }
    return paths;
}

bool isNodeRenamable(const Node& node, const QSet<int>& lockedPlaylistIds) {
    // Library Protection: no editor in the tree.
    if (!mixxx::library::isActionAllowed(mixxx::library::ProtectedAction::RenameTrackSet)) {
        return false;
    }
    switch (node.kind) {
    case NodeKind::Playlist:
        return !lockedPlaylistIds.contains(node.id);
    case NodeKind::Folder:
        return true;
    case NodeKind::None:
        break;
    }
    return false;
}

int createPlaylistWithTracks(PlaylistDAO& dao, QString name, const QList<TrackId>& trackIds) {
    const int playlistId = dao.createUniquePlaylist(&name);
    if (playlistId == kInvalidPlaylistId) {
        return kInvalidPlaylistId;
    }
    if (!dao.appendTracksToPlaylist(trackIds, playlistId)) {
        dao.deletePlaylist(playlistId);
        return kInvalidPlaylistId;
    }
    return playlistId;
}

FolderImport createPlaylistFromFolder(PlaylistDAO& dao,
        TrackCollectionManager* pTrackCollectionManager,
        const QString& folderPath,
        int maxFiles) {
    FolderImport result;
    if (!pTrackCollectionManager) {
        return result;
    }
    // Choice: the tracks of the subfolders are in the playlist too, sorted by
    // path (the folder walk has no fixed order).
    QList<mixxx::FileInfo> fileInfos = DragAndDropHelper::supportedTracksFromUrls(
            {QUrl::fromLocalFile(folderPath)}, false, false);
    if (fileInfos.size() > maxFiles) {
        result.tooManyFiles = true;
        return result;
    }
    if (fileInfos.isEmpty()) {
        return result;
    }
    std::sort(fileInfos.begin(),
            fileInfos.end(),
            [](const mixxx::FileInfo& a, const mixxx::FileInfo& b) {
                return a.location().compare(b.location(), Qt::CaseInsensitive) < 0;
            });
    const QList<TrackId> trackIds =
            pTrackCollectionManager->resolveTrackIds(fileInfos, nullptr);
    if (trackIds.isEmpty()) {
        return result;
    }

    QString name = QFileInfo(folderPath).fileName();
    if (name.isEmpty()) {
        // The root of a drive, such as "E:/".
        name = QDir::toNativeSeparators(folderPath);
        while (name.endsWith(QDir::separator())) {
            name.chop(1);
        }
    }
    result.playlistId = createPlaylistWithTracks(dao, name, trackIds);
    if (result.playlistId != kInvalidPlaylistId) {
        result.trackCount = trackIds.size();
    }
    return result;
}

} // namespace playlistdrop
// End of AI-generated text.
