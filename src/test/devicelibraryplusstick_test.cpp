#include "library/rekordbox/devicelibraryplusstick.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <utime.h>
#endif

#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/devicelibraryplusmapping.h"
#include "mixxxtest.h"

namespace {

using mixxx::rekordbox::DeviceLibraryPlusDb;
using mixxx::rekordbox::dlpBackupLibrary;
using mixxx::rekordbox::dlpBytesNeeded;
using mixxx::rekordbox::DlpFileSystem;
using mixxx::rekordbox::dlpFileSystemFromType;
using mixxx::rekordbox::dlpMoveLeftoverFiles;
// AI-generated (Claude Code). Start of AI-generated text.
using mixxx::rekordbox::DlpRestoreResult;
using mixxx::rekordbox::dlpRestoreLibrary;
using mixxx::rekordbox::DlpStickInfo;
// End of AI-generated text.
using mixxx::rekordbox::dlpRemoveAnlzLeftovers;
using mixxx::rekordbox::dlpRemoveOldWorkingCopies;
using mixxx::rekordbox::dlpStickInfo;
using mixxx::rekordbox::fatSafeName;

bool writeFile(const QString& path, const QByteArray& content) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(content) == content.size();
}

/// QFile cannot open a folder, so the test sets the modification time of a
/// folder with the system call.
bool setFolderModificationTime(const QString& path, const QDateTime& time) {
#ifdef Q_OS_WIN
    const HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),
            FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    // 100 ns steps since 1601-01-01.
    const ULONGLONG ticks = (static_cast<ULONGLONG>(time.toSecsSinceEpoch()) + 11644473600ULL) *
            10000000ULL;
    FILETIME fileTime;
    fileTime.dwLowDateTime = static_cast<DWORD>(ticks & 0xffffffffULL);
    fileTime.dwHighDateTime = static_cast<DWORD>(ticks >> 32);
    const bool ok = SetFileTime(handle, nullptr, nullptr, &fileTime) != 0;
    CloseHandle(handle);
    return ok;
#else
    utimbuf times;
    times.actime = static_cast<time_t>(time.toSecsSinceEpoch());
    times.modtime = times.actime;
    return utime(QFile::encodeName(path).constData(), &times) == 0;
#endif
}

QString libraryPath(const QString& root) {
    return QDir(root).filePath(DeviceLibraryPlusDb::kRelativePath);
}

QString rekordboxFolder(const QString& root) {
    return QFileInfo(libraryPath(root)).absolutePath();
}

QStringList folderNames(const QString& path) {
    return QDir(path).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
}

class DeviceLibraryPlusStickTest : public MixxxTest {
};

/// Sets the working folder of the process and sets it back at the end. An
/// empty path would be a path below the working folder; the tests check that
/// the functions do not use it.
class WorkingFolderGuard {
  public:
    explicit WorkingFolderGuard(const QString& folder)
            : m_previous(QDir::currentPath()),
              m_changed(QDir::setCurrent(folder)) {
    }
    ~WorkingFolderGuard() {
        QDir::setCurrent(m_previous);
    }
    bool isChanged() const {
        return m_changed;
    }

  private:
    const QString m_previous;
    const bool m_changed;
};

TEST_F(DeviceLibraryPlusStickTest, FileSystemTypes) {
    for (const char* type : {"vfat", "msdos", "fat32", "fat"}) {
        EXPECT_EQ(DlpFileSystem::Fat32, dlpFileSystemFromType(type)) << type;
    }
    EXPECT_EQ(DlpFileSystem::Fat32, dlpFileSystemFromType("FAT32"));
    EXPECT_EQ(DlpFileSystem::Fat32, dlpFileSystemFromType("VFAT"));
    EXPECT_EQ(DlpFileSystem::ExFat, dlpFileSystemFromType("exfat"));
    EXPECT_EQ(DlpFileSystem::ExFat, dlpFileSystemFromType("exFAT"));
    for (const char* type : {"ntfs", "ext2", "ext3", "ext4", "btrfs", "xfs", "apfs", "hfs", "hfsplus"}) {
        EXPECT_EQ(DlpFileSystem::NotForPlayers, dlpFileSystemFromType(type)) << type;
    }
    EXPECT_EQ(DlpFileSystem::NotForPlayers, dlpFileSystemFromType("NTFS"));
    EXPECT_EQ(DlpFileSystem::Unknown, dlpFileSystemFromType("fuseblk"));
    EXPECT_EQ(DlpFileSystem::Unknown, dlpFileSystemFromType(""));
}

