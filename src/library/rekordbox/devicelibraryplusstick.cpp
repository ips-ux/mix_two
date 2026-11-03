#include "library/rekordbox/devicelibraryplusstick.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QStringList>
#include <QtDebug>

#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/devicelibraryplusmapping.h"

namespace mixxx {
namespace rekordbox {

namespace {

constexpr int kBackupsToKeep = 5;
constexpr qint64 kBytesPerTrack = 512 * 1024;
constexpr qint64 kReserveBytes = 64 * 1024 * 1024;

const QString kLibraryFileName = QStringLiteral("exportLibrary.db");
const QString kBackupFolderFormat = QStringLiteral("yyyyMMdd-HHmmss");

QRegularExpression anchored(const QString& pattern) {
    return QRegularExpression(QRegularExpression::anchoredPattern(pattern));
}

/// Removes the files in `folder` whose names match `pattern`. A file is a
/// real file: a folder or a symbolic link with a matching name stays.
int removeMatchingFiles(const QDir& folder, const QRegularExpression& pattern) {
    int removed = 0;
    const QFileInfoList entries = folder.entryInfoList(
            QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo& entry : entries) {
        if (!pattern.match(entry.fileName()).hasMatch()) {
            continue;
        }
        if (QFile::remove(entry.absoluteFilePath())) {
            ++removed;
        } else {
            qWarning() << "DeviceLibraryPlus: cannot remove the file"
                       << entry.absoluteFilePath();
        }
    }
    return removed;
}

/// True when the two files have the same bytes.
bool sameBytes(const QString& first, const QString& second) {
    QFile a(first);
    QFile b(second);
    if (!a.open(QIODevice::ReadOnly) || !b.open(QIODevice::ReadOnly) || a.size() != b.size()) {
        return false;
    }
    constexpr qint64 kChunk = 1024 * 1024;
    while (!a.atEnd()) {
        const QByteArray chunkA = a.read(kChunk);
        const QByteArray chunkB = b.read(kChunk);
        if (chunkA.isEmpty() || chunkA != chunkB) {
            return false;
        }
    }
    return b.atEnd() && a.error() == QFileDevice::NoError && b.error() == QFileDevice::NoError;
}

/// Moves a file: a copy, a check that the copy has the same bytes, then the
/// removal of the original. `target` must not exist. When a step fails, the
/// original stays and the function removes only its own copy.
bool moveFile(const QString& source, const QString& target) {
    if (QFileInfo::exists(target) || !QFile::copy(source, target)) {
        return false;
    }
    if (!sameBytes(source, target)) {
        QFile::remove(target);
        return false;
    }
    if (!QFile::remove(source)) {
        // The copy stays on the computer; the original stays on the stick.
        return false;
    }
    return true;
}

/// The name in `folder` for a file: the name, or the name with " (2)",
/// " (3)", ... when a file or folder of that name is there.
QString freeFileName(const QDir& folder, const QString& name) {
    QString candidate = folder.filePath(name);
    for (int number = 2; QFileInfo::exists(candidate) || QFileInfo(candidate).isSymLink();
            ++number) {
        candidate = folder.filePath(name + QStringLiteral(" (%1)").arg(number));
    }
    return candidate;
}

/// An empty path, or a path of only white space. Such a path would be a path
/// below the working folder of the process.
bool isBlank(const QString& path) {
    return path.trimmed().isEmpty();
}

QString backupFailed(const QString& reason, QString* pErrorMessage) {
    if (pErrorMessage) {
        *pErrorMessage = dlpMessageBackupFailed(reason);
    }
    return QString();
}

/// Removes the oldest time-stamp folders of the device beyond the newest
/// `kBackupsToKeep`. The folder `keep` stays in any case.
void removeOldBackups(const QDir& deviceDir, const QString& keep) {
    const QRegularExpression stampPattern = anchored(QStringLiteral("\\d{8}-\\d{6}"));
    QStringList stamps = deviceDir.entryList(
            QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDir::Name);
    stamps = stamps.filter(stampPattern);
    // The names sort as text in time order. The newest is the last.
    for (int i = 0; i < static_cast<int>(stamps.size()) - kBackupsToKeep; ++i) {
        if (stamps.at(i) == keep) {
            continue;
        }
        const QString path = deviceDir.filePath(stamps.at(i));
        if (!QDir(path).removeRecursively()) {
            qWarning() << "DeviceLibraryPlus: cannot remove the old backup" << path;
        }
    }
}

} // namespace

DlpFileSystem dlpFileSystemFromType(const QByteArray& type) {
    const QByteArray lower = type.toLower();
    if (lower == "vfat" || lower == "msdos" || lower == "fat32" || lower == "fat") {
        return DlpFileSystem::Fat32;
    }
    if (lower == "exfat") {
        return DlpFileSystem::ExFat;
    }
    for (const char* name : {"ntfs",
                 "ext2",
                 "ext3",
                 "ext4",
                 "btrfs",
                 "xfs",
                 "apfs",
                 "hfs",
                 "hfsplus"}) {
        if (lower == name) {
            return DlpFileSystem::NotForPlayers;
        }
    }
    return DlpFileSystem::Unknown;
}

DlpStickInfo dlpStickInfo(const QString& root) {
    DlpStickInfo info;
    info.root = root;
    const QStorageInfo storage(root);
    info.isReady = storage.isValid() && storage.isReady();
    if (storage.isValid()) {
        info.volumeLabel = storage.name();
        info.fileSystem = dlpFileSystemFromType(storage.fileSystemType());
        info.isReadOnly = storage.isReadOnly();
        info.bytesFree = storage.bytesAvailable();
    }
    const QDir rootDir(root);
    info.hasPlusLibrary = QFileInfo(rootDir.filePath(DeviceLibraryPlusDb::kRelativePath)).isFile();
    const QString legacyPath = QFileInfo(DeviceLibraryPlusDb::kRelativePath).path() +
            QStringLiteral("/export.pdb");
    info.hasLegacyLibrary = QFileInfo(rootDir.filePath(legacyPath)).isFile();
    return info;
}

qint64 dlpBytesNeeded(qint64 audioBytesToCopy, int trackCount) {
    return audioBytesToCopy + static_cast<qint64>(trackCount) * kBytesPerTrack + kReserveBytes;
}

QString dlpBackupLibrary(const QString& stickRoot,
        const QString& backupBaseDir,
        const QString& deviceName,
        const QDateTime& now,
        QString* pErrorMessage) {
    // Ruling R7: refuse values that would make a path below the working folder.
    if (isBlank(stickRoot)) {
        return backupFailed(QCoreApplication::translate("DeviceLibraryPlus",
                                    "The path of the stick is empty."),
                pErrorMessage);
    }
    if (isBlank(backupBaseDir)) {
        return backupFailed(QCoreApplication::translate("DeviceLibraryPlus",
                                    "The path of the backup folder is empty."),
                pErrorMessage);
    }
    if (!now.isValid()) {
        return backupFailed(QCoreApplication::translate("DeviceLibraryPlus",
                                    "The time of the backup is not valid."),
                pErrorMessage);
    }
    const QString source = QDir(stickRoot).filePath(DeviceLibraryPlusDb::kRelativePath);
    const QFileInfo sourceInfo(source);
    if (!sourceInfo.isFile()) {
        return backupFailed(QCoreApplication::translate("DeviceLibraryPlus",
                                    "The stick has no library."),
                pErrorMessage);
    }

    const QDir deviceDir(QDir(backupBaseDir).filePath(fatSafeName(deviceName)));
    const QString stamp = QLocale::c().toString(now, kBackupFolderFormat);
    const QString stampDir = deviceDir.filePath(stamp);
    if (!QDir().mkpath(stampDir)) {
        return backupFailed(QCoreApplication::translate("DeviceLibraryPlus",
                                    "Mix Two cannot make the backup folder."),
                pErrorMessage);
    }

    const QString target = QDir(stampDir).filePath(kLibraryFileName);
    // The same second twice: the new copy replaces the old one.
    QFile::remove(target);
    if (!QFile::copy(source, target) || QFileInfo(target).size() != sourceInfo.size()) {
        QFile::remove(target);
        QDir().rmdir(stampDir);
        return backupFailed(QCoreApplication::translate("DeviceLibraryPlus",
                                    "Mix Two cannot copy the library."),
                pErrorMessage);
    }

    removeOldBackups(deviceDir, stamp);
    return target;
}

int dlpMoveLeftoverFiles(const QString& stickRoot,
        const QString& backupBaseDir,
        const QString& deviceName,
        const QDateTime& now) {
    // Ruling R16: the names can also be files of the DJ (for example
    // "exportLibrary.db.backup"), so Mix Two never deletes them.
    if (isBlank(stickRoot) || isBlank(backupBaseDir) || !now.isValid()) {
        return 0;
    }
    const QDir folder(QFileInfo(QDir(stickRoot).filePath(DeviceLibraryPlusDb::kRelativePath))
                    .absolutePath());
    const QRegularExpression pattern = anchored(
            QStringLiteral("exportLibrary\\.db\\.[A-Za-z0-9]{6}|exportLibrary\\.db\\.tmp"));
    QFileInfoList leftovers;
    for (const QFileInfo& entry : folder.entryInfoList(
                 QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Hidden)) {
        if (pattern.match(entry.fileName()).hasMatch()) {
            leftovers.append(entry);
        }
    }
    if (leftovers.isEmpty()) {
        return 0;
    }
    const QString targetDir = QDir(QDir(backupBaseDir).filePath(fatSafeName(deviceName)))
                                      .filePath(QStringLiteral("leftovers/") +
                                              QLocale::c().toString(now, kBackupFolderFormat));
    if (!QDir().mkpath(targetDir)) {
        qWarning() << "DeviceLibraryPlus: cannot make the folder for the leftover files"
                   << targetDir;
        return 0;
    }
    int moved = 0;
    for (const QFileInfo& entry : std::as_const(leftovers)) {
        if (moveFile(entry.absoluteFilePath(), freeFileName(QDir(targetDir), entry.fileName()))) {
            ++moved;
        } else {
            qWarning() << "DeviceLibraryPlus: cannot move the leftover file"
                       << entry.absoluteFilePath();
        }
    }
    return moved;
}

int dlpRemoveOldWorkingCopies(const QDateTime& olderThan) {
    const QDir tempDir(QDir::tempPath());
    const QFileInfoList entries = tempDir.entryInfoList(
            QStringList{QStringLiteral("mixtwo-dlp-*")},
            QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Hidden);
    int removed = 0;
    for (const QFileInfo& entry : entries) {
        // The name filter is a wildcard; check the prefix again.
        if (!entry.fileName().startsWith(QStringLiteral("mixtwo-dlp-")) ||
                entry.isSymLink() || !(entry.lastModified() < olderThan)) {
            continue;
        }
        if (QDir(entry.absoluteFilePath()).removeRecursively()) {
            ++removed;
        } else {
            qWarning() << "DeviceLibraryPlus: cannot remove the old working copy"
                       << entry.absoluteFilePath();
        }
    }
    return removed;
}

int dlpRemoveAnlzLeftovers(const QString& anlzFolder) {
    if (isBlank(anlzFolder)) {
        return 0;
    }
    const QRegularExpression pattern = anchored(
            QStringLiteral("ANLZ0000\\.(DAT|EXT|2EX)\\.[A-Za-z0-9]{6}"));
    return removeMatchingFiles(QDir(anlzFolder), pattern);
}

QString dlpMessageCannotWrite() {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "Mix Two cannot write to the stick.");
}

QString dlpMessageNotEnoughSpace(qint64 megabytesMissing) {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "The stick does not have sufficient free space. It needs %1 MB more.")
            .arg(megabytesMissing);
}

QString dlpMessageFileSystemNotForPlayers(const QString& fileSystemName) {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "Players cannot read this file system (%1). You can continue.")
            .arg(fileSystemName);
}

QString dlpMessageHasLegacyLibrary() {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "This stick also has a Device Library. Players that use the "
            "Device Library do not show the new tracks.");
}

QString dlpMessageNoTrackExported() {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "No track was exported. The stick did not change.");
}

QString dlpMessageBackupFailed(const QString& reason) {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "Mix Two cannot make a backup of the library on the stick: %1")
            .arg(reason);
}

} // namespace rekordbox
} // namespace mixxx
