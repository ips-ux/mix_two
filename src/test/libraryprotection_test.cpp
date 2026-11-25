// AI-generated (Claude Code). Start of AI-generated text.
// Tests for Library Protection: the decision function, and the library parts
// that a unit test can reach (the track list model, the removal of tracks, the
// relocation of tracks and the rename and move of playlists and folders).
// The menus, the dialogs and the sidebar need the running program.

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <memory>
#include <vector>

#include "control/controlobject.h"
#include <QSharedPointer>
#include <QSqlQuery>

#include "library/basetrackcache.h"
#include "library/dao/playlistdao.h"
#include "library/dao/trackschema.h"
#include "library/librarytablemodel.h"
#include "library/libraryprotection.h"
#include "library/library_prefs.h"
#include "library/playlisttablemodel.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/trackset/playlistdrop.h"
#include "mixer/playerinfo.h"
#include "test/librarytest.h"
#include "test/mixxxtest.h"
#include "track/track.h"

namespace {

using mixxx::library::isActionAllowed;
using mixxx::library::ProtectedAction;

constexpr ProtectedAction kChangeActions[] = {
        ProtectedAction::EditTrackInfo,
        ProtectedAction::RemoveTracks,
        ProtectedAction::RenameTrackSet,
        ProtectedAction::DeleteTrackSet,
        ProtectedAction::MoveTrackSet,
        ProtectedAction::RelocateTracks,
};

} // namespace

TEST(LibraryProtectionPolicyTest, ProtectionOffAllowsEveryAction) {
    for (const ProtectedAction action : kChangeActions) {
        EXPECT_TRUE(isActionAllowed(false, action));
    }
    EXPECT_TRUE(isActionAllowed(false, ProtectedAction::EditPlayCount));
}

TEST(LibraryProtectionPolicyTest, ProtectionOnBlocksTheChangesButNotThePlayCount) {
    for (const ProtectedAction action : kChangeActions) {
        EXPECT_FALSE(isActionAllowed(true, action));
    }
    // What a DJ does in a set stays possible.
    EXPECT_TRUE(isActionAllowed(true, ProtectedAction::EditPlayCount));
}

TEST(LibraryProtectionPolicyTest, TheDefaultIsOffAndTheStateComesFromTheConfiguration) {
    EXPECT_FALSE(mixxx::library::prefs::kLibraryProtectionDefault);

    auto pConfig = UserSettingsPointer(new UserSettings(QString()));
    mixxx::library::setLibraryProtectionOn(true);
    // No saved value: the default (off) applies.
    mixxx::library::loadLibraryProtection(pConfig);
    EXPECT_FALSE(mixxx::library::isLibraryProtectionOn());

    pConfig->setValue(mixxx::library::prefs::kLibraryProtectionConfigKey, true);
    mixxx::library::loadLibraryProtection(pConfig);
    EXPECT_TRUE(mixxx::library::isLibraryProtectionOn());
    EXPECT_FALSE(isActionAllowed(ProtectedAction::EditTrackInfo));
    EXPECT_TRUE(isActionAllowed(ProtectedAction::EditPlayCount));

    mixxx::library::setLibraryProtectionOn(false);
    EXPECT_TRUE(isActionAllowed(ProtectedAction::EditTrackInfo));
}

class LibraryProtectionLibraryTest : public LibraryTest {
  protected:
    void SetUp() override {
        LibraryTest::SetUp();
        mixxx::library::setLibraryProtectionOn(false);
        m_pCrossfader = std::make_unique<ControlObject>(
                ConfigKey(QStringLiteral("[Master]"), QStringLiteral("crossfader")));
        for (const char* key : {"num_decks", "num_samplers", "num_preview_decks"}) {
            m_appControls.push_back(std::make_unique<ControlObject>(
                    ConfigKey(QStringLiteral("[App]"), QString::fromLatin1(key))));
        }
        PlayerInfo::create();
        connectTrackSource();
    }
    void TearDown() override {
        // Never leave the protection on for the next test.
        mixxx::library::setLibraryProtectionOn(false);
        PlayerInfo::destroy();
        m_appControls.clear();
        m_pCrossfader.reset();
        internalCollection()->disconnectTrackSource();
        m_pTrackSource.reset();
        LibraryTest::TearDown();
    }

    PlaylistDAO& dao() const {
        return internalCollection()->getPlaylistDAO();
    }

