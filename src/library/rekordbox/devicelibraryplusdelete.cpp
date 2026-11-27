// AI-generated (Claude Code). Start of AI-generated text.
#include "library/rekordbox/devicelibraryplusdelete.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QHash>
#include <QSet>
#include <QtDebug>
#include <memory>
#include <optional>
#include <utility>

#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/devicelibraryplusexportcleanup.h"
#include "library/rekordbox/devicelibraryplusmapping.h"
#include "library/rekordbox/devicelibraryplusstick.h"

namespace mixxx {
namespace rekordbox {

namespace {

DlpDeletePlaylistResult failedDelete(const QString& message) {
    DlpDeletePlaylistResult result;
    result.errorMessage = message;
    return result;
}

QString deleteFailedMessage(const QString& reason) {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "Mix Two cannot delete the playlist: %1")
            .arg(reason);
}

QString noPlaylistMessage() {
    return deleteFailedMessage(QCoreApplication::translate("DeviceLibraryPlus",
            "The stick has no such playlist."));
}

QString folderMessage() {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "This is a folder of playlists. Mix Two does not delete folders.");
}

QString invalidMasterDbIdMessage() {
    return deleteFailedMessage(QCoreApplication::translate("DeviceLibraryPlus",
            "The library of Mix Two has no valid ID."));
}

/// The playlist at a path of names: the folders by name, then the last name
/// as a playlist or a folder. Empty when there is none.
std::optional<qint64> findByPath(DeviceLibraryPlusDb* pDb, const QStringList& namePath) {
    qint64 parentId = 0;
    for (int i = 0; i < namePath.size() - 1; ++i) {
        const std::optional<qint64> folderId = pDb->findFolder(namePath.at(i), parentId);
        if (!folderId) {
            return std::nullopt;
        }
        parentId = *folderId;
    }
    return pDb->findPlaylist(namePath.last(), parentId);
}

/// The name of the backup folder of a stick whose library has no device name.
const QString kDefaultDeviceName = QStringLiteral("Mix Two");

/// `playlistId` is not set when the call is by path (`namePath`).
DlpDeletePlaylistResult deletePlaylist(const QString& stickRoot,
        qint64 masterDbId,
        std::optional<qint64> playlistId,
        const QStringList& namePath,
        const DlpLibraryCheck& isInLibrary,
        const QString& backupBaseDir) {
    if (stickRoot.trimmed().isEmpty()) {
        return failedDelete(deleteFailedMessage(QCoreApplication::translate("DeviceLibraryPlus",
                "The path of the stick is empty.")));
    }
    if (masterDbId <= 0) {
        return failedDelete(invalidMasterDbIdMessage());
    }
    QString error;
    const std::unique_ptr<DeviceLibraryPlusDb> pDb = DeviceLibraryPlusDb::open(stickRoot, &error);
    if (!pDb) {
        return failedDelete(deleteFailedMessage(error));
    }
    const auto failWithDbError = [&pDb]() {
        return failedDelete(deleteFailedMessage(pDb->lastErrorMessage()));
    };

    // Find the playlist.
    if (!playlistId) {
        if (namePath.isEmpty()) {
            return failedDelete(noPlaylistMessage());
        }
        playlistId = findByPath(pDb.get(), namePath);
    }
    if (!pDb->lastErrorMessage().isEmpty()) {
        return failWithDbError();
    }
    const QList<DlpPlaylistRef> playlists = pDb->listPlaylists();
    if (!pDb->lastErrorMessage().isEmpty()) {
        return failWithDbError();
    }
    std::optional<DlpPlaylistRef> playlist;
    for (const DlpPlaylistRef& candidate : playlists) {
        if (playlistId && candidate.id == *playlistId) {
            playlist = candidate;
        }
    }
    if (!playlist) {
        return failedDelete(noPlaylistMessage());
    }
    if (playlist->isFolder) {
        // Deleting folders is not part of this function; a folder with
        // children is a folder too.
        DlpDeletePlaylistResult result =
                failedDelete(deleteFailedMessage(folderMessage()));
        result.refused.append({playlist->name, folderMessage()});
        return result;
    }

    // Sort the tracks of the playlist.
    const QList<qint64> contentIds = pDb->listPlaylistContentIds(playlist->id);
    const QSet<qint64> heldElsewhere = pDb->contentIdsInOtherPlaylists(playlist->id);
    const QList<DlpContentRef> rows = pDb->listContents();
    const QSet<qint64> sharedImages = pDb->imageIdsOfAlbumsAndPlaylists();
    const QList<QPair<qint64, QString>> images = pDb->listImages();
    if (!pDb->lastErrorMessage().isEmpty()) {
        return failWithDbError();
    }
    QHash<qint64, DlpContentRef> rowsById;
    for (const DlpContentRef& row : rows) {
        rowsById.insert(row.id, row);
    }
    QHash<qint64, QString> imagePaths;
    for (const auto& image : images) {
        imagePaths.insert(image.first, image.second);
    }

    DlpDeletePlaylistResult result;
    QList<qint64> removeIds;
    DlpReplacedFiles files;
    QSet<qint64> removedImageIds;
    for (const qint64 contentId : contentIds) {
        const auto it = rowsById.constFind(contentId);
        if (it == rowsById.constEnd()) {
            // The entry has no track. It goes with the playlist.
            continue;
        }
        const DlpContentRef& row = *it;
        // A stick has one library: the rows of Mix Two, of rekordbox and of
        // other programs follow the same rule.
        if (heldElsewhere.contains(contentId)) {
            ++result.tracksKept;
        } else {
            removeIds.append(contentId);
            DlpRemovedTrack removed;
            removed.contentId = row.id;
            removed.title = row.title;
            removed.artist = row.artistName;
            removed.stickPath = row.path;
            removed.masterDbId = row.masterDbId;
            removed.masterContentId = row.masterContentId;
            result.removedTracks.append(removed);
            if (!row.path.isEmpty()) {
                files.audioPaths.append(row.path);
            }
            if (!row.analysisDataFilePath.isEmpty()) {
                files.anlzFolders.append(
                        row.analysisDataFilePath.left(row.analysisDataFilePath.lastIndexOf('/')));
            }
            if (row.imageId && !sharedImages.contains(*row.imageId)) {
                removedImageIds.insert(*row.imageId);
            }
        }
    }
    for (const qint64 imageId : std::as_const(removedImageIds)) {
        const QString imagePath = imagePaths.value(imageId);
        if (!imagePath.isEmpty()) {
            files.imagePaths.append(imagePath);
        }
    }
    if (isInLibrary) {
        for (DlpRemovedTrack& removed : result.removedTracks) {
            removed.inMixTwoLibrary = isInLibrary(removed);
        }
    }

    // The backup of the library on the stick, before the stick changes.
    if (!backupBaseDir.trimmed().isEmpty()) {
        const QString deviceName = pDb->deviceName().trimmed().isEmpty()
                ? kDefaultDeviceName
                : pDb->deviceName().trimmed();
        if (dlpBackupLibrary(stickRoot,
                    backupBaseDir,
                    deviceName,
                    QDateTime::currentDateTime(),
                    &error)
                        .isEmpty()) {
            return failedDelete(deleteFailedMessage(error));
        }
    }

    // The change to the library: one transaction, then the stick.
    pDb->removePlaylist(playlist->id, removeIds);
    if (pDb->lastErrorMessage().isEmpty()) {
        pDb->updateContentCount();
    }
    if (!pDb->lastErrorMessage().isEmpty()) {
        return failWithDbError();
    }
    if (!pDb->save(&error)) {
        return failedDelete(deleteFailedMessage(error));
    }
    result.ok = true;
    result.playlistsRemoved = 1;
    result.tracksRemoved = static_cast<int>(removeIds.size());

    // The library on the stick has no row of the removed tracks now. Delete
    // their files that no remaining row uses.
    const QList<DlpContentRef> remaining = pDb->listContents();
    if (!pDb->lastErrorMessage().isEmpty()) {
        qWarning() << "Device Library Plus delete: cannot read the rows; no file is removed";
        return result;
    }
    // The stick root with `/` and no `/` at the end, as the export has it.
    QString root = QDir::cleanPath(stickRoot);
    if (root.endsWith(QLatin1Char('/'))) {
        root.chop(1);
    }
    const int removed = dlpRemoveReplacedFiles(
            root, files, remaining, imagePaths, &result.filesNotDeleted);
    qDebug() << "Device Library Plus delete: removed" << removed << "files";
    return result;
}

} // namespace

DlpDeletePlaylistResult dlpDeletePlaylist(const QString& stickRoot,
        qint64 masterDbId,
        qint64 playlistId,
        const DlpLibraryCheck& isInLibrary,
        const QString& backupBaseDir) {
    return deletePlaylist(
            stickRoot, masterDbId, playlistId, QStringList(), isInLibrary, backupBaseDir);
}

DlpDeletePlaylistResult dlpDeletePlaylistByPath(const QString& stickRoot,
        qint64 masterDbId,
        const QStringList& namePath,
        const DlpLibraryCheck& isInLibrary,
        const QString& backupBaseDir) {
    return deletePlaylist(
            stickRoot, masterDbId, std::nullopt, namePath, isInLibrary, backupBaseDir);
}

} // namespace rekordbox
} // namespace mixxx
// End of AI-generated text.