TEST_F(DeviceLibraryPlusStickTest, StickInfoFindsLibraries) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());

    auto info = dlpStickInfo(stick.path());
    EXPECT_EQ(stick.path(), info.root);
    EXPECT_TRUE(info.isReady);
    EXPECT_GT(info.bytesFree, 0);
    EXPECT_FALSE(info.hasPlusLibrary);
    EXPECT_FALSE(info.hasLegacyLibrary);

    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "plus"));
    info = dlpStickInfo(stick.path());
    EXPECT_TRUE(info.hasPlusLibrary);
    EXPECT_FALSE(info.hasLegacyLibrary);

    ASSERT_TRUE(writeFile(
            QDir(rekordboxFolder(stick.path())).filePath("export.pdb"), "pdb"));
    info = dlpStickInfo(stick.path());
    EXPECT_TRUE(info.hasPlusLibrary);
    EXPECT_TRUE(info.hasLegacyLibrary);

    QTemporaryDir other;
    ASSERT_TRUE(other.isValid());
    ASSERT_TRUE(writeFile(
            QDir(rekordboxFolder(other.path())).filePath("export.pdb"), "pdb"));
    info = dlpStickInfo(other.path());
    EXPECT_FALSE(info.hasPlusLibrary);
    EXPECT_TRUE(info.hasLegacyLibrary);
}

TEST_F(DeviceLibraryPlusStickTest, BytesNeeded) {
    constexpr qint64 kMiB = 1024 * 1024;
    EXPECT_EQ(64 * kMiB, dlpBytesNeeded(0, 0));
    EXPECT_EQ(64 * kMiB + 512 * 1024 * 10, dlpBytesNeeded(0, 10));
    EXPECT_EQ(100 * kMiB + 64 * kMiB + 512 * 1024 * 3, dlpBytesNeeded(100 * kMiB, 3));
    // More than 4 GiB must not overflow.
    EXPECT_EQ(10000000000 + 64 * kMiB + 512 * 1024, dlpBytesNeeded(10000000000, 1));
    EXPECT_EQ(4294967295, mixxx::rekordbox::kFat32MaxFileSize);
}

TEST_F(DeviceLibraryPlusStickTest, BackupCopiesOnlyTheLibrary) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    const QByteArray content(5000, 'x');
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), content));
    ASSERT_TRUE(writeFile(
            QDir(rekordboxFolder(stick.path())).filePath("export.pdb"), "pdb"));
    ASSERT_TRUE(writeFile(QDir(stick.path()).filePath("Contents/a.mp3"), "mp3"));

    QString error;
    const QDateTime now(QDate(2026, 11, 2), QTime(13, 5, 9));
    const QString copy = dlpBackupLibrary(
            stick.path(), backups.path(), "My: Stick?", now, &error);
    ASSERT_FALSE(copy.isEmpty()) << error.toStdString();
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(QDir(backups.path())
                      .filePath(fatSafeName("My: Stick?") +
                              "/20261102-130509/exportLibrary.db"),
            copy);

    QFile file(copy);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(content, file.readAll());
    file.close();

    // Only the one file, in the one time-stamp folder, in the one device folder.
    const QString deviceDir = QDir(backups.path()).filePath(fatSafeName("My: Stick?"));
    EXPECT_EQ(QStringList{fatSafeName("My: Stick?")}, folderNames(backups.path()));
    EXPECT_EQ(QStringList{"20261102-130509"}, folderNames(deviceDir));
    EXPECT_EQ(QStringList{"exportLibrary.db"},
            QDir(deviceDir + "/20261102-130509")
                    .entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot));

    // The stick is not changed.
    EXPECT_TRUE(QFileInfo::exists(
            QDir(rekordboxFolder(stick.path())).filePath("export.pdb")));
    EXPECT_TRUE(QFileInfo::exists(libraryPath(stick.path())));
}

TEST_F(DeviceLibraryPlusStickTest, BackupFailsWithoutLibrary) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    const QDateTime now(QDate(2026, 11, 2), QTime(13, 5, 9));
    QString error;
    EXPECT_TRUE(dlpBackupLibrary(stick.path(), backups.path(), "Stick", now, &error).isEmpty());
    EXPECT_FALSE(error.isEmpty());
    // The error pointer is optional.
    EXPECT_TRUE(dlpBackupLibrary(stick.path(), backups.path(), "Stick", now, nullptr).isEmpty());
}

