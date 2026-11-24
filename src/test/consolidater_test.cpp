// AI-generated (Claude Code). Start of AI-generated text.
// Tests for consolidate: copy, check by hash, relocate, trash the original.
#include "library/consolidate/consolidater.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>

#include "library/consolidate/folderpattern.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "test/librarytest.h"
#include "track/globaltrackcache.h"
#include "track/track.h"

namespace {

using mixxx::TrashResult;
using mixxx::library::consolidateFile;
using mixxx::library::consolidateTargetPath;
using mixxx::library::ConsolidateOps;
using mixxx::library::ConsolidateStatus;
using mixxx::library::checkLibraryRoot;
using mixxx::library::hashFileContent;

bool writeFile(const QString& path, const QByteArray& content) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(content) == content.size();
}

QByteArray readFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

/// Fake system calls that count what the consolidation does. The copy and the
/// hash use the real disk (in a temp folder); the trash is a list.
struct FakeOps {
    int relocateCalls = 0;
    bool relocateResult = true;
    QString relocatedTo;
    QStringList trashed;
    QStringList removedCopies;
    TrashResult trashResult = TrashResult::Moved;
    // When set, the copy gets other content than the original.
    bool corruptCopy = false;
    // When set, the copy call writes half of the file and then fails.
    bool failCopyHalfWay = false;
    // When set, the hash of the original cannot be read.
    bool sourceUnreadable = false;
    bool syncResult = true;
    // What happened, in order: "copy", "sync", "hash:<name>", "relocate", "trash".
    QStringList events;
    // How many relocate calls had happened when the trash was called.
    int relocateCallsAtTrash = -1;

    ConsolidateOps ops() {
        ConsolidateOps result = mixxx::library::systemConsolidateOps(
                [this](const QString& newLocation) {
                    events << QStringLiteral("relocate");
                    ++relocateCalls;
                    relocatedTo = newLocation;
                    return relocateResult;
                });
        const auto realCopy = result.copyFile;
        const auto realHash = result.hashFile;
        result.copyFile = [this, realCopy](const QString& from, const QString& to) {
            events << QStringLiteral("copy");
            if (failCopyHalfWay) {
                writeFile(to, QByteArrayLiteral("music da"));
                return false;
            }
            if (!realCopy(from, to)) {
                return false;
            }
            if (corruptCopy) {
                return writeFile(to, QByteArrayLiteral("corrupt"));
            }
            return true;
        };
        result.syncCopy = [this](const QString&) {
            events << QStringLiteral("sync");
            return syncResult;
        };
        result.hashFile = [this, realHash](const QString& path) {
            events << QStringLiteral("hash:") + QFileInfo(path).path().section('/', -2);
            if (sourceUnreadable && path.contains(QStringLiteral("/old/"))) {
                return QByteArray();
            }
            return realHash(path);
        };
        result.removeCopy = [this](const QString& path) {
            removedCopies << path;
            return QFile::remove(path);
        };
        result.trashOriginal = [this](const QString& path) {
            events << QStringLiteral("trash");
            relocateCallsAtTrash = relocateCalls;
            if (trashResult == TrashResult::Moved) {
                trashed << path;
                QFile::remove(path);
            }
            return trashResult;
        };
        return result;
    }
};

class ConsolidaterTest : public MixxxTest {
  protected:
    QString path(const QString& relative) const {
        return getTestDataDir().filePath(relative);
    }
    const QByteArray content = QByteArrayLiteral("music data music data");
};

TEST_F(ConsolidaterTest, HashOfSameContentIsEqual) {
    ASSERT_TRUE(writeFile(path("a/one.mp3"), content));
    ASSERT_TRUE(writeFile(path("b/two.mp3"), content));
    ASSERT_TRUE(writeFile(path("b/three.mp3"), QByteArrayLiteral("other")));
    EXPECT_FALSE(hashFileContent(path("a/one.mp3")).isEmpty());
    EXPECT_EQ(hashFileContent(path("a/one.mp3")), hashFileContent(path("b/two.mp3")));
    EXPECT_NE(hashFileContent(path("a/one.mp3")), hashFileContent(path("b/three.mp3")));
    EXPECT_TRUE(hashFileContent(path("none.mp3")).isEmpty());
}