    /// The library window makes the track source that the track list models
    /// need. This does the same for the models in the tests.
    void connectTrackSource() {
        const QString idColumn = LIBRARYTABLE_ID;
        QStringList columns = {
                LIBRARYTABLE_ID,
                LIBRARYTABLE_PLAYED,
                LIBRARYTABLE_TIMESPLAYED,
                LIBRARYTABLE_LAST_PLAYED_AT,
                // has to be up here otherwise Played and TimesPlayed are not shown
                LIBRARYTABLE_ALBUMARTIST,
                LIBRARYTABLE_ALBUM,
                LIBRARYTABLE_ARTIST,
                LIBRARYTABLE_TITLE,
                LIBRARYTABLE_YEAR,
                LIBRARYTABLE_RATING,
                LIBRARYTABLE_GENRE,
                LIBRARYTABLE_COMPOSER,
                LIBRARYTABLE_GROUPING,
                LIBRARYTABLE_TRACKNUMBER,
                LIBRARYTABLE_KEY,
                LIBRARYTABLE_KEY_ID,
                LIBRARYTABLE_TUNING_FREQUENCY,
                LIBRARYTABLE_BPM,
                LIBRARYTABLE_BPM_LOCK,
                LIBRARYTABLE_BEATS_VERSION,
                LIBRARYTABLE_DURATION,
                LIBRARYTABLE_BITRATE,
                LIBRARYTABLE_REPLAYGAIN,
                LIBRARYTABLE_FILETYPE,
                LIBRARYTABLE_DATETIMEADDED,
                TRACKLOCATIONSTABLE_LOCATION,
                TRACKLOCATIONSTABLE_DIRECTORY,
                TRACKLOCATIONSTABLE_FSDELETED,
                LIBRARYTABLE_COMMENT,
                LIBRARYTABLE_MIXXXDELETED,
                LIBRARYTABLE_COLOR,
                LIBRARYTABLE_COVERART_SOURCE,
                LIBRARYTABLE_COVERART_TYPE,
                LIBRARYTABLE_COVERART_LOCATION,
                LIBRARYTABLE_COVERART_COLOR,
                LIBRARYTABLE_COVERART_DIGEST,
                LIBRARYTABLE_COVERART_HASH,
                LIBRARYTABLE_WAVESUMMARYHEX};
        QStringList searchColumns = {
                LIBRARYTABLE_ARTIST,
                LIBRARYTABLE_ALBUM,
                LIBRARYTABLE_ALBUMARTIST,
                TRACKLOCATIONSTABLE_LOCATION,
                LIBRARYTABLE_GROUPING,
                LIBRARYTABLE_COMMENT,
                LIBRARYTABLE_TITLE,
                LIBRARYTABLE_GENRE,
                LIBRARYTABLE_CRATE};

        QStringList qualifiedTableColumns;
        for (const auto& col : columns) {
            qualifiedTableColumns.append(mixxx::trackschema::tableForColumn(col) +
                    QLatin1Char('.') + col);
        }
        QSqlQuery query(internalCollection()->database());
        const QString tableName = QStringLiteral("library_cache_view");
        ASSERT_TRUE(query.exec(QStringLiteral(
                "CREATE TEMPORARY VIEW IF NOT EXISTS %1 AS SELECT %2 FROM library "
                "INNER JOIN track_locations ON library.location = track_locations.id")
                                      .arg(tableName, qualifiedTableColumns.join(","))));
        m_pTrackSource = QSharedPointer<BaseTrackCache>(new BaseTrackCache(internalCollection(),
                tableName,
                idColumn,
                columns,
                searchColumns,
                true));
        internalCollection()->connectTrackSource(m_pTrackSource);
    }

    TrackId makeTrack(const QString& name) {
        const QString target = getTestDataDir().filePath(name);
        EXPECT_TRUE(QFile::copy(getTestDir().filePath(QStringLiteral("sine-30.wav")), target));
        const TrackPointer pTrack = getOrAddTrackByLocation(target);
        EXPECT_TRUE(pTrack);
        return pTrack ? pTrack->getId() : TrackId();
    }

    /// A playlist with two tracks, shown in a model.
    int makePlaylistWithTracks(const QString& name, TrackId a, TrackId b) {
        const int playlistId = dao().createPlaylist(name);
        EXPECT_NE(kInvalidPlaylistId, playlistId);
        EXPECT_TRUE(dao().appendTrackToPlaylist(a, playlistId));
        EXPECT_TRUE(dao().appendTrackToPlaylist(b, playlistId));
        return playlistId;
    }