TEST_F(DeviceLibraryPlusStickTest, BackupKeepsNewestFive) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "library"));

    const QString deviceDir = QDir(backups.path()).filePath("Stick");
    // Things that are not time-stamp folders stay.
    ASSERT_TRUE(QDir().mkpath(deviceDir + "/notes"));
    ASSERT_TRUE(writeFile(deviceDir + "/notes/keep.txt", "keep"));
    ASSERT_TRUE(QDir().mkpath(deviceDir + "/20200101-1200"));
    ASSERT_TRUE(writeFile(deviceDir + "/20200101-000000.txt", "file"));
    // The folder of another device is not touched.
    ASSERT_TRUE(writeFile(backups.path() + "/Other/20200101-000000/exportLibrary.db", "o"));

    const QDateTime start(QDate(2026, 11, 2), QTime(13, 5, 0));
    QStringList expected;
    for (int i = 0; i < 7; ++i) {
        QString error;
        const QString copy = dlpBackupLibrary(
                stick.path(), backups.path(), "Stick", start.addSecs(i), &error);
        ASSERT_FALSE(copy.isEmpty()) << error.toStdString();
        expected.append(start.addSecs(i).toString("yyyyMMdd-HHmmss"));
    }

    const QStringList names = folderNames(deviceDir);
    QStringList stamped = names;
    stamped.removeAll("notes");
    stamped.removeAll("20200101-1200");
    EXPECT_EQ(expected.mid(2), stamped);
    EXPECT_EQ(5, stamped.size());
    EXPECT_TRUE(names.contains("notes"));
    EXPECT_TRUE(QFileInfo::exists(deviceDir + "/notes/keep.txt"));
    EXPECT_TRUE(QFileInfo::exists(deviceDir + "/20200101-000000.txt"));
    EXPECT_TRUE(QFileInfo::exists(backups.path() + "/Other/20200101-000000/exportLibrary.db"));
}

TEST_F(DeviceLibraryPlusStickTest, MovesOnlyLeftovers) {
    // Ruling R16: Mix Two never deletes these files. It moves them to the
    // computer, into the leftovers folder of the device.
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    const QDir folder(rekordboxFolder(stick.path()));
    const QStringList kept = {"exportLibrary.db",
            "exportLibrary.db-wal",
            "exportLibrary.db.bak",
            "exportLibrary.db.Ab12C",
            "exportLibrary.db.Ab12Cde",
            "exportLibrary.db.Ab12C_",
            "xexportLibrary.db.Ab12Cd",
            "export.pdb"};
    // "exportLibrary.db.backup" can be a copy that the DJ made.
    const QStringList leftovers = {
            "exportLibrary.db.Ab12Cd", "exportLibrary.db.tmp", "exportLibrary.db.backup"};
    for (const QString& name : kept + leftovers) {
        ASSERT_TRUE(writeFile(folder.filePath(name), "data of " + name.toUtf8()))
                << name.toStdString();
    }
    // A folder with a leftover name, and a file below it, stay.
    ASSERT_TRUE(writeFile(folder.filePath("exportLibrary.db.Zz99Yy/exportLibrary.db.tmp"), "d"));
    // A leftover name in another folder stays.
    ASSERT_TRUE(writeFile(QDir(stick.path()).filePath("exportLibrary.db.tmp"), "d"));

    const QDateTime now(QDate(2026, 11, 2), QTime(13, 5, 9));
    EXPECT_EQ(3, dlpMoveLeftoverFiles(stick.path(), backups.path(), "My: Stick?", now));
    const QDir moved(QDir(backups.path())
                    .filePath(fatSafeName("My: Stick?") + "/leftovers/20261102-130509"));
    for (const QString& name : leftovers) {
        EXPECT_FALSE(folder.exists(name)) << name.toStdString();
        // Byte-identical.
        QFile file(moved.filePath(name));
        ASSERT_TRUE(file.open(QIODevice::ReadOnly)) << name.toStdString();
        EXPECT_EQ("data of " + name.toUtf8(), file.readAll()) << name.toStdString();
    }
    EXPECT_EQ(leftovers.size(),
            moved.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).size());
    for (const QString& name : kept) {
        EXPECT_TRUE(folder.exists(name)) << name.toStdString();
    }
    EXPECT_TRUE(folder.exists("exportLibrary.db.Zz99Yy/exportLibrary.db.tmp"));
    EXPECT_TRUE(QFileInfo::exists(QDir(stick.path()).filePath("exportLibrary.db.tmp")));
    EXPECT_EQ(0, dlpMoveLeftoverFiles(stick.path(), backups.path(), "My: Stick?", now));
}

TEST_F(DeviceLibraryPlusStickTest, MovedLeftoverGetsNumberOnClash) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    const QDateTime now(QDate(2026, 11, 2), QTime(13, 5, 9));
    const QString movedDir = backups.path() + "/Stick/leftovers/20261102-130509";
    // The same second twice: the files in the folder stay as they are.
    ASSERT_TRUE(writeFile(movedDir + "/exportLibrary.db.tmp", "first"));
    ASSERT_TRUE(writeFile(movedDir + "/exportLibrary.db.tmp (2)", "second"));
    ASSERT_TRUE(writeFile(QDir(rekordboxFolder(stick.path())).filePath("exportLibrary.db.tmp"),
            "third"));
    EXPECT_EQ(1, dlpMoveLeftoverFiles(stick.path(), backups.path(), "Stick", now));
    const auto contentOf = [](const QString& path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    };
    EXPECT_EQ("first", contentOf(movedDir + "/exportLibrary.db.tmp"));
    EXPECT_EQ("second", contentOf(movedDir + "/exportLibrary.db.tmp (2)"));
    EXPECT_EQ("third", contentOf(movedDir + "/exportLibrary.db.tmp (3)"));
    EXPECT_FALSE(QFileInfo::exists(
            QDir(rekordboxFolder(stick.path())).filePath("exportLibrary.db.tmp")));
}

