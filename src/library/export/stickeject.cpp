// AI-generated (Claude Code). Start of AI-generated text.
#include "library/export/stickeject.h"

#include <QCoreApplication>
#include <QDir>
#include <QtGlobal>

#if defined(Q_OS_WIN)
#include <windows.h>
// winioctl.h must come after windows.h.
#include <winioctl.h>

#include <string>
#elif defined(Q_OS_LINUX)
#include <QProcess>
#include <QStorageInfo>

#include "util/removablevolume.h"
#endif

namespace mixxx {

namespace {

QString tr(const char* text) {
    return QCoreApplication::translate("StickEject", text);
}

bool startsWithDriveLetter(const QString& path) {
    return path.size() >= 2 && path.at(1) == QLatin1Char(':') && path.at(0).isLetter();
}

/// The path with `/` as separator, no `.` or `..`, and no trailing `/`
/// (except for the root `/`). A drive root `C:/` becomes `C:`. Empty for an
/// empty or white-space path.
QString normalizedPath(const QString& path) {
    if (path.trimmed().isEmpty()) {
        return QString();
    }
    QString result = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
    if (result.size() > 1 && result.endsWith(QLatin1Char('/'))) {
        result.chop(1);
    }
    return result;
}

bool isSameVolumeRoot(const QString& a, const QString& b) {
    const QString na = normalizedPath(a);
    const QString nb = normalizedPath(b);
    if (na.isEmpty() || nb.isEmpty()) {
        return false;
    }
    const Qt::CaseSensitivity cs = (startsWithDriveLetter(na) && startsWithDriveLetter(nb))
            ? Qt::CaseInsensitive
            : Qt::CaseSensitive;
    return na.compare(nb, cs) == 0;
}

#if defined(Q_OS_WIN)

/// Windows. Choice: the documented volume calls, not the shell verb. The
/// code opens the volume (`\\.\E:`), locks it, dismounts it and ejects the
/// media with `DeviceIoControl`. A failed lock is the sure sign that a
/// program still uses the stick, and the result is known when the call
/// returns. The shell verb (as the tray icon of the system uses it) shows its
/// own windows and gives no result to the caller.
class WindowsStickEjector : public StickEjector {
  public:
    StickEjectResult eject(const QString& stickRoot) override {
        const QString root = normalizedPath(stickRoot);
        // Only a drive letter. A volume that has a folder as mount point or a
        // network path is not a stick that this code ejects.
        if (!startsWithDriveLetter(root) || root.size() != 2) {
            return fail(tr("The stick has no drive letter."));
        }
        const wchar_t letter = static_cast<wchar_t>(root.at(0).toUpper().unicode());

        // Never the drive of the system.
        wchar_t systemDrive[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"SystemDrive", systemDrive, MAX_PATH) > 0 &&
                towupper(systemDrive[0]) == letter) {
            return fail(tr("This is the drive of the system."));
        }

        const std::wstring driveRoot = std::wstring(1, letter) + L":\\";
        const UINT driveType = GetDriveTypeW(driveRoot.c_str());
        if (driveType != DRIVE_REMOVABLE && driveType != DRIVE_FIXED) {
            return fail(tr("This drive is not a removable drive."));
        }

        const std::wstring volumePath = std::wstring(L"\\\\.\\") + letter + L":";
        HANDLE handle = CreateFileW(volumePath.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            return fail(tr("Mix Two cannot open the stick (error %1).")
                                .arg(GetLastError()));
        }

        StickEjectResult result = ejectOpenVolume(handle, driveType);
        CloseHandle(handle);
        return result;
    }

  private:
    static constexpr int kLockTries = 4;
    static constexpr DWORD kLockRetryMs = 500;

    static StickEjectResult fail(const QString& message) {
        return {false, message};
    }

