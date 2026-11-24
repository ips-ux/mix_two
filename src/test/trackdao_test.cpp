#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "library/library_prefs.h"
#include "test/librarytest.h"
#include "track/globaltrackcache.h"
#include "track/track.h"

using ::testing::UnorderedElementsAre;

class TrackDAOTest : public LibraryTest {
};


TEST_F(TrackDAOTest, detectMovedTracks) {
    TrackDAO& trackDAO = internalCollection()->getTrackDAO();

    QString filename = QStringLiteral("file.mp3");

    mixxx::FileInfo oldFile(QDir(QDir::tempPath() + QStringLiteral("/old/dir1")), filename);
    mixxx::FileInfo newFile(QDir(QDir::tempPath() + QStringLiteral("/new/dir1")), filename);
    mixxx::FileInfo otherFile(QDir(QDir::tempPath() + QStringLiteral("/new")), filename);

    TrackPointer pOldTrack = Track::newTemporary(mixxx::FileAccess(oldFile));
    TrackPointer pNewTrack = Track::newTemporary(mixxx::FileAccess(newFile));
    TrackPointer pOtherTrack = Track::newTemporary(mixxx::FileAccess(otherFile));

    // Arbitrary duration
    pOldTrack->setDuration(135);
    pNewTrack->setDuration(135.7);
    pOtherTrack->setDuration(135.7);

    TrackId oldId = internalCollection()->addTrack(pOldTrack, false);
    TrackId newId = internalCollection()->addTrack(pNewTrack, false);
    internalCollection()->addTrack(pOtherTrack, false);

    // Mark as missing
    QSqlQuery query(dbConnection());
    query.prepare("UPDATE track_locations SET fs_deleted=1 WHERE location=:location");
    query.bindValue(":location", oldFile.location());
    query.exec();

    QList<RelocatedTrack> relocatedTracks;
    QStringList addedTracks(newFile.location());
    bool cancel = false;
    trackDAO.detectMovedTracks(&relocatedTracks, addedTracks, &cancel);

    QSet<TrackId> updatedTrackIds;
    QSet<TrackId> removedTrackIds;
    for (const auto& relocatedTrack : std::as_const(relocatedTracks)) {
        updatedTrackIds.insert(relocatedTrack.updatedTrackRef().getId());
        removedTrackIds.insert(relocatedTrack.deletedTrackId());
    }

    EXPECT_THAT(updatedTrackIds, UnorderedElementsAre(oldId));
    EXPECT_THAT(removedTrackIds, UnorderedElementsAre(newId));

    QSet<QString> trackLocations = trackDAO.getAllTrackLocations();
    EXPECT_THAT(trackLocations, UnorderedElementsAre(newFile.location(), otherFile.location()));
}

// Regression test for the bug where a BPM-locked track without a beatgrid
// loses its lock when reloaded from the database.
// https://github.com/mixxxdj/mixxx/issues/15196
TEST_F(TrackDAOTest, bpmLockPreservedForTrackWithoutBeats) {
    const mixxx::FileInfo fileInfo(
            QDir(QDir::tempPath()), QStringLiteral("bpmlocked-no-beats.mp3"));
    TrackPointer pTrack = Track::newTemporary(mixxx::FileAccess(fileInfo));
    pTrack->setDuration(135);
    // Lock the BPM although the track has no beatgrid at all.
    pTrack->setBpmLocked(true);
    ASSERT_FALSE(pTrack->getBeats());
    ASSERT_TRUE(pTrack->isBpmLocked());

    const TrackId trackId = internalCollection()->addTrack(pTrack, false);
    ASSERT_TRUE(trackId.isValid());

    // Dropping the last reference evicts the track from the cache
    // synchronously (eviction runs as a direct call on this thread), so the
    // lookup below reloads it from the database instead of returning the
    // cached in-memory object whose lock flag was never lost.
    pTrack.reset();
    ASSERT_TRUE(GlobalTrackCacheLocker().isEmpty());

    const TrackPointer pReloaded = internalCollection()->getTrackById(trackId);
    ASSERT_TRUE(pReloaded);
    EXPECT_FALSE(pReloaded->getBeats());
    EXPECT_TRUE(pReloaded->isBpmLocked());
}

