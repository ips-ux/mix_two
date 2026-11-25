#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <QList>
#include <QString>
#include <QStringList>
#include <memory>

namespace mixxx {

/// What an eject did. `message` tells the user why it failed (empty when `ok`).
struct StickEjectResult {
    bool ok = false;
    QString message;
};

/// The system call that makes a stick safe to remove. It has one
/// implementation for Windows, one for Linux, and a fake in the tests. The
/// caller must give it only the volume of the stick of the finished export;
/// use ejectExportedStick() for that.
class StickEjector {
  public:
    virtual ~StickEjector() = default;
    /// Ejects the volume that has the root `stickRoot`. It blocks the caller.
    /// The worst case on Windows is about 3 seconds (4 tries to lock the
    /// volume). The worst case on Linux is about 40 seconds (the unmount and
    /// the power-off each have a time-out). The caller shows a busy cursor.
    /// Call it on the main thread.
    virtual StickEjectResult eject(const QString& stickRoot) = 0;

    /// A text that names the volume that has the root `stickRoot` now: on
    /// Windows the serial number, the label and the file system; on Linux the
    /// UUID of the file system. It is empty when the system gives no identity.
    /// Two volumes that have the same text are the same stick. Call it on the
    /// main thread.
    virtual QString volumeIdentity(const QString& stickRoot) = 0;
};

/// The ejector of this system: Windows (volume lock, dismount and eject with
/// `DeviceIoControl`) or Linux (`udisksctl`). On another system its eject()
/// always fails with a message.
std::unique_ptr<StickEjector> makeSystemStickEjector();

/// True when `path` is the stick root or a file or folder below it. The
/// separators `/` and `\` are the same, a drive letter ignores letter case,
/// and other paths compare with letter case. An empty path or root gives
/// false. The function does not touch the disk.
bool isPathOnStick(const QString& path, const QString& stickRoot);

/// Ejects `rootToEject` only when all of these are true; else it does not call
/// the ejector and it gives a message for the user:
/// - `exportedStickRoot` (the volume of the export that just ended) and
///   `rootToEject` are the same volume;
/// - `exportedIdentity` (the identity that the ejector read when the export
///   began) is not empty and is the identity that the volume at `rootToEject`
///   has now, so a stick that the user swapped under the same drive letter is
///   not ejected;
/// - no path of `openFilePaths` (the files that Mix Two has open, for example
///   the tracks that are loaded in a deck) is on that stick.
/// A failed eject gives a message that tells the user not to remove the stick
/// yet.
StickEjectResult ejectExportedStick(StickEjector& ejector,
        const QString& exportedStickRoot,
        const QString& exportedIdentity,
        const QString& rootToEject,
        const QStringList& openFilePaths);

/// The device calls of the Windows eject, for one open volume. The real class
/// uses `DeviceIoControl`; the tests use a fake. ejectVolumeSteps() calls them
/// in the order of the documented procedure.
class VolumeSteps {
  public:
    virtual ~VolumeSteps() = default;
    /// True when the volume is on a removable device (a stick or a card).
    virtual bool isStickDevice() = 0;
    virtual void flush() = 0;
    virtual bool lock() = 0;
    virtual void pause() = 0;
    virtual bool unlock() = 0;
    virtual bool dismount() = 0;
    /// Lets the system remove the media. The result does not matter.
    virtual void allowRemoval() = 0;
    virtual bool eject() = 0;
    /// Closes the handle of the volume.
    virtual void close() = 0;
    /// The system error code of the last failed call (for the log).
    virtual unsigned long lastError() = 0;
};

/// Runs the steps: check the device, flush, lock (4 tries with a pause),
/// dismount, allow removal, eject. It never dismounts a volume that it could
/// not lock. After a failed dismount or a failed eject it unlocks. It calls
/// `close()` exactly once on every path.
StickEjectResult ejectVolumeSteps(VolumeSteps& steps);

/// True when a Linux block device is a stick: the `removable` flag of its disk
/// in sysfs is set, or its device path in sysfs (`sysfsDevicePath`, the
/// resolved path of the disk) goes through a USB device (a component `usb<N>`)
/// or an MMC/SD host (`mmc_host`). An internal disk that a desktop mounts below
/// `/media` has neither.
bool isLinuxStickDrive(bool removableFlag, const QString& sysfsDevicePath);

/// One program call of the Linux eject.
struct EjectCommand {
    QString program;
    QStringList arguments;
};

/// The calls that eject the block device `device` on Linux: unmount, then
/// power off. The list is empty when `device` is not a plain path below
/// `/dev/`. The function does not run anything.
QList<EjectCommand> linuxEjectCommands(const QString& device);

} // namespace mixxx

// End of AI-generated text.
