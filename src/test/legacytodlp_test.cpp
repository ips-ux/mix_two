// AI-generated (Claude Code). Start of AI-generated text.

#include <gtest/gtest.h>

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QTemporaryDir>
#include <utility>

#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/legacyfromdlp.h"
#include "library/rekordbox/legacylibrary.h"
#include "library/rekordbox/legacytodlp.h"

namespace mixxx::rekordbox {

namespace {

const QString kLegacyPath = QStringLiteral("PIONEER/rekordbox/export.pdb");
const QString kLegacyExtPath = QStringLiteral("PIONEER/rekordbox/exportExt.pdb");
const QString kPlusPath = QStringLiteral("PIONEER/rekordbox/exportLibrary.db");

/// The 8 colours of a new Device Library Plus, as legacyFromDlp() reads them.
QList<LegacyNamedRow> defaultColors() {
    QTemporaryDir stick;
    auto pDb = DeviceLibraryPlusDb::create(stick.path(), QStringLiteral("Colors"), 1, nullptr);
    if (!pDb) {
        return {};
    }
    const auto library = legacyFromDlp(*pDb, nullptr);
    return library ? library->colors : QList<LegacyNamedRow>{};
}

/// A legacy library with dense IDs (1 and up, in list order), so that
/// legacyFromDlp() gives the same IDs back.
LegacyLibrary sampleLibrary() {
    LegacyLibrary library;
    library.artists = {{1, "Artist"}, {2, "Composer"}, {3, "Original"}, {4, "Remixer"}};
    library.genres = {{1, "House"}, {2, "Techno"}};
    library.albums = {{1, 1, "Album"}, {2, 0, "No Artist Album"}};
    library.labels = {{1, "Label"}};
    library.keys = {{1, "Dm"}, {2, "8A"}};
    library.colors = defaultColors();
    library.artwork = {{1, "/PIONEER/Artwork/00001/a1.jpg"}};
    library.playlists = {{1, 0, 0, true, "Folder"},
            {2, 1, 0, false, "First"},
            {3, 1, 1, false, "Second"},
            {4, 0, 1, false, "Top"}};
    library.playlistEntries = {{2, 1, 1}, {2, 3, 2}, {2, 1, 3}, {3, 2, 1}, {4, 3, 1}};
    library.historyPlaylists = {{1, "HISTORY 001"}};
    library.historyEntries = {{1, 2, 1}, {1, 1, 2}};

    LegacyTrack track;
    track.id = 1;
    track.title = "Title";
    track.artistId = 1;
    track.composerId = 2;
    track.originalArtistId = 3;
    track.remixerId = 4;
    track.albumId = 1;
    track.genreId = 1;
    track.labelId = 1;
    track.keyId = 1;
    track.colorId = 2;
    track.artworkId = 1;
    track.tempo = 12843;
    track.duration = 207;
    track.rating = 4;
    track.year = 2001;
    track.trackNumber = 3;
    track.discNumber = 2;
    track.playCount = 11;
    track.bitrate = 320;
    track.sampleRate = 44100;
    track.sampleDepth = 16;
    track.fileSize = 8;
    track.fileType = 1;
    track.filePath = "/Contents/Artist/Album/a.mp3";
    track.fileName = "a.mp3";
    track.analyzePath = "/PIONEER/USBANLZ/P016/0000875E/ANLZ0000.DAT";
    track.analyzeDate = "2026-11-23";
    track.dateAdded = "2026-11-24";
    track.releaseDate = "2001-01-02";
    track.mixName = "Club Mix";
    track.comment = "Test comment";
    track.isrc = "USABC0100001";
    track.message = "Message";
    library.tracks.append(track);

    track.id = 2;
    track.title = QStringLiteral("\u65e5\u672c\U0001F3B5");
    track.albumId = 2;
    track.genreId = 2;
    track.keyId = 2;
    track.artworkId = 0;
    track.colorId = 0;
    track.autoLoadHotCues = false;
    track.fileType = 11;
    track.filePath = "/Contents/b.wav";
    track.fileName = "b.wav";
    track.analyzePath = "/PIONEER/USBANLZ/P02A/00001234/ANLZ0000.DAT";
    library.tracks.append(track);

    track = {};
    track.id = 3;
    track.title = "Plain";
    track.fileType = 5;
    track.fileSize = 8;
    track.filePath = "/Contents/c.flac";
    track.fileName = "c.flac";
    track.analyzePath = "/PIONEER/USBANLZ/P03B/00005678/ANLZ0000.DAT";
    library.tracks.append(track);
    return library;
}

void expectSameTrack(const LegacyTrack& actual, const LegacyTrack& expected) {
    SCOPED_TRACE(expected.id);
    EXPECT_EQ(actual.id, expected.id);
    EXPECT_EQ(actual.title, expected.title);
    EXPECT_EQ(actual.artistId, expected.artistId);
    EXPECT_EQ(actual.composerId, expected.composerId);
    EXPECT_EQ(actual.originalArtistId, expected.originalArtistId);
    EXPECT_EQ(actual.remixerId, expected.remixerId);
    EXPECT_EQ(actual.albumId, expected.albumId);
    EXPECT_EQ(actual.genreId, expected.genreId);
    EXPECT_EQ(actual.labelId, expected.labelId);
    EXPECT_EQ(actual.keyId, expected.keyId);
    EXPECT_EQ(actual.colorId, expected.colorId);
    EXPECT_EQ(actual.artworkId, expected.artworkId);
    EXPECT_EQ(actual.tempo, expected.tempo);
    EXPECT_EQ(actual.duration, expected.duration);
    EXPECT_EQ(actual.rating, expected.rating);
    EXPECT_EQ(actual.year, expected.year);
    EXPECT_EQ(actual.trackNumber, expected.trackNumber);
    EXPECT_EQ(actual.discNumber, expected.discNumber);
    EXPECT_EQ(actual.playCount, expected.playCount);
    EXPECT_EQ(actual.bitrate, expected.bitrate);
    EXPECT_EQ(actual.sampleRate, expected.sampleRate);
    EXPECT_EQ(actual.sampleDepth, expected.sampleDepth);
    EXPECT_EQ(actual.fileSize, expected.fileSize);
    EXPECT_EQ(actual.fileType, expected.fileType);
    EXPECT_EQ(actual.filePath, expected.filePath);
    EXPECT_EQ(actual.fileName, expected.fileName);
    EXPECT_EQ(actual.analyzePath, expected.analyzePath);
    EXPECT_EQ(actual.analyzeDate, expected.analyzeDate);
    EXPECT_EQ(actual.dateAdded, expected.dateAdded);
    EXPECT_EQ(actual.releaseDate, expected.releaseDate);
    EXPECT_EQ(actual.mixName, expected.mixName);
    EXPECT_EQ(actual.comment, expected.comment);
    EXPECT_EQ(actual.isrc, expected.isrc);
    EXPECT_EQ(actual.message, expected.message);
    EXPECT_EQ(actual.autoLoadHotCues, expected.autoLoadHotCues);
}

template<typename Row, typename Compare>
void expectSameRows(const QList<Row>& actual, const QList<Row>& expected, Compare compare) {
    ASSERT_EQ(actual.size(), expected.size());
    for (qsizetype i = 0; i < expected.size(); ++i) {
        SCOPED_TRACE(i);
        compare(actual[i], expected[i]);
    }
}

void expectSameLibrary(const LegacyLibrary& actual, const LegacyLibrary& expected) {
    const auto named = [](const LegacyNamedRow& a, const LegacyNamedRow& e) {
        EXPECT_EQ(a.id, e.id);
        EXPECT_EQ(a.name, e.name);
    };
    expectSameRows(actual.tracks, expected.tracks, expectSameTrack);
    {
        SCOPED_TRACE("artists");
        expectSameRows(actual.artists, expected.artists, named);
    }
    {
        SCOPED_TRACE("genres");
        expectSameRows(actual.genres, expected.genres, named);
    }
    {
        SCOPED_TRACE("keys");
        expectSameRows(actual.keys, expected.keys, named);
    }
    {
        SCOPED_TRACE("labels");
        expectSameRows(actual.labels, expected.labels, named);
    }
    {
        SCOPED_TRACE("colors");
        expectSameRows(actual.colors, expected.colors, named);
    }
    expectSameRows(actual.albums, expected.albums, [](const LegacyAlbum& a, const LegacyAlbum& e) {
        EXPECT_EQ(a.id, e.id);
        EXPECT_EQ(a.artistId, e.artistId);
        EXPECT_EQ(a.name, e.name);
    });
    expectSameRows(actual.artwork,
            expected.artwork,
            [](const LegacyArtwork& a, const LegacyArtwork& e) {
                EXPECT_EQ(a.id, e.id);
                EXPECT_EQ(a.path, e.path);
            });
    expectSameRows(actual.playlists,
            expected.playlists,
            [](const LegacyPlaylistNode& a, const LegacyPlaylistNode& e) {
                EXPECT_EQ(a.id, e.id);
                EXPECT_EQ(a.parentId, e.parentId);
                EXPECT_EQ(a.sortOrder, e.sortOrder);
                EXPECT_EQ(a.isFolder, e.isFolder);
                EXPECT_EQ(a.name, e.name);
            });
    expectSameRows(actual.playlistEntries,
            expected.playlistEntries,
            [](const LegacyPlaylistEntry& a, const LegacyPlaylistEntry& e) {
                EXPECT_EQ(a.playlistId, e.playlistId);
                EXPECT_EQ(a.trackId, e.trackId);
                EXPECT_EQ(a.entryIndex, e.entryIndex);
            });
    expectSameRows(actual.historyPlaylists,
            expected.historyPlaylists,
            [](const LegacyHistoryPlaylist& a, const LegacyHistoryPlaylist& e) {
                EXPECT_EQ(a.id, e.id);
                EXPECT_EQ(a.name, e.name);
            });
    expectSameRows(actual.historyEntries,
            expected.historyEntries,
            [](const LegacyHistoryEntry& a, const LegacyHistoryEntry& e) {
                EXPECT_EQ(a.playlistId, e.playlistId);
                EXPECT_EQ(a.trackId, e.trackId);
                EXPECT_EQ(a.entryIndex, e.entryIndex);
            });
}

bool writeFile(const QString& path, const QByteArray& bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray readFile(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/// Each file of a folder (path from the folder) and the SHA-256 of its bytes.
QMap<QString, QByteArray> snapshot(const QString& root) {
    QMap<QString, QByteArray> files;
    QDirIterator it(root,
            QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
            QDirIterator::Subdirectories);
    const QDir rootDir(root);
    while (it.hasNext()) {
        const QString path = it.next();
        files.insert(rootDir.relativeFilePath(path),
                QCryptographicHash::hash(readFile(path), QCryptographicHash::Sha256));
    }
    return files;
}

} // namespace

class LegacyToDlpTest : public testing::Test {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_stick.isValid());
        ASSERT_TRUE(m_backups.isValid());
        m_library = sampleLibrary();
        ASSERT_EQ(8, m_library.colors.size());
    }

