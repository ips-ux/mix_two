#include "library/rekordbox/devicelibraryplusexportcleanup.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QtDebug>

#include "library/rekordbox/devicelibraryplusmapping.h"

namespace mixxx {
namespace rekordbox {

namespace {

const QString kAnlzRootKey = QStringLiteral("/pioneer/usbanlz/");
const QString kArtworkRootKey = QStringLiteral("/pioneer/artwork/");
const QString kContentsRootKey = QStringLiteral("/contents/");
const QStringList kAnlzFileNames = {QStringLiteral("ANLZ0000.DAT"),
        QStringLiteral("ANLZ0000.EXT"),
        QStringLiteral("ANLZ0000.2EX")};

/// A safe stick path below the folder of `rootKey` (a dlpPathKey() that ends
/// with '/').
bool isBelow(const QString& stickPath, const QString& rootKey) {
    return dlpIsSafeStickPath(stickPath) && dlpPathKey(stickPath).startsWith(rootKey);
}

QString folderOf(const QString& stickPath) {
    return stickPath.left(stickPath.lastIndexOf(QLatin1Char('/')));
}

/// Removes one real file. A missing file is not an error; a folder or a
/// symbolic link is never removed.
bool removeFile(const QString& path) {
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) {
        return false;
    }
    if (info.isSymLink() || !info.isFile()) {
        qWarning() << "DeviceLibraryPlus: did not remove a replaced path that is not a file";
        qDebug() << "DeviceLibraryPlus: the path is" << path;
        return false;
    }
    QFile file(path);
    if (!file.remove()) {
        qWarning() << "DeviceLibraryPlus: cannot remove a replaced file:" << file.errorString();
        qDebug() << "DeviceLibraryPlus: the file is" << path;
        return false;
    }
    return true;
}

/// Removes the folder when it is empty and not a symbolic link.
void removeEmptyFolder(const QString& path) {
    const QFileInfo info(path);
    if (info.isDir() && !info.isSymLink() &&
            QDir(path).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden |
                    QDir::System)) {
        QDir().rmdir(path);
    }
}

} // namespace

int dlpRemoveReplacedFiles(const QString& stickRoot,
        const DlpReplacedFiles& replaced,
        const QList<DlpContentRef>& rows,
        const QHash<qint64, QString>& imagePaths) {
    if (stickRoot.trimmed().isEmpty()) {
        return 0;
    }
    QSet<QString> usedPaths;
    QSet<QString> usedAnlzFolders;
    QSet<QString> usedImagePaths;
    for (const DlpContentRef& row : rows) {
        usedPaths.insert(dlpPathKey(row.path));
        if (!row.analysisDataFilePath.isEmpty()) {
            usedAnlzFolders.insert(dlpPathKey(folderOf(row.analysisDataFilePath)));
        }
        if (row.imageId) {
            const QString imagePath = imagePaths.value(*row.imageId);
            usedImagePaths.insert(dlpPathKey(imagePath));
            if (imagePath.endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive)) {
                usedImagePaths.insert(dlpPathKey(imagePath.chopped(4) + QStringLiteral("_m.jpg")));
            }
        }
    }

    int removed = 0;
    QSet<QString> done;
    for (const QString& folder : replaced.anlzFolders) {
        const QString key = dlpPathKey(folder);
        if (!isBelow(folder, kAnlzRootKey) || usedAnlzFolders.contains(key) ||
                done.contains(key)) {
            continue;
        }
        done.insert(key);
        if (QFileInfo(stickRoot + folder).isSymLink()) {
            continue;
        }
        for (const QString& name : kAnlzFileNames) {
            if (removeFile(stickRoot + folder + QLatin1Char('/') + name)) {
                ++removed;
            }
        }
        removeEmptyFolder(stickRoot + folder);
        const QString parent = folderOf(folder);
        if (isBelow(parent, kAnlzRootKey)) {
            removeEmptyFolder(stickRoot + parent);
        }
    }
    for (const QString& smallPath : replaced.imagePaths) {
        const QString key = dlpPathKey(smallPath);
        if (!isBelow(smallPath, kArtworkRootKey) || !key.endsWith(QStringLiteral(".jpg")) ||
                usedImagePaths.contains(key) || done.contains(key)) {
            continue;
        }
        done.insert(key);
        const QString mediumPath = smallPath.chopped(4) + QStringLiteral("_m.jpg");
        for (const QString& path : {smallPath, mediumPath}) {
            if (!usedImagePaths.contains(dlpPathKey(path)) && removeFile(stickRoot + path)) {
                ++removed;
            }
        }
    }
    for (const QString& audioPath : replaced.audioPaths) {
        const QString key = dlpPathKey(audioPath);
        if (!isBelow(audioPath, kContentsRootKey) || usedPaths.contains(key) ||
                done.contains(key)) {
            continue;
        }
        done.insert(key);
        if (removeFile(stickRoot + audioPath)) {
            ++removed;
        }
    }
    return removed;
}

} // namespace rekordbox
} // namespace mixxx
