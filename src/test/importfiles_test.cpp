// AI-generated (Claude Code). Start of AI-generated text.
// Tests for "Import Track", "Import Folder" and "Import To Collection": the
// import adds tracks to the library and never moves, copies or changes a file.
#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "test/librarytest.h"
#include "track/globaltrackcache.h"
#include "track/track.h"

namespace {

class ImportFilesTest : public LibraryTest {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_dir.isValid());
    }

    // Copies a sound file of the test data into the temporary folder.
    QString makeSoundFile(const QString& relativeTarget, const QString& source) {
        const QString target = m_dir.filePath(relativeTarget);
        QDir().mkpath(QFileInfo(target).absolutePath());
        EXPECT_TRUE(QFile::copy(getTestDir().filePath(source), target));
        return target;
    }

    QString makeTextFile(const QString& relativeTarget) {
        const QString target = m_dir.filePath(relativeTarget);
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile file(target);
        EXPECT_TRUE(file.open(QIODevice::WriteOnly));
        file.write("not music");
        return target;
    }

    int count(const QString& table) {
        QSqlQuery query(dbConnection());
        EXPECT_TRUE(query.exec("SELECT COUNT(*) FROM " + table));
        query.next();
        return query.value(0).toInt();
    }

    static QByteArray readAll(const QString& path) {
        QFile file(path);
        EXPECT_TRUE(file.open(QIODevice::ReadOnly));
        return file.readAll();
    }

    QTemporaryDir m_dir;
};

const QString kSound1 = QStringLiteral("id3-test-data/artist.mp3");
const QString kSound2 = QStringLiteral("id3-test-data/all.mp3");

} // namespace

TEST_F(ImportFilesTest, ImportAddsTracksAndNoDirectoryRow) {
    const QString a = makeSoundFile("a.mp3", kSound1);
    const QString b = makeSoundFile("b.mp3", kSound2);
    const int tracksBefore = count("library");
    const int directoriesBefore = count("directories");

    const auto result = trackCollectionManager()->importFiles({a, b});

    EXPECT_EQ(2, result.added);
    EXPECT_EQ(0, result.alreadyInLibrary);
    EXPECT_EQ(0, result.skipped);
    EXPECT_EQ(tracksBefore + 2, count("library"));
    EXPECT_EQ(directoriesBefore, count("directories"));
}

TEST_F(ImportFilesTest, ImportNeverChangesOrMovesTheFile) {
    const QString a = makeSoundFile("sub/a.mp3", kSound1);
    const QByteArray before = readAll(a);

    const auto result = trackCollectionManager()->importFiles({a});

    ASSERT_EQ(1, result.added);
    EXPECT_TRUE(QFileInfo::exists(a));
    EXPECT_EQ(before, readAll(a));
    // Nothing else appears in the folder: no copy, no sidecar file.
    EXPECT_EQ(QStringList{QStringLiteral("a.mp3")}, QDir(m_dir.filePath("sub")).entryList(QDir::Files));
}

TEST_F(ImportFilesTest, FileThatIsInTheLibraryIsNotAddedTwice) {
    const QString a = makeSoundFile("a.mp3", kSound1);

    EXPECT_EQ(1, trackCollectionManager()->importFiles({a}).added);
    const int tracksAfterFirst = count("library");
    const auto second = trackCollectionManager()->importFiles({a});

    EXPECT_EQ(0, second.added);
    EXPECT_EQ(1, second.alreadyInLibrary);
    EXPECT_EQ(tracksAfterFirst, count("library"));
}

TEST_F(ImportFilesTest, SameFileTwiceInOneCallIsAddedOnce) {
    const QString a = makeSoundFile("a.mp3", kSound1);

    const auto result = trackCollectionManager()->importFiles({a, a});

    EXPECT_EQ(1, result.added);
    EXPECT_EQ(1, result.alreadyInLibrary);
}

TEST_F(ImportFilesTest, MissingAndUnsupportedFilesAreSkipped) {
    const QString text = makeTextFile("notes.txt");
    const QString missing = m_dir.filePath("gone.mp3");
    const int tracksBefore = count("library");

    const auto result = trackCollectionManager()->importFiles({text, missing});

    EXPECT_EQ(0, result.added);
    EXPECT_EQ(2, result.skipped);
    EXPECT_EQ(tracksBefore, count("library"));
}

TEST_F(ImportFilesTest, FolderListHasSoundFilesOfAllLevelsInOrder) {
    const QString b = makeSoundFile("b.mp3", kSound1);
    const QString a = makeSoundFile("deep/er/a.mp3", kSound2);
    makeTextFile("readme.txt");

    const QStringList files = mixxx::library::soundFilesInFolder(m_dir.path());

    ASSERT_EQ(2, files.size());
    EXPECT_EQ(QFileInfo(b).absoluteFilePath(), QFileInfo(files.at(0)).absoluteFilePath());
    EXPECT_EQ(QFileInfo(a).absoluteFilePath(), QFileInfo(files.at(1)).absoluteFilePath());
}

