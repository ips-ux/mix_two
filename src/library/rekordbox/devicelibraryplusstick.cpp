#include "library/rekordbox/devicelibraryplusstick.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
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

// AI-generated (Claude Code). Start of AI-generated text.
/// The time-stamp folder of `time`: the name that dlpBackupLibrary() gives it.
QString stampOf(const QDateTime& time) {
    return QLocale::c().toString(time, kBackupFolderFormat);
}

DlpRestoreResult failedRestore(const QString& reason) {
    DlpRestoreResult result;
    result.errorMessage = dlpMessageRestoreFailed(reason);
    return result;
}

QString copyFailedReason(const QString& stickPath) {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "Mix Two cannot write the file %1 to the stick.")
            .arg(stickPath);
}

/// The newest time-stamp folder in `deviceDir` that holds a library file, or an
/// empty string.
QString newestBackupStamp(const QDir& deviceDir) {
    const QRegularExpression stampPattern = anchored(QStringLiteral("\\d{8}-\\d{6}"));
    const QStringList stamps = deviceDir
                                       .entryList(QDir::Dirs | QDir::NoSymLinks |
                                                       QDir::NoDotAndDotDot,
                                               QDir::Name)
                                       .filter(stampPattern);
    // The names sort as text in time order. The newest is the last.
    for (auto it = stamps.crbegin(); it != stamps.crend(); ++it) {
        const QFileInfo library(QDir(deviceDir.filePath(*it)).filePath(kLibraryFileName));
        if (library.isFile() && !library.isSymLink()) {
            return *it;
        }
    }
    return QString();
}
// End of AI-generated text.

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

// AI-generated (Claude Code). Start of AI-generated text.
QString dlpFindPathNoCase(const QString& root, const QString& relativePath) {
    if (root.trimmed().isEmpty() || !QFileInfo(root).isDir()) {
        return {};
    }
    QString current = root;
    const QStringList names = relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString& name : names) {
        const QDir folder(current);
        const QStringList entries = folder.entryList(
                QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                QDir::Name);
        QString match;
        for (const QString& entry : entries) {
            if (entry == name) {
                match = entry;
                break;
            }
            if (match.isEmpty() && entry.compare(name, Qt::CaseInsensitive) == 0) {
                match = entry;
            }
        }
        if (match.isEmpty()) {
            return {};
        }
        current = folder.filePath(match);
    }
    return current;
}
// End of AI-generated text.

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
    // The folder names on the stick can have any letter case.
    info.hasPlusLibrary = QFileInfo(rootDir.filePath(DeviceLibraryPlusDb::kRelativePath)).isFile() ||
            QFileInfo(dlpFindPathNoCase(root, DeviceLibraryPlusDb::kRelativePath)).isFile();
    const QString legacyPath = QFileInfo(DeviceLibraryPlusDb::kRelativePath).path() +
            QStringLiteral("/export.pdb");
    info.hasLegacyLibrary = QFileInfo(rootDir.filePath(legacyPath)).isFile() ||
            QFileInfo(dlpFindPathNoCase(root, legacyPath)).isFile();
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

// AI-generated (Claude Code). Start of AI-generated text.
DlpRestoreResult dlpRestoreLibrary(const QString& stickRoot,
        const QString& backupBaseDir,
        const QString& deviceName,
        const QDateTime& now) {
    // An empty path would be a path below the working folder: do not even
    // read the stick.
    if (isBlank(stickRoot)) {
        return failedRestore(QCoreApplication::translate("DeviceLibraryPlus",
                "The path of the stick is empty."));
    }
    return dlpRestoreLibrary(dlpStickInfo(stickRoot), backupBaseDir, deviceName, now);
}

