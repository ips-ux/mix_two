#include <gtest/gtest.h>

#include <QDir>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QUrl>

#include <memory>
#include <vector>

#include "control/controlobject.h"
#include "library/dao/playlistdao.h"
#include "library/libraryfeature.h"
#include "library/playlisttablemodel.h"
#include "library/sidebarmodel.h"
#include "library/treeitem.h"
#include "library/trackset/baseplaylistfeature.h"
#include "library/treeitemmodel.h"
#include "mixer/playerinfo.h"
#include "library/trackset/playlistdrop.h"
#include "test/librarytest.h"

// AI-generated (Claude Code). Start of AI-generated text.
/// Lets a test give the model a real playlist id while it shows a folder, so
/// that a guard that is missing would change that playlist.
class PlaylistTableModelTestAccess {
  public:
    static void setPlaylistId(PlaylistTableModel& model, int playlistId) {
        model.m_iPlaylistId = playlistId;
    }
};
// End of AI-generated text.

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

    const FolderImport first = createPlaylistFromFolder(
            dao(), trackCollectionManager(), folderPath);
    const int playlistId = first.playlistId;
    ASSERT_NE(kInvalidPlaylistId, playlistId);
    EXPECT_EQ(3, first.trackCount);
    EXPECT_FALSE(first.tooManyFiles);
    EXPECT_EQ(QStringLiteral("Friday Warmup"), dao().getPlaylistName(playlistId));
    EXPECT_EQ(3, dao().getTrackIds(playlistId).size());
    EXPECT_EQ(kInvalidPlaylistFolderId, dao().getPlaylistFolderId(playlistId));

    // A second drop of the same folder gets a unique name.
    const int second = createPlaylistFromFolder(
            dao(), trackCollectionManager(), folderPath)
                               .playlistId;
    ASSERT_NE(kInvalidPlaylistId, second);
    EXPECT_NE(second, playlistId);
    EXPECT_NE(QStringLiteral("Friday Warmup"), dao().getPlaylistName(second));
}

TEST_F(PlaylistFoldersTest, DroppedFolderWithoutTracksMakesNoPlaylist) {
    const QDir root = getTestDataDir();
    ASSERT_TRUE(root.mkpath(QStringLiteral("Empty")));
    const QString folderPath = root.filePath(QStringLiteral("Empty"));
    const FolderImport result =
            createPlaylistFromFolder(dao(), trackCollectionManager(), folderPath);
    EXPECT_EQ(kInvalidPlaylistId, result.playlistId);
    EXPECT_FALSE(result.tooManyFiles);
    EXPECT_EQ(kInvalidPlaylistId, dao().getPlaylistIdFromName(QStringLiteral("Empty")));
}
// End of AI-generated text.

// AI-generated (Claude Code). Start of AI-generated text.
TEST_F(PlaylistFoldersTest, CaseOnlyRenameIsAllowed) {
    const int playlist = dao().createPlaylist(QStringLiteral("Friday"));
    const int folder = dao().createFolder(QStringLiteral("Sets"));
    EXPECT_EQ(RenameResult::Ok,
            renameNode(dao(), parseNodeId(nodeIdForPlaylist(playlist)), QStringLiteral("FRIDAY")));
    EXPECT_EQ(QStringLiteral("FRIDAY"), dao().getPlaylistName(playlist));
    EXPECT_EQ(RenameResult::Ok,
            renameNode(dao(), parseNodeId(nodeIdForFolder(folder)), QStringLiteral("sets")));
    EXPECT_EQ(QStringLiteral("sets"), dao().getFolderName(folder));
}

TEST_F(PlaylistFoldersTest, LockedPlaylistIsNotRenamableInTheTree) {
    const QSet<int> locked = {7};
    EXPECT_FALSE(isNodeRenamable(parseNodeId(nodeIdForPlaylist(7)), locked));
    EXPECT_TRUE(isNodeRenamable(parseNodeId(nodeIdForPlaylist(8)), locked));
    EXPECT_TRUE(isNodeRenamable(parseNodeId(nodeIdForFolder(7)), locked));
    EXPECT_FALSE(isNodeRenamable(Node(), locked));
}