TEST_F(ConsolidaterTest, TargetPathKeepsTheFileName) {
    EXPECT_EQ(QStringLiteral("/lib/Daft Punk/One More Time.mp3"),
            consolidateTargetPath(QStringLiteral("/old/stuff/One More Time.mp3"),
                    QStringLiteral("/lib"),
                    QStringLiteral("Daft Punk")));
    EXPECT_EQ(QStringLiteral("/lib/One More Time.mp3"),
            consolidateTargetPath(QStringLiteral("/old/One More Time.mp3"),
                    QStringLiteral("/lib/"),
                    QString()));
}

TEST_F(ConsolidaterTest, CopiesChecksRelocatesAndTrashesTheOriginal) {
    const QString source = path("old/Song.mp3");
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, content));

    FakeOps fake;
    const auto outcome = consolidateFile(source, target, fake.ops());

    EXPECT_EQ(ConsolidateStatus::Consolidated, outcome.status);
    EXPECT_EQ(target, outcome.newLocation);
    ASSERT_TRUE(outcome.originalTrash.has_value());
    EXPECT_EQ(TrashResult::Moved, *outcome.originalTrash);
    EXPECT_EQ(content, readFile(target));
    EXPECT_FALSE(QFile::exists(source));
    EXPECT_EQ(QStringList{source}, fake.trashed);
    EXPECT_EQ(1, fake.relocateCalls);
    EXPECT_EQ(target, fake.relocatedTo);
    // The file name is the same.
    EXPECT_EQ(QFileInfo(source).fileName(), QFileInfo(target).fileName());
}

TEST_F(ConsolidaterTest, HashMismatchKeepsTheOriginal) {
    const QString source = path("old/Song.mp3");
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, content));

    FakeOps fake;
    fake.corruptCopy = true;
    const auto outcome = consolidateFile(source, target, fake.ops());

    EXPECT_EQ(ConsolidateStatus::HashMismatch, outcome.status);
    EXPECT_FALSE(outcome.originalTrash.has_value());
    EXPECT_EQ(content, readFile(source));
    EXPECT_FALSE(QFile::exists(target));
    EXPECT_EQ(QStringList{target}, fake.removedCopies);
    EXPECT_TRUE(fake.trashed.isEmpty());
    // The track location did not change.
    EXPECT_EQ(0, fake.relocateCalls);
}

TEST_F(ConsolidaterTest, RemovableOriginalStays) {
    const QString source = path("stick/Song.mp3");
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, content));

    FakeOps fake;
    fake.trashResult = TrashResult::RefusedUnsafe;
    const auto outcome = consolidateFile(source, target, fake.ops());

    // The track points to the copy, but the original is not touched.
    EXPECT_EQ(ConsolidateStatus::Consolidated, outcome.status);
    ASSERT_TRUE(outcome.originalTrash.has_value());
    EXPECT_EQ(TrashResult::RefusedUnsafe, *outcome.originalTrash);
    EXPECT_EQ(content, readFile(source));
    EXPECT_EQ(content, readFile(target));
    EXPECT_EQ(1, fake.relocateCalls);
}

TEST_F(ConsolidaterTest, RealTrashHelperRefusesAFileOnAnUnsafeVolume) {
    // The real trash helper with an injected volume check, so the whole path
    // from consolidateFile() to the helper is covered.
    const QString source = path("stick/Song.mp3");
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, content));

    int moveCalls = 0;
    ConsolidateOps ops = mixxx::library::systemConsolidateOps(
            [](const QString&) { return true; });
    ops.trashOriginal = [&moveCalls](const QString& file) {
        return mixxx::moveToTrashChecked(
                file,
                [](const QString&) { return false; }, // the volume is not safe
                [&moveCalls](const QString&) {
                    ++moveCalls;
                    return true;
                });
    };
    const auto outcome = consolidateFile(source, target, ops);
    EXPECT_EQ(ConsolidateStatus::Consolidated, outcome.status);
    EXPECT_EQ(0, moveCalls);
    EXPECT_TRUE(QFile::exists(source));
}