    /// A fixed drive is only a stick when its bus is USB, SD or MMC.
    static bool isStickBus(HANDLE handle) {
        STORAGE_PROPERTY_QUERY query = {};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;
        alignas(STORAGE_DEVICE_DESCRIPTOR) BYTE buffer[1024] = {};
        DWORD returned = 0;
        if (!DeviceIoControl(handle,
                    IOCTL_STORAGE_QUERY_PROPERTY,
                    &query,
                    sizeof(query),
                    buffer,
                    sizeof(buffer),
                    &returned,
                    nullptr) ||
                returned < sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
            return false;
        }
        const auto* pDescriptor = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(buffer);
        return pDescriptor->BusType == BusTypeUsb ||
                pDescriptor->BusType == BusTypeSd ||
                pDescriptor->BusType == BusTypeMmc;
    }

    static StickEjectResult ejectOpenVolume(HANDLE handle, UINT driveType) {
        if (driveType == DRIVE_FIXED && !isStickBus(handle)) {
            return fail(tr("This drive is not a removable drive."));
        }

        DWORD returned = 0;
        // Write what the system still holds in its cache.
        FlushFileBuffers(handle);

        bool locked = false;
        for (int attempt = 0; attempt < kLockTries && !locked; ++attempt) {
            locked = DeviceIoControl(handle,
                    FSCTL_LOCK_VOLUME,
                    nullptr,
                    0,
                    nullptr,
                    0,
                    &returned,
                    nullptr);
            if (!locked && attempt + 1 < kLockTries) {
                Sleep(kLockRetryMs);
            }
        }
        if (!locked) {
            return fail(tr("Another program uses the stick."));
        }

        if (!DeviceIoControl(handle,
                    FSCTL_DISMOUNT_VOLUME,
                    nullptr,
                    0,
                    nullptr,
                    0,
                    &returned,
                    nullptr)) {
            const DWORD error = GetLastError();
            DeviceIoControl(handle, FSCTL_UNLOCK_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr);
            return fail(tr("The system cannot dismount the stick (error %1).").arg(error));
        }

        // The result of this call does not matter: the eject call below
        // decides. It lets the system remove the media.
        PREVENT_MEDIA_REMOVAL removal = {};
        removal.PreventMediaRemoval = FALSE;
        DeviceIoControl(handle,
                IOCTL_STORAGE_MEDIA_REMOVAL,
                &removal,
                sizeof(removal),
                nullptr,
                0,
                &returned,
                nullptr);

        if (!DeviceIoControl(handle,
                    IOCTL_STORAGE_EJECT_MEDIA,
                    nullptr,
                    0,
                    nullptr,
                    0,
                    &returned,
                    nullptr)) {
            const DWORD error = GetLastError();
            DeviceIoControl(handle, FSCTL_UNLOCK_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr);
            return fail(tr("The system cannot eject the stick (error %1).").arg(error));
        }
        return {true, QString()};
    }
};

#elif defined(Q_OS_LINUX)

/// Linux. Choice: `udisksctl` (the tool of the udisks service that desktop
/// systems use for removable media), run without a shell. It unmounts the
/// stick and then powers off the drive. A failed unmount is a failure. A
/// failed power-off is not: the stick is unmounted, so it is safe to remove.
class LinuxStickEjector : public StickEjector {
  public:
    StickEjectResult eject(const QString& stickRoot) override {
        const QString root = normalizedPath(stickRoot);
        const QStorageInfo info(root);
        if (root.isEmpty() || !info.isValid() || !info.isReady()) {
            return {false, tr("Mix Two cannot find the stick.")};
        }
        // The root of the volume itself, not a folder on it.
        if (normalizedPath(info.rootPath()) != root) {
            return {false, tr("This is not the root folder of a stick.")};
        }
        if (!isRemovableVolume(root)) {
            return {false, tr("This drive is not a removable drive.")};
        }
        const QList<EjectCommand> commands =
                linuxEjectCommands(QString::fromUtf8(info.device()));
        if (commands.isEmpty()) {
            return {false, tr("Mix Two cannot find the device of the stick.")};
        }
        for (int i = 0; i < commands.size(); ++i) {
            const bool isUnmount = (i == 0);
            QProcess process;
            process.start(commands.at(i).program, commands.at(i).arguments);
            if (!process.waitForStarted(kStartTimeoutMs)) {
                if (isUnmount) {
                    return {false, tr("Mix Two cannot start the tool udisksctl.")};
                }
                break;
            }
            if (!process.waitForFinished(kRunTimeoutMs)) {
                process.kill();
                process.waitForFinished(kStartTimeoutMs);
                if (isUnmount) {
                    return {false, tr("The system does not answer in time.")};
                }
                break;
            }
            if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
                if (isUnmount) {
                    const QString detail =
                            QString::fromUtf8(process.readAllStandardError()).trimmed().left(200);
                    return {false,
                            detail.isEmpty()
                                    ? tr("The system cannot unmount the stick.")
                                    : tr("The system cannot unmount the stick: %1").arg(detail)};
                }
                // The power-off failed: the stick is unmounted. It is safe.
                break;
            }
        }
        return {true, QString()};
    }