    std::unique_ptr<ControlObject> m_pCrossfader;
    std::vector<std::unique_ptr<ControlObject>> m_appControls;
    QSharedPointer<BaseTrackCache> m_pTrackSource;
};

TEST_F(LibraryProtectionLibraryTest, TrackListHasNoEditFlagWithProtectionOn) {
    const TrackId t1 = makeTrack(QStringLiteral("p1.wav"));
    const TrackId t2 = makeTrack(QStringLiteral("p2.wav"));
    const int playlistId = makePlaylistWithTracks(QStringLiteral("P"), t1, t2);
    PlaylistTableModel model(nullptr, trackCollectionManager(), "mixxx.db.model.test_protect");
    model.selectPlaylist(playlistId);
    model.select();
    ASSERT_EQ(2, model.rowCount());

    const auto cell = [&](ColumnCache::Column column) {
        return model.index(0, model.fieldIndex(column));
    };

    // Off: the cells are editable, as before.
    EXPECT_TRUE(model.flags(cell(ColumnCache::COLUMN_LIBRARYTABLE_TITLE)) & Qt::ItemIsEditable);
    EXPECT_TRUE(model.flags(cell(ColumnCache::COLUMN_LIBRARYTABLE_COMMENT)) & Qt::ItemIsEditable);
    EXPECT_TRUE(model.flags(cell(ColumnCache::COLUMN_LIBRARYTABLE_BPM_LOCK)) &
            Qt::ItemIsUserCheckable);

    mixxx::library::setLibraryProtectionOn(true);
    for (const auto column : {ColumnCache::COLUMN_LIBRARYTABLE_TITLE,
                 ColumnCache::COLUMN_LIBRARYTABLE_ARTIST,
                 ColumnCache::COLUMN_LIBRARYTABLE_COMMENT,
                 ColumnCache::COLUMN_LIBRARYTABLE_RATING,
                 ColumnCache::COLUMN_LIBRARYTABLE_BPM,
                 ColumnCache::COLUMN_LIBRARYTABLE_KEY}) {
        EXPECT_FALSE(model.flags(cell(column)) & Qt::ItemIsEditable) << column;
    }
    EXPECT_FALSE(model.flags(cell(ColumnCache::COLUMN_LIBRARYTABLE_BPM_LOCK)) &
            Qt::ItemIsUserCheckable);
    EXPECT_FALSE(model.flags(cell(ColumnCache::COLUMN_LIBRARYTABLE_BPM)) &
            Qt::ItemIsUserCheckable);
    // The played mark and the play count stay checkable.
    EXPECT_TRUE(model.flags(cell(ColumnCache::COLUMN_LIBRARYTABLE_PLAYED)) &
            Qt::ItemIsUserCheckable);
    EXPECT_TRUE(model.flags(cell(ColumnCache::COLUMN_LIBRARYTABLE_TIMESPLAYED)) &
            Qt::ItemIsUserCheckable);
    // A cell that can be dragged to a deck stays draggable.
    EXPECT_TRUE(model.flags(cell(ColumnCache::COLUMN_LIBRARYTABLE_TITLE)) & Qt::ItemIsDragEnabled);
}

TEST_F(LibraryProtectionLibraryTest, SetDataRefusesTrackDataButTakesThePlayCount) {
    const TrackId t1 = makeTrack(QStringLiteral("s1.wav"));
    const TrackId t2 = makeTrack(QStringLiteral("s2.wav"));
    const int playlistId = makePlaylistWithTracks(QStringLiteral("S"), t1, t2);
    PlaylistTableModel model(nullptr, trackCollectionManager(), "mixxx.db.model.test_protect");
    model.selectPlaylist(playlistId);
    model.select();
    ASSERT_EQ(2, model.rowCount());
    const QModelIndex titleIndex =
            model.index(0, model.fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TITLE));
    const QModelIndex playsIndex =
            model.index(0, model.fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TIMESPLAYED));
    const TrackPointer pTrack = model.getTrack(titleIndex);
    ASSERT_TRUE(pTrack);

    mixxx::library::setLibraryProtectionOn(true);
    pTrack->setTitle(QStringLiteral("Before"));
    EXPECT_FALSE(model.setData(titleIndex, QStringLiteral("After"), Qt::EditRole));
    EXPECT_EQ(QStringLiteral("Before"), pTrack->getTitle());

    // The play count is not track data: it changes.
    EXPECT_TRUE(model.setData(playsIndex, 5, Qt::EditRole));
    EXPECT_EQ(5, pTrack->getPlayCounter().getTimesPlayed());

    // Off again: the title changes.
    mixxx::library::setLibraryProtectionOn(false);
    EXPECT_TRUE(model.setData(titleIndex, QStringLiteral("After"), Qt::EditRole));
    EXPECT_EQ(QStringLiteral("After"), pTrack->getTitle());
}

