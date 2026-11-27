// AI-generated (Claude Code). Start of AI-generated text.
#include <gtest/gtest.h>

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QTemporaryDir>

#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/devicelibraryplusdelete.h"
#include "library/rekordbox/devicelibraryplusmapping.h"
#include "library/rekordbox/legacyfromdlp.h"
#include "library/rekordbox/stickplaylistdelete.h"

namespace mixxx::rekordbox {
namespace {

constexpr qint64 kMasterDbId = 1234;

/// Writes a file with `size` bytes of `fill`.
bool writeFile(const QString& path, int size, char fill) {
    if (!QDir().mkpath(QFileInfo(path).path())) {
        return false;
    }
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(QByteArray(size, fill)) == size;
}

/// Each file below the root with its size and SHA-256.
QMap<QString, QString> snapshot(const QString& root) {
    QMap<QString, QString> result;
    QDirIterator it(root,
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
            QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        QString value = QString::number(info.size());
        if (info.isFile()) {
            QFile file(info.filePath());
            if (file.open(QIODevice::ReadOnly)) {
                value += QLatin1Char(' ') +
                        QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256)
                                .toHex();
            }
        }
        result.insert(QDir(root).relativeFilePath(info.filePath()), value);
    }
    return result;
}

/// A stick with a Device Library Plus: the playlist "Set" holds the tracks
/// "Kept" (also in "Other"), "In library" and "Not in library"; the folder
/// "Crates" holds no playlist. Each track has an audio file in /Contents.
class StickPlaylistDeleteTest : public testing::Test {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_stick.isValid());
        ASSERT_TRUE(m_backups.isValid());
        QString error;
        auto pDb = DeviceLibraryPlusDb::create(
                root(), QStringLiteral("TEST STICK"), kMasterDbId, &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        const qint64 kept = addTrack(pDb.get(), "Kept", "/Contents/A/kept.mp3", 'k');
        const qint64 inLib = addTrack(pDb.get(), "In library", "/Contents/A/inlib.mp3", 'i');
        const qint64 notInLib =
                addTrack(pDb.get(), "Not in library", "/Contents/B/notinlib.mp3", 'n');
        const qint64 set = pDb->addPlaylist(QStringLiteral("Set"), 0, false);
        const qint64 other = pDb->addPlaylist(QStringLiteral("Other"), 0, false);
        pDb->addPlaylist(QStringLiteral("Crates"), 0, true);
        pDb->setPlaylistContents(set, {kept, inLib, notInLib});
        pDb->setPlaylistContents(other, {kept});
        pDb->updateContentCount();
        ASSERT_TRUE(pDb->lastErrorMessage().isEmpty()) << qPrintable(pDb->lastErrorMessage());
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }

    qint64 addTrack(DeviceLibraryPlusDb* pDb, const char* title, const QString& path, char fill) {
        EXPECT_TRUE(writeFile(root() + path, 1000, fill));
        DlpContent content;
        content.title = QString::fromUtf8(title);
        content.artistId = pDb->findOrAddArtist(QStringLiteral("Artist"));
        content.path = path;
        content.fileName = path.section(QLatin1Char('/'), -1);
        content.fileType = 1;
        content.dateAdded = QStringLiteral("2026-11-26");
        content.dateCreated = QStringLiteral("2026-11-26");
        const qint64 id = pDb->addContent(content);
        EXPECT_GT(id, 0);
        return id;
    }

    QString root() const {
        return QDir::cleanPath(m_stick.path());
    }

    /// The request for "Set". The library holds "In library" only.
    StickDeleteRequest request(const QString& playlist = QStringLiteral("Set")) const {
        StickDeleteRequest result;
        result.stickRoot = root();
        result.masterDbId = kMasterDbId;
        result.namePath = {playlist};
        result.isInLibrary = [](const DlpRemovedTrack& track) {
            return track.title == QStringLiteral("In library");
        };
        result.backupBaseDir = m_backups.path();
        return result;
    }

    QStringList playlistNames() const {
        QString error;
        const auto pDb = DeviceLibraryPlusDb::open(root(), &error);
        QStringList names;
        if (!pDb) {
            ADD_FAILURE() << qPrintable(error);
            return names;
        }
        for (const auto& playlist : pDb->listPlaylists()) {
            names.append(playlist.name);
        }
        return names;
    }

    QTemporaryDir m_stick;
    QTemporaryDir m_backups;
};

