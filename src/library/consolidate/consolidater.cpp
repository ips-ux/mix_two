// AI-generated (Claude Code). Start of AI-generated text.
#include "library/consolidate/consolidater.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "library/consolidate/folderpattern.h"
#include "library/trackcollection.h"
#include "track/track.h"

namespace mixxx {
namespace library {

QByteArray hashFileContent(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    constexpr qint64 kChunkSize = 1 << 20;
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(kChunkSize);
        if (chunk.isEmpty()) {
            // A read error: the hash would be wrong.
            return {};
        }
        hash.addData(chunk);
    }
    if (file.error() != QFileDevice::NoError) {
        return {};
    }
    return hash.result();
}

bool syncFileToDisk(const QString& path) {
    // Open for writing without a cut: a handle with write access is needed
    // to flush on Windows.
    QFile file(path);
    if (!file.open(QIODevice::ReadWrite)) {
        return false;
    }
    if (!file.flush()) {
        return false;
    }
#ifdef Q_OS_WIN
    const intptr_t osHandle = _get_osfhandle(file.handle());
    if (osHandle == -1 || !FlushFileBuffers(reinterpret_cast<HANDLE>(osHandle))) {
        return false;
    }
#else
    if (fsync(file.handle()) != 0) {
        return false;
    }
#endif
    file.close();
#ifndef Q_OS_WIN
    // Also write the folder entry. A failure here is not an error: some file
    // systems do not allow it.
    const int dirFd = ::open(QFileInfo(path).absolutePath().toLocal8Bit().constData(), O_RDONLY);
    if (dirFd >= 0) {
        ::fsync(dirFd);
        ::close(dirFd);
    }
#endif
    return true;
}

std::optional<ConsolidateStatus> checkLibraryRoot(const QString& libraryRoot,
        const std::function<bool(const QString&)>& isTrashSafe) {
    if (libraryRoot.trimmed().isEmpty() || !QFileInfo(libraryRoot).isAbsolute()) {
        return ConsolidateStatus::InvalidLibraryRoot;
    }
    if (!isTrashSafe(libraryRoot)) {
        return ConsolidateStatus::UnsafeLibraryRoot;
    }
    return std::nullopt;
}

ConsolidateOps systemConsolidateOps(std::function<bool(const QString&)> relocate) {
    ConsolidateOps ops;
    ops.copyFile = [](const QString& from, const QString& to) {
        // QFile::copy never overwrites an existing file.
        return QFile::copy(from, to);
    };
    ops.syncCopy = [](const QString& path) { return syncFileToDisk(path); };
    ops.hashFile = [](const QString& path) { return hashFileContent(path); };
    ops.relocate = std::move(relocate);
    ops.removeCopy = [](const QString& path) { return QFile::remove(path); };
    ops.trashOriginal = [](const QString& path) { return moveToTrashChecked(path); };
    return ops;
}

QString consolidateTargetPath(const QString& sourcePath,
        const QString& libraryRoot,
        const QString& folder) {
    const QString fileName = QFileInfo(sourcePath).fileName();
    QString path = libraryRoot;
    if (!folder.isEmpty()) {
        path += QLatin1Char('/') + folder;
    }
    path += QLatin1Char('/') + fileName;
    return QDir::cleanPath(path);
}

ConsolidateOutcome consolidateFile(const QString& sourcePath,
        const QString& targetPath,
        const ConsolidateOps& ops) {
    ConsolidateOutcome outcome;

    const QFileInfo source(sourcePath);
    if (!source.isFile()) {
        outcome.status = ConsolidateStatus::SourceMissing;
        return outcome;
    }
    const QFileInfo target(targetPath);
    if (target.exists()) {
        // Choice: Mix Two never overwrites a file, also not one with the same
        // content.
        outcome.status = (source == target) ? ConsolidateStatus::AlreadyInLibrary
                                            : ConsolidateStatus::TargetExists;
        return outcome;
    }

    // Remember the first folder that does not exist yet, so that a failure
    // can remove the folders that this function makes (and only those).
    const QString targetDir = target.absolutePath();
    QString firstNewDir;
    for (QString probe = targetDir; !QDir(probe).exists();) {
        firstNewDir = probe;
        const QString parent = QFileInfo(probe).absolutePath();
        if (parent == probe) {
            break;
        }
        probe = parent;
    }
    const auto removeNewFolders = [&]() {
        // QDir::rmdir only removes an empty folder.
        if (firstNewDir.isEmpty()) {
            return;
        }
        QString current = targetDir;
        while (QDir().rmdir(current) && current != firstNewDir) {
            current = QFileInfo(current).absolutePath();
        }
    };
    if (!QDir().mkpath(targetDir)) {
        removeNewFolders();
        outcome.status = ConsolidateStatus::CopyFailed;
        return outcome;
    }

    // 1. Copy. The file at `targetPath` did not exist before, so whatever is
    // there after a failed copy is this function's partial copy: remove it.
    if (!ops.copyFile(sourcePath, targetPath)) {
        ops.removeCopy(targetPath);
        removeNewFolders();
        outcome.status = ConsolidateStatus::CopyFailed;
        return outcome;
    }

    // 2. Write the copy to the disk, then read it again to check it by
    // content hash. From here on, the file at `targetPath` is the copy that
    // this function made, so it may remove it.
    if (!ops.syncCopy(targetPath)) {
        ops.removeCopy(targetPath);
        removeNewFolders();
        outcome.status = ConsolidateStatus::CopyFailed;
        return outcome;
    }
    const QByteArray sourceHash = ops.hashFile(sourcePath);
    const QByteArray copyHash = ops.hashFile(targetPath);
    if (sourceHash.isEmpty() || sourceHash != copyHash) {
        ops.removeCopy(targetPath);
        removeNewFolders();
        outcome.status = ConsolidateStatus::HashMismatch;
        return outcome;
    }

    // 3. Point the track at the copy.
    if (!ops.relocate(targetPath)) {
        ops.removeCopy(targetPath);
        removeNewFolders();
        outcome.status = ConsolidateStatus::RelocateFailed;
        return outcome;
    }

    // 4. The track now uses the copy, so the original can go to the trash.
    outcome.status = ConsolidateStatus::Consolidated;
    outcome.newLocation = targetPath;
    outcome.originalTrash = ops.trashOriginal(sourcePath);
    return outcome;
}

ConsolidateOutcome consolidateTrack(const TrackPointer& pTrack,
        const QString& libraryRoot,
        const QString& pattern,
        TrackCollection* pCollection,
        const std::function<TrashResult(const QString&)>& trashOriginal) {
    ConsolidateOutcome outcome;
    if (!pTrack || !pCollection) {
        outcome.status = ConsolidateStatus::InvalidTrack;
        return outcome;
    }
    const auto rootProblem = checkLibraryRoot(
            libraryRoot, [](const QString& root) { return isTrashSafeVolume(root); });
    if (rootProblem) {
        outcome.status = *rootProblem;
        return outcome;
    }
    const TrackId trackId = pTrack->getId();
    const QString sourcePath = pTrack->getLocation();
    if (!trackId.isValid() || sourcePath.isEmpty()) {
        outcome.status = ConsolidateStatus::InvalidTrack;
        return outcome;
    }

    const QString folder = libraryFolderFor(pTrack->getRecord(), pattern);
    const QString targetPath = consolidateTargetPath(sourcePath, libraryRoot, folder);

    ConsolidateOps ops = systemConsolidateOps(
            [pCollection, trackId](const QString& newLocation) {
                return pCollection->relocateTrackLocation(trackId, newLocation);
            });
    if (trashOriginal) {
        ops.trashOriginal = trashOriginal;
    }
    return consolidateFile(sourcePath, targetPath, ops);
}

} // namespace library
} // namespace mixxx

// End of AI-generated text.
