// AI-generated (Claude Code). Start of AI-generated text.
#include "library/rekordbox/stickwatcher.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QStorageInfo>
#include <QThread>
#include <QTimer>
#include <QtConcurrentRun>
#include <optional>
#include <utility>

#include "library/rekordbox/devicelibraryplusstick.h"
#include "moc_stickwatcher.cpp"

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#include <dbt.h>
#elif defined(Q_OS_LINUX)
#include <fcntl.h>
#include <mntent.h>
#include <poll.h>
#include <unistd.h>

#include <cerrno>
#endif

namespace mixxx::rekordbox {
namespace {

QString findFile(const QString& root, const QString& relativePath) {
    const QFileInfo exact(QDir(root).filePath(relativePath));
    if (exact.isFile()) {
        return exact.filePath();
    }
    const QFileInfo folded(dlpFindPathNoCase(root, relativePath));
    return folded.isFile() ? folded.filePath() : QString{};
}

#if defined(Q_OS_WIN)
class VolumeEventFilter final : public QAbstractNativeEventFilter {
  public:
    explicit VolumeEventFilter(StickWatcher* watcher)
            : m_watcher(watcher) {}

    bool nativeEventFilter(const QByteArray& type,
            void* message,
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            long*) override {
#else
            qintptr*) override {
#endif
        if (type != "windows_generic_MSG" && type != "windows_dispatcher_MSG") {
            return false;
        }
        const auto* msg = static_cast<const MSG*>(message);
        if (msg->message != WM_DEVICECHANGE || !msg->lParam ||
                (msg->wParam != DBT_DEVICEARRIVAL && msg->wParam != DBT_DEVICEREMOVECOMPLETE)) {
            return false;
        }
        const auto* header = reinterpret_cast<const DEV_BROADCAST_HDR*>(msg->lParam);
        if (header->dbch_devicetype != DBT_DEVTYP_VOLUME ||
                header->dbch_size < sizeof(DEV_BROADCAST_VOLUME)) {
            return false;
        }
        const auto* volume = reinterpret_cast<const DEV_BROADCAST_VOLUME*>(header);
        const auto roots = driveRootsFromMask(volume->dbcv_unitmask);
        const bool arrived = msg->wParam == DBT_DEVICEARRIVAL;
        QMetaObject::invokeMethod(m_watcher, [watcher = m_watcher, roots, arrived] {
            for (const auto& root : roots) {
                if (arrived) {
                    watcher->volumeArrived(root);
                } else {
                    watcher->volumeDeparted(root);
                }
            }
        }, Qt::QueuedConnection);
        return false;
    }

