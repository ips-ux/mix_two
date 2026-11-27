#pragma once

// AI-generated (Claude Code). Start of AI-generated text.

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <optional>

#include "library/rekordbox/legacylibrary.h"

namespace mixxx::rekordbox {

class DeviceLibraryPlusDb;

/// Mix Two: reads the bytes of a legacy device library (`export.pdb`) into
/// the plain model, with the same table reader that shows a legacy stick.
/// It is the reverse of legacyPdbBytes(). It does no file I/O. Returns no
/// model and sets *pError (if it is not null) when the bytes are not a
/// legacy library or a list refers to a missing row.
std::optional<LegacyLibrary> legacyLibraryFromPdb(const QByteArray& bytes, QString* pError);

/// Mix Two: writes the rows of a legacy library into a Device Library Plus
/// database that has no tracks and no playlists (a database that
/// DeviceLibraryPlusDb::create() made), in one batch. The reverse of
/// legacyFromDlp(): legacyFromDlp() of the result gives the same library.
/// The tracks keep their stick paths, ANLZ paths and artwork paths: the
/// files on the stick stay where they are. Each track gets `masterDbId` and
/// its legacy ID as masterContentId. It does not save the database. Returns
/// false and sets *pError (if it is not null) on an error; then the database
/// has no new rows.
bool dlpFromLegacy(DeviceLibraryPlusDb& db,
        const LegacyLibrary& library,
        qint64 masterDbId,
        QString* pError);

/// What updateLegacyStickToDlp() does.
struct LegacyToDlpOptions {
    /// True: `export.pdb` stays on the stick. False: the function removes
    /// `export.pdb` and `exportExt.pdb` after the new library is saved.
    bool keepLegacy = true;
    /// The backup goes to `<backupBaseDir>/<fatSafeName(deviceName)>/<yyyyMMdd-HHmmss>/`
    /// (the time stamp is `now`), the folders that dlpBackupLibrary() uses.
    QString backupBaseDir;
    QString deviceName;
    QDateTime now;
    qint64 masterDbId = 0;
};

/// The result of updateLegacyStickToDlp(), for the summary that the DJ sees.
struct LegacyToDlpResult {
    bool ok = false;
    /// A message for the user (ASD-STE100) when `ok` is false.
    QString errorMessage;
    /// The folder with the copy of `export.pdb` (and `exportExt.pdb`).
    QString backupFolder;
    int trackCount = 0;
    int playlistCount = 0;
    int historyCount = 0;
    /// True when `export.pdb` is still on the stick after the update.
    bool legacyKept = true;
    /// The files that the function wrote or removed on the stick, as paths
    /// from the root of the stick.
    QStringList writtenFiles;
    QStringList removedFiles;
};

/// Mix Two: gives a stick that has only the legacy device library a Device
/// Library Plus with the same tracks, playlists and histories. The function
/// 1. refuses a stick that has `exportLibrary.db` or has no `export.pdb`;
/// 2. copies `export.pdb` (and `exportExt.pdb` when it exists) to the backup
///    folder; it does not go on when this fails;
/// 3. reads `export.pdb` (legacyLibraryFromPdb()), makes the new library
///    (DeviceLibraryPlusDb::create(), dlpFromLegacy()) and saves it;
/// 4. when `keepLegacy` is false, removes `export.pdb` and `exportExt.pdb`.
/// It copies, moves or changes no audio, ANLZ or artwork file. A failure
/// before step 3 saves leaves the stick as it was.
LegacyToDlpResult updateLegacyStickToDlp(
        const QString& stickRoot, const LegacyToDlpOptions& options);

/// Mix Two: removes `export.pdb` and `exportExt.pdb` (each one that is there)
/// from `PIONEER/rekordbox/` of the stick, with any letter case. Adds each
/// removed file (a path from the root of the stick) to `pRemovedFiles` (when
/// it is not null). Returns false and sets *pError (when it is not null) when
/// a file stays.
bool removeLegacyLibraryFiles(
        const QString& stickRoot, QStringList* pRemovedFiles, QString* pError);

/// Mix Two: removes `PIONEER/rekordbox/exportLibrary.db` from the stick, to
/// undo an update. Adds the path from the root of the stick to
/// `pRemovedFiles` (when it is not null). Returns false and sets *pError
/// (when it is not null) when the file stays.
bool removePlusLibraryFile(const QString& stickRoot, QStringList* pRemovedFiles, QString* pError);

} // namespace mixxx::rekordbox

// End of AI-generated text.