TEST_F(DeviceLibraryPlusStickTest, FailedMoveLeavesLeftover) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    const QString leftover = QDir(rekordboxFolder(stick.path())).filePath("exportLibrary.db.tmp");
    ASSERT_TRUE(writeFile(leftover, "data"));
    // A file where the device folder must be: Mix Two cannot make the folder.
    const QString base = QDir(backups.path()).filePath("base");
    ASSERT_TRUE(writeFile(base + "/Stick", "a file, not a folder"));
    const QDateTime now(QDate(2026, 11, 2), QTime(13, 5, 9));
    EXPECT_EQ(0, dlpMoveLeftoverFiles(stick.path(), base, "Stick", now));
    EXPECT_TRUE(QFileInfo::exists(leftover));
}

TEST_F(DeviceLibraryPlusStickTest, BackupKeepsLeftoversFolder) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "library"));
    ASSERT_TRUE(writeFile(QDir(rekordboxFolder(stick.path())).filePath("exportLibrary.db.tmp"),
            "left"));
    const QDateTime start(QDate(2026, 11, 2), QTime(13, 5, 0));
    ASSERT_EQ(1, dlpMoveLeftoverFiles(stick.path(), backups.path(), "Stick", start));
    for (int i = 1; i <= 7; ++i) {
        ASSERT_FALSE(dlpBackupLibrary(
                stick.path(), backups.path(), "Stick", start.addSecs(i), nullptr)
                        .isEmpty());
    }
    const QString deviceDir = QDir(backups.path()).filePath("Stick");
    QStringList names = folderNames(deviceDir);
    // The retention does not count or remove the leftovers folder.
    EXPECT_EQ(1, names.removeAll("leftovers"));
    EXPECT_EQ(5, names.size());
    EXPECT_TRUE(QFileInfo::exists(deviceDir + "/leftovers/20261102-130500/exportLibrary.db.tmp"));
}

TEST_F(DeviceLibraryPlusStickTest, RemovalIgnoresMissingFolders) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    const QDateTime now(QDate(2026, 11, 2), QTime(13, 5, 9));
    EXPECT_EQ(0, dlpMoveLeftoverFiles(stick.path(), backups.path(), "Stick", now));
    EXPECT_EQ(0, dlpRemoveAnlzLeftovers(stick.path() + "/missing"));
    // No leftovers: no folder on the computer.
    EXPECT_TRUE(folderNames(backups.path()).isEmpty());
}

TEST_F(DeviceLibraryPlusStickTest, RemovesAnlzLeftovers) {
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    const QDir folder(stick.path());
    const QStringList leftovers = {
            "ANLZ0000.DAT.Ab12Cd", "ANLZ0000.EXT.000000", "ANLZ0000.2EX.zzZZ99"};
    const QStringList kept = {"ANLZ0000.DAT",
            "ANLZ0000.EXT",
            "ANLZ0000.2EX",
            "ANLZ0000.DAT.Ab12C",
            "ANLZ0000.DAT.Ab12Cde",
            "ANLZ0000.DAT.tmp",
            "ANLZ0000.XYZ.Ab12Cd",
            "ANLZ0001.DAT.Ab12Cd",
            "b1.jpg",
            "sub/ANLZ0000.DAT.Ab12Cd"};
    for (const QString& name : kept + leftovers) {
        ASSERT_TRUE(writeFile(folder.filePath(name), "data")) << name.toStdString();
    }

    EXPECT_EQ(3, dlpRemoveAnlzLeftovers(stick.path()));
    for (const QString& name : leftovers) {
        EXPECT_FALSE(folder.exists(name)) << name.toStdString();
    }
    for (const QString& name : kept) {
        EXPECT_TRUE(folder.exists(name)) << name.toStdString();
    }
}

TEST_F(DeviceLibraryPlusStickTest, RemovesOnlyOldWorkingCopies) {
    // The test uses its own folders in the temporary folder. The cut-off date
    // is so old that no other folder of the computer is before it.
    const QDir tempDir(QDir::tempPath());
    const QString uniq = QString::number(QDateTime::currentMSecsSinceEpoch());
    const QString oldName = "mixtwo-dlp-OLD" + uniq;
    const QString newName = "mixtwo-dlp-NEW" + uniq;
    const QString otherName = "other-dlp-OLD" + uniq;
    const QString noDashName = "mixtwo-dlpOLD" + uniq;
    const QStringList all = {oldName, newName, otherName, noDashName};
    for (const QString& name : all) {
        ASSERT_TRUE(QDir().mkpath(tempDir.filePath(name)));
        ASSERT_TRUE(writeFile(tempDir.filePath(name + "/inside.txt"), "x"));
    }

    const QDateTime ancient(QDate(1990, 1, 1), QTime(0, 0));
    for (const QString& name : {oldName, otherName, noDashName}) {
        ASSERT_TRUE(setFolderModificationTime(tempDir.filePath(name), ancient))
                << name.toStdString();
        ASSERT_EQ(ancient, QFileInfo(tempDir.filePath(name)).lastModified());
    }

    EXPECT_EQ(1, dlpRemoveOldWorkingCopies(ancient.addDays(1)));
    EXPECT_FALSE(tempDir.exists(oldName));
    EXPECT_TRUE(tempDir.exists(newName));
    EXPECT_TRUE(tempDir.exists(otherName));
    EXPECT_TRUE(tempDir.exists(noDashName));

    for (const QString& name : all) {
        QDir(tempDir.filePath(name)).removeRecursively();
    }
}

