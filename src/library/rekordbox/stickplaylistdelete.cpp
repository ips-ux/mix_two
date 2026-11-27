// AI-generated (Claude Code). Start of AI-generated text.
#include "library/rekordbox/stickplaylistdelete.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <utility>

#include "library/rekordbox/devicelibraryplusmapping.h"

namespace mixxx {
namespace rekordbox {

namespace {

QString tr(const char* text) {
    return QCoreApplication::translate("StickPlaylistDelete", text);
}

QString trN(const char* text, int n) {
    return QCoreApplication::translate("StickPlaylistDelete", text, nullptr, n);
}

/// The stick root with `/` and no `/` at the end, so that a stick path (which
/// starts with `/`) can follow it.
QString cleanRoot(const QString& stickRoot) {
    QString root = QDir::cleanPath(QDir::fromNativeSeparators(stickRoot));
    if (root.endsWith(QLatin1Char('/'))) {
        root.chop(1);
    }
    return root;
}

/// The SHA-256 of a file; empty when it cannot be read.
QByteArray fileHash(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return {};
    }
    return hash.result();
}

} // namespace

StickDeleteOutcome runStickPlaylistDelete(
        const StickDeleteRequest& request, const StickDeleteHooks& hooks) {
    StickDeleteOutcome outcome;
    // 1. The first question.
    if (hooks.confirmDelete && !hooks.confirmDelete()) {
        outcome.status = StickDeleteStatus::Canceled;
        return outcome;
    }
    // 2. The tracks that the delete removes. The stick does not change.
    outcome.result = dlpPreviewDeletePlaylistByPath(
            request.stickRoot, request.masterDbId, request.namePath, request.isInLibrary);
    if (!outcome.result.ok) {
        outcome.status = StickDeleteStatus::Failed;
        outcome.message = outcome.result.errorMessage;
        return outcome;
    }
    for (const DlpRemovedTrack& track : std::as_const(outcome.result.removedTracks)) {
        if (!track.inMixTwoLibrary) {
            outcome.notInLibrary.append(track);
        }
    }
    // 3. The offer to add the tracks that are not in the library, while their
    // files are still on the stick.
    if (!outcome.notInLibrary.isEmpty() && hooks.offerImport &&
            hooks.offerImport(outcome.notInLibrary)) {
        const QString root = cleanRoot(request.stickRoot);
        for (const DlpRemovedTrack& track : std::as_const(outcome.notInLibrary)) {
            if (hooks.importTrack && hooks.importTrack(root + track.stickPath, track)) {
                ++outcome.tracksImported;
            } else {
                outcome.importFailed.append(track.stickPath);
            }
        }
        if (!outcome.importFailed.isEmpty()) {
            // A failed import stops the delete, so that no track goes from
            // the stick before it is in the library.
            outcome.status = StickDeleteStatus::Failed;
            outcome.message =
                    trN("Mix Two cannot add %n track(s) to the library. "
                        "Mix Two did not delete the playlist from the stick.",
                            static_cast<int>(outcome.importFailed.size()));
            return outcome;
        }
    }
    // 4. The delete, with the backup. It sorts the tracks again, because the
    // stick can have changed since the preview. The answers are known, so it
    // gets no library check.
    outcome.result = dlpDeletePlaylistByPath(request.stickRoot,
            request.masterDbId,
            request.namePath,
            DlpLibraryCheck(),
            request.backupBaseDir);
    if (!outcome.result.ok) {
        outcome.status = StickDeleteStatus::Failed;
        outcome.message = outcome.result.errorMessage;
        return outcome;
    }
    outcome.status = StickDeleteStatus::Deleted;
    return outcome;
}

QString stickDeleteSummary(const StickDeleteOutcome& outcome) {
    QStringList lines;
    switch (outcome.status) {
    case StickDeleteStatus::Canceled:
        lines.append(tr("Mix Two did not change the stick."));
        break;
    case StickDeleteStatus::Failed:
        lines.append(outcome.message);
        for (const QString& path : outcome.importFailed) {
            lines.append(path);
        }
        break;
    case StickDeleteStatus::Deleted: {
        const DlpDeletePlaylistResult& result = outcome.result;
        lines.append(tr("Mix Two deleted the playlist from the stick."));
        lines.append(trN("Mix Two deleted %n track(s) from the stick.", result.tracksRemoved));
        if (result.tracksKept > 0) {
            lines.append(trN("%n track(s) stay on the stick, because another list uses them.",
                    result.tracksKept));
        }
        if (outcome.tracksImported > 0) {
            lines.append(trN("Mix Two added %n track(s) to the library.",
                    outcome.tracksImported));
        }
        if (result.legacyLibraryWritten) {
            lines.append(tr("Mix Two also updated the older device library (export.pdb)."));
        }
        if (!result.legacyLibraryError.isEmpty()) {
            lines.append(tr("Mix Two cannot update the older device library (export.pdb). "
                            "The files of the deleted tracks stay on the stick."));
            lines.append(result.legacyLibraryError);
        }
        if (!result.filesNotDeleted.isEmpty()) {
            lines.append(trN("Mix Two cannot delete %n file(s). They stay on the stick.",
                    static_cast<int>(result.filesNotDeleted.size())));
        }
        break;
    }
    }
    return lines.join(QLatin1Char('\n'));
}

DlpLibraryCheck dlpFileNameAndSizeCheck(
        const QString& stickRoot, const LibraryFilesOfSize& filesOfSize) {
    return [root = cleanRoot(stickRoot), filesOfSize](const DlpRemovedTrack& track) {
        if (!filesOfSize || track.stickPath.isEmpty()) {
            return false;
        }
        const QFileInfo stickFile(root + track.stickPath);
        if (!stickFile.isFile()) {
            return false;
        }
        // The export copies the bytes, so the size is the same.
        const QList<LibraryFileRef> candidates = filesOfSize(stickFile.size());
        if (candidates.isEmpty()) {
            return false;
        }
        // The name that the export gives the copy.
        for (const LibraryFileRef& candidate : candidates) {
            if (fatSafeName(candidate.fileName)
                            .compare(stickFile.fileName(), Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        // Another name: compare the bytes. Only files of the same size get
        // here, so this reads few files.
        const QByteArray stickHash = fileHash(stickFile.filePath());
        if (stickHash.isEmpty()) {
            return false;
        }
        for (const LibraryFileRef& candidate : candidates) {
            if (QFileInfo(candidate.location).size() == stickFile.size() &&
                    fileHash(candidate.location) == stickHash) {
                return true;
            }
        }
        return false;
    };
}

} // namespace rekordbox
} // namespace mixxx
// End of AI-generated text.