TEST_F(ConsolidaterTest, RelocateFailureKeepsTheOriginalAndRemovesTheCopy) {
    const QString source = path("old/Song.mp3");
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, content));

    FakeOps fake;
    fake.relocateResult = false;
    const auto outcome = consolidateFile(source, target, fake.ops());

    EXPECT_EQ(ConsolidateStatus::RelocateFailed, outcome.status);
    EXPECT_EQ(content, readFile(source));
    EXPECT_FALSE(QFile::exists(target));
    EXPECT_TRUE(fake.trashed.isEmpty());
}

TEST_F(ConsolidaterTest, TrashFailureIsNotAnErrorOfTheConsolidation) {
    const QString source = path("old/Song.mp3");
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, content));

    FakeOps fake;
    fake.trashResult = TrashResult::Failed;
    const auto outcome = consolidateFile(source, target, fake.ops());

    EXPECT_EQ(ConsolidateStatus::Consolidated, outcome.status);
    ASSERT_TRUE(outcome.originalTrash.has_value());
    EXPECT_EQ(TrashResult::Failed, *outcome.originalTrash);
    EXPECT_TRUE(QFile::exists(source));
    EXPECT_TRUE(QFile::exists(target));
}

TEST_F(ConsolidaterTest, CopyFailingHalfWayLeavesNothingBehind) {
    const QString source = path("old/Song.mp3");
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, content));

    FakeOps fake;
    fake.failCopyHalfWay = true;
    const auto outcome = consolidateFile(source, target, fake.ops());

    EXPECT_EQ(ConsolidateStatus::CopyFailed, outcome.status);
    EXPECT_FALSE(outcome.originalTrash.has_value());
    // No half file and no new folder stays.
    EXPECT_FALSE(QFile::exists(target));
    EXPECT_FALSE(QDir(path("lib/Artist")).exists());
    EXPECT_FALSE(QDir(path("lib")).exists());
    // The original and the library are unchanged.
    EXPECT_EQ(content, readFile(source));
    EXPECT_EQ(0, fake.relocateCalls);
    EXPECT_TRUE(fake.trashed.isEmpty());
}

TEST_F(ConsolidaterTest, FailedCopyKeepsFoldersThatExistedBefore) {
    const QString source = path("old/Song.mp3");
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, content));
    ASSERT_TRUE(QDir().mkpath(path("lib")));
    ASSERT_TRUE(writeFile(path("lib/Other.mp3"), content));

    FakeOps fake;
    fake.failCopyHalfWay = true;
    EXPECT_EQ(ConsolidateStatus::CopyFailed,
            consolidateFile(source, target, fake.ops()).status);
    EXPECT_FALSE(QDir(path("lib/Artist")).exists());
    EXPECT_TRUE(QFile::exists(path("lib/Other.mp3")));
}

TEST_F(ConsolidaterTest, UnreadableSourceHashKeepsTheOriginal) {
    const QString source = path("old/Song.mp3");
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, content));

    FakeOps fake;
    fake.sourceUnreadable = true;
    const auto outcome = consolidateFile(source, target, fake.ops());

    EXPECT_EQ(ConsolidateStatus::HashMismatch, outcome.status);
    EXPECT_EQ(content, readFile(source));
    EXPECT_FALSE(QFile::exists(target));
    EXPECT_EQ(0, fake.relocateCalls);
    EXPECT_TRUE(fake.trashed.isEmpty());
}

TEST_F(ConsolidaterTest, FailedSyncRemovesTheCopyAndKeepsTheOriginal) {
    const QString source = path("old/Song.mp3");
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, content));

    FakeOps fake;
    fake.syncResult = false;
    const auto outcome = consolidateFile(source, target, fake.ops());

    EXPECT_EQ(ConsolidateStatus::CopyFailed, outcome.status);
    EXPECT_EQ(content, readFile(source));
    EXPECT_FALSE(QFile::exists(target));
    EXPECT_EQ(0, fake.relocateCalls);
    EXPECT_TRUE(fake.trashed.isEmpty());
    // No hash was read before the sync succeeded.
    EXPECT_EQ((QStringList{QStringLiteral("copy"), QStringLiteral("sync")}), fake.events);
}

