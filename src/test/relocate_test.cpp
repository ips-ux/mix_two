// AI-generated (Claude Code). Start of AI-generated text.
// Tests for Relocate and Auto Relocate of missing tracks. The files are in a
// temp folder. A relocation changes only the location that the library
// stores: the tests also check that no file on disk changes.

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "test/librarytest.h"
#include "track/track.h"

namespace {

using mixxx::library::decodeRelocateFolders;
using mixxx::library::encodeRelocateFolders;
using mixxx::library::findRelocateCandidates;
using mixxx::library::RelocateCandidate;

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

} // namespace

class RelocateTest : public LibraryTest {
  protected:
    QString path(const QString& relative) const {
        return getTestDataDir().filePath(relative);
    }

    /// Adds a track for a file that exists, then removes the file: the track
    /// is missing.
    TrackId addMissingTrack(const QString& relative, const QByteArray& content) {
        const QString file = path(relative);
        EXPECT_TRUE(writeFile(file, content));
        const TrackId id = addTrackFor(file);
        EXPECT_TRUE(QFile::remove(file));
        return id;
    }

    /// Adds a track for a file to the library.
    TrackId addTrackFor(const QString& file) {
        TrackPointer pTrack = getOrAddTrackByLocation(file);
        EXPECT_TRUE(pTrack);
        return pTrack ? pTrack->getId() : TrackId();
    }

    QString locationOf(TrackId id) {
        return internalCollection()->getTrackDAO().getTrackLocation(id);
    }

    QString expectedLocation(const QString& relative) const {
        return mixxx::FileInfo(path(relative)).location();
    }
};

TEST_F(RelocateTest, OneTrackGetsTheChosenFile) {
    const TrackId id = addMissingTrack("old/Song.mp3", QByteArrayLiteral("song-data"));
    ASSERT_TRUE(writeFile(path("new/Song.mp3"), QByteArrayLiteral("song-data")));

    const auto outcome = internalCollection()->relocateTracksToFile(
            {id}, path("new/Song.mp3"));

    EXPECT_EQ(1, outcome.relocated);
    EXPECT_EQ(0, outcome.notFound);
    // The track keeps its id and points at the new file.
    EXPECT_EQ(expectedLocation("new/Song.mp3"), locationOf(id));
    // The file on disk is as it was.
    EXPECT_EQ(QByteArrayLiteral("song-data"), readFile(path("new/Song.mp3")));
}

TEST_F(RelocateTest, TheChosenFileMayHaveAnotherName) {
    const TrackId id = addMissingTrack("old/Song.mp3", QByteArrayLiteral("x"));
    ASSERT_TRUE(writeFile(path("new/Song (renamed).mp3"), QByteArrayLiteral("x")));

    const auto outcome = internalCollection()->relocateTracksToFile(
            {id}, path("new/Song (renamed).mp3"));

    EXPECT_EQ(1, outcome.relocated);
    EXPECT_EQ(expectedLocation("new/Song (renamed).mp3"), locationOf(id));
}

TEST_F(RelocateTest, ThreeTracksFindTheirFilesInTheFolderOfTheFirst) {
    const TrackId a = addMissingTrack("old/a.mp3", QByteArrayLiteral("aaa"));
    const TrackId b = addMissingTrack("old/sub/b.mp3", QByteArrayLiteral("bbb"));
    const TrackId c = addMissingTrack("other/c.mp3", QByteArrayLiteral("ccc"));
    ASSERT_TRUE(writeFile(path("new/a.mp3"), QByteArrayLiteral("aaa")));
    ASSERT_TRUE(writeFile(path("new/b.mp3"), QByteArrayLiteral("bbb")));
    ASSERT_TRUE(writeFile(path("new/c.mp3"), QByteArrayLiteral("ccc")));

    const auto outcome = internalCollection()->relocateTracksToFile(
            {a, b, c}, path("new/a.mp3"));

    EXPECT_EQ(3, outcome.relocated);
    EXPECT_EQ(expectedLocation("new/a.mp3"), locationOf(a));
    EXPECT_EQ(expectedLocation("new/b.mp3"), locationOf(b));
    EXPECT_EQ(expectedLocation("new/c.mp3"), locationOf(c));
}