TEST_F(PlaylistFoldersTest, FolderWithTooManyFilesMakesNothing) {
    const QDir root = getTestDataDir();
    ASSERT_TRUE(root.mkpath(QStringLiteral("Big")));
    const QString source = getTestDir().filePath(QStringLiteral("sine-30.wav"));
    for (const char* name : {"a.wav", "b.wav", "c.wav"}) {
        ASSERT_TRUE(QFile::copy(source, root.filePath(QStringLiteral("Big/") + QLatin1String(name))));
    }
    const QString folderPath = root.filePath(QStringLiteral("Big"));

    // With a limit of 2 the folder of 3 files is too large.
    const FolderImport tooMany =
            createPlaylistFromFolder(dao(), trackCollectionManager(), folderPath, 2);
    EXPECT_TRUE(tooMany.tooManyFiles);
    EXPECT_EQ(kInvalidPlaylistId, tooMany.playlistId);
    EXPECT_EQ(kInvalidPlaylistId, dao().getPlaylistIdFromName(QStringLiteral("Big")));
    // Nothing went into the library either.
    {
        QSqlQuery query(internalCollection()->database());
        ASSERT_TRUE(query.exec(QStringLiteral("SELECT COUNT(*) FROM library")));
        ASSERT_TRUE(query.next());
        EXPECT_EQ(0, query.value(0).toInt());
    }

    // A limit that fits the folder makes the playlist.
    const FolderImport fits =
            createPlaylistFromFolder(dao(), trackCollectionManager(), folderPath, 3);
    EXPECT_FALSE(fits.tooManyFiles);
    EXPECT_NE(kInvalidPlaylistId, fits.playlistId);
    EXPECT_EQ(3, dao().getTrackIds(fits.playlistId).size());
    EXPECT_EQ(2000, kMaxDroppedFolderTracks);
}

// AI-generated (Claude Code). Start of AI-generated text.
// The view of a folder: every track of the playlists in it and below it, each
// track once, and read-only.
class FolderViewTest : public PlaylistFoldersTest {
  protected:
    void SetUp() override {
        PlaylistFoldersTest::SetUp();
        m_pCrossfader = std::make_unique<ControlObject>(
                ConfigKey(QStringLiteral("[Master]"), QStringLiteral("crossfader")));
        for (const char* key : {"num_decks", "num_samplers", "num_preview_decks"}) {
            m_appControls.push_back(std::make_unique<ControlObject>(
                    ConfigKey(QStringLiteral("[App]"), QString::fromLatin1(key))));
        }
        PlayerInfo::create();
    }
    void TearDown() override {
        PlayerInfo::destroy();
        m_appControls.clear();
        m_pCrossfader.reset();
        PlaylistFoldersTest::TearDown();
    }
    std::unique_ptr<ControlObject> m_pCrossfader;
    std::vector<std::unique_ptr<ControlObject>> m_appControls;

    TrackId makeTrack(const QString& name) {
        const QString target = getTestDataDir().filePath(name);
        EXPECT_TRUE(QFile::copy(getTestDir().filePath(QStringLiteral("sine-30.wav")), target));
        const TrackPointer pTrack = getOrAddTrackByLocation(target);
        EXPECT_TRUE(pTrack);
        return pTrack ? pTrack->getId() : TrackId();
    }

    QSet<TrackId> shownTrackIds(PlaylistTableModel& model) {
        QSet<TrackId> ids;
        for (int row = 0; row < model.rowCount(); ++row) {
            ids.insert(model.getTrackId(model.index(row, 0)));
        }
        return ids;
    }
};