TEST_F(TrackDAOTest, markTrackLocationsAsVerifiedRecoversPresentFilesOnly) {
    // Regression cover for both directions of mixxxdj/mixxx#13533:
    //   1. A track erroneously flagged fs_deleted=1 must be revived when the
    //      file is still present in an unchanged-hash directory (the original
    //      bug).
    //   2. A track legitimately flagged fs_deleted=1 because the file was
    //      removed before the saved directory hash was last updated must NOT
    //      be revived on a subsequent unchanged-hash rescan.
    //
    // markTrackLocationsAsVerified is the cleanup-phase entry point that the
    // scanner uses for every verified location, including those collected
    // from unchanged-hash directories. It should clear fs_deleted /
    // needs_verification for exactly the locations passed in, and leave
    // unrelated rows untouched.
    TrackDAO& trackDAO = internalCollection()->getTrackDAO();

    QDir dir(QDir::tempPath() + QStringLiteral("/verified_dir"));
    mixxx::FileInfo presentFile(dir, QStringLiteral("present.mp3"));
    mixxx::FileInfo deletedFile(dir, QStringLiteral("deleted.mp3"));

    TrackPointer pPresent = Track::newTemporary(mixxx::FileAccess(presentFile));
    TrackPointer pDeleted = Track::newTemporary(mixxx::FileAccess(deletedFile));
    pPresent->setDuration(180);
    pDeleted->setDuration(180);

    TrackId presentId = internalCollection()->addTrack(pPresent, false);
    TrackId deletedId = internalCollection()->addTrack(pDeleted, false);
    ASSERT_TRUE(presentId.isValid());
    ASSERT_TRUE(deletedId.isValid());

    // Both rows simulate the post-`invalidateTrackLocationsInLibrary` state at
    // the start of a scan. The "deleted" row additionally carries the
    // fs_deleted=1 that the previous scan's verifyRemainingTracks set when
    // the file disappeared from disk between scans.
    QSqlQuery setQuery(dbConnection());
    setQuery.prepare(
            "UPDATE track_locations "
            "SET fs_deleted=:fs_deleted, needs_verification=1 "
            "WHERE location=:location");
    setQuery.bindValue(":fs_deleted", 1);
    setQuery.bindValue(":location", presentFile.location());
    ASSERT_TRUE(setQuery.exec());
    setQuery.bindValue(":fs_deleted", 1);
    setQuery.bindValue(":location", deletedFile.location());
    ASSERT_TRUE(setQuery.exec());

    // The recursive scanner only feeds locations whose file is currently
    // present in the directory into markTrackLocationsAsVerified. Simulate
    // that for a delete-then-rescan: only `presentFile` is in the list.
    trackDAO.markTrackLocationsAsVerified(QStringList{presentFile.location()});

    QSqlQuery readQuery(dbConnection());
    readQuery.prepare(
            "SELECT fs_deleted, needs_verification FROM track_locations "
            "WHERE location=:location");

    readQuery.bindValue(":location", presentFile.location());
    ASSERT_TRUE(readQuery.exec());
    ASSERT_TRUE(readQuery.next());
    EXPECT_EQ(0, readQuery.value(0).toInt())
            << "fs_deleted should be cleared for a still-present file";
    EXPECT_EQ(0, readQuery.value(1).toInt())
            << "needs_verification should be cleared for a still-present file";

    readQuery.bindValue(":location", deletedFile.location());
    ASSERT_TRUE(readQuery.exec());
    ASSERT_TRUE(readQuery.next());
    EXPECT_EQ(1, readQuery.value(0).toInt())
            << "fs_deleted must be preserved for a file no longer in the directory";
    EXPECT_EQ(1, readQuery.value(1).toInt())
            << "needs_verification must remain set so verifyRemainingTracks can confirm deletion";
}

// AI-generated (Claude Code). Start of AI-generated text.
namespace {

int countPlayedTracks(const QSqlDatabase& db) {
    QSqlQuery query(db);
    EXPECT_TRUE(query.exec(QStringLiteral("SELECT COUNT(*) FROM library WHERE played>0")));
    return query.next() ? query.value(0).toInt() : -1;
}

} // namespace

TEST_F(TrackDAOTest, clearPlayedClearsAllPlayedMarks) {
    TrackPointer pTrack = getOrAddTrackByLocation(
            getTestDir().filePath(QStringLiteral("id3-test-data/artist.mp3")));
    ASSERT_TRUE(pTrack);
    QSqlQuery update(dbConnection());
    ASSERT_TRUE(update.exec(QStringLiteral("UPDATE library SET played=1")));
    ASSERT_EQ(1, countPlayedTracks(dbConnection()));

    EXPECT_TRUE(internalCollection()->getTrackDAO().clearPlayed());
    EXPECT_EQ(0, countPlayedTracks(dbConnection()));
}

TEST_F(TrackDAOTest, finishClearsPlayedByDefault) {
    TrackPointer pTrack = getOrAddTrackByLocation(
            getTestDir().filePath(QStringLiteral("id3-test-data/artist.mp3")));
    ASSERT_TRUE(pTrack);
    QSqlQuery update(dbConnection());
    ASSERT_TRUE(update.exec(QStringLiteral("UPDATE library SET played=1")));

    internalCollection()->getTrackDAO().finish();
    EXPECT_EQ(0, countPlayedTracks(dbConnection()));
}

