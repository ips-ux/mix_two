#include <gtest/gtest.h>

#include <QSignalSpy>

#include "library/dao/playlistdao.h"
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

} // namespace
