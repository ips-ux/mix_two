#include <gtest/gtest.h>

#include <QDir>
#include <QSignalSpy>
#include <QUrl>

#include "library/dao/playlistdao.h"
#include "library/trackset/playlistdrop.h"
#include "test/librarytest.h"

namespace {

class PlaylistFoldersTest : public LibraryTest {
  protected:
    PlaylistDAO& dao() const {
        return internalCollection()->getPlaylistDAO();
    }
};

TEST_F(PlaylistFoldersTest, CreateNestedFolders) {
    const int house = dao().createFolder(QStringLiteral("House"));
    ASSERT_NE(kInvalidPlaylistFolderId, house);
    const int deep = dao().createFolder(QStringLiteral("Deep"), house);
    ASSERT_NE(kInvalidPlaylistFolderId, deep);

    EXPECT_EQ(house, dao().getFolderIdFromName(QStringLiteral("House"), kInvalidPlaylistFolderId));
    EXPECT_EQ(deep, dao().getFolderIdFromName(QStringLiteral("Deep"), house));
    // The name is unique among siblings only.
    EXPECT_EQ(kInvalidPlaylistFolderId,
            dao().getFolderIdFromName(QStringLiteral("Deep"), kInvalidPlaylistFolderId));
    EXPECT_EQ(QSet<int>({house, deep}), dao().getFolderAndSubfolderIds(house));

    const QList<PlaylistDAO::PlaylistFolder> folders = dao().getFolders();
    ASSERT_EQ(2, folders.size());
    for (const auto& folder : folders) {
        EXPECT_EQ(folder.id == deep ? house : kInvalidPlaylistFolderId, folder.parentId);
    }
}

TEST_F(PlaylistFoldersTest, FolderNamesAreUniqueAmongSiblings) {
    const int house = dao().createFolder(QStringLiteral("House"));
    EXPECT_EQ(kInvalidPlaylistFolderId, dao().createFolder(QStringLiteral("House")));
    EXPECT_EQ(kInvalidPlaylistFolderId, dao().createFolder(QStringLiteral("  ")));
    // The same name is fine in another folder.
    EXPECT_NE(kInvalidPlaylistFolderId, dao().createFolder(QStringLiteral("House"), house));

    const int techno = dao().createFolder(QStringLiteral("Techno"));
    EXPECT_FALSE(dao().renameFolder(techno, QStringLiteral("House")));
    EXPECT_TRUE(dao().renameFolder(techno, QStringLiteral("Techno 2026")));
    EXPECT_EQ(QStringLiteral("Techno 2026"), dao().getFolderName(techno));
}

TEST_F(PlaylistFoldersTest, PlaylistsMoveBetweenFolders) {
    const int folder = dao().createFolder(QStringLiteral("Sets"));
    const int playlist = dao().createPlaylist(QStringLiteral("Friday"));
    ASSERT_NE(kInvalidPlaylistId, playlist);
    EXPECT_EQ(kInvalidPlaylistFolderId, dao().getPlaylistFolderId(playlist));

    QSignalSpy spy(&dao(), &PlaylistDAO::foldersChanged);
    EXPECT_TRUE(dao().setPlaylistFolder(playlist, folder));
    EXPECT_EQ(1, spy.count());
    EXPECT_EQ(folder, dao().getPlaylistFolderId(playlist));
    EXPECT_EQ(folder, dao().getPlaylistFolderIds(PlaylistDAO::PLHT_NOT_HIDDEN).value(playlist));
    EXPECT_EQ(QList<int>({playlist}), dao().getPlaylistIdsInFolder(folder));

    EXPECT_TRUE(dao().setPlaylistFolder(playlist, kInvalidPlaylistFolderId));
    EXPECT_EQ(kInvalidPlaylistFolderId, dao().getPlaylistFolderId(playlist));
    EXPECT_TRUE(dao().getPlaylistFolderIds(PlaylistDAO::PLHT_NOT_HIDDEN).isEmpty());
}

TEST_F(PlaylistFoldersTest, FolderCannotMoveIntoItself) {
    const int outer = dao().createFolder(QStringLiteral("Outer"));
    const int inner = dao().createFolder(QStringLiteral("Inner"), outer);
    EXPECT_FALSE(dao().moveFolder(outer, outer));
    EXPECT_FALSE(dao().moveFolder(outer, inner));
    EXPECT_TRUE(dao().moveFolder(inner, kInvalidPlaylistFolderId));
    EXPECT_TRUE(dao().moveFolder(outer, inner));
    EXPECT_EQ(QSet<int>({inner, outer}), dao().getFolderAndSubfolderIds(inner));
}

TEST_F(PlaylistFoldersTest, DeleteFolderDeletesItsPlaylists) {
    const int folder = dao().createFolder(QStringLiteral("Old"));
    const int subfolder = dao().createFolder(QStringLiteral("Older"), folder);
    const int inFolder = dao().createPlaylist(QStringLiteral("A"));
    const int inSubfolder = dao().createPlaylist(QStringLiteral("B"));
    const int outside = dao().createPlaylist(QStringLiteral("C"));
    dao().setPlaylistFolder(inFolder, folder);
    dao().setPlaylistFolder(inSubfolder, subfolder);

    EXPECT_TRUE(dao().deleteFolder(folder));

    EXPECT_TRUE(dao().getFolders().isEmpty());
    EXPECT_FALSE(dao().playlistExists(inFolder));
    EXPECT_FALSE(dao().playlistExists(inSubfolder));
    EXPECT_TRUE(dao().playlistExists(outside));
}

TEST_F(PlaylistFoldersTest, DeleteFolderKeepsLockedPlaylists) {
    const int folder = dao().createFolder(QStringLiteral("Keep"));
    const int locked = dao().createPlaylist(QStringLiteral("Locked"));
    dao().setPlaylistFolder(locked, folder);
    dao().setPlaylistLocked(locked, true);

    EXPECT_FALSE(dao().deleteFolder(folder));

    EXPECT_EQ(1, dao().getFolders().size());
    EXPECT_TRUE(dao().playlistExists(locked));
}

// AI-generated (Claude Code). Start of AI-generated text.
// Drop of an Explorer folder on [Playlists], drag of playlists and folders in
// the sidebar, rename in the tree.
using namespace playlistdrop;

TEST_F(PlaylistFoldersTest, NodeIdsRoundTrip) {
    EXPECT_EQ(Node({NodeKind::Playlist, 12}), parseNodeId(nodeIdForPlaylist(12)));
    EXPECT_EQ(Node({NodeKind::Folder, 3}), parseNodeId(nodeIdForFolder(3)));
    EXPECT_EQ(NodeKind::None, parseNodeId(QString()).kind);
    EXPECT_EQ(NodeKind::None, parseNodeId(QStringLiteral("playlist:x")).kind);
    EXPECT_EQ(NodeKind::None, parseNodeId(QStringLiteral("crate:4")).kind);
}

TEST_F(PlaylistFoldersTest, DropPlaylistOnFolder) {
    const int folder = dao().createFolder(QStringLiteral("Sets"));
    const int playlist = dao().createPlaylist(QStringLiteral("Friday"));

    EXPECT_TRUE(canDropNode(dao(), parseNodeId(nodeIdForPlaylist(playlist)), folder));
    EXPECT_TRUE(dropPlaylistOnFolder(dao(), playlist, folder));
    EXPECT_EQ(folder, dao().getPlaylistFolderId(playlist));
    // Already there: nothing to do.
    EXPECT_FALSE(canDropNode(dao(), parseNodeId(nodeIdForPlaylist(playlist)), folder));
    // The top level takes it back.
    EXPECT_TRUE(canDropNode(dao(),
            parseNodeId(nodeIdForPlaylist(playlist)),
            kInvalidPlaylistFolderId));
    EXPECT_TRUE(dropPlaylistOnFolder(dao(), playlist, kInvalidPlaylistFolderId));
    EXPECT_EQ(kInvalidPlaylistFolderId, dao().getPlaylistFolderId(playlist));
    EXPECT_FALSE(canDropNode(dao(),
            parseNodeId(nodeIdForPlaylist(playlist)),
            kInvalidPlaylistFolderId));
}

TEST_F(PlaylistFoldersTest, DropFolderOnFolderNeverIntoItself) {
    const int outer = dao().createFolder(QStringLiteral("Outer"));
    const int inner = dao().createFolder(QStringLiteral("Inner"), outer);
    const int other = dao().createFolder(QStringLiteral("Other"));

    const Node outerNode = parseNodeId(nodeIdForFolder(outer));
    EXPECT_FALSE(canDropNode(dao(), outerNode, outer));
    EXPECT_FALSE(canDropNode(dao(), outerNode, inner));
    EXPECT_FALSE(dropFolderOnFolder(dao(), outer, outer));
    EXPECT_FALSE(dropFolderOnFolder(dao(), outer, inner));
    EXPECT_EQ(kInvalidPlaylistFolderId, dao().getFolders().value(0).parentId);

    EXPECT_TRUE(canDropNode(dao(), outerNode, other));
    EXPECT_TRUE(dropFolderOnFolder(dao(), outer, other));
    EXPECT_EQ(QSet<int>({outer, inner}), dao().getFolderAndSubfolderIds(outer));
    EXPECT_EQ(QSet<int>({other, outer, inner}), dao().getFolderAndSubfolderIds(other));
}

TEST_F(PlaylistFoldersTest, DropFolderWithTakenNameFails) {
    const int a = dao().createFolder(QStringLiteral("House"));
    const int b = dao().createFolder(QStringLiteral("Target"));
    dao().createFolder(QStringLiteral("House"), b);
    EXPECT_FALSE(dropFolderOnFolder(dao(), a, b));
}

TEST_F(PlaylistFoldersTest, UnknownNodesAreRefused) {
    const int folder = dao().createFolder(QStringLiteral("Sets"));
    EXPECT_FALSE(canDropNode(dao(), Node(), folder));
    EXPECT_FALSE(canDropNode(dao(), parseNodeId(nodeIdForPlaylist(9999)), folder));
    EXPECT_FALSE(canDropNode(dao(), parseNodeId(nodeIdForFolder(9999)), folder));
}

TEST_F(PlaylistFoldersTest, RenameNodes) {
    const int playlist = dao().createPlaylist(QStringLiteral("Friday"));
    dao().createPlaylist(QStringLiteral("Saturday"));
    const int folder = dao().createFolder(QStringLiteral("Sets"));
    dao().createFolder(QStringLiteral("Clubs"));

    const Node playlistNode = parseNodeId(nodeIdForPlaylist(playlist));
    EXPECT_EQ(RenameResult::Blank, renameNode(dao(), playlistNode, QStringLiteral("  ")));
    EXPECT_EQ(RenameResult::NameTaken, renameNode(dao(), playlistNode, QStringLiteral("Saturday")));
    EXPECT_EQ(RenameResult::Ok, renameNode(dao(), playlistNode, QStringLiteral(" Sunday ")));
    EXPECT_EQ(QStringLiteral("Sunday"), dao().getPlaylistName(playlist));
    EXPECT_EQ(QStringLiteral("Sunday"), nodeName(dao(), playlistNode));
    dao().setPlaylistLocked(playlist, true);
    EXPECT_EQ(RenameResult::Locked, renameNode(dao(), playlistNode, QStringLiteral("Monday")));
    EXPECT_EQ(QStringLiteral("Sunday"), dao().getPlaylistName(playlist));

    const Node folderNode = parseNodeId(nodeIdForFolder(folder));
    EXPECT_EQ(RenameResult::Blank, renameNode(dao(), folderNode, QString()));
    EXPECT_EQ(RenameResult::NameTaken, renameNode(dao(), folderNode, QStringLiteral("Clubs")));
    EXPECT_EQ(RenameResult::Ok, renameNode(dao(), folderNode, QStringLiteral("Gigs")));
    EXPECT_EQ(QStringLiteral("Gigs"), nodeName(dao(), folderNode));
}

TEST_F(PlaylistFoldersTest, DroppedFolderMakesPlaylistNamedAfterIt) {
    const QDir root = getTestDataDir();
    ASSERT_TRUE(root.mkpath(QStringLiteral("Friday Warmup/Extra")));
    const QString source = getTestDir().filePath(QStringLiteral("sine-30.wav"));
    const QStringList copies = {QStringLiteral("Friday Warmup/b.wav"),
            QStringLiteral("Friday Warmup/a.wav"),
            QStringLiteral("Friday Warmup/Extra/c.wav")};
    for (const QString& copy : copies) {
        ASSERT_TRUE(QFile::copy(source, root.filePath(copy)));
    }
    // A file that is not a track is ignored.
    QFile note(root.filePath(QStringLiteral("Friday Warmup/notes.txt")));
    ASSERT_TRUE(note.open(QIODevice::WriteOnly));
    note.write("x");
    note.close();

    const QString folderPath = root.filePath(QStringLiteral("Friday Warmup"));
    EXPECT_EQ(QStringList({folderPath}),
            existingFolderPathsFromUrls({QUrl::fromLocalFile(folderPath),
                    QUrl::fromLocalFile(root.filePath(copies.first()))}));

    const int playlistId = createPlaylistFromFolder(
            dao(), trackCollectionManager(), folderPath);
    ASSERT_NE(kInvalidPlaylistId, playlistId);
    EXPECT_EQ(QStringLiteral("Friday Warmup"), dao().getPlaylistName(playlistId));
    EXPECT_EQ(3, dao().getTrackIds(playlistId).size());
    EXPECT_EQ(kInvalidPlaylistFolderId, dao().getPlaylistFolderId(playlistId));

    // A second drop of the same folder gets a unique name.
    const int second = createPlaylistFromFolder(
            dao(), trackCollectionManager(), folderPath);
    ASSERT_NE(kInvalidPlaylistId, second);
    EXPECT_NE(second, playlistId);
    EXPECT_NE(QStringLiteral("Friday Warmup"), dao().getPlaylistName(second));
}

TEST_F(PlaylistFoldersTest, DroppedFolderWithoutTracksMakesNoPlaylist) {
    const QDir root = getTestDataDir();
    ASSERT_TRUE(root.mkpath(QStringLiteral("Empty")));
    const QString folderPath = root.filePath(QStringLiteral("Empty"));
    EXPECT_EQ(kInvalidPlaylistId,
            createPlaylistFromFolder(dao(), trackCollectionManager(), folderPath));
    EXPECT_FALSE(dao().getPlaylistIdFromName(QStringLiteral("Empty")) != kInvalidPlaylistId);
}
// End of AI-generated text.

} // namespace