TEST_F(StickPlaylistDeleteTest, PreviewListsTheTracksAndChangesNothing) {
    const auto before = snapshot(root());
    const auto preview = dlpPreviewDeletePlaylistByPath(
            root(), kMasterDbId, {QStringLiteral("Set")}, request().isInLibrary);
    ASSERT_TRUE(preview.ok) << qPrintable(preview.errorMessage);
    EXPECT_EQ(preview.tracksKept, 1);
    EXPECT_EQ(preview.tracksRemoved, 0);
    EXPECT_EQ(preview.playlistsRemoved, 0);
    ASSERT_EQ(preview.removedTracks.size(), 2);
    EXPECT_EQ(preview.removedTracks[0].title, QStringLiteral("In library"));
    EXPECT_TRUE(preview.removedTracks[0].inMixTwoLibrary);
    EXPECT_EQ(preview.removedTracks[1].title, QStringLiteral("Not in library"));
    EXPECT_EQ(preview.removedTracks[1].stickPath, QStringLiteral("/Contents/B/notinlib.mp3"));
    EXPECT_FALSE(preview.removedTracks[1].inMixTwoLibrary);
    EXPECT_EQ(snapshot(root()), before);
    EXPECT_TRUE(QDir(m_backups.path()).isEmpty());
}

TEST_F(StickPlaylistDeleteTest, PreviewRefusesAFolder) {
    const auto before = snapshot(root());
    const auto preview = dlpPreviewDeletePlaylistByPath(
            root(), kMasterDbId, {QStringLiteral("Crates")});
    EXPECT_FALSE(preview.ok);
    EXPECT_EQ(preview.refused.size(), 1);
    EXPECT_FALSE(preview.errorMessage.isEmpty());
    EXPECT_EQ(snapshot(root()), before);
}

TEST_F(StickPlaylistDeleteTest, NoToTheFirstQuestionChangesNothing) {
    const auto before = snapshot(root());
    int offers = 0;
    StickDeleteHooks hooks;
    hooks.confirmDelete = [] { return false; };
    hooks.offerImport = [&](const QList<DlpRemovedTrack>&) {
        ++offers;
        return true;
    };
    const auto outcome = runStickPlaylistDelete(request(), hooks);
    EXPECT_EQ(outcome.status, StickDeleteStatus::Canceled);
    EXPECT_EQ(offers, 0);
    EXPECT_EQ(snapshot(root()), before);
}

TEST_F(StickPlaylistDeleteTest, OffersOnlyTheTracksThatAreNotInTheLibraryBeforeTheDelete) {
    QStringList offered;
    bool stickUnchangedAtOffer = false;
    const auto before = snapshot(root());
    StickDeleteHooks hooks;
    hooks.confirmDelete = [] { return true; };
    hooks.offerImport = [&](const QList<DlpRemovedTrack>& tracks) {
        for (const auto& track : tracks) {
            offered.append(track.title);
        }
        stickUnchangedAtOffer = snapshot(root()) == before;
        return false;
    };
    hooks.importTrack = [](const QString&, const DlpRemovedTrack&) {
        ADD_FAILURE() << "no import after no";
        return false;
    };
    const auto outcome = runStickPlaylistDelete(request(), hooks);
    EXPECT_EQ(offered, QStringList{QStringLiteral("Not in library")});
    EXPECT_TRUE(stickUnchangedAtOffer);
    // "No" to the offer still deletes.
    EXPECT_EQ(outcome.status, StickDeleteStatus::Deleted);
    EXPECT_EQ(outcome.result.tracksRemoved, 2);
    EXPECT_EQ(outcome.notInLibrary.size(), 1);
    EXPECT_EQ(outcome.tracksImported, 0);
    EXPECT_FALSE(QFileInfo::exists(root() + "/Contents/B/notinlib.mp3"));
    EXPECT_TRUE(QFileInfo::exists(root() + "/Contents/A/kept.mp3"));
    EXPECT_FALSE(playlistNames().contains(QStringLiteral("Set")));
}

TEST_F(StickPlaylistDeleteTest, ImportsWhileTheFileIsOnTheStickThenDeletes) {
    QStringList imported;
    bool fileThereAtImport = false;
    bool playlistThereAtImport = false;
    StickDeleteHooks hooks;
    hooks.confirmDelete = [] { return true; };
    hooks.offerImport = [](const QList<DlpRemovedTrack>&) { return true; };
    hooks.importTrack = [&](const QString& stickFilePath, const DlpRemovedTrack& track) {
        imported.append(track.title);
        fileThereAtImport = QFileInfo::exists(stickFilePath) &&
                QFileInfo(stickFilePath).canonicalFilePath() ==
                        QFileInfo(root() + "/Contents/B/notinlib.mp3").canonicalFilePath();
        playlistThereAtImport = playlistNames().contains(QStringLiteral("Set"));
        return true;
    };
    const auto outcome = runStickPlaylistDelete(request(), hooks);
    EXPECT_EQ(imported, QStringList{QStringLiteral("Not in library")});
    EXPECT_TRUE(fileThereAtImport);
    EXPECT_TRUE(playlistThereAtImport);
    EXPECT_EQ(outcome.status, StickDeleteStatus::Deleted);
    EXPECT_EQ(outcome.tracksImported, 1);
    EXPECT_TRUE(outcome.importFailed.isEmpty());
    EXPECT_FALSE(QFileInfo::exists(root() + "/Contents/B/notinlib.mp3"));
    // The backup of the library before the delete.
    EXPECT_FALSE(QDir(m_backups.path()).isEmpty());
}

