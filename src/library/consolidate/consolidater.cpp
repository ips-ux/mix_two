// AI-generated (Claude Code). Start of AI-generated text.
#include "library/consolidate/consolidater.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

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

ConsolidateOps systemConsolidateOps(std::function<bool(const QString&)> relocate) {
    ConsolidateOps ops;
    ops.copyFile = [](const QString& from, const QString& to) {
        // QFile::copy never overwrites an existing file.
        return QFile::copy(from, to);
    };
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
    if (!QDir().mkpath(target.absolutePath())) {
        outcome.status = ConsolidateStatus::CopyFailed;
        return outcome;
    }

    // 1. Copy. When this fails, the function deletes nothing: the copy call
    // leaves no file at the target when it fails.
    if (!ops.copyFile(sourcePath, targetPath)) {
        outcome.status = ConsolidateStatus::CopyFailed;
        return outcome;
    }

    // 2. Check the copy by content hash. From here on, the file at
    // `targetPath` is the copy that this function made, so it may remove it.
    const QByteArray sourceHash = ops.hashFile(sourcePath);
    const QByteArray copyHash = ops.hashFile(targetPath);
    if (sourceHash.isEmpty() || sourceHash != copyHash) {
        ops.removeCopy(targetPath);
        outcome.status = ConsolidateStatus::HashMismatch;
        return outcome;
    }

    // 3. Point the track at the copy.
    if (!ops.relocate(targetPath)) {
        ops.removeCopy(targetPath);
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
