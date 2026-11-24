// AI-generated (Claude Code). Start of AI-generated text.
#include "util/removablevolume.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace mixxx {

namespace {

bool startsWithDriveLetter(const QString& path) {
    return path.size() >= 2 && path.at(1) == QLatin1Char(':') && path.at(0).isLetter();
}

/// The path with `/` as separator, no `.` or `..`, and no trailing `/`
/// (except for the root `/`). A drive root `C:/` becomes `C:`.
QString normalizedPath(const QString& path) {
    QString result = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (result.size() > 1 && result.endsWith(QLatin1Char('/'))) {
        result.chop(1);
    }
    return result;
}

/// True when `path` is `root` or below it. Both are normalized.
bool isBelowOrEqual(const QString& path, const QString& root) {
    const Qt::CaseSensitivity sensitivity =
            startsWithDriveLetter(root) ? Qt::CaseInsensitive : Qt::CaseSensitive;
    if (root == QLatin1String("/")) {
        return path.startsWith(QLatin1Char('/'));
    }
    if (!path.startsWith(root, sensitivity)) {
        return false;
    }
    return path.size() == root.size() || path.at(root.size()) == QLatin1Char('/');
}

bool isBelow(const QString& path, const QString& root) {
    return path.size() > root.size() && isBelowOrEqual(path, root);
}

} // namespace

QStringList removableMountParentPaths() {
#if defined(Q_OS_LINUX)
    const QString user = QString::fromLocal8Bit(qgetenv("USER"));
    return {QStringLiteral("/media"),
            QStringLiteral("/media/") + user,
            QStringLiteral("/run/media/") + user};
#else
    return {};
#endif
}

namespace {

bool isNetworkPath(const QString& path) {
    return QDir::fromNativeSeparators(path.trimmed()).startsWith(QLatin1String("//"));
}

/// The volume with the longest root that holds the normalized path.
const VolumeInfo* volumeOf(const QString& normalized, const QList<VolumeInfo>& volumes) {
    const VolumeInfo* pBest = nullptr;
    int bestLength = -1;
    for (const VolumeInfo& volume : volumes) {
        const QString root = normalizedPath(volume.rootPath);
        if (root.isEmpty() || !isBelowOrEqual(normalized, root)) {
            continue;
        }
        if (root.size() > bestLength) {
            bestLength = root.size();
            pBest = &volume;
        }
    }
    return pBest;
}

bool isBelowMountParent(const QString& normalized, const QStringList& mountParents) {
    for (const QString& parent : mountParents) {
        const QString normalizedParent = normalizedPath(parent);
        if (!normalizedParent.isEmpty() && normalizedParent != QLatin1String("/") &&
                isBelow(normalized, normalizedParent)) {
            return true;
        }
    }
    return false;
}

QList<VolumeInfo> systemVolumes() {
    QList<VolumeInfo> volumes;
    const QList<QStorageInfo> mounted = QStorageInfo::mountedVolumes();
    volumes.reserve(mounted.size());
    for (const QStorageInfo& storage : mounted) {
        VolumeInfo volume;
        volume.rootPath = storage.rootPath();
#ifdef Q_OS_WIN
        const QString nativeRoot = QDir::toNativeSeparators(storage.rootPath());
        const UINT type = GetDriveTypeW(reinterpret_cast<LPCWSTR>(nativeRoot.utf16()));
        volume.isRemovableDrive = type == DRIVE_REMOVABLE || type == DRIVE_CDROM;
        volume.isNetworkDrive = type == DRIVE_REMOTE;
#endif
        volumes.append(volume);
    }
    return volumes;
}

QString canonicalPathOrEmpty(const QString& path) {
    return QFileInfo(path).canonicalFilePath();
}

} // namespace

bool isRemovableVolume(const QString& path,
        const QList<VolumeInfo>& volumes,
        const QStringList& mountParents) {
    if (path.trimmed().isEmpty()) {
        return false;
    }
    const QString normalized = normalizedPath(path);
    if (isBelowMountParent(normalized, mountParents)) {
        return true;
    }
    const VolumeInfo* pVolume = volumeOf(normalized, volumes);
    return pVolume && pVolume->isRemovableDrive;
}

bool isTrashSafeVolume(const QString& path,
        const QList<VolumeInfo>& volumes,
        const QStringList& mountParents) {
    if (path.trimmed().isEmpty()) {
        return true;
    }
    if (isNetworkPath(path) || isRemovableVolume(path, volumes, mountParents)) {
        return false;
    }
    const VolumeInfo* pVolume = volumeOf(normalizedPath(path), volumes);
    return !(pVolume && pVolume->isNetworkDrive);
}

bool isTrashSafeVolume(const QString& path,
        const std::function<QString(const QString&)>& resolve) {
    return isTrashSafeVolume(path, resolve, systemVolumes(), removableMountParentPaths());
}

bool isTrashSafeVolume(const QString& path,
        const std::function<QString(const QString&)>& resolve,
        const QList<VolumeInfo>& volumes,
        const QStringList& parents) {
    if (!isTrashSafeVolume(QFileInfo(path).absoluteFilePath(), volumes, parents)) {
        return false;
    }
    // A junction, a link or a substitute drive can point to a stick or a
    // share. Judge the target too. A file that does not exist has no target.
    const QString resolved = resolve(path);
    return resolved.isEmpty() || isTrashSafeVolume(resolved, volumes, parents);
}

bool isTrashSafeVolume(const QString& path) {
    return isTrashSafeVolume(path, canonicalPathOrEmpty);
}

bool isRemovableVolume(const QString& path) {
    const QList<VolumeInfo> volumes = systemVolumes();
    const QStringList parents = removableMountParentPaths();
    if (isRemovableVolume(QFileInfo(path).absoluteFilePath(), volumes, parents)) {
        return true;
    }
    const QString resolved = canonicalPathOrEmpty(path);
    return !resolved.isEmpty() && isRemovableVolume(resolved, volumes, parents);
}

TrashResult moveToTrashChecked(const QString& path,
        const std::function<bool(const QString&)>& isTrashSafe,
        const std::function<bool(const QString&)>& moveToTrash) {
    // Choice: a file where the trash is not safe (removable or network
    // drive) is never moved. The caller tells the user that the file stays.
    if (!isTrashSafe(path)) {
        return TrashResult::RefusedUnsafe;
    }
    return moveToTrash(path) ? TrashResult::Moved : TrashResult::Failed;
}

TrashResult moveToTrashChecked(const QString& path) {
    return moveToTrashChecked(
            path,
            [](const QString& file) { return isTrashSafeVolume(file); },
            [](const QString& file) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                return QFile::remove(file);
#else
                return QFile::moveToTrash(file);
#endif
            });
}

} // namespace mixxx
// End of AI-generated text.