TEST_F(FolderViewTest, FolderShowsTracksOfAllLevelsOnce) {
    const TrackId t1 = makeTrack(QStringLiteral("f1.wav"));
    const TrackId t2 = makeTrack(QStringLiteral("f2.wav"));
    const TrackId t3 = makeTrack(QStringLiteral("f3.wav"));
    const TrackId outside = makeTrack(QStringLiteral("f4.wav"));
    ASSERT_TRUE(t1.isValid() && t2.isValid() && t3.isValid() && outside.isValid());

    const int top = dao().createFolder(QStringLiteral("Top"));
    const int deep = dao().createFolder(QStringLiteral("Deep"), top);
    const int a = dao().createPlaylist(QStringLiteral("A"));
    const int b = dao().createPlaylist(QStringLiteral("B"));
    const int c = dao().createPlaylist(QStringLiteral("C"));
    const int other = dao().createPlaylist(QStringLiteral("Other"));
    ASSERT_TRUE(dao().setPlaylistFolder(a, top));
    ASSERT_TRUE(dao().setPlaylistFolder(b, top));
    ASSERT_TRUE(dao().setPlaylistFolder(c, deep));
    dao().appendTrackToPlaylist(t1, a);
    dao().appendTrackToPlaylist(t2, a);
    // t2 is in two playlists: it shows once.
    dao().appendTrackToPlaylist(t2, b);
    dao().appendTrackToPlaylist(t3, c);
    dao().appendTrackToPlaylist(outside, other);

    PlaylistTableModel model(nullptr, trackCollectionManager(), "mixxx.db.model.test_folder");
    model.selectPlaylistFolder(top);
    model.select();
    EXPECT_TRUE(model.isFolderView());
    EXPECT_EQ(3, model.rowCount());
    EXPECT_EQ(QSet<TrackId>({t1, t2, t3}), shownTrackIds(model));

    // The inner folder shows its own tracks only.
    model.selectPlaylistFolder(deep);
    model.select();
    EXPECT_EQ(QSet<TrackId>({t3}), shownTrackIds(model));

    // A playlist after a folder is a plain playlist again.
    model.selectPlaylist(other);
    model.select();
    EXPECT_FALSE(model.isFolderView());
    EXPECT_EQ(QSet<TrackId>({outside}), shownTrackIds(model));
}

TEST_F(FolderViewTest, FolderViewIsReadOnly) {
    const TrackId t1 = makeTrack(QStringLiteral("g1.wav"));
    const TrackId t2 = makeTrack(QStringLiteral("g2.wav"));
    const TrackId t3 = makeTrack(QStringLiteral("g3.wav"));
    const TrackId t4 = makeTrack(QStringLiteral("g4.wav"));
    const int top = dao().createFolder(QStringLiteral("Top"));
    const int a = dao().createPlaylist(QStringLiteral("A"));
    ASSERT_TRUE(dao().setPlaylistFolder(a, top));
    dao().appendTrackToPlaylist(t1, a);
    dao().appendTrackToPlaylist(t2, a);
    dao().appendTrackToPlaylist(t3, a);

    PlaylistTableModel model(nullptr, trackCollectionManager(), "mixxx.db.model.test_folder");
    model.selectPlaylistFolder(top);
    model.select();
    ASSERT_EQ(3, model.rowCount());

    EXPECT_TRUE(model.isLocked());
    EXPECT_FALSE(model.hasCapabilities(TrackModel::Capability::ReceiveDrops));
    EXPECT_FALSE(model.hasCapabilities(TrackModel::Capability::Reorder));
    EXPECT_FALSE(model.hasCapabilities(TrackModel::Capability::Remove));
    EXPECT_FALSE(model.hasCapabilities(TrackModel::Capability::RemovePlaylist));
    EXPECT_FALSE(model.hasCapabilities(TrackModel::Capability::Hide));
    EXPECT_TRUE(model.hasCapabilities(TrackModel::Capability::Locked));
    // What changes the track, not a playlist, stays on.
    EXPECT_TRUE(model.hasCapabilities(TrackModel::Capability::EditMetadata));
    EXPECT_TRUE(model.hasCapabilities(TrackModel::Capability::ResetPlayed));
    EXPECT_TRUE(model.hasCapabilities(TrackModel::Capability::LoadToDeck));

    // Give the model the id of a real, unlocked playlist. Only the guards of
    // the folder view keep that playlist as it is.
    PlaylistTableModelTestAccess::setPlaylistId(model, a);
    int pos = 0;
    EXPECT_EQ(0, model.addTracksWithTrackIds(model.index(0, 0), {t4}, &pos));
    EXPECT_FALSE(model.appendTrack(t4));
    model.removeTrack(model.index(0, 0));
    model.removeTracks({model.index(0, 0), model.index(1, 0)});
    model.moveTrack(model.index(0, 0), model.index(2, 0));
    model.shuffleTracks();
    model.orderTracksByCurrPos();
    EXPECT_EQ(QList<TrackId>({t1, t2, t3}), dao().getTrackIdsInPlaylistOrder(a));
    EXPECT_EQ(3, model.rowCount());
}