DlpRestoreResult dlpRestoreLibrary(const DlpStickInfo& info,
        const QString& backupBaseDir,
        const QString& deviceName,
        const QDateTime& now) {
    if (isBlank(info.root)) {
        return failedRestore(QCoreApplication::translate("DeviceLibraryPlus",
                "The path of the stick is empty."));
    }
    if (isBlank(backupBaseDir)) {
        return failedRestore(QCoreApplication::translate("DeviceLibraryPlus",
                "The path of the backup folder is empty."));
    }
    if (!now.isValid()) {
        return failedRestore(QCoreApplication::translate("DeviceLibraryPlus",
                "The time of the backup is not valid."));
    }
    if (!info.isReady) {
        return failedRestore(QCoreApplication::translate("DeviceLibraryPlus",
                "The stick is not ready."));
    }
    if (info.isReadOnly) {
        DlpRestoreResult result;
        result.errorMessage = dlpMessageCannotWrite();
        return result;
    }

    // The backups of this stick: the folder that dlpBackupLibrary() uses.
    const QDir deviceDir(QDir(backupBaseDir).filePath(fatSafeName(deviceName)));
    const QString stamp = newestBackupStamp(deviceDir);
    if (stamp.isEmpty()) {
        return failedRestore(QCoreApplication::translate("DeviceLibraryPlus",
                "Mix Two has no backup of the library of this stick."));
    }
    const QString backupFolder = deviceDir.filePath(stamp);
    const QString source = QDir(backupFolder).filePath(kLibraryFileName);
    const QString target = QDir(info.root).filePath(DeviceLibraryPlusDb::kRelativePath);
    const QString stickPath = QStringLiteral("/") + DeviceLibraryPlusDb::kRelativePath;

    // Step 2: copy the backup to a temporary file next to the library. The
    // copy has its own bytes, so a later step cannot change it. Nothing is
    // written to the library until commit().
    if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
        return failedRestore(copyFailedReason(stickPath));
    }
    QSaveFile staged(target);
    QFile backup(source);
    if (!backup.open(QIODevice::ReadOnly) || !staged.open(QIODevice::WriteOnly)) {
        return failedRestore(copyFailedReason(stickPath));
    }
    constexpr qint64 kChunk = 1024 * 1024;
    while (!backup.atEnd()) {
        const QByteArray chunk = backup.read(kChunk);
        if (chunk.isEmpty() || staged.write(chunk) != chunk.size()) {
            staged.cancelWriting();
            return failedRestore(copyFailedReason(stickPath));
        }
    }
    if (backup.error() != QFileDevice::NoError) {
        staged.cancelWriting();
        return failedRestore(copyFailedReason(stickPath));
    }

    DlpRestoreResult result;
    // Step 3: a backup of the library that the restore replaces. The time
    // stamp is one that no folder has: the backup replaces no folder, also not
    // the one that is restored (the same second).
    if (QFileInfo(target).isFile()) {
        QDateTime safetyTime = now;
        for (int tries = 0; tries < 3600 && QFileInfo::exists(deviceDir.filePath(stampOf(safetyTime)));
                ++tries) {
            safetyTime = safetyTime.addSecs(1);
        }
        QString backupError;
        result.safetyBackup = dlpBackupLibrary(
                info.root, backupBaseDir, deviceName, safetyTime, &backupError);
        if (result.safetyBackup.isEmpty()) {
            staged.cancelWriting();
            DlpRestoreResult failed;
            failed.errorMessage = backupError;
            return failed;
        }
    }

    // Step 4: the rename. The library is the old file or the new file, never
    // a part of either.
    if (!staged.commit()) {
        DlpRestoreResult failed = failedRestore(copyFailedReason(stickPath));
        failed.safetyBackup = result.safetyBackup;
        return failed;
    }
    result.ok = true;
    result.restoredFrom = backupFolder;
    result.restoredFiles.append(stickPath);
    return result;
}
// End of AI-generated text.

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

// AI-generated (Claude Code). Start of AI-generated text.
QString dlpMessageRestoreFailed(const QString& reason) {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "Mix Two cannot restore the backup of the library: %1")
            .arg(reason);
}
// End of AI-generated text.

} // namespace rekordbox
} // namespace mixxx