TEST_F(ImportFilesTest, FolderListOfMissingFolderIsEmpty) {
    EXPECT_TRUE(mixxx::library::soundFilesInFolder(m_dir.filePath("nothing")).isEmpty());
}

TEST_F(ImportFilesTest, ImportWithTimeBudgetHandlesFilesOneStepAtATime) {
    QStringList files;
    for (int i = 0; i < 5; ++i) {
        files << makeSoundFile(QStringLiteral("t%1.mp3").arg(i), i % 2 ? kSound1 : kSound2);
    }
    auto index = trackCollectionManager()->makeImportIndex();
    mixxx::library::ImportFilesResult total;
    int pos = 0;
    int steps = 0;
    while (pos < files.size()) {
        int processed = 0;
        // A budget of zero still handles one file, so a step always advances.
        total += trackCollectionManager()->importFiles(
                files.mid(pos), &index, std::chrono::milliseconds(0), &processed);
        ASSERT_EQ(1, processed);
        pos += processed;
        ++steps;
    }
    EXPECT_EQ(5, steps);
    EXPECT_EQ(5, total.added);
    EXPECT_EQ(0, total.alreadyInLibrary);
}

TEST_F(ImportFilesTest, HiddenTrackIsShownAgainWithItsData) {
    const QString a = makeSoundFile("a.mp3", kSound1);
    ASSERT_EQ(1, trackCollectionManager()->importFiles({a}).added);
    const QList<TrackId> ids = trackCollectionManager()->resolveTrackIdsFromLocations({a});
    ASSERT_EQ(1, ids.size());
    ASSERT_TRUE(trackCollectionManager()->hideTracks(ids));
    EXPECT_EQ(1, count("library WHERE mixxx_deleted=1"));
    const int tracksBefore = count("library");

    const auto result = trackCollectionManager()->importFiles({a});

    EXPECT_EQ(0, result.added);
    EXPECT_EQ(0, result.alreadyInLibrary);
    EXPECT_EQ(1, result.shownAgain);
    EXPECT_EQ(0, count("library WHERE mixxx_deleted=1"));
    // The same row: the id and the data stay.
    EXPECT_EQ(tracksBefore, count("library"));
    EXPECT_EQ(ids, trackCollectionManager()->resolveTrackIdsFromLocations({a}));
}

TEST_F(ImportFilesTest, SameFileUnderAnotherPathIsNotAddedTwice) {
    const QString a = makeSoundFile("a.mp3", kSound1);
    // A second real file stands for a second spelling of the path (another
    // letter case, a link): the injected resolver says that both are one file.
    const QString alias = makeSoundFile("alias_of_a.mp3", kSound1);
    const auto resolver = [a, alias](const QString& path) {
        return path == alias ? a : path;
    };
    auto index = trackCollectionManager()->makeImportIndex(resolver);

    EXPECT_EQ(1, trackCollectionManager()->importFiles({a}, &index).added);
    const int tracksAfterFirst = count("library");
    const auto second = trackCollectionManager()->importFiles({alias}, &index);

    EXPECT_EQ(0, second.added);
    EXPECT_EQ(1, second.alreadyInLibrary);
    EXPECT_EQ(tracksAfterFirst, count("library"));
}

TEST_F(ImportFilesTest, IndexFromTheLibraryFindsTheTrackUnderAnotherPath) {
    // The track is in the database and not loaded in the track cache.
    const QString a = makeSoundFile("a.mp3", kSound1);
    ASSERT_EQ(1, trackCollectionManager()->importFiles({a}).added);
    const QString alias = makeSoundFile("alias_of_a.mp3", kSound1);
    const auto resolver = [a, alias](const QString& path) {
        return path == alias ? a : path;
    };
    auto index = trackCollectionManager()->makeImportIndex(resolver);

    EXPECT_EQ(1, trackCollectionManager()->importFiles({alias}, &index).alreadyInLibrary);
    EXPECT_EQ(1, count("library"));
}

#ifdef Q_OS_WIN
TEST_F(ImportFilesTest, DifferentLetterCaseIsTheSameFileOnWindows) {
    const QString a = makeSoundFile("Mixed.mp3", kSound1);
    EXPECT_TRUE(mixxx::library::isSameImportPath(
            mixxx::library::canonicalImportPath(a),
            mixxx::library::canonicalImportPath(a.toUpper())));
    ASSERT_EQ(1, trackCollectionManager()->importFiles({a}).added);
    const auto result = trackCollectionManager()->importFiles({a.toUpper()});
    EXPECT_EQ(0, result.added);
    EXPECT_EQ(1, result.alreadyInLibrary);
    EXPECT_EQ(1, count("library"));
}
#endif

// End of AI-generated text.