TEST_F(FolderViewTest, EmptyFolderAndHiddenTrack) {
    const TrackId t1 = makeTrack(QStringLiteral("h1.wav"));
    const TrackId t2 = makeTrack(QStringLiteral("h2.wav"));
    const int top = dao().createFolder(QStringLiteral("Top"));
    const int sub = dao().createFolder(QStringLiteral("Sub"), top);

    PlaylistTableModel model(nullptr, trackCollectionManager(), "mixxx.db.model.test_folder");
    // A folder with no playlist, also with an empty folder in it.
    model.selectPlaylistFolder(top);
    model.select();
    EXPECT_EQ(0, model.rowCount());

    const int a = dao().createPlaylist(QStringLiteral("A"));
    ASSERT_TRUE(dao().setPlaylistFolder(a, sub));
    dao().appendTrackToPlaylist(t1, a);
    dao().appendTrackToPlaylist(t2, a);
    model.selectPlaylistFolder(top);
    model.select();
    EXPECT_EQ(2, model.rowCount());

    // A hidden (deleted) track is not in the list.
    QSqlQuery query(dbConnection());
    ASSERT_TRUE(query.exec(QStringLiteral("UPDATE library SET mixxx_deleted = 1 WHERE id = %1")
                                   .arg(t2.toString())));
    model.selectPlaylistFolder(top);
    model.select();
    EXPECT_EQ(QSet<TrackId>({t1}), shownTrackIds(model));
}

TEST_F(FolderViewTest, FolderViewFollowsMembershipAndContent) {
    const TrackId t1 = makeTrack(QStringLiteral("i1.wav"));
    const TrackId t2 = makeTrack(QStringLiteral("i2.wav"));
    const TrackId t3 = makeTrack(QStringLiteral("i3.wav"));
    const int top = dao().createFolder(QStringLiteral("Top"));
    const int a = dao().createPlaylist(QStringLiteral("A"));
    const int b = dao().createPlaylist(QStringLiteral("B"));
    ASSERT_TRUE(dao().setPlaylistFolder(a, top));
    dao().appendTrackToPlaylist(t1, a);
    dao().appendTrackToPlaylist(t2, b);

    PlaylistTableModel model(nullptr, trackCollectionManager(), "mixxx.db.model.test_folder");
    model.selectPlaylistFolder(top);
    model.select();
    EXPECT_EQ(QSet<TrackId>({t1}), shownTrackIds(model));

    // A playlist moves into the folder.
    ASSERT_TRUE(dao().setPlaylistFolder(b, top));
    EXPECT_EQ(QSet<TrackId>({t1, t2}), shownTrackIds(model));
    // A track is added to a playlist of the folder.
    dao().appendTrackToPlaylist(t3, a);
    EXPECT_EQ(QSet<TrackId>({t1, t2, t3}), shownTrackIds(model));
    // A playlist leaves the folder.
    ASSERT_TRUE(dao().setPlaylistFolder(a, kInvalidPlaylistFolderId));
    EXPECT_EQ(QSet<TrackId>({t2}), shownTrackIds(model));
    // A new playlist in a folder, and a playlist that is deleted.
    const int c = dao().createPlaylist(QStringLiteral("C"));
    ASSERT_TRUE(dao().setPlaylistFolder(c, top));
    dao().appendTrackToPlaylist(t3, c);
    EXPECT_EQ(QSet<TrackId>({t2, t3}), shownTrackIds(model));
    dao().deletePlaylist(b);
    EXPECT_EQ(QSet<TrackId>({t3}), shownTrackIds(model));
}