TEST_F(DeviceLibraryPlusStickTest, BackupRefusesEmptyStickRoot) {
    QTemporaryDir work;
    QTemporaryDir backups;
    ASSERT_TRUE(work.isValid());
    ASSERT_TRUE(backups.isValid());
    // A library below the working folder: an empty path must not find it.
    ASSERT_TRUE(writeFile(libraryPath(work.path()), "library"));
    WorkingFolderGuard guard(work.path());
    ASSERT_TRUE(guard.isChanged());
    const QDateTime now(QDate(2026, 11, 2), QTime(13, 5, 9));
    for (const QString& root : {QString(), QStringLiteral("   ")}) {
        QString error;
        EXPECT_TRUE(dlpBackupLibrary(root, backups.path(), "Stick", now, &error).isEmpty());
        EXPECT_FALSE(error.isEmpty());
        EXPECT_TRUE(dlpBackupLibrary(root, backups.path(), "Stick", now, nullptr).isEmpty());
    }
    EXPECT_TRUE(folderNames(backups.path()).isEmpty());
}

TEST_F(DeviceLibraryPlusStickTest, BackupRefusesEmptyBackupFolder) {
    QTemporaryDir stick;
    QTemporaryDir work;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(work.isValid());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "library"));
    WorkingFolderGuard guard(work.path());
    ASSERT_TRUE(guard.isChanged());
    const QDateTime now(QDate(2026, 11, 2), QTime(13, 5, 9));
    for (const QString& base : {QString(), QStringLiteral("  	 ")}) {
        QString error;
        EXPECT_TRUE(dlpBackupLibrary(stick.path(), base, "Stick", now, &error).isEmpty());
        EXPECT_FALSE(error.isEmpty());
    }
    // Nothing is made below the working folder.
    EXPECT_TRUE(QDir(work.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
}

TEST_F(DeviceLibraryPlusStickTest, BackupRefusesInvalidTime) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "library"));
    QString error;
    EXPECT_TRUE(dlpBackupLibrary(stick.path(), backups.path(), "Stick", QDateTime(), &error)
                    .isEmpty());
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(folderNames(backups.path()).isEmpty());
}

TEST_F(DeviceLibraryPlusStickTest, MoveLeftoverFilesRefusesEmptyPaths) {
    QTemporaryDir work;
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(work.isValid());
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    // A leftover below the working folder: an empty path must not move it.
    const QString leftover = QDir(rekordboxFolder(work.path())).filePath("exportLibrary.db.tmp");
    ASSERT_TRUE(writeFile(leftover, "data"));
    const QString stickLeftover =
            QDir(rekordboxFolder(stick.path())).filePath("exportLibrary.db.tmp");
    ASSERT_TRUE(writeFile(stickLeftover, "data"));
    WorkingFolderGuard guard(work.path());
    ASSERT_TRUE(guard.isChanged());
    const QDateTime now(QDate(2026, 11, 2), QTime(13, 5, 9));
    EXPECT_EQ(0, dlpMoveLeftoverFiles(QString(), backups.path(), "Stick", now));
    EXPECT_EQ(0, dlpMoveLeftoverFiles(QStringLiteral("  "), backups.path(), "Stick", now));
    EXPECT_TRUE(QFileInfo::exists(leftover));
    // An empty backup folder or an invalid time moves nothing.
    EXPECT_EQ(0, dlpMoveLeftoverFiles(stick.path(), QString(), "Stick", now));
    EXPECT_EQ(0, dlpMoveLeftoverFiles(stick.path(), QStringLiteral(" "), "Stick", now));
    EXPECT_EQ(0, dlpMoveLeftoverFiles(stick.path(), backups.path(), "Stick", QDateTime()));
    EXPECT_TRUE(QFileInfo::exists(stickLeftover));
    EXPECT_TRUE(QDir(work.path()).entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty());
    EXPECT_TRUE(folderNames(backups.path()).isEmpty());
}

