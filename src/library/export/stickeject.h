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
    /// Ejects the volume that has the root `stickRoot`. It blocks for a few
    /// seconds at most. Call it on the main thread.
    virtual StickEjectResult eject(const QString& stickRoot) = 0;
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
/// - no path of `openFilePaths` (the files that Mix Two has open, for example
///   the tracks that are loaded in a deck) is on that stick.
/// A failed eject gives a message that tells the user not to remove the stick
/// yet.
StickEjectResult ejectExportedStick(StickEjector& ejector,
        const QString& exportedStickRoot,
        const QString& rootToEject,
        const QStringList& openFilePaths);

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
