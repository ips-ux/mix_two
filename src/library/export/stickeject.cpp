// AI-generated (Claude Code). Start of AI-generated text.
#include "library/export/stickeject.h"

#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QtDebug>
#include <QtGlobal>

#if defined(Q_OS_WIN)
#include <windows.h>
// winioctl.h must come after windows.h.
#include <winioctl.h>

#include <cwctype>
#include <string>
#elif defined(Q_OS_LINUX)
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStorageInfo>
#endif

namespace mixxx {

namespace {

/// The context of the texts for the user. lupdate finds the calls of tr().
struct StickEjectText {
    Q_DECLARE_TR_FUNCTIONS(StickEject)
};

bool isDriveLetter(QChar c) {
    const ushort u = c.toUpper().unicode();
    return u >= 'A' && u <= 'Z';
}

bool startsWithDriveLetter(const QString& path) {
    return path.size() >= 2 && path.at(1) == QLatin1Char(':') && isDriveLetter(path.at(0));
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

/// The device calls on a real volume handle.
class WindowsVolumeSteps : public VolumeSteps {
  public:
    WindowsVolumeSteps(HANDLE handle, UINT driveType)
            : m_handle(handle), m_driveType(driveType) {
    }

    bool isStickDevice() override {
        if (m_driveType == DRIVE_REMOVABLE) {
            return true;
        }
        if (m_driveType != DRIVE_FIXED) {
            return false;
        }
        // A fixed drive is only a stick when its bus is USB, SD or MMC.
        STORAGE_PROPERTY_QUERY query = {};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;
        alignas(STORAGE_DEVICE_DESCRIPTOR) BYTE buffer[1024] = {};
        DWORD returned = 0;
        if (!DeviceIoControl(m_handle,
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
    void flush() override {
        FlushFileBuffers(m_handle);
    }
    bool lock() override {
        return control(FSCTL_LOCK_VOLUME);
    }
    void pause() override {
        Sleep(500);
    }
    bool unlock() override {
        return control(FSCTL_UNLOCK_VOLUME);
    }
    bool dismount() override {
        return control(FSCTL_DISMOUNT_VOLUME);
    }
    void allowRemoval() override {
        PREVENT_MEDIA_REMOVAL removal = {};
        removal.PreventMediaRemoval = FALSE;
        DWORD returned = 0;
        DeviceIoControl(m_handle,
                IOCTL_STORAGE_MEDIA_REMOVAL,
                &removal,
                sizeof(removal),
                nullptr,
                0,
                &returned,
                nullptr);
    }
    bool eject() override {
        return control(IOCTL_STORAGE_EJECT_MEDIA);
    }
    void close() override {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
    unsigned long lastError() override {
        return m_lastError;
    }

  private:
    bool control(DWORD code) {
        DWORD returned = 0;
        const bool ok = DeviceIoControl(m_handle, code, nullptr, 0, nullptr, 0, &returned, nullptr);
        m_lastError = ok ? 0 : GetLastError();
        return ok;
    }

    HANDLE m_handle;
    UINT m_driveType;
    DWORD m_lastError = 0;
};

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
            return {false, StickEjectText::tr("The stick has no drive letter.")};
        }
        const wchar_t letter = static_cast<wchar_t>(root.at(0).toUpper().unicode());

        // Never the drive of the system.
        wchar_t systemDrive[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"SystemDrive", systemDrive, MAX_PATH) > 0 &&
                towupper(systemDrive[0]) == letter) {
            return {false, StickEjectText::tr("This is the drive of the system.")};
        }

        const std::wstring driveRoot = std::wstring(1, letter) + L":\\";
        const UINT driveType = GetDriveTypeW(driveRoot.c_str());
        if (driveType != DRIVE_REMOVABLE && driveType != DRIVE_FIXED) {
            return {false, StickEjectText::tr("This drive is not a removable drive.")};
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
            qWarning() << "Stick eject: cannot open the volume, error" << GetLastError();
            return {false, StickEjectText::tr("Mix Two cannot open the stick.")};
        }
        WindowsVolumeSteps steps(handle, driveType);
        return ejectVolumeSteps(steps);
    }

    QString volumeIdentity(const QString& stickRoot) override {
        const QString root = normalizedPath(stickRoot);
        if (!startsWithDriveLetter(root) || root.size() != 2) {
            return QString();
        }
        const std::wstring driveRoot = std::wstring(1, static_cast<wchar_t>(root.at(0).toUpper().unicode())) + L":\\";
        wchar_t label[MAX_PATH + 1] = {};
        wchar_t fileSystem[MAX_PATH + 1] = {};
        DWORD serial = 0;
        if (!GetVolumeInformationW(driveRoot.c_str(),
                    label,
                    MAX_PATH,
                    &serial,
                    nullptr,
                    nullptr,
                    fileSystem,
                    MAX_PATH)) {
            return QString();
        }
        return QStringLiteral("%1|%2|%3")
                .arg(serial, 8, 16, QLatin1Char('0'))
                .arg(QString::fromWCharArray(label), QString::fromWCharArray(fileSystem));
    }
};

#elif defined(Q_OS_LINUX)

/// The sysfs facts of the drive behind the block device `device` (for example
/// `/dev/sdb1`), read from files only. False when they cannot be read.
bool linuxDeviceIsStick(const QString& device) {
    // Choice: sysfs (`/sys/class/block/<name>`), not a program, so no command
    // line is built from a path.
    static const QRegularExpression kName(QStringLiteral("^[A-Za-z0-9_.-]+$"));
    const QString name = QFileInfo(device).fileName();
    if (!device.startsWith(QLatin1String("/dev/")) || !kName.match(name).hasMatch()) {
        return false;
    }
    const QString deviceDir =
            QFileInfo(QStringLiteral("/sys/class/block/") + name).canonicalFilePath();
    if (deviceDir.isEmpty()) {
        return false;
    }
    // A partition lies in the folder of its disk.
    QString diskDir = deviceDir;
    if (QFileInfo::exists(deviceDir + QStringLiteral("/partition"))) {
        diskDir = QFileInfo(deviceDir + QStringLiteral("/..")).canonicalFilePath();
    }
    if (diskDir.isEmpty()) {
        return false;
    }
    QFile removableFile(diskDir + QStringLiteral("/removable"));
    bool removable = false;
    if (removableFile.open(QIODevice::ReadOnly)) {
        removable = removableFile.read(8).trimmed() == "1";
    }
    return isLinuxStickDrive(removable, diskDir);
}

/// The UUID of the file system on `device`, from `/dev/disk/by-uuid`. Empty
/// when there is none.
QString linuxVolumeUuid(const QString& device) {
    const QString wanted = QFileInfo(device).canonicalFilePath();
    if (wanted.isEmpty()) {
        return QString();
    }
    const QFileInfoList entries = QDir(QStringLiteral("/dev/disk/by-uuid"))
                                          .entryInfoList(QDir::AllEntries | QDir::System |
                                                  QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        if (entry.canonicalFilePath() == wanted) {
            return entry.fileName();
        }
    }
    return QString();
}

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
            return {false, StickEjectText::tr("Mix Two cannot find the stick.")};
        }
        // The root of the volume itself, not a folder on it.
        if (normalizedPath(info.rootPath()) != root) {
            return {false, StickEjectText::tr("This is not the root folder of a stick.")};
        }
        const QString device = QString::fromUtf8(info.device());
        // The mount point does not tell: desktops also mount internal disks
        // below /media. The drive behind the device must be a stick.
        if (!linuxDeviceIsStick(device)) {
            return {false, StickEjectText::tr("This drive is not a removable drive.")};
        }
        const QList<EjectCommand> commands = linuxEjectCommands(device);
        if (commands.isEmpty()) {
            return {false, StickEjectText::tr("Mix Two cannot find the device of the stick.")};
        }
        for (int i = 0; i < commands.size(); ++i) {
            const bool isUnmount = (i == 0);
            QProcess process;
            process.start(commands.at(i).program, commands.at(i).arguments);
            if (!process.waitForStarted(kStartTimeoutMs)) {
                if (isUnmount) {
                    return {false, StickEjectText::tr("Mix Two cannot start the tool udisksctl.")};
                }
                break;
            }
            const int timeout = isUnmount ? kUnmountTimeoutMs : kPowerOffTimeoutMs;
            if (!process.waitForFinished(timeout)) {
                process.kill();
                process.waitForFinished(kStartTimeoutMs);
                if (isUnmount) {
                    return {false, StickEjectText::tr("The system does not answer in time.")};
                }
                break;
            }
            if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
                // The tool output goes to the log, not to the user.
                qWarning() << "Stick eject:" << commands.at(i).program
                           << commands.at(i).arguments << "failed:"
                           << process.readAllStandardError().trimmed();
                if (isUnmount) {
                    return {false, StickEjectText::tr("The system cannot unmount the stick.")};
                }
                // The power-off failed: the stick is unmounted. It is safe.
                break;
            }
        }
        return {true, QString()};
    }

    QString volumeIdentity(const QString& stickRoot) override {
        const QString root = normalizedPath(stickRoot);
        const QStorageInfo info(root);
        if (root.isEmpty() || !info.isValid() || !info.isReady()) {
            return QString();
        }
        const QString uuid = linuxVolumeUuid(QString::fromUtf8(info.device()));
        return uuid.isEmpty() ? QString() : QStringLiteral("uuid:") + uuid;
    }

  private:
    // The worst case is 3 + 15 + 3 s for the unmount and 3 + 10 + 3 s for the
    // power-off: about 37 s.
    static constexpr int kStartTimeoutMs = 3000;
    static constexpr int kUnmountTimeoutMs = 15000;
    static constexpr int kPowerOffTimeoutMs = 10000;
};