TEST_F(ConsolidaterTest, StepsRunInOrderAndTrashComesAfterRelocate) {
    const QString source = path("old/Song.mp3");
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, content));

    FakeOps fake;
    ASSERT_EQ(ConsolidateStatus::Consolidated,
            consolidateFile(source, target, fake.ops()).status);

    // Copy, flush to disk, hash of the original, hash of the copy (read
    // again), new location, and only then the trash.
    ASSERT_EQ(6, fake.events.size());
    EXPECT_EQ(QStringLiteral("copy"), fake.events[0]);
    EXPECT_EQ(QStringLiteral("sync"), fake.events[1]);
    EXPECT_TRUE(fake.events[2].startsWith(QStringLiteral("hash:")));
    EXPECT_TRUE(fake.events[3].startsWith(QStringLiteral("hash:")));
    EXPECT_EQ(QStringLiteral("relocate"), fake.events[4]);
    EXPECT_EQ(QStringLiteral("trash"), fake.events[5]);
    EXPECT_EQ(1, fake.relocateCallsAtTrash);
}

TEST_F(ConsolidaterTest, LibraryRootMustBeAbsoluteAndTrashSafe) {
    const auto safe = [](const QString&) { return true; };
    const auto unsafe = [](const QString&) { return false; };
    EXPECT_EQ(ConsolidateStatus::InvalidLibraryRoot, *checkLibraryRoot(QString(), safe));
    EXPECT_EQ(ConsolidateStatus::InvalidLibraryRoot, *checkLibraryRoot(QStringLiteral("  "), safe));
    EXPECT_EQ(ConsolidateStatus::InvalidLibraryRoot,
            *checkLibraryRoot(QStringLiteral("music/lib"), safe));
    EXPECT_EQ(ConsolidateStatus::UnsafeLibraryRoot, *checkLibraryRoot(path("lib"), unsafe));
    EXPECT_FALSE(checkLibraryRoot(path("lib"), safe).has_value());
}

TEST_F(ConsolidaterTest, MissingSourceChangesNothing) {
    FakeOps fake;
    const auto outcome = consolidateFile(
            path("old/none.mp3"), path("lib/none.mp3"), fake.ops());
    EXPECT_EQ(ConsolidateStatus::SourceMissing, outcome.status);
    EXPECT_FALSE(QFile::exists(path("lib/none.mp3")));
    EXPECT_EQ(0, fake.relocateCalls);
}

TEST_F(ConsolidaterTest, ExistingTargetIsNeverOverwritten) {
    const QString source = path("old/Song.mp3");
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, content));
    ASSERT_TRUE(writeFile(target, QByteArrayLiteral("another song")));

    FakeOps fake;
    const auto outcome = consolidateFile(source, target, fake.ops());

    EXPECT_EQ(ConsolidateStatus::TargetExists, outcome.status);
    EXPECT_EQ(QByteArrayLiteral("another song"), readFile(target));
    EXPECT_EQ(content, readFile(source));
    EXPECT_TRUE(fake.removedCopies.isEmpty());
    EXPECT_EQ(0, fake.relocateCalls);
}

TEST_F(ConsolidaterTest, FileAlreadyAtTargetIsLeftAlone) {
    const QString source = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, content));

    FakeOps fake;
    const auto outcome = consolidateFile(source, source, fake.ops());

    EXPECT_EQ(ConsolidateStatus::AlreadyInLibrary, outcome.status);
    EXPECT_EQ(content, readFile(source));
    EXPECT_TRUE(fake.trashed.isEmpty());
    EXPECT_EQ(0, fake.relocateCalls);
}

} // namespace

// The track location in the database. These tests are outside the unnamed
// namespace because TrackCollection names them as friends.
class ConsolidateTrackTest : public LibraryTest {
  protected:
    QString path(const QString& relative) const {
        return getTestDataDir().filePath(relative);
    }
    QString locationInDb(TrackId id) {
        QSqlQuery query(dbConnection());
        query.prepare(QStringLiteral(
                "SELECT track_locations.location, track_locations.filename, "
                "track_locations.directory FROM library INNER JOIN track_locations "
                "ON library.location=track_locations.id WHERE library.id=:id"));
        query.bindValue(":id", id.toVariant());
        EXPECT_TRUE(query.exec());
        EXPECT_TRUE(query.next());
        return query.value(0).toString() + '|' + query.value(1).toString() + '|' +
                query.value(2).toString();
    }
};