TEST_F(FolderViewTest, NoPlaylistAfterFolderLeavesNoFolderTable) {
    const TrackId t1 = makeTrack(QStringLiteral("j1.wav"));
    const int top = dao().createFolder(QStringLiteral("Top"));
    const int a = dao().createPlaylist(QStringLiteral("A"));
    ASSERT_TRUE(dao().setPlaylistFolder(a, top));
    dao().appendTrackToPlaylist(t1, a);

    PlaylistTableModel model(nullptr, trackCollectionManager(), "mixxx.db.model.test_folder");
    model.selectPlaylistFolder(top);
    model.select();
    ASSERT_EQ(1, model.rowCount());
    model.selectPlaylist(kInvalidPlaylistId);
    model.select();
    EXPECT_FALSE(model.isFolderView());
    EXPECT_EQ(0, model.rowCount());
}
// End of AI-generated text.

namespace {
/// A feature of another kind: its tree items can be edited and dragged only
/// when asked for.
class StubFeature : public LibraryFeature {
  public:
    StubFeature(bool supportsNodes, const QSet<int>& locked)
            : LibraryFeature(nullptr, UserSettingsPointer(), QString()),
              m_supportsNodes(supportsNodes),
              m_locked(locked),
              m_pModel(new TreeItemModel(this)) {
        m_pModel->setRootItem(TreeItem::newRoot(this));
        m_pModel->getRootItem()->appendChild(QStringLiteral("one"), 1);
        m_pModel->getRootItem()->appendChild(QStringLiteral("two"), 2);
    }
    QVariant title() override {
        return QStringLiteral("Stub");
    }
    TreeItemModel* sidebarModel() const override {
        return m_pModel;
    }
    void activate() override {
    }
    QString dragIdOfChild(const QModelIndex& index) override {
        return m_supportsNodes ? nodeIdForPlaylist(index.row() + 1) : QString();
    }
    bool canRenameChildInline(const QModelIndex& index) override {
        return m_supportsNodes &&
                isNodeRenamable(parseNodeId(dragIdOfChild(index)), m_locked);
    }

  private:
    const bool m_supportsNodes;
    const QSet<int> m_locked;
    TreeItemModel* m_pModel;
};
} // namespace

TEST_F(PlaylistFoldersTest, SidebarFlagsFollowTheFeature) {
    StubFeature plain(false, {});
    StubFeature playlists(true, {2});
    SidebarModel sidebar;
    sidebar.addLibraryFeature(&plain);
    sidebar.addLibraryFeature(&playlists);

    const auto childFlags = [&](int featureRow, int childRow) {
        return sidebar.flags(sidebar.index(childRow, 0, sidebar.index(featureRow, 0)));
    };
    // The items of a feature that does not ask for it: no edit, no drag.
    EXPECT_FALSE(childFlags(0, 0) & Qt::ItemIsEditable);
    EXPECT_FALSE(childFlags(0, 0) & Qt::ItemIsDragEnabled);
    // An unlocked playlist: both.
    EXPECT_TRUE(childFlags(1, 0) & Qt::ItemIsEditable);
    EXPECT_TRUE(childFlags(1, 0) & Qt::ItemIsDragEnabled);
    // A locked playlist: no editor, but it still moves.
    EXPECT_FALSE(childFlags(1, 1) & Qt::ItemIsEditable);
    EXPECT_TRUE(childFlags(1, 1) & Qt::ItemIsDragEnabled);
    // The root items of the features are never editable or draggable.
    EXPECT_FALSE(sidebar.flags(sidebar.index(1, 0)) & Qt::ItemIsEditable);
    EXPECT_FALSE(sidebar.flags(sidebar.index(1, 0)) & Qt::ItemIsDragEnabled);
}
// End of AI-generated text.

} // namespace