  private:
    StickWatcher* const m_watcher;
};
#else
#if defined(Q_OS_LINUX)
constexpr auto kMountFile = "/proc/self/mounts";
#endif

std::optional<QSet<QString>> mountedRoots() {
    QSet<QString> roots;
#if defined(Q_OS_LINUX)
    // Read the mount table without asking any mounted filesystem for metadata.
    auto* mounts = setmntent(kMountFile, "r");
    if (!mounts) {
        return std::nullopt;
    }
    mntent entry;
    char buffer[16384];
    while (getmntent_r(mounts, &entry, buffer, sizeof(buffer))) {
        if (QByteArray(entry.mnt_fsname).startsWith("/dev/")) {
            roots.insert(QFile::decodeName(entry.mnt_dir));
        }
    }
    endmntent(mounts);
#else
    for (const auto& volume : QStorageInfo::mountedVolumes()) {
        if (volume.isValid() && volume.isReady()) {
            roots.insert(volume.rootPath());
        }
    }
#endif
    return roots;
}
#endif

} // namespace

StickFormats detectStickFormats(const QString& root) {
    StickFormats formats;
    if (root.isEmpty()) {
        return formats;
    }
    formats.legacyPath = findFile(root, QStringLiteral("PIONEER/rekordbox/export.pdb"));
    if (!formats.hasLegacy()) {
        formats.legacyPath = findFile(root, QStringLiteral(".PIONEER/rekordbox/export.pdb"));
    }
    formats.deviceLibraryPlusPath = findFile(root, QStringLiteral("PIONEER/rekordbox/exportLibrary.db"));
    return formats;
}

QStringList driveRootsFromMask(quint32 mask) {
    QStringList roots;
    for (int letter = 0; letter < 26; ++letter) {
        if (mask & (quint32{1} << letter)) {
            roots.append(QStringLiteral("%1:/").arg(QChar(static_cast<char16_t>(u'A' + letter))));
        }
    }
    return roots;
}

StickWatcher::StickWatcher(QObject* parent)
        : QObject(parent) {
}

StickWatcher::~StickWatcher() {
    if (m_nativeFilter && QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(m_nativeFilter.get());
    }
    if (m_mountMonitor) {
        m_mountMonitor->requestInterruption();
        m_mountMonitor->wait();
    }
}

void StickWatcher::start() {
#if defined(Q_OS_WIN)
    m_nativeFilter = std::make_unique<VolumeEventFilter>(this);
    QCoreApplication::instance()->installNativeEventFilter(m_nativeFilter.get());
#elif defined(Q_OS_LINUX)
    m_mountMonitor.reset(QThread::create([this] {
        const auto requestSnapshot = [this] {
            QMetaObject::invokeMethod(this, [this] { scanMounts(false); }, Qt::QueuedConnection);
        };
        const int fd = open(kMountFile, O_RDONLY | O_CLOEXEC);
        // Subscribe before the initial snapshot so a mount change cannot fall between them.
        requestSnapshot();
        if (fd < 0) {
            return;
        }
        pollfd mounts{fd, POLLPRI, 0};
        while (!QThread::currentThread()->isInterruptionRequested()) {
            // The timeout limits the wait at shutdown. It does not rescan the roots.
            const int ready = poll(&mounts, 1, 200);
            if (ready > 0) {
                if (!(mounts.revents & POLLPRI)) {
                    break;
                }
                requestSnapshot();
            } else if (ready < 0 && errno != EINTR) {
                break;
            }
        }
        close(fd);
    }));
    m_mountMonitor->start();
    return;
#else
    auto* timer = new QTimer(this);
    timer->setInterval(2000);
    connect(timer, &QTimer::timeout, this, [this] { scanMounts(false); });
    timer->start();
#endif
    scanMounts(false);
}

void StickWatcher::refresh() {
    scanMounts(true);
}

void StickWatcher::scanMounts(bool recheck) {
#if defined(Q_OS_WIN)
    const DWORD mask = GetLogicalDrives();
    if (mask) {
        const auto roots = driveRootsFromMask(mask);
        applySnapshot(QSet<QString>(roots.begin(), roots.end()), recheck);
    }
#else
    if (m_snapshotPending) {
        m_rescanPending = true;
        m_recheckPending |= recheck;
        return;
    }
    m_snapshotPending = true;
    auto* job = new QFutureWatcher<std::optional<QSet<QString>>>(this);
    connect(job, &QFutureWatcherBase::finished, this, [this, job, recheck] {
        const auto roots = job->result();
        job->deleteLater();
        if (roots) {
            applySnapshot(*roots, recheck);
        }
        m_snapshotPending = false;
        if (std::exchange(m_rescanPending, false)) {
            scanMounts(std::exchange(m_recheckPending, false));
        }
    });
    job->setFuture(QtConcurrent::run(mountedRoots));
#endif
}

void StickWatcher::applySnapshot(const QSet<QString>& roots, bool recheck) {
    const auto previous = m_roots;
    for (const auto& root : previous - roots) {
        volumeDeparted(root);
    }
    for (const auto& root : roots) {
        if (!previous.contains(root)) {
            volumeArrived(root);
        } else if (recheck) {
            emit volumeRecheckRequested(root);
        }
    }
}

void StickWatcher::volumeArrived(const QString& root) {
    if (!root.isEmpty() && !m_roots.contains(root)) {
        m_roots.insert(root);
        emit volumeAdded(root);
    }
}

void StickWatcher::volumeDeparted(const QString& root) {
    if (m_roots.remove(root)) {
        emit volumeRemoved(root);
    }
}

} // namespace mixxx::rekordbox
// End of AI-generated text.