TEST_F(ConsolidateTrackTest, RelocateTrackLocationChangesTheDatabaseRow) {
    const QString source = path("old/Song.mp3");
    ASSERT_TRUE(writeFile(source, QByteArrayLiteral("x")));
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(target, QByteArrayLiteral("x")));

    TrackPointer pTrack = Track::newTemporary(mixxx::FileAccess(mixxx::FileInfo(source)));
    const TrackId id = internalCollection()->addTrack(pTrack, false);
    ASSERT_TRUE(id.isValid());

    ASSERT_TRUE(internalCollection()->relocateTrackLocation(id, target));

    const mixxx::FileInfo newInfo(target);
    EXPECT_EQ(newInfo.location() + '|' + newInfo.fileName() + '|' + newInfo.locationPath(),
            locationInDb(id));
    EXPECT_EQ(newInfo.location(),
            internalCollection()->getTrackDAO().getTrackLocation(id));
}

TEST_F(ConsolidateTrackTest, RelocateTrackLocationMovesTheCachedTrackToo) {
    const QString source = path("old/Song.mp3");
    const QString target = path("lib/Artist/Song.mp3");
    ASSERT_TRUE(writeFile(source, QByteArrayLiteral("x")));
    ASSERT_TRUE(writeFile(target, QByteArrayLiteral("x")));

    TrackPointer pTemp = Track::newTemporary(mixxx::FileAccess(mixxx::FileInfo(source)));
    const TrackId id = internalCollection()->addTrack(pTemp, false);
    ASSERT_TRUE(id.isValid());
    pTemp.reset();

    // A track that is in the track cache, as a track in a deck is.
    TrackPointer held = internalCollection()->getTrackById(id);
    ASSERT_TRUE(held);
    const mixxx::FileInfo newInfo(target);
    const mixxx::FileInfo oldInfo(source);
    ASSERT_EQ(oldInfo.location(), held->getLocation());

    ASSERT_TRUE(internalCollection()->relocateTrackLocation(id, target));

    // The held object has the new location.
    EXPECT_EQ(newInfo.location(), held->getLocation());
    // A new lookup by id gives the same object.
    EXPECT_EQ(held.get(), GlobalTrackCacheLocker().lookupTrackById(id).get());
    // A lookup by the new path gives the same object, not a second one.
    EXPECT_EQ(held.get(),
            GlobalTrackCacheLocker()
                    .lookupTrackByRef(TrackRef::fromFilePath(target))
                    .get());
    // The old path is not in the cache any more.
    EXPECT_FALSE(GlobalTrackCacheLocker().lookupTrackByRef(TrackRef::fromFilePath(source)));
    // Loading the track from the library gives the same object.
    EXPECT_EQ(held.get(), internalCollection()->getTrackById(id).get());
}

TEST_F(ConsolidateTrackTest, TrashRunsAfterTheLibraryHasTheNewLocation) {
    const QString source = path("old/Song.mp3");
    ASSERT_TRUE(writeFile(source, QByteArrayLiteral("song bytes")));
    TrackPointer pTrack = Track::newTemporary(mixxx::FileAccess(mixxx::FileInfo(source)));
    pTrack->setArtist(QStringLiteral("Daft Punk"));
    const TrackId id = internalCollection()->addTrack(pTrack, false);
    ASSERT_TRUE(id.isValid());

    QString locationAtTrash;
    QString trashedFile;
    const auto outcome = mixxx::library::consolidateTrack(
            pTrack,
            path("lib"),
            mixxx::library::defaultFolderPattern(),
            internalCollection(),
            [&](const QString& file) {
                trashedFile = file;
                locationAtTrash = internalCollection()->getTrackDAO().getTrackLocation(id);
                return TrashResult::Moved;
            });
    ASSERT_EQ(ConsolidateStatus::Consolidated, outcome.status);
    EXPECT_EQ(mixxx::FileInfo(source).location(), mixxx::FileInfo(trashedFile).location());
    // When the trash was called, the library already had the new location.
    EXPECT_EQ(mixxx::FileInfo(path("lib/Daft Punk/Song.mp3")).location(), locationAtTrash);
}