TEST_F(RelocateTest, ATrackWithoutAFileInTheFolderStaysMissing) {
    const TrackId a = addMissingTrack("old/a.mp3", QByteArrayLiteral("aaa"));
    const TrackId b = addMissingTrack("old/b.mp3", QByteArrayLiteral("bbb"));
    ASSERT_TRUE(writeFile(path("new/a.mp3"), QByteArrayLiteral("aaa")));
    const QString oldB = locationOf(b);

    const auto outcome = internalCollection()->relocateTracksToFile(
            {a, b}, path("new/a.mp3"));

    EXPECT_EQ(1, outcome.relocated);
    EXPECT_EQ(1, outcome.notFound);
    EXPECT_EQ(oldB, locationOf(b));
}

TEST_F(RelocateTest, ATrackWhoseFileIsPresentIsNotChanged) {
    const TrackId missing = addMissingTrack("old/a.mp3", QByteArrayLiteral("aaa"));
    // A second track that still has its file.
    ASSERT_TRUE(writeFile(path("here/b.mp3"), QByteArrayLiteral("bbb")));
    const TrackId present = addTrackFor(path("here/b.mp3"));
    ASSERT_TRUE(present.isValid());
    ASSERT_TRUE(writeFile(path("new/a.mp3"), QByteArrayLiteral("aaa")));
    ASSERT_TRUE(writeFile(path("new/b.mp3"), QByteArrayLiteral("bbb")));
    const QString oldPresent = locationOf(present);

    const auto outcome = internalCollection()->relocateTracksToFile(
            {missing, present}, path("new/a.mp3"));

    EXPECT_EQ(1, outcome.relocated);
    EXPECT_EQ(1, outcome.skipped);
    EXPECT_EQ(oldPresent, locationOf(present));
}

TEST_F(RelocateTest, AFileThatAnotherTrackUsesIsRefused) {
    const TrackId missing = addMissingTrack("old/a.mp3", QByteArrayLiteral("aaa"));
    ASSERT_TRUE(writeFile(path("new/a.mp3"), QByteArrayLiteral("aaa")));
    ASSERT_TRUE(addTrackFor(path("new/a.mp3")).isValid());
    const QString oldLocation = locationOf(missing);

    const auto outcome = internalCollection()->relocateTracksToFile(
            {missing}, path("new/a.mp3"));

    EXPECT_EQ(0, outcome.relocated);
    EXPECT_EQ(1, outcome.failed);
    EXPECT_EQ(oldLocation, locationOf(missing));
}

TEST_F(RelocateTest, ChoosingAFileThatDoesNotExistChangesNothing) {
    const TrackId id = addMissingTrack("old/a.mp3", QByteArrayLiteral("aaa"));
    const QString oldLocation = locationOf(id);

    const auto outcome = internalCollection()->relocateTracksToFile(
            {id}, path("new/nothing.mp3"));

    EXPECT_EQ(0, outcome.relocated);
    EXPECT_EQ(oldLocation, locationOf(id));
}

TEST_F(RelocateTest, AutoRelocateFindsAMovedFileInASearchFolder) {
    const TrackId id = addMissingTrack("old/Song.mp3", QByteArrayLiteral("song-data"));
    ASSERT_TRUE(writeFile(path("search/deep/er/Song.mp3"), QByteArrayLiteral("song-data")));

    const auto candidates = findRelocateCandidates({path("search")});
    const auto outcome = internalCollection()->autoRelocateTracks({id}, candidates);

    EXPECT_EQ(1, outcome.relocated);
    EXPECT_EQ(expectedLocation("search/deep/er/Song.mp3"), locationOf(id));
    EXPECT_EQ(QByteArrayLiteral("song-data"), readFile(path("search/deep/er/Song.mp3")));
}

TEST_F(RelocateTest, AutoRelocateDoesNotSearchAFolderOutsideTheList) {
    const TrackId id = addMissingTrack("old/Song.mp3", QByteArrayLiteral("song-data"));
    ASSERT_TRUE(writeFile(path("listed/Other.mp3"), QByteArrayLiteral("other")));
    ASSERT_TRUE(writeFile(path("unlisted/Song.mp3"), QByteArrayLiteral("song-data")));
    const QString oldLocation = locationOf(id);

    const auto candidates = findRelocateCandidates({path("listed")});
    const auto outcome = internalCollection()->autoRelocateTracks({id}, candidates);

    EXPECT_EQ(0, outcome.relocated);
    EXPECT_EQ(1, outcome.notFound);
    EXPECT_EQ(oldLocation, locationOf(id));
}

