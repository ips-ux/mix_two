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

TEST_F(ImportFilesTest, ChunkedImportGivesTheSameResultAsOneCall) {
    QStringList files;
    for (int i = 0; i < 5; ++i) {
        files << makeSoundFile(QStringLiteral("t%1.mp3").arg(i), i % 2 ? kSound1 : kSound2);
    }
    mixxx::library::ImportFilesResult total;
    int steps = 0;
    for (const QStringList& chunk : mixxx::library::splitImportList(files, 2)) {
        total += trackCollectionManager()->importFiles(chunk);
        ++steps;
    }
    EXPECT_EQ(3, steps);
    EXPECT_EQ(5, total.added + total.alreadyInLibrary);
}
// End of AI-generated text.