#else

class UnsupportedStickEjector : public StickEjector {
  public:
    StickEjectResult eject(const QString&) override {
        return {false, StickEjectText::tr("Mix Two cannot eject a stick on this system.")};
    }
    QString volumeIdentity(const QString&) override {
        return QString();
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

StickEjectResult ejectVolumeSteps(VolumeSteps& steps) {
    constexpr int kLockTries = 4;
    if (!steps.isStickDevice()) {
        steps.close();
        return {false, StickEjectText::tr("This drive is not a removable drive.")};
    }
    steps.flush();

    bool locked = false;
    for (int attempt = 0; attempt < kLockTries && !locked; ++attempt) {
        locked = steps.lock();
        if (!locked && attempt + 1 < kLockTries) {
            steps.pause();
        }
    }
    if (!locked) {
        qWarning() << "Stick eject: the lock failed, error" << steps.lastError();
        steps.close();
        // The program can be Mix Two itself.
        return {false,
                StickEjectText::tr("A program uses the stick. This program can be Mix Two. "
                                   "Close the files of the stick, then try again.")};
    }

    if (!steps.dismount()) {
        qWarning() << "Stick eject: the dismount failed, error" << steps.lastError();
        steps.unlock();
        steps.close();
        return {false, StickEjectText::tr("The system cannot dismount the stick.")};
    }
    // The result of this call does not matter: the eject call decides.
    steps.allowRemoval();
    if (!steps.eject()) {
        qWarning() << "Stick eject: the eject failed, error" << steps.lastError();
        steps.unlock();
        steps.close();
        return {false, StickEjectText::tr("The system cannot eject the stick.")};
    }
    steps.close();
    return {true, QString()};
}

bool isLinuxStickDrive(bool removableFlag, const QString& sysfsDevicePath) {
    if (removableFlag) {
        return true;
    }
    static const QRegularExpression kUsbPort(QStringLiteral("^usb[0-9]+$"));
    const QStringList parts = sysfsDevicePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        if (part == QLatin1String("mmc_host") || kUsbPort.match(part).hasMatch()) {
            return true;
        }
    }
    return false;
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
        const QString& exportedIdentity,
        const QString& rootToEject,
        const QStringList& openFilePaths) {
    if (normalizedPath(exportedStickRoot).isEmpty() || normalizedPath(rootToEject).isEmpty()) {
        return {false, StickEjectText::tr("Mix Two does not know the stick of this export.")};
    }
    if (!isSameVolumeRoot(exportedStickRoot, rootToEject)) {
        return {false, StickEjectText::tr("Mix Two ejects only the stick of the export that ended.")};
    }
    // The same drive letter can hold another stick now.
    if (exportedIdentity.isEmpty()) {
        return {false, StickEjectText::tr("Mix Two cannot identify the stick.")};
    }
    const QString currentIdentity = ejector.volumeIdentity(rootToEject);
    if (currentIdentity.isEmpty()) {
        return {false, StickEjectText::tr("Mix Two cannot identify the stick.")};
    }
    if (currentIdentity != exportedIdentity) {
        return {false, StickEjectText::tr("Mix Two ejects only the stick of the export that ended.")};
    }
    for (const QString& path : openFilePaths) {
        if (isPathOnStick(path, rootToEject)) {
            return {false,
                    StickEjectText::tr("A track of the stick is loaded in a deck. "
                                       "Unload the track, then eject the stick again.")};
        }
    }
    const StickEjectResult result = ejector.eject(rootToEject);
    if (result.ok) {
        return result;
    }
    return {false,
            StickEjectText::tr("Mix Two cannot eject the stick.") + QLatin1Char(' ') +
                    result.message + QLatin1Char(' ') +
                    StickEjectText::tr("Do not remove the stick yet.")};
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