TEST_F(TrackDAOTest, finishKeepsPlayedWhenResetOnExitIsOff) {
    config()->setValue(mixxx::library::prefs::kResetPlayedOnExitConfigKey, false);
    TrackPointer pTrack = getOrAddTrackByLocation(
            getTestDir().filePath(QStringLiteral("id3-test-data/artist.mp3")));
    ASSERT_TRUE(pTrack);
    QSqlQuery update(dbConnection());
    ASSERT_TRUE(update.exec(QStringLiteral("UPDATE library SET played=1")));

    internalCollection()->getTrackDAO().finish();
    EXPECT_EQ(1, countPlayedTracks(dbConnection()));
}
TEST_F(TrackDAOTest, clearPlayedClearsCachedTrackAndKeepsPlayCount) {
    TrackPointer pTrack = getOrAddTrackByLocation(
            getTestDir().filePath(QStringLiteral("id3-test-data/artist.mp3")));
    ASSERT_TRUE(pTrack);
    pTrack->setPlayCounter(PlayCounter(3));
    pTrack->updatePlayedStatusKeepPlayCount(true);
    ASSERT_TRUE(pTrack->getPlayCounter().isPlayed());
    trackCollectionManager()->saveTrack(pTrack);
    ASSERT_EQ(1, countPlayedTracks(dbConnection()));

    EXPECT_TRUE(internalCollection()->getTrackDAO().clearPlayed());

    // The track object, the database and the play count.
    EXPECT_FALSE(pTrack->getPlayCounter().isPlayed());
    EXPECT_EQ(0, countPlayedTracks(dbConnection()));
    EXPECT_EQ(3, pTrack->getPlayCounter().getTimesPlayed());

    // A later save of the track does not bring the mark back.
    pTrack->setComment(QStringLiteral("changed after reset"));
    trackCollectionManager()->saveTrack(pTrack);
    EXPECT_EQ(0, countPlayedTracks(dbConnection()));
    EXPECT_EQ(3, pTrack->getPlayCounter().getTimesPlayed());
}
// End of AI-generated text.

// AI-generated (Claude Code). Start of AI-generated text.
TEST_F(TrackDAOTest, hotCueAutoLoadIsOnForNewTrackAndSaved) {
    const QString dir = getTestDir().filePath(QStringLiteral("id3-test-data/"));
    TrackPointer pTrack = getOrAddTrackByLocation(dir + QStringLiteral("artist.mp3"));
    TrackPointer pOffTrack = getOrAddTrackByLocation(dir + QStringLiteral("all.mp3"));
    ASSERT_TRUE(pTrack);
    ASSERT_TRUE(pOffTrack);
    const TrackId onId = pTrack->getId();
    const TrackId offId = pOffTrack->getId();
    ASSERT_TRUE(onId.isValid());
    ASSERT_TRUE(offId.isValid());

    // A track that comes into the library has the flag on.
    EXPECT_TRUE(pTrack->isHotCueAutoLoad());
    EXPECT_TRUE(pOffTrack->isHotCueAutoLoad());

    // A change of the flag of a track in the library is saved.
    pTrack->setHotCueAutoLoad(false);
    pOffTrack->setHotCueAutoLoad(false);
    EXPECT_EQ(TrackCollectionManager::SaveTrackResult::Saved,
            trackCollectionManager()->saveTrack(pTrack));
    EXPECT_EQ(TrackCollectionManager::SaveTrackResult::Saved,
            trackCollectionManager()->saveTrack(pOffTrack));

    // Dropping the last reference evicts the tracks, so that the lookups
    // below read the database.
    pTrack.reset();
    pOffTrack.reset();
    ASSERT_TRUE(GlobalTrackCacheLocker().isEmpty());

    TrackPointer pReloaded = trackCollectionManager()->getTrackById(onId);
    ASSERT_TRUE(pReloaded);
    EXPECT_FALSE(pReloaded->isHotCueAutoLoad());
    // The flag can go back on and stays on.
    pReloaded->setHotCueAutoLoad(true);
    EXPECT_EQ(TrackCollectionManager::SaveTrackResult::Saved,
            trackCollectionManager()->saveTrack(pReloaded));
    pReloaded.reset();
    ASSERT_TRUE(GlobalTrackCacheLocker().isEmpty());
    const TrackPointer pAgain = trackCollectionManager()->getTrackById(onId);
    ASSERT_TRUE(pAgain);
    EXPECT_TRUE(pAgain->isHotCueAutoLoad());
    const TrackPointer pOffReloaded = trackCollectionManager()->getTrackById(offId);
    ASSERT_TRUE(pOffReloaded);
    EXPECT_FALSE(pOffReloaded->isHotCueAutoLoad());
}
// End of AI-generated text.