    /// A stick with the legacy library of m_library, its audio, ANLZ and
    /// artwork files, and a file of the DJ that the update must not touch.
    void makeLegacyStick(bool withExt) {
        m_pdbBytes = legacyPdbBytes(m_library);
        ASSERT_FALSE(m_pdbBytes.isEmpty());
        ASSERT_TRUE(writeFile(stickPath(kLegacyPath), m_pdbBytes));
        if (withExt) {
            ASSERT_TRUE(writeFile(stickPath(kLegacyExtPath), QByteArray("ext bytes")));
        }
        for (const auto& track : std::as_const(m_library.tracks)) {
            ASSERT_TRUE(writeFile(stickPath(track.filePath), QByteArray("audio-") + track.fileName.toUtf8()));
            ASSERT_TRUE(writeFile(stickPath(track.analyzePath), QByteArray("PMAI")));
            QString ext = track.analyzePath;
            ext.replace(QStringLiteral(".DAT"), QStringLiteral(".EXT"));
            ASSERT_TRUE(writeFile(stickPath(ext), QByteArray("PMAI-EXT")));
        }
        ASSERT_TRUE(writeFile(stickPath(m_library.artwork.first().path), QByteArray("jpeg")));
        ASSERT_TRUE(writeFile(stickPath(QStringLiteral("Notes/readme.txt")), QByteArray("mine")));
    }

