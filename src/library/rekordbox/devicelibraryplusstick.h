#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QtGlobal>

namespace mixxx {
namespace rekordbox {

/// Mix Two: the checks of a stick before the export, the backup of its
/// library, and the files that an export that did not finish leaves. The
/// functions have no state and no thread rules. The export job calls them on
/// its worker thread, and the dialog calls dlpStickInfo() and the message
/// functions on the main thread.

/// The largest file that a FAT32 stick holds.
constexpr qint64 kFat32MaxFileSize = 4294967295;

/// The file system of a stick, as far as the players are concerned.
enum class DlpFileSystem {
    Fat32,
    ExFat,
    /// A file system that the players cannot read (NTFS, ext4, APFS, ...).
    NotForPlayers,
    Unknown,
};

/// The file system for the type name that `QStorageInfo::fileSystemType()`
/// gives (case-insensitive): `vfat`, `msdos`, `fat32` and `fat` give Fat32,
/// `exfat` gives ExFat, `ntfs`, `ext2`, `ext3`, `ext4`, `btrfs`, `xfs`,
/// `apfs`, `hfs` and `hfsplus` give NotForPlayers. Any other name gives
/// Unknown.
DlpFileSystem dlpFileSystemFromType(const QByteArray& type);

/// What the dialog and the export job need to know about a stick.
struct DlpStickInfo {
    QString root;
    QString volumeLabel;
    DlpFileSystem fileSystem = DlpFileSystem::Unknown;
    bool isReady = false;
    bool isReadOnly = false;
    qint64 bytesFree = 0;
    /// `PIONEER/rekordbox/exportLibrary.db` exists.
    bool hasPlusLibrary = false;
    /// `PIONEER/rekordbox/export.pdb` exists.
    bool hasLegacyLibrary = false;
};

/// Reads the volume data of the stick at `root` and looks for its two
/// libraries. It writes nothing.
DlpStickInfo dlpStickInfo(const QString& root);

/// The free space that an export needs: the audio bytes to copy, 512 KiB per
/// track (ANLZ files, artwork and the growth of the database) and 64 MiB of
/// reserve.
qint64 dlpBytesNeeded(qint64 audioBytesToCopy, int trackCount);

/// Copies `PIONEER/rekordbox/exportLibrary.db` (only this file) of the stick
/// to `<backupBaseDir>/<fatSafeName(deviceName)>/<yyyyMMdd-HHmmss>/exportLibrary.db`
/// (the time stamp is `now`). When the copy has the same size as the
/// original, the function removes the oldest time-stamp folders of this
/// device beyond the newest 5. It removes only folders whose names have the
/// form `yyyyMMdd-HHmmss`. Returns the path of the copy. Returns an empty
/// string and sets `pErrorMessage` (when it is not null) when the backup
/// fails. It refuses an empty (or white-space) `stickRoot` or `backupBaseDir`
/// and an invalid `now`; then it does nothing else.
QString dlpBackupLibrary(const QString& stickRoot,
        const QString& backupBaseDir,
        const QString& deviceName,
        const QDateTime& now,
        QString* pErrorMessage);

/// Moves the files in `PIONEER/rekordbox` of the stick that an export that
/// did not finish can leave, `exportLibrary.db.XXXXXX` (6 letters or digits)
/// and `exportLibrary.db.tmp`, to the computer (Ruling R16): into
/// `<backupBaseDir>/<fatSafeName(deviceName)>/leftovers/<yyyyMMdd-HHmmss>/`
/// (the time stamp is `now`, in the C locale). The names can also be files of
/// the DJ (for example `exportLibrary.db.backup`), so the function never
/// deletes one: it copies the file, checks that the copy has the same bytes,
/// and only then removes the file on the stick. A name that is in the target
/// folder already gets " (2)", " (3)", ... at the end. A file that it cannot
/// move stays on the stick, with a log line. It moves only real files (no
/// folder, no symbolic link), does not enter other folders, and makes the
/// target folder only when there is a file to move. The backup retention of
/// dlpBackupLibrary() does not count or remove the `leftovers` folder.
/// Returns the number of moved files. An empty (or white-space) `stickRoot`
/// or `backupBaseDir`, or an invalid `now`, moves nothing and gives 0.
int dlpMoveLeftoverFiles(const QString& stickRoot,
        const QString& backupBaseDir,
        const QString& deviceName,
        const QDateTime& now);

/// Removes the folders `mixtwo-dlp-*` in the temporary folder of the computer
/// (the working copies of DeviceLibraryPlusDb) that were last changed before
/// `olderThan`. It removes no other folder and does not follow a symbolic
/// link out of the temporary folder. Returns the number of removed folders.
int dlpRemoveOldWorkingCopies(const QDateTime& olderThan);

/// Removes the files `ANLZ0000.DAT.XXXXXX`, `ANLZ0000.EXT.XXXXXX` and
/// `ANLZ0000.2EX.XXXXXX` (6 letters or digits) in the folder. It removes
/// nothing else and does not enter other folders. Returns the number of
/// removed files. An empty (or white-space) path removes nothing and gives 0.
int dlpRemoveAnlzLeftovers(const QString& anlzFolder);

/// The texts that the dialog and the export job show (ASD-STE100). The same
/// function gives the same text in both places.

/// The stick is read-only or Mix Two cannot write to it.
QString dlpMessageCannotWrite();
/// The stick needs `megabytesMissing` MB more.
QString dlpMessageNotEnoughSpace(qint64 megabytesMissing);
/// Players cannot read the file system. The user can continue.
QString dlpMessageFileSystemNotForPlayers(const QString& fileSystemName);
/// The stick also has a Device Library (spec 7.4).
QString dlpMessageHasLegacyLibrary();
/// The export had tracks, and no track has a row on the stick: every track
/// was skipped, and no older row of it is on the stick (Ruling R20).
QString dlpMessageNoTrackExported();
/// The backup of the library failed for the reason in `reason`.
QString dlpMessageBackupFailed(const QString& reason);

} // namespace rekordbox
} // namespace mixxx
