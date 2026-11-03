#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include "library/rekordbox/devicelibraryplusdb.h"

namespace mixxx {
namespace rekordbox {

/// Mix Two: the files on a stick that an export replaced for the rows of Mix
/// Two (Rulings R17 and R18). The exporter records them while it works; after
/// a good save() it removes those that no row uses now
/// (dlpRemoveReplacedFiles()). All paths are stick paths, from the root of
/// the stick.
struct DlpReplacedFiles {
    /// The old ANLZ folders of re-exported tracks (no file name).
    QStringList anlzFolders;
    /// The old `bN.jpg` files of re-exported tracks.
    QStringList imagePaths;
    /// The old audio copies in `/Contents` of tracks whose file changed.
    QStringList audioPaths;
};

/// Mix Two: removes the replaced files that no content row uses now. Call it
/// only after save() wrote the library to the stick: `rows` are all content
/// rows of that library, and `imagePaths` has the path of each image row.
/// `stickRoot` is the stick root without a `/` at the end. It removes only:
/// - ANLZ0000.DAT, ANLZ0000.EXT and ANLZ0000.2EX in a folder below
///   `/PIONEER/USBANLZ` that no row uses, then that folder and its parent
///   below `/PIONEER/USBANLZ` (the `P###` folder) when they are empty;
/// - `bN.jpg` and `bN_m.jpg` below `/PIONEER/Artwork` when no row has an
///   image with that path (the image rows stay);
/// - an audio file below `/Contents` whose path no row has.
/// Every path must pass dlpIsSafeStickPath(). The function removes no
/// folder that has files and no symbolic link. It never fails: a file that it
/// cannot remove stays, with a log line. Returns the number of removed files.
int dlpRemoveReplacedFiles(const QString& stickRoot,
        const DlpReplacedFiles& replaced,
        const QList<DlpContentRef>& rows,
        const QHash<qint64, QString>& imagePaths);

} // namespace rekordbox
} // namespace mixxx