    QString stickPath(QString relative) const {
        if (relative.startsWith(QLatin1Char('/'))) {
            relative.remove(0, 1);
        }
        return QDir(m_stick.path()).filePath(relative);
    }

    LegacyToDlpOptions options(bool keepLegacy) const {
        LegacyToDlpOptions result;
        result.keepLegacy = keepLegacy;
        result.backupBaseDir = m_backups.path();
        result.deviceName = QStringLiteral("STICK");
        result.now = QDateTime(QDate(2026, 11, 26), QTime(10, 0, 0));
        result.masterDbId = 42;
        return result;
    }

    /// The library that the stick's Device Library Plus holds.
    std::optional<LegacyLibrary> readPlus() const {
        QString error;
        auto pDb = DeviceLibraryPlusDb::open(m_stick.path(), &error);
        EXPECT_TRUE(pDb) << qPrintable(error);
        if (!pDb) {
            return std::nullopt;
        }
        auto library = legacyFromDlp(*pDb, &error);
        EXPECT_TRUE(library) << qPrintable(error);
        return library;
    }

    QTemporaryDir m_stick;
    QTemporaryDir m_backups;
    LegacyLibrary m_library;
    QByteArray m_pdbBytes;
};

TEST_F(LegacyToDlpTest, PdbReaderReadsWhatTheWriterWrote) {
    const QByteArray bytes = legacyPdbBytes(m_library);
    ASSERT_FALSE(bytes.isEmpty());
    QString error;
    const auto parsed = legacyLibraryFromPdb(bytes, &error);
    ASSERT_TRUE(parsed) << qPrintable(error);
    expectSameLibrary(*parsed, m_library);
}

TEST_F(LegacyToDlpTest, PdbReaderRefusesBytesThatAreNotALibrary) {
    QString error;
    EXPECT_FALSE(legacyLibraryFromPdb(QByteArray(4096, 'x'), &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(legacyLibraryFromPdb(QByteArray(), &error));
}

TEST_F(LegacyToDlpTest, RowsRoundTripThroughDeviceLibraryPlus) {
    QString error;
    auto pDb = DeviceLibraryPlusDb::create(m_stick.path(), QStringLiteral("STICK"), 42, &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    ASSERT_TRUE(dlpFromLegacy(*pDb, m_library, 42, &error)) << qPrintable(error);
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty()) << qPrintable(pDb->lastErrorMessage());
    const auto back = legacyFromDlp(*pDb, &error);
    ASSERT_TRUE(back) << qPrintable(error);
    expectSameLibrary(*back, m_library);
    // Each track keeps its legacy ID as the ID in its library.
    const auto contents = pDb->listContents();
    ASSERT_EQ(3, contents.size());
    for (int i = 0; i < contents.size(); ++i) {
        EXPECT_EQ(42, contents[i].masterDbId);
        EXPECT_EQ(static_cast<qint64>(m_library.tracks[i].id), contents[i].masterContentId);
        EXPECT_EQ(m_library.tracks[i].filePath, contents[i].path);
        EXPECT_EQ(m_library.tracks[i].analyzePath, contents[i].analysisDataFilePath);
    }
}

TEST_F(LegacyToDlpTest, ConverterRefusesALibraryWithAMissingReference) {
    m_library.playlistEntries.append({2, 99, 4});
    QString error;
    auto pDb = DeviceLibraryPlusDb::create(m_stick.path(), QStringLiteral("STICK"), 42, &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    EXPECT_FALSE(dlpFromLegacy(*pDb, m_library, 42, &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(pDb->listContents().isEmpty());
    EXPECT_TRUE(pDb->listPlaylists().isEmpty());
}

TEST_F(LegacyToDlpTest, UpdateKeepsLegacyLibraryAndChangesNothingElse) {
    ASSERT_NO_FATAL_FAILURE(makeLegacyStick(true));
    const auto before = snapshot(m_stick.path());

    const LegacyToDlpResult result = updateLegacyStickToDlp(m_stick.path(), options(true));
    ASSERT_TRUE(result.ok) << qPrintable(result.errorMessage);
    EXPECT_TRUE(result.legacyKept);
    EXPECT_EQ(3, result.trackCount);
    EXPECT_EQ(3, result.playlistCount);
    EXPECT_EQ(1, result.historyCount);
    EXPECT_EQ(QStringList{QStringLiteral("/") + kPlusPath}, result.writtenFiles);
    EXPECT_TRUE(result.removedFiles.isEmpty());

    auto after = snapshot(m_stick.path());
    ASSERT_TRUE(after.contains(kPlusPath));
    after.remove(kPlusPath);
    EXPECT_EQ(before, after);

    const auto plus = readPlus();
    ASSERT_TRUE(plus);
    expectSameLibrary(*plus, m_library);
}

TEST_F(LegacyToDlpTest, UpdateBacksUpTheLegacyFilesFirst) {
    ASSERT_NO_FATAL_FAILURE(makeLegacyStick(true));
    const LegacyToDlpResult result = updateLegacyStickToDlp(m_stick.path(), options(false));
    ASSERT_TRUE(result.ok) << qPrintable(result.errorMessage);
    ASSERT_FALSE(result.backupFolder.isEmpty());
    EXPECT_EQ(QDir(m_backups.path()).filePath(QStringLiteral("STICK/20261126-100000")),
            result.backupFolder);
    EXPECT_EQ(m_pdbBytes, readFile(QDir(result.backupFolder).filePath(QStringLiteral("export.pdb"))));
    EXPECT_EQ(QByteArray("ext bytes"),
            readFile(QDir(result.backupFolder).filePath(QStringLiteral("exportExt.pdb"))));
}

TEST_F(LegacyToDlpTest, UpdateDropsLegacyLibraryAfterTheSave) {
    ASSERT_NO_FATAL_FAILURE(makeLegacyStick(true));
    auto before = snapshot(m_stick.path());

    const LegacyToDlpResult result = updateLegacyStickToDlp(m_stick.path(), options(false));
    ASSERT_TRUE(result.ok) << qPrintable(result.errorMessage);
    EXPECT_FALSE(result.legacyKept);
    QStringList removed = result.removedFiles;
    removed.sort();
    EXPECT_EQ((QStringList{QStringLiteral("/") + kLegacyPath, QStringLiteral("/") + kLegacyExtPath}),
            removed);
    EXPECT_FALSE(QFileInfo::exists(stickPath(kLegacyPath)));
    EXPECT_FALSE(QFileInfo::exists(stickPath(kLegacyExtPath)));

    auto after = snapshot(m_stick.path());
    ASSERT_TRUE(after.contains(kPlusPath));
    after.remove(kPlusPath);
    before.remove(kLegacyPath);
    before.remove(kLegacyExtPath);
    EXPECT_EQ(before, after);

    const auto plus = readPlus();
    ASSERT_TRUE(plus);
    expectSameLibrary(*plus, m_library);
}

TEST_F(LegacyToDlpTest, UpdateRefusesAStickThatHasDeviceLibraryPlus) {
    ASSERT_NO_FATAL_FAILURE(makeLegacyStick(false));
    {
        QString error;
        auto pDb = DeviceLibraryPlusDb::create(m_stick.path(), QStringLiteral("STICK"), 7, &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    const auto before = snapshot(m_stick.path());
    const LegacyToDlpResult result = updateLegacyStickToDlp(m_stick.path(), options(false));
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.errorMessage.isEmpty());
    EXPECT_EQ(before, snapshot(m_stick.path()));
}

TEST_F(LegacyToDlpTest, FailedBackupLeavesTheStickAsItWas) {
    ASSERT_NO_FATAL_FAILURE(makeLegacyStick(false));
    const auto before = snapshot(m_stick.path());
    LegacyToDlpOptions noBackup = options(false);
    noBackup.backupBaseDir.clear();
    const LegacyToDlpResult result = updateLegacyStickToDlp(m_stick.path(), noBackup);
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.errorMessage.isEmpty());
    EXPECT_EQ(before, snapshot(m_stick.path()));
}

TEST_F(LegacyToDlpTest, BrokenLegacyLibraryLeavesTheStickAsItWas) {
    ASSERT_TRUE(writeFile(stickPath(kLegacyPath), QByteArray(4096, 'x')));
    const auto before = snapshot(m_stick.path());
    const LegacyToDlpResult result = updateLegacyStickToDlp(m_stick.path(), options(false));
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.errorMessage.isEmpty());
    EXPECT_EQ(before, snapshot(m_stick.path()));
    // The backup is made before the read, so the broken file is kept there.
    EXPECT_FALSE(result.backupFolder.isEmpty());
}

} // namespace mixxx::rekordbox

// End of AI-generated text.