TEST_F(LibraryProtectionLibraryTest, TracksStayInAPlaylistWithProtectionOn) {
    const TrackId t1 = makeTrack(QStringLiteral("r1.wav"));
    const TrackId t2 = makeTrack(QStringLiteral("r2.wav"));
    const int playlistId = makePlaylistWithTracks(QStringLiteral("R"), t1, t2);
    PlaylistTableModel model(nullptr, trackCollectionManager(), "mixxx.db.model.test_protect");
    model.selectPlaylist(playlistId);
    model.select();
    ASSERT_EQ(2, model.rowCount());

    mixxx::library::setLibraryProtectionOn(true);
    model.removeTracks({model.index(0, 0)});
    model.select();
    EXPECT_EQ(2, model.rowCount());

    mixxx::library::setLibraryProtectionOn(false);
    model.removeTracks({model.index(0, 0)});
    model.select();
    EXPECT_EQ(1, model.rowCount());
}

TEST_F(LibraryProtectionLibraryTest, TheAutoDjQueueStaysOpenToTheDj) {
    // The Auto DJ queue is a performance list, not library data. The queue
    // takes part in a set (Auto DJ removes the tracks that it loaded).
    const TrackId t1 = makeTrack(QStringLiteral("q1.wav"));
    const TrackId t2 = makeTrack(QStringLiteral("q2.wav"));
    int queueId = dao().getPlaylistIdFromName(QStringLiteral(AUTODJ_TABLE));
    if (queueId == kInvalidPlaylistId) {
        queueId = dao().createPlaylist(
                QStringLiteral(AUTODJ_TABLE), PlaylistDAO::PLHT_AUTO_DJ);
    }
    ASSERT_NE(kInvalidPlaylistId, queueId);
    ASSERT_TRUE(dao().appendTrackToPlaylist(t1, queueId));
    ASSERT_TRUE(dao().appendTrackToPlaylist(t2, queueId));
    PlaylistTableModel model(nullptr, trackCollectionManager(), "mixxx.db.model.test_protect");
    model.selectPlaylist(queueId);
    model.select();
    ASSERT_EQ(2, model.rowCount());

    mixxx::library::setLibraryProtectionOn(true);
    model.removeTracks({model.index(0, 0)});
    model.select();
    EXPECT_EQ(1, model.rowCount());
}

TEST_F(LibraryProtectionLibraryTest, HideAndPurgeKeepTheTrackWithProtectionOn) {
    const TrackId t1 = makeTrack(QStringLiteral("h1.wav"));
    const TrackId t2 = makeTrack(QStringLiteral("h2.wav"));
    ASSERT_TRUE(t1.isValid());
    ASSERT_TRUE(t2.isValid());
    LibraryTableModel model(nullptr, trackCollectionManager(), "mixxx.db.model.test_protect");
    model.setTableModel();
    model.select();
    ASSERT_EQ(2, model.rowCount());

    mixxx::library::setLibraryProtectionOn(true);
    model.hideTracks({model.index(0, 0)});
    model.select();
    EXPECT_EQ(2, model.rowCount());
    EXPECT_FALSE(trackCollectionManager()->hideTracks({t1}));

    mixxx::library::setLibraryProtectionOn(false);
    model.hideTracks({model.index(0, 0)});
    model.select();
    EXPECT_EQ(1, model.rowCount());

    // Purge: the track that is hidden stays in the database.
    const QString location = internalCollection()->getTrackDAO().getTrackLocation(t1);
    ASSERT_FALSE(location.isEmpty());
    const QList<TrackRef> refs = {TrackRef::fromFilePath(location, t1)};
    mixxx::library::setLibraryProtectionOn(true);
    trackCollectionManager()->purgeTracks(refs);
    EXPECT_FALSE(internalCollection()->getTrackDAO().getTrackLocation(t1).isEmpty());

    mixxx::library::setLibraryProtectionOn(false);
    trackCollectionManager()->purgeTracks(refs);
    EXPECT_TRUE(internalCollection()->getTrackDAO().getTrackLocation(t1).isEmpty());
}

