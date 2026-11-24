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

/// The same check with the volumes and mounts of this computer.
bool isRemovableVolume(const QString& path);

/// What moveToTrashChecked() did.
enum class TrashResult {
    /// The file is now in the trash.
    Moved,
    /// The file is on a removable volume. Mix Two did not touch it.
    RefusedRemovable,
    /// The system could not move the file to the trash.
    Failed,
};

/// Moves the file to the trash, but never a file on a removable volume (the
/// user can pull the drive out, and then the file is gone). `isRemovable` and
/// `moveToTrash` are the two system calls; the overload without them uses the
/// real ones.
TrashResult moveToTrashChecked(const QString& path,
        const std::function<bool(const QString&)>& isRemovable,
        const std::function<bool(const QString&)>& moveToTrash);
TrashResult moveToTrashChecked(const QString& path);

} // namespace mixxx

// End of AI-generated text.