TEST_F(ConsolidateTrackTest, RelativeLibraryRootChangesNothing) {
    const QString source = path("old/Song.mp3");
    ASSERT_TRUE(writeFile(source, QByteArrayLiteral("song bytes")));
    TrackPointer pTrack = Track::newTemporary(mixxx::FileAccess(mixxx::FileInfo(source)));
    const TrackId id = internalCollection()->addTrack(pTrack, false);
    ASSERT_TRUE(id.isValid());
    bool trashCalled = false;
    const auto trash = [&trashCalled](const QString&) {
        trashCalled = true;
        return TrashResult::Moved;
    };
    for (const QString& root : {QString(), QStringLiteral("lib")}) {
        const auto outcome = mixxx::library::consolidateTrack(pTrack,
                root,
                mixxx::library::defaultFolderPattern(),
                internalCollection(),
                trash);
        EXPECT_EQ(ConsolidateStatus::InvalidLibraryRoot, outcome.status);
    }
    EXPECT_FALSE(trashCalled);
    EXPECT_TRUE(QFile::exists(source));
    EXPECT_EQ(mixxx::FileInfo(source).location(),
            internalCollection()->getTrackDAO().getTrackLocation(id));
}

TEST_F(ConsolidateTrackTest, RelocateTrackLocationRefusesALocationInUse) {
    const QString source = path("old/Song.mp3");
    const QString other = path("lib/Other.mp3");
    ASSERT_TRUE(writeFile(source, QByteArrayLiteral("x")));
    ASSERT_TRUE(writeFile(other, QByteArrayLiteral("y")));
    TrackPointer pTrack = Track::newTemporary(mixxx::FileAccess(mixxx::FileInfo(source)));
    TrackPointer pOther = Track::newTemporary(mixxx::FileAccess(mixxx::FileInfo(other)));
    const TrackId id = internalCollection()->addTrack(pTrack, false);
    internalCollection()->addTrack(pOther, false);

    const QString before = locationInDb(id);
    EXPECT_FALSE(internalCollection()->relocateTrackLocation(id, other));
    EXPECT_EQ(before, locationInDb(id));
    EXPECT_FALSE(internalCollection()->relocateTrackLocation(TrackId(), other));
}

TEST_F(ConsolidateTrackTest, ConsolidateTrackMovesTheFileAndTheLocation) {
    const QString source = path("old/Song.mp3");
    ASSERT_TRUE(writeFile(source, QByteArrayLiteral("song bytes")));

    TrackPointer pTrack = Track::newTemporary(mixxx::FileAccess(mixxx::FileInfo(source)));
    pTrack->setArtist(QStringLiteral("Daft Punk"));
    const TrackId id = internalCollection()->addTrack(pTrack, false);
    ASSERT_TRUE(id.isValid());

    // A fake trash, so that the test does not fill the real one.
    QStringList trashed;
    const QString root = path("lib");
    const auto outcome = mixxx::library::consolidateTrack(
            pTrack,
            root,
            mixxx::library::defaultFolderPattern(),
            internalCollection(),
            [&trashed](const QString& file) {
                trashed << file;
                return TrashResult::Moved;
            });

    ASSERT_EQ(ConsolidateStatus::Consolidated, outcome.status);
    const QString expected = mixxx::FileInfo(path("lib/Daft Punk/Song.mp3")).location();
    EXPECT_EQ(expected, mixxx::FileInfo(outcome.newLocation).location());
    EXPECT_EQ(QByteArrayLiteral("song bytes"), readFile(expected));
    EXPECT_EQ(expected, internalCollection()->getTrackDAO().getTrackLocation(id));
    EXPECT_EQ(QStringList{source}, trashed);
}

// End of AI-generated text.