TEST_F(DeviceLibraryPlusStickTest, RemoveAnlzLeftoversRefusesEmptyPath) {
    QTemporaryDir work;
    ASSERT_TRUE(work.isValid());
    const QString leftover = QDir(work.path()).filePath("ANLZ0000.DAT.Ab12Cd");
    ASSERT_TRUE(writeFile(leftover, "data"));
    WorkingFolderGuard guard(work.path());
    ASSERT_TRUE(guard.isChanged());
    EXPECT_EQ(0, dlpRemoveAnlzLeftovers(QString()));
    EXPECT_EQ(0, dlpRemoveAnlzLeftovers(QStringLiteral(" 	")));
    EXPECT_TRUE(QFileInfo::exists(leftover));
}

// AI-generated (Claude Code). Start of AI-generated text.
QByteArray readFileBytes(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

TEST_F(DeviceLibraryPlusStickTest, RestoreCopiesBackupBack) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    const QByteArray original(5000, 'a');
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), original));
    ASSERT_TRUE(writeFile(QDir(stick.path()).filePath("Contents/a.mp3"), "mp3"));
    QString error;
    const QDateTime backupTime(QDate(2026, 11, 2), QTime(13, 5, 9));
    ASSERT_FALSE(dlpBackupLibrary(stick.path(), backups.path(), "Stick", backupTime, &error)
                         .isEmpty())
            << error.toStdString();
    // The DJ changes the library on the stick.
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), QByteArray(7000, 'b')));

    const QDateTime now(QDate(2026, 11, 3), QTime(9, 0, 0));
    const DlpRestoreResult result =
            dlpRestoreLibrary(stick.path(), backups.path(), "Stick", now);
    ASSERT_TRUE(result.ok) << result.errorMessage.toStdString();
    EXPECT_TRUE(result.errorMessage.isEmpty());
    EXPECT_EQ(original, readFileBytes(libraryPath(stick.path())));
    EXPECT_EQ(QStringLiteral("20261102-130509"), QFileInfo(result.restoredFrom).fileName());
    EXPECT_EQ(QStringList{QStringLiteral("/PIONEER/rekordbox/exportLibrary.db")},
            result.restoredFiles);
    // Nothing else changes on the stick, and no temporary file stays.
    EXPECT_EQ(QByteArray("mp3"), readFileBytes(QDir(stick.path()).filePath("Contents/a.mp3")));
    EXPECT_EQ(QStringList{QStringLiteral("exportLibrary.db")},
            QDir(rekordboxFolder(stick.path())).entryList(QDir::Files | QDir::Hidden));
}

TEST_F(DeviceLibraryPlusStickTest, RestoreMakesBackupOfPresentLibrary) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "first"));
    QString error;
    ASSERT_FALSE(dlpBackupLibrary(stick.path(),
            backups.path(),
            "Stick",
            QDateTime(QDate(2026, 11, 2), QTime(13, 5, 9)),
            &error)
                         .isEmpty());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "second"));

    const QDateTime now(QDate(2026, 11, 3), QTime(9, 0, 0));
    const DlpRestoreResult result =
            dlpRestoreLibrary(stick.path(), backups.path(), "Stick", now);
    ASSERT_TRUE(result.ok) << result.errorMessage.toStdString();
    EXPECT_EQ(QByteArray("first"), readFileBytes(libraryPath(stick.path())));
    // The backup of the library that the restore replaced is there.
    const QString deviceDir = QDir(backups.path()).filePath("Stick");
    EXPECT_EQ((QStringList{"20261102-130509", "20261103-090000"}), folderNames(deviceDir));
    EXPECT_EQ(QDir(deviceDir).filePath("20261103-090000/exportLibrary.db"), result.safetyBackup);
    EXPECT_EQ(QByteArray("second"), readFileBytes(result.safetyBackup));
    // The backup that was restored stays.
    EXPECT_EQ(QByteArray("first"),
            readFileBytes(QDir(deviceDir).filePath("20261102-130509/exportLibrary.db")));
}

TEST_F(DeviceLibraryPlusStickTest, RestoreKeepsTheRestoredBackupInTheSameSecond) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "first"));
    const QDateTime time(QDate(2026, 11, 2), QTime(13, 5, 9));
    QString error;
    ASSERT_FALSE(dlpBackupLibrary(stick.path(), backups.path(), "Stick", time, &error).isEmpty());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "second"));

    // The same second as the backup: the backup of "second" must not replace
    // the backup that the restore uses.
    const DlpRestoreResult result = dlpRestoreLibrary(stick.path(), backups.path(), "Stick", time);
    ASSERT_TRUE(result.ok) << result.errorMessage.toStdString();
    EXPECT_EQ(QByteArray("first"), readFileBytes(libraryPath(stick.path())));
    EXPECT_EQ(QByteArray("first"),
            readFileBytes(QDir(backups.path()).filePath("Stick/20261102-130509/exportLibrary.db")));
    EXPECT_EQ(QByteArray("second"), readFileBytes(result.safetyBackup));
    EXPECT_NE(QFileInfo(result.safetyBackup).absolutePath(), result.restoredFrom);
}