TEST_F(StickPlaylistDeleteTest, AFailedImportStopsAndTheStickDoesNotChange) {
    const auto before = snapshot(root());
    StickDeleteHooks hooks;
    hooks.confirmDelete = [] { return true; };
    hooks.offerImport = [](const QList<DlpRemovedTrack>&) { return true; };
    hooks.importTrack = [](const QString&, const DlpRemovedTrack&) { return false; };
    const auto outcome = runStickPlaylistDelete(request(), hooks);
    EXPECT_EQ(outcome.status, StickDeleteStatus::Failed);
    EXPECT_FALSE(outcome.message.isEmpty());
    EXPECT_EQ(outcome.importFailed, QStringList{QStringLiteral("/Contents/B/notinlib.mp3")});
    EXPECT_EQ(snapshot(root()), before);
}

TEST_F(StickPlaylistDeleteTest, NoOfferWhenEveryTrackIsInTheLibrary) {
    int offers = 0;
    StickDeleteHooks hooks;
    hooks.confirmDelete = [] { return true; };
    hooks.offerImport = [&](const QList<DlpRemovedTrack>&) {
        ++offers;
        return true;
    };
    auto allInLibrary = request();
    allInLibrary.isInLibrary = [](const DlpRemovedTrack&) { return true; };
    const auto outcome = runStickPlaylistDelete(allInLibrary, hooks);
    EXPECT_EQ(offers, 0);
    EXPECT_EQ(outcome.status, StickDeleteStatus::Deleted);
    EXPECT_TRUE(outcome.notInLibrary.isEmpty());
}

TEST_F(StickPlaylistDeleteTest, AFolderFailsWithoutAQuestionAboutTracks) {
    const auto before = snapshot(root());
    int offers = 0;
    StickDeleteHooks hooks;
    hooks.confirmDelete = [] { return true; };
    hooks.offerImport = [&](const QList<DlpRemovedTrack>&) {
        ++offers;
        return true;
    };
    const auto outcome = runStickPlaylistDelete(request(QStringLiteral("Crates")), hooks);
    EXPECT_EQ(outcome.status, StickDeleteStatus::Failed);
    EXPECT_FALSE(outcome.message.isEmpty());
    EXPECT_EQ(offers, 0);
    EXPECT_EQ(snapshot(root()), before);
}

TEST_F(StickPlaylistDeleteTest, TheSummaryTellsTheCounts) {
    StickDeleteHooks hooks;
    hooks.confirmDelete = [] { return true; };
    hooks.offerImport = [](const QList<DlpRemovedTrack>&) { return true; };
    hooks.importTrack = [](const QString&, const DlpRemovedTrack&) { return true; };
    const auto outcome = runStickPlaylistDelete(request(), hooks);
    const QString text = stickDeleteSummary(outcome);
    EXPECT_TRUE(text.contains(QStringLiteral("2"))) << qPrintable(text);
    EXPECT_TRUE(text.contains(QStringLiteral("1"))) << qPrintable(text);
    EXPECT_FALSE(text.contains(QLatin1Char('!')));
}