TEST_F(RelocateTest, AutoRelocateNeedsTheSameSize) {
    const TrackId id = addMissingTrack("old/Song.mp3", QByteArrayLiteral("song-data"));
    ASSERT_TRUE(writeFile(path("search/Song.mp3"), QByteArrayLiteral("a different song")));
    const QString oldLocation = locationOf(id);

    const auto outcome = internalCollection()->autoRelocateTracks(
            {id}, findRelocateCandidates({path("search")}));

    EXPECT_EQ(0, outcome.relocated);
    EXPECT_EQ(1, outcome.notFound);
    EXPECT_EQ(oldLocation, locationOf(id));
}

TEST_F(RelocateTest, AutoRelocatePrefersTheFolderThatLooksLikeTheOldOne) {
    const TrackId id = addMissingTrack("music/Abba/Hits/Waterloo.mp3", QByteArrayLiteral("waterloo"));
    ASSERT_TRUE(writeFile(path("search/Falko/Waterloo.mp3"), QByteArrayLiteral("waterloo")));
    ASSERT_TRUE(writeFile(path("search/Abba/Hits/Waterloo.mp3"), QByteArrayLiteral("waterloo")));

    const auto outcome = internalCollection()->autoRelocateTracks(
            {id}, findRelocateCandidates({path("search")}));

    EXPECT_EQ(1, outcome.relocated);
    EXPECT_EQ(expectedLocation("search/Abba/Hits/Waterloo.mp3"), locationOf(id));
}

TEST_F(RelocateTest, AutoRelocateGivesOneFileToOneTrackOnly) {
    const TrackId first = addMissingTrack("old1/Song.mp3", QByteArrayLiteral("same"));
    const TrackId second = addMissingTrack("old2/Song.mp3", QByteArrayLiteral("same"));
    ASSERT_TRUE(writeFile(path("search/Song.mp3"), QByteArrayLiteral("same")));

    const auto outcome = internalCollection()->autoRelocateTracks(
            {first, second}, findRelocateCandidates({path("search")}));

    EXPECT_EQ(1, outcome.relocated);
    EXPECT_EQ(1, outcome.notFound);
}

TEST_F(RelocateTest, AutoRelocateSkipsATrackWhoseFileIsPresent) {
    ASSERT_TRUE(writeFile(path("here/Song.mp3"), QByteArrayLiteral("song-data")));
    const TrackId id = addTrackFor(path("here/Song.mp3"));
    ASSERT_TRUE(writeFile(path("search/Song.mp3"), QByteArrayLiteral("song-data")));
    const QString oldLocation = locationOf(id);

    const auto outcome = internalCollection()->autoRelocateTracks(
            {id}, findRelocateCandidates({path("search")}));

    EXPECT_EQ(0, outcome.relocated);
    EXPECT_EQ(1, outcome.skipped);
    EXPECT_EQ(oldLocation, locationOf(id));
}

TEST_F(RelocateTest, CandidatesAreAudioFilesOfTheListedFolders) {
    ASSERT_TRUE(writeFile(path("search/a.mp3"), QByteArrayLiteral("12345")));
    ASSERT_TRUE(writeFile(path("search/sub/b.flac"), QByteArrayLiteral("123")));
    ASSERT_TRUE(writeFile(path("search/notes.txt"), QByteArrayLiteral("text")));
    ASSERT_TRUE(writeFile(path("elsewhere/c.mp3"), QByteArrayLiteral("1")));

    const auto candidates = findRelocateCandidates({path("search"), path("missing-folder")});

    QStringList names;
    for (const RelocateCandidate& candidate : candidates) {
        names << candidate.fileName;
        if (candidate.fileName == QLatin1String("a.mp3")) {
            EXPECT_EQ(5, candidate.sizeInBytes);
        }
    }
    names.sort();
    EXPECT_EQ((QStringList{"a.mp3", "b.flac"}), names);
}

TEST_F(RelocateTest, TheFolderListRoundTrips) {
    const QStringList folders{"C:/Music", "D:/with \"quote\"", "/home/dj/mix|set"};
    EXPECT_EQ(folders, decodeRelocateFolders(encodeRelocateFolders(folders)));
    EXPECT_TRUE(decodeRelocateFolders(QString()).isEmpty());
    EXPECT_TRUE(decodeRelocateFolders(QStringLiteral("not json")).isEmpty());
    EXPECT_TRUE(decodeRelocateFolders(encodeRelocateFolders({})).isEmpty());
}
// End of AI-generated text.