TEST_F(DeviceLibraryPlusStickTest, RestoreFailsWithoutBackup) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "present"));
    // A folder that is not a time stamp and an empty time-stamp folder are no
    // backups.
    ASSERT_TRUE(QDir().mkpath(backups.path() + "/Stick/notes"));
    ASSERT_TRUE(QDir().mkpath(backups.path() + "/Stick/20260101-000000"));

    const QDateTime now(QDate(2026, 11, 3), QTime(9, 0, 0));
    const DlpRestoreResult result = dlpRestoreLibrary(stick.path(), backups.path(), "Stick", now);
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.errorMessage.isEmpty());
    EXPECT_EQ(QByteArray("present"), readFileBytes(libraryPath(stick.path())));
    // No backup of the stick was made: the restore stopped first.
    EXPECT_EQ((QStringList{"20260101-000000", "notes"}), folderNames(backups.path() + "/Stick"));
}

TEST_F(DeviceLibraryPlusStickTest, RestoreDoesNotUseBackupOfAnotherStick) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "mine"));
    ASSERT_TRUE(writeFile(backups.path() + "/Other/20261102-130509/exportLibrary.db", "theirs"));

    const QDateTime now(QDate(2026, 11, 3), QTime(9, 0, 0));
    const DlpRestoreResult result = dlpRestoreLibrary(stick.path(), backups.path(), "Stick", now);
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.errorMessage.isEmpty());
    EXPECT_EQ(QByteArray("mine"), readFileBytes(libraryPath(stick.path())));
    EXPECT_EQ(QByteArray("theirs"),
            readFileBytes(backups.path() + "/Other/20261102-130509/exportLibrary.db"));
}

TEST_F(DeviceLibraryPlusStickTest, RestoreUsesNewestOfThreeBackups) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "now"));
    ASSERT_TRUE(writeFile(backups.path() + "/Stick/20261101-100000/exportLibrary.db", "oldest"));
    ASSERT_TRUE(writeFile(backups.path() + "/Stick/20261103-100000/exportLibrary.db", "newest"));
    ASSERT_TRUE(writeFile(backups.path() + "/Stick/20261102-100000/exportLibrary.db", "middle"));
    // A newer folder without a library is not a backup.
    ASSERT_TRUE(QDir().mkpath(backups.path() + "/Stick/20261104-100000"));

    const QDateTime now(QDate(2026, 11, 5), QTime(9, 0, 0));
    const DlpRestoreResult result = dlpRestoreLibrary(stick.path(), backups.path(), "Stick", now);
    ASSERT_TRUE(result.ok) << result.errorMessage.toStdString();
    EXPECT_EQ(QByteArray("newest"), readFileBytes(libraryPath(stick.path())));
    EXPECT_EQ(QStringLiteral("20261103-100000"), QFileInfo(result.restoredFrom).fileName());
}

TEST_F(DeviceLibraryPlusStickTest, RestoreKeepsRestoredBackupWhenFiveExist) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "now"));
    for (int day = 1; day <= 5; ++day) {
        ASSERT_TRUE(writeFile(QStringLiteral("%1/Stick/2026110%2-100000/exportLibrary.db")
                                      .arg(backups.path())
                                      .arg(day),
                QByteArray::number(day)));
    }
    // The time of the restore is older than all backups.
    const QDateTime now(QDate(2026, 10, 1), QTime(9, 0, 0));
    const DlpRestoreResult result = dlpRestoreLibrary(stick.path(), backups.path(), "Stick", now);
    ASSERT_TRUE(result.ok) << result.errorMessage.toStdString();
    EXPECT_EQ(QByteArray("5"), readFileBytes(libraryPath(stick.path())));
    EXPECT_TRUE(QFileInfo::exists(result.safetyBackup));
    EXPECT_TRUE(QFileInfo::exists(backups.path() + "/Stick/20261105-100000/exportLibrary.db"));
}

TEST_F(DeviceLibraryPlusStickTest, RestoreFailsOnMissingOrReadOnlyStick) {
    QTemporaryDir backups;
    ASSERT_TRUE(backups.isValid());
    ASSERT_TRUE(writeFile(backups.path() + "/Stick/20261102-130509/exportLibrary.db", "backup"));
    const QDateTime now(QDate(2026, 11, 3), QTime(9, 0, 0));

    // A stick that is not there.
    QTemporaryDir parent;
    ASSERT_TRUE(parent.isValid());
    const QString missing = parent.filePath("no-stick");
    DlpRestoreResult result = dlpRestoreLibrary(missing, backups.path(), "Stick", now);
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.errorMessage.isEmpty());
    EXPECT_FALSE(QFileInfo::exists(missing));
    EXPECT_EQ(QStringList{"20261102-130509"}, folderNames(backups.path() + "/Stick"));

    // A stick that the operating system reports as read-only.
    QTemporaryDir stick;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "present"));
    DlpStickInfo info = dlpStickInfo(stick.path());
    ASSERT_TRUE(info.isReady);
    info.isReadOnly = true;
    result = dlpRestoreLibrary(info, backups.path(), "Stick", now);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(mixxx::rekordbox::dlpMessageCannotWrite(), result.errorMessage);
    EXPECT_EQ(QByteArray("present"), readFileBytes(libraryPath(stick.path())));
    EXPECT_EQ(QStringList{"20261102-130509"}, folderNames(backups.path() + "/Stick"));

    // A stick that is not ready.
    info.isReadOnly = false;
    info.isReady = false;
    result = dlpRestoreLibrary(info, backups.path(), "Stick", now);
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.errorMessage.isEmpty());
    EXPECT_EQ(QByteArray("present"), readFileBytes(libraryPath(stick.path())));
}

