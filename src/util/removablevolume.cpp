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

bool isRemovableVolume(const QString& path,
        const QList<VolumeInfo>& volumes,
        const QStringList& mountParents) {
    if (path.trimmed().isEmpty()) {
        return false;
    }
    const QString normalized = normalizedPath(path);
    for (const QString& parent : mountParents) {
        const QString normalizedParent = normalizedPath(parent);
        if (!normalizedParent.isEmpty() && normalizedParent != QLatin1String("/") &&
                isBelow(normalized, normalizedParent)) {
            return true;
        }
    }
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
    return pBest && pBest->isRemovableDrive;
}

bool isRemovableVolume(const QString& path) {
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
#endif
        volumes.append(volume);
    }
    return isRemovableVolume(QFileInfo(path).absoluteFilePath(),
            volumes,
            removableMountParentPaths());
}

TrashResult moveToTrashChecked(const QString& path,
        const std::function<bool(const QString&)>& isRemovable,
        const std::function<bool(const QString&)>& moveToTrash) {
    // Choice: a file on a removable volume is never moved. The caller tells
    // the user that the file stays.
    if (isRemovable(path)) {
        return TrashResult::RefusedRemovable;
    }
    return moveToTrash(path) ? TrashResult::Moved : TrashResult::Failed;
}

TrashResult moveToTrashChecked(const QString& path) {
    return moveToTrashChecked(
            path,
            [](const QString& file) { return isRemovableVolume(file); },
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