TEST_F(StickPlaylistDeleteTest, TheDeleteWritesExportPdbAgainOnAStickWithBothFormats) {
    // Give the stick an export.pdb from its Device Library Plus.
    QString error;
    QString pdbPath;
    {
        const auto pDb = DeviceLibraryPlusDb::open(root(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        pdbPath = QFileInfo(pDb->stickFilePath()).absolutePath() + QStringLiteral("/export.pdb");
        ASSERT_EQ(writeLegacyLibraryFromDlp(*pDb, root(), pdbPath), QString());
    }
    QFile before(pdbPath);
    ASSERT_TRUE(before.open(QIODevice::ReadOnly));
    const QByteArray oldBytes = before.readAll();
    before.close();

    StickDeleteHooks hooks;
    hooks.offerImport = [](const QList<DlpRemovedTrack>&) { return false; };
    const auto outcome = runStickPlaylistDelete(request(), hooks);
    ASSERT_EQ(outcome.status, StickDeleteStatus::Deleted) << qPrintable(outcome.message);
    EXPECT_TRUE(outcome.result.legacyLibraryWritten);
    EXPECT_TRUE(outcome.result.legacyLibraryError.isEmpty());

    // export.pdb now holds what the changed Device Library Plus gives.
    const auto pDb = DeviceLibraryPlusDb::open(root(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    const auto library = legacyFromDlp(*pDb, &error);
    ASSERT_TRUE(library) << qPrintable(error);
    QFile after(pdbPath);
    ASSERT_TRUE(after.open(QIODevice::ReadOnly));
    const QByteArray newBytes = after.readAll();
    EXPECT_NE(newBytes, oldBytes);
    EXPECT_EQ(newBytes, legacyPdbBytes(*library));
    EXPECT_TRUE(stickDeleteSummary(outcome).contains(QStringLiteral("export.pdb")));
}

TEST_F(StickPlaylistDeleteTest, AStickWithoutExportPdbGetsNone) {
    StickDeleteHooks hooks;
    hooks.offerImport = [](const QList<DlpRemovedTrack>&) { return false; };
    const auto outcome = runStickPlaylistDelete(request(), hooks);
    ASSERT_EQ(outcome.status, StickDeleteStatus::Deleted) << qPrintable(outcome.message);
    EXPECT_FALSE(outcome.result.legacyLibraryWritten);
    QString error;
    const auto pDb = DeviceLibraryPlusDb::open(root(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    EXPECT_FALSE(QFileInfo::exists(
            QFileInfo(pDb->stickFilePath()).absolutePath() + QStringLiteral("/export.pdb")));
}

/// The library check: by size, then by name after fatSafeName() or by bytes.
class StickLibraryCheckTest : public testing::Test {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_stick.isValid());
        ASSERT_TRUE(m_library.isValid());
    }
    DlpRemovedTrack stickTrack(const QString& stickPath, int size, char fill) {
        EXPECT_TRUE(writeFile(m_stick.path() + stickPath, size, fill));
        DlpRemovedTrack track;
        track.stickPath = stickPath;
        return track;
    }
    LibraryFileRef libraryFile(const QString& name, int size, char fill) {
        const QString path = m_library.path() + QLatin1Char('/') + name;
        EXPECT_TRUE(writeFile(path, size, fill));
        return {path, name};
    }
    DlpLibraryCheck check(const QList<LibraryFileRef>& files) {
        return dlpFileNameAndSizeCheck(m_stick.path(), [files](qint64 size) {
            QList<LibraryFileRef> result;
            for (const auto& file : files) {
                if (QFileInfo(file.location).size() == size) {
                    result.append(file);
                }
            }
            return result;
        });
    }
    QTemporaryDir m_stick;
    QTemporaryDir m_library;
};

TEST_F(StickLibraryCheckTest, SameNameAndSizeIsInTheLibrary) {
    const auto files = QList<LibraryFileRef>{libraryFile("Song.mp3", 500, 'a')};
    EXPECT_TRUE(check(files)(stickTrack("/Contents/X/Y/song.MP3", 500, 'b')));
}

TEST_F(StickLibraryCheckTest, TheNameThatTheExportGivesIsInTheLibrary) {
    // fatSafeName() changes the characters that FAT does not permit. Windows
    // does not permit them either, so only the name in the reference has them.
    auto file = libraryFile("AxB.mp3", 500, 'a');
    file.fileName = QStringLiteral("A?B.mp3");
    const auto files = QList<LibraryFileRef>{file};
    EXPECT_TRUE(check(files)(stickTrack("/Contents/X/Y/" + fatSafeName(QStringLiteral("A?B.mp3")), 500, 'b')));
}

TEST_F(StickLibraryCheckTest, SameNameOtherSizeIsNotInTheLibrary) {
    const auto files = QList<LibraryFileRef>{libraryFile("song.mp3", 500, 'a')};
    EXPECT_FALSE(check(files)(stickTrack("/Contents/X/Y/song.mp3", 501, 'a')));
}

TEST_F(StickLibraryCheckTest, OtherNameSameBytesIsInTheLibrary) {
    const auto files = QList<LibraryFileRef>{libraryFile("original.mp3", 500, 'a')};
    EXPECT_TRUE(check(files)(stickTrack("/Contents/X/Y/renamed.mp3", 500, 'a')));
}

TEST_F(StickLibraryCheckTest, OtherNameOtherBytesIsNotInTheLibrary) {
    const auto files = QList<LibraryFileRef>{libraryFile("original.mp3", 500, 'a')};
    EXPECT_FALSE(check(files)(stickTrack("/Contents/X/Y/renamed.mp3", 500, 'b')));
}

TEST_F(StickLibraryCheckTest, AMissingStickFileIsNotInTheLibrary) {
    const auto files = QList<LibraryFileRef>{libraryFile("song.mp3", 500, 'a')};
    DlpRemovedTrack track;
    track.stickPath = QStringLiteral("/Contents/X/Y/song.mp3");
    EXPECT_FALSE(check(files)(track));
}

} // namespace
} // namespace mixxx::rekordbox
// End of AI-generated text.