TEST_F(DeviceLibraryPlusStickTest, RestoreRefusesEmptyPathsAndInvalidTime) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    ASSERT_TRUE(writeFile(libraryPath(stick.path()), "present"));
    ASSERT_TRUE(writeFile(backups.path() + "/Stick/20261102-130509/exportLibrary.db", "backup"));
    const QDateTime now(QDate(2026, 11, 3), QTime(9, 0, 0));
    EXPECT_FALSE(dlpRestoreLibrary(QString(), backups.path(), "Stick", now).ok);
    EXPECT_FALSE(dlpRestoreLibrary(QStringLiteral("  "), backups.path(), "Stick", now).ok);
    EXPECT_FALSE(dlpRestoreLibrary(stick.path(), QString(), "Stick", now).ok);
    EXPECT_FALSE(dlpRestoreLibrary(stick.path(), backups.path(), "Stick", QDateTime()).ok);
    EXPECT_EQ(QByteArray("present"), readFileBytes(libraryPath(stick.path())));
}

TEST_F(DeviceLibraryPlusStickTest, RestoreToStickWithoutLibrary) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    ASSERT_TRUE(writeFile(backups.path() + "/Stick/20261102-130509/exportLibrary.db", "backup"));
    const QDateTime now(QDate(2026, 11, 3), QTime(9, 0, 0));
    const DlpRestoreResult result = dlpRestoreLibrary(stick.path(), backups.path(), "Stick", now);
    ASSERT_TRUE(result.ok) << result.errorMessage.toStdString();
    EXPECT_EQ(QByteArray("backup"), readFileBytes(libraryPath(stick.path())));
    // The stick had no library, so there is nothing to back up.
    EXPECT_TRUE(result.safetyBackup.isEmpty());
    EXPECT_EQ(QStringList{"20261102-130509"}, folderNames(backups.path() + "/Stick"));
}

TEST_F(DeviceLibraryPlusStickTest, RestoreFailureNamesTheFile) {
    QTemporaryDir stick;
    QTemporaryDir backups;
    ASSERT_TRUE(stick.isValid());
    ASSERT_TRUE(backups.isValid());
    ASSERT_TRUE(writeFile(backups.path() + "/Stick/20261102-130509/exportLibrary.db", "backup"));
    // A file named PIONEER at the root: the folder for the library cannot be
    // made, so the copy fails.
    ASSERT_TRUE(writeFile(QDir(stick.path()).filePath("PIONEER"), "not a folder"));
    const QDateTime now(QDate(2026, 11, 3), QTime(9, 0, 0));
    const DlpRestoreResult result = dlpRestoreLibrary(stick.path(), backups.path(), "Stick", now);
    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.errorMessage.contains(QStringLiteral("exportLibrary.db")))
            << result.errorMessage.toStdString();
    EXPECT_TRUE(result.restoredFiles.isEmpty());
    EXPECT_EQ(QByteArray("not a folder"), readFileBytes(QDir(stick.path()).filePath("PIONEER")));
}

TEST_F(DeviceLibraryPlusStickTest, RestoreMessageIsFilled) {
    EXPECT_EQ(QString("Mix Two cannot restore the backup of the library: disk full"),
            mixxx::rekordbox::dlpMessageRestoreFailed("disk full"));
}
// End of AI-generated text.

TEST_F(DeviceLibraryPlusStickTest, MessagesAreFilled) {
    using namespace mixxx::rekordbox;
    EXPECT_EQ(QString("Mix Two cannot write to the stick."), dlpMessageCannotWrite());
    EXPECT_EQ(QString("The stick does not have sufficient free space. It needs 12 MB more."),
            dlpMessageNotEnoughSpace(12));
    EXPECT_EQ(QString("Players cannot read this file system (NTFS). You can continue."),
            dlpMessageFileSystemNotForPlayers("NTFS"));
    EXPECT_EQ(QString("This stick also has a Device Library. Players that use the "
                      "Device Library do not show the new tracks."),
            dlpMessageHasLegacyLibrary());
    EXPECT_EQ(QString("Mix Two cannot make a backup of the library on the stick: disk full"),
            dlpMessageBackupFailed("disk full"));
}

} // namespace
