#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

namespace mixxx {

/// One mounted volume: its root path and whether the system calls it a
/// removable drive (on Windows: the drive type is "removable").
struct VolumeInfo {
    QString rootPath;
    bool isRemovableDrive = false;
    /// The system calls it a network drive (on Windows: the drive type is
    /// "remote"). A network drive has no trash that keeps a file.
    bool isNetworkDrive = false;
};

/// The folders under which Linux mounts removable media: `/media`,
/// `/media/<user>` and `/run/media/<user>`. The list is empty on other
/// systems.
QStringList removableMountParentPaths();

/// True when `path` is on a removable volume. The volumes and the mount
/// parents come from the caller, so a test can give a fake system.
/// - A path below one of `mountParents` is on a removable volume.
/// - Else the volume with the longest root that holds the path decides.
/// A path that starts with a drive letter compares without regard to letter
/// case; any other path compares with letter case. The function does not
/// touch the disk.
bool isRemovableVolume(const QString& path,
        const QList<VolumeInfo>& volumes,
        const QStringList& mountParents);

/// The same check with the volumes and mounts of this computer. It judges the
/// path and, when the file exists, its resolved path.
bool isRemovableVolume(const QString& path);

/// True when a move to the trash keeps the file safe: the path is not below
/// a mount parent, not a network path (`//server/share` or the same with
/// backslashes), and not on a removable or a network volume. Choice: where
/// Mix Two does not know the volume type (for example `/mnt` on Linux, or
/// macOS), the answer is true; the trash of the system decides there.
bool isTrashSafeVolume(const QString& path,
        const QList<VolumeInfo>& volumes,
        const QStringList& mountParents);

/// The same check for this computer. The function judges the path as it is
/// and, when the file exists, its resolved path, so a link, a junction or a
/// substitute drive that points to a stick or a share is refused. `resolve`
/// gives the resolved path (empty when the file does not exist); the overload
/// without it uses `QFileInfo::canonicalFilePath()`.
bool isTrashSafeVolume(const QString& path,
        const std::function<QString(const QString&)>& resolve,
        const QList<VolumeInfo>& volumes,
        const QStringList& mountParents);
bool isTrashSafeVolume(const QString& path,
        const std::function<QString(const QString&)>& resolve);
bool isTrashSafeVolume(const QString& path);

/// What moveToTrashChecked() did.
enum class TrashResult {
    /// The file is now in the trash.
    Moved,
    /// The file is on a removable or network drive, where the trash does not
    /// keep it safe. Mix Two did not touch it.
    RefusedUnsafe,
    /// The system could not move the file to the trash.
    Failed,
};

/// Moves the file to the trash, but only where the trash is safe: never a
/// file on a removable or network drive (the user can pull the drive out, and
/// a share has no trash; the file is then gone). `isTrashSafe` and
/// `moveToTrash` are the two system calls; the overload without them uses
/// isTrashSafeVolume() and the real trash.
TrashResult moveToTrashChecked(const QString& path,
        const std::function<bool(const QString&)>& isTrashSafe,
        const std::function<bool(const QString&)>& moveToTrash);
TrashResult moveToTrashChecked(const QString& path);

} // namespace mixxx

// End of AI-generated text.