TEST_F(LibraryProtectionLibraryTest, RelocateChangesNothingWithProtectionOn) {
    const QString oldFile = getTestDataDir().filePath(QStringLiteral("old/Song.mp3"));
    const QString newFile = getTestDataDir().filePath(QStringLiteral("new/Song.mp3"));
    QDir().mkpath(QFileInfo(oldFile).absolutePath());
    QDir().mkpath(QFileInfo(newFile).absolutePath());
    for (const QString& path : {oldFile, newFile}) {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write("song-data");
    }
    const TrackPointer pTrack = getOrAddTrackByLocation(oldFile);
    ASSERT_TRUE(pTrack);
    const TrackId id = pTrack->getId();
    ASSERT_TRUE(QFile::remove(oldFile));
    const QString oldLocation = internalCollection()->getTrackDAO().getTrackLocation(id);

    mixxx::library::setLibraryProtectionOn(true);
    auto outcome = internalCollection()->relocateTracksToFile({id}, id, newFile);
    EXPECT_EQ(0, outcome.relocated);
    EXPECT_EQ(oldLocation, internalCollection()->getTrackDAO().getTrackLocation(id));

    mixxx::library::setLibraryProtectionOn(false);
    outcome = internalCollection()->relocateTracksToFile({id}, id, newFile);
    EXPECT_EQ(1, outcome.relocated);
    EXPECT_NE(oldLocation, internalCollection()->getTrackDAO().getTrackLocation(id));
}

TEST_F(LibraryProtectionLibraryTest, PlaylistsAndFoldersKeepNameAndPlaceWithProtectionOn) {
    const int folder = dao().createFolder(QStringLiteral("Folder"));
    const int other = dao().createFolder(QStringLiteral("Other"));
    const int playlist = dao().createPlaylist(QStringLiteral("List"));
    ASSERT_NE(kInvalidPlaylistFolderId, folder);
    ASSERT_NE(kInvalidPlaylistFolderId, other);
    ASSERT_NE(kInvalidPlaylistId, playlist);
    using playlistdrop::Node;
    using playlistdrop::NodeKind;

    mixxx::library::setLibraryProtectionOn(true);
    EXPECT_EQ(playlistdrop::RenameResult::Protected,
            playlistdrop::renameNode(dao(), Node{NodeKind::Playlist, playlist}, QStringLiteral("New")));
    EXPECT_EQ(QStringLiteral("List"), dao().getPlaylistName(playlist));
    EXPECT_EQ(playlistdrop::RenameResult::Protected,
            playlistdrop::renameNode(dao(), Node{NodeKind::Folder, folder}, QStringLiteral("New")));
    EXPECT_EQ(QStringLiteral("Folder"), dao().getFolderName(folder));

    EXPECT_FALSE(playlistdrop::canDropNode(dao(), Node{NodeKind::Playlist, playlist}, folder));
    EXPECT_FALSE(playlistdrop::dropPlaylistOnFolder(dao(), playlist, folder));
    EXPECT_EQ(kInvalidPlaylistFolderId, dao().getPlaylistFolderId(playlist));
    EXPECT_FALSE(playlistdrop::dropFolderOnFolder(dao(), other, folder));
    EXPECT_FALSE(playlistdrop::isNodeRenamable(Node{NodeKind::Folder, folder}, {}));
    EXPECT_FALSE(playlistdrop::isNodeRenamable(Node{NodeKind::Playlist, playlist}, {}));

    mixxx::library::setLibraryProtectionOn(false);
    EXPECT_EQ(playlistdrop::RenameResult::Ok,
            playlistdrop::renameNode(dao(), Node{NodeKind::Playlist, playlist}, QStringLiteral("New")));
    EXPECT_EQ(QStringLiteral("New"), dao().getPlaylistName(playlist));
    EXPECT_TRUE(playlistdrop::dropPlaylistOnFolder(dao(), playlist, folder));
    EXPECT_EQ(folder, dao().getPlaylistFolderId(playlist));
}

// End of AI-generated text.
