#pragma once
// AI-generated (Claude Code). Start of AI-generated text.
// Logic for the playlist sidebar that a unit test can reach: drop of an
// Explorer folder on [Playlists], drag of playlists and folders into folders,
// rename in the tree. No widget code is in this file.

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "library/dao/playlistdao.h"

class TrackCollectionManager;

namespace playlistdrop {

/// A sidebar item that can be dragged or renamed.
enum class NodeKind {
    None,
    Playlist,
    Folder,
};

struct Node {
    NodeKind kind = NodeKind::None;
    int id = -1;

    bool operator==(const Node& other) const {
        return kind == other.kind && id == other.id;
    }
};

/// "playlist:<id>" or "folder:<id>". The text goes into the drag data.
QString nodeIdForPlaylist(int playlistId);
QString nodeIdForFolder(int folderId);
/// Returns a node of kind None when the text is not a node id.
Node parseNodeId(const QString& text);

/// The plain name of the item (without count and duration), or an empty string.
QString nodeName(const PlaylistDAO& dao, const Node& node);

/// True when the item can go into the folder now. kInvalidPlaylistFolderId
/// stands for the top level. False for the folder it is in already, for a folder
/// that would go into itself or below itself, and for unknown items.
bool canDropNode(const PlaylistDAO& dao, const Node& node, int targetFolderId);
bool dropPlaylistOnFolder(PlaylistDAO& dao, int playlistId, int targetFolderId);
/// Fails (false) when the target folder has a folder of the same name.
bool dropFolderOnFolder(PlaylistDAO& dao, int folderId, int targetFolderId);

enum class RenameResult {
    Ok,
    Blank,
    Locked,
    NameTaken,
};
/// Renames a playlist or a folder. The new name is trimmed.
RenameResult renameNode(PlaylistDAO& dao, const Node& node, const QString& newName);

/// The paths of the URLs that are existing local folders.
QStringList existingFolderPathsFromUrls(const QList<QUrl>& urls);

/// Makes a playlist at the top level. It has the name of the folder and the
/// tracks in the folder and below it, sorted by path. The tracks that are not
/// in the library go into the library. Returns kInvalidPlaylistId when the
/// folder holds no track.
int createPlaylistFromFolder(PlaylistDAO& dao,
        TrackCollectionManager* pTrackCollectionManager,
        const QString& folderPath);

} // namespace playlistdrop
// End of AI-generated text.