  private:
    static constexpr int kStartTimeoutMs = 3000;
    static constexpr int kRunTimeoutMs = 20000;
};

#else

class UnsupportedStickEjector : public StickEjector {
  public:
    StickEjectResult eject(const QString&) override {
        return {false, tr("Mix Two cannot eject a stick on this system.")};
    }
};

#endif

} // namespace

std::unique_ptr<StickEjector> makeSystemStickEjector() {
#if defined(Q_OS_WIN)
    return std::make_unique<WindowsStickEjector>();
#elif defined(Q_OS_LINUX)
    return std::make_unique<LinuxStickEjector>();
#else
    return std::make_unique<UnsupportedStickEjector>();
#endif
}

bool isPathOnStick(const QString& path, const QString& stickRoot) {
    const QString p = normalizedPath(path);
    const QString r = normalizedPath(stickRoot);
    // A root of "/" would hold every path of the computer: it is never a stick.
    if (p.isEmpty() || r.isEmpty() || r == QLatin1String("/")) {
        return false;
    }
    const Qt::CaseSensitivity cs = (startsWithDriveLetter(p) && startsWithDriveLetter(r))
            ? Qt::CaseInsensitive
            : Qt::CaseSensitive;
    return p.compare(r, cs) == 0 || p.startsWith(r + QLatin1Char('/'), cs);
}

StickEjectResult ejectExportedStick(StickEjector& ejector,
        const QString& exportedStickRoot,
        const QString& rootToEject,
        const QStringList& openFilePaths) {
    if (normalizedPath(exportedStickRoot).isEmpty() || normalizedPath(rootToEject).isEmpty()) {
        return {false, tr("Mix Two does not know the stick of this export.")};
    }
    if (!isSameVolumeRoot(exportedStickRoot, rootToEject)) {
        return {false, tr("Mix Two ejects only the stick of the export that ended.")};
    }
    for (const QString& path : openFilePaths) {
        if (isPathOnStick(path, rootToEject)) {
            return {false,
                    tr("A track of the stick is loaded in a deck. "
                       "Unload the track, then eject the stick again.")};
        }
    }
    const StickEjectResult result = ejector.eject(rootToEject);
    if (result.ok) {
        return result;
    }
    return {false,
            tr("Mix Two cannot eject the stick.") + QLatin1Char(' ') + result.message +
                    QLatin1Char(' ') + tr("Do not remove the stick yet.")};
}

QList<EjectCommand> linuxEjectCommands(const QString& device) {
    // A plain path below /dev/: no "..", no space, and it starts with "/dev/",
    // so it cannot look like an option of the program.
    static const QString kDevPrefix = QStringLiteral("/dev/");
    if (!device.startsWith(kDevPrefix) || device.size() == kDevPrefix.size() ||
            device.contains(QStringLiteral("..")) || device.contains(QLatin1Char(' '))) {
        return {};
    }
    return {{QStringLiteral("udisksctl"),
                    {QStringLiteral("unmount"), QStringLiteral("-b"), device}},
            {QStringLiteral("udisksctl"),
                    {QStringLiteral("power-off"), QStringLiteral("-b"), device}}};
}

} // namespace mixxx

// End of AI-generated text.
