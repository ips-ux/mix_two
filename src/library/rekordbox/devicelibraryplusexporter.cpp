#include "library/rekordbox/devicelibraryplusexporter.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QtDebug>
#include <algorithm>
#include <cmath>
#include <utility>

#include "library/rekordbox/anlztrackdata.h"
#include "library/rekordbox/anlzwriter.h"
#include "library/rekordbox/devicelibraryplusexportcleanup.h"
#include "library/rekordbox/devicelibraryplusmapping.h"
#include "library/rekordbox/devicelibraryplusstick.h"
#include "track/track.h"
#include "track/trackmetadata.h"

namespace mixxx {
namespace rekordbox {

namespace {

const QString kAnlzFileName = QStringLiteral("ANLZ0000.DAT");
const QString kArtworkFolder = QStringLiteral("/PIONEER/Artwork/00001");
/// The folder of the crates. It is not translated: the next export must find
/// it again.
const QString kCratesFolderName = QStringLiteral("Crates");
const QString kPartSuffix = QStringLiteral(".mixtwo-part");
constexpr int kSmallArtworkSize = 80;
constexpr int kMediumArtworkSize = 240;
constexpr int kJpegQuality = 90;
/// The fileType of MP3 (spec F13).
constexpr int kMp3FileType = 1;
constexpr int kMaxFatNameLength = 255;

QString missingFileMessage() {
    return QCoreApplication::translate("DeviceLibraryPlus", "The track file is missing.");
}

QString fileTypeMessage() {
    return QCoreApplication::translate("DeviceLibraryPlus", "Players cannot play this file type.");
}

QString largeFileMessage() {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "The file is larger than 4 GB. FAT32 cannot hold it.");
}

QString invalidMasterDbIdMessage() {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "The library of Mix Two has no valid ID.");
}

QString stickFolderMessage() {
    return QCoreApplication::translate("DeviceLibraryPlus", "The stick folder does not exist.");
}

QString notInLibraryMessage() {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "The track is not in the Mix Two library.");
}

QString copyFailedMessage(const QString& reason) {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "Mix Two cannot copy the track to the stick: %1")
            .arg(reason);
}

QString artworkFailedMessage() {
    return QCoreApplication::translate("DeviceLibraryPlus", "Mix Two cannot write the artwork.");
}

QString otherProgramMessage() {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "The stick already has this track from another program. Mix Two did not change it.");
}

QString olderCopyMessage() {
    return QCoreApplication::translate("DeviceLibraryPlus",
            "The stick has an older copy of this track. The playlist uses it.");
}

/// The folder of a stick path: the part before the last '/'.
QString folderOf(const QString& stickPath) {
    return stickPath.left(stickPath.lastIndexOf(QLatin1Char('/')));
}

/// The file name of a stick path: the part after the last '/'.
QString fileNameOf(const QString& stickPath) {
    return stickPath.mid(stickPath.lastIndexOf(QLatin1Char('/')) + 1);
}

/// An ID of a typed row method; -1 (an error of the database) gives none.
std::optional<qint64> idOrNone(qint64 id) {
    if (id < 0) {
        return std::nullopt;
    }
    return id;
}

/// The row of a name in the artist, genre or key table. No row for an empty
/// name.
std::optional<qint64> idOfName(DeviceLibraryPlusDb* pDb,
        qint64 (DeviceLibraryPlusDb::*findOrAdd)(const QString&),
        const QString& name) {
    if (name.isEmpty()) {
        return std::nullopt;
    }
    return idOrNone((pDb->*findOrAdd)(name));
}

/// The file name with " (2)", " (3)", ... before the extension; number 1 gives
/// the name. The name is a fatSafeName(); the base is cut so that the result
/// has at most 255 UTF-16 units and the number stays.
QString numberedFileName(const QString& safeName, int number) {
    if (number <= 1) {
        return safeName;
    }
    const QString numberText = QStringLiteral(" (%1)").arg(number);
    const int dot = safeName.lastIndexOf(QLatin1Char('.'));
    QString base = dot > 0 ? safeName.left(dot) : safeName;
    const QString extension = dot > 0 ? safeName.mid(dot) : QString();
    const int maxBase = std::max(1,
            static_cast<int>(kMaxFatNameLength - numberText.size() - extension.size()));
    if (base.size() > maxBase) {
        base.truncate(maxBase);
        if (base.back().isHighSurrogate()) {
            base.chop(1);
        }
    }
    return base + numberText + extension;
}

/// The N of the stick path "/PIONEER/Artwork/00001/bN.jpg", or 0.
int artworkNumber(const QString& stickPath) {
    static const QRegularExpression kPattern(
            QStringLiteral("^/pioneer/artwork/00001/b([0-9]{1,9})\\.jpg$"));
    const QRegularExpressionMatch match = kPattern.match(dlpPathKey(stickPath));
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

/// A safe JPEG path below /PIONEER/Artwork: the export uses artwork only
/// there.
bool isArtworkPath(const QString& stickPath) {
    const QString key = dlpPathKey(stickPath);
    return dlpIsSafeStickPath(stickPath) && key.startsWith(QStringLiteral("/pioneer/artwork/")) &&
            key.endsWith(QStringLiteral(".jpg"));
}

/// A path below /Contents: the folder of the copies that the export makes.
bool isContentsPath(const QString& stickPath) {
    return dlpPathKey(stickPath).startsWith(QStringLiteral("/contents/"));
}

/// True when a file or a symbolic link (also a broken one) is at the path.
bool isTaken(const QFileInfo& info) {
    return info.exists() || info.isSymLink();
}

/// The 240x240 file of an image: "bN_m.jpg" next to "bN.jpg".
QString mediumArtworkPath(const QString& smallPath) {
    return smallPath.chopped(4) + QStringLiteral("_m.jpg");
}

/// The JPEG bytes of the two files of an image.
struct ArtworkBytes {
    QByteArray smallJpeg;
    QByteArray mediumJpeg;
};

QByteArray jpegBytes(const QImage& square, int size) {
    const QImage scaled =
            square.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                    .convertToFormat(QImage::Format_RGB32);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !scaled.save(&buffer, "JPEG", kJpegQuality)) {
        return QByteArray();
    }
    return bytes;
}

/// bN.jpg (80x80) and bN_m.jpg (240x240) from the centre square of the cover.
/// Empty bytes when the cover cannot be encoded.
ArtworkBytes artworkBytes(const QImage& cover) {
    const int side = std::min(cover.width(), cover.height());
    if (side <= 0) {
        return ArtworkBytes();
    }
    const QImage square = cover.copy(
            (cover.width() - side) / 2, (cover.height() - side) / 2, side, side);
    return ArtworkBytes{jpegBytes(square, kSmallArtworkSize), jpegBytes(square, kMediumArtworkSize)};
}

QByteArray readAllOf(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

bool writeBytes(const QByteArray& bytes, const QString& path) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

/// Writes the two files of a new image. `smallPath` is the absolute path of
/// bN.jpg; neither file exists. On a failure no file of the image stays.
bool writeArtwork(const ArtworkBytes& bytes, const QString& smallPath) {
    if (!QDir().mkpath(QFileInfo(smallPath).path())) {
        return false;
    }
    if (!writeBytes(bytes.smallJpeg, smallPath)) {
        return false;
    }
    if (!writeBytes(bytes.mediumJpeg, mediumArtworkPath(smallPath))) {
        // The export made bN.jpg a moment ago; no row uses it.
        QFile::remove(smallPath);
        return false;
    }
    return true;
}

/// Copies the file to `target` through `<target>.mixtwo-part` and a rename,
/// so that a copy that does not finish never has the name of the target.
/// Returns an empty string, or the reason of the failure.
QString copyAudio(const QString& source, const QString& target) {
    if (!QDir().mkpath(QFileInfo(target).path())) {
        return QCoreApplication::translate("DeviceLibraryPlus", "Mix Two cannot make the folder.");
    }
    const QString part = target + kPartSuffix;
    if (QFileInfo::exists(part)) {
        QFile leftover(part);
        if (!leftover.remove()) {
            return leftover.errorString();
        }
    }
    QFile sourceFile(source);
    if (!sourceFile.copy(part)) {
        const QString reason = sourceFile.errorString();
        QFile::remove(part);
        return reason;
    }
    QFile partFile(part);
    if (!partFile.rename(target)) {
        const QString reason = partFile.errorString();
        QFile::remove(part);
        return reason;
    }
    return QString();
}

/// The stick root with '/' and without a '/' at the end.
QString normalizedRoot(const QString& stickRoot) {
    QString root = QDir::cleanPath(QDir::fromNativeSeparators(stickRoot));
    if (root.endsWith(QLatin1Char('/'))) {
        root.chop(1);
    }
    return root;
}

} // namespace

DeviceLibraryPlusExporter::DeviceLibraryPlusExporter(DeviceLibraryPlusDb* pDb, Options options)
        : m_pDb(pDb),
          m_options(std::move(options)),
          m_root(normalizedRoot(m_options.stickRoot)) {
}

bool DeviceLibraryPlusExporter::prepare(QString* pErrorMessage) {
    // Ruling R10: refuse options that would write to a wrong place.
    const auto refuse = [pErrorMessage](const QString& message) {
        if (pErrorMessage) {
            *pErrorMessage = message;
        }
        return false;
    };
    if (m_options.masterDbId <= 0) {
        return refuse(invalidMasterDbIdMessage());
    }
    if (m_options.stickRoot.trimmed().isEmpty() || !QDir::isAbsolutePath(m_options.stickRoot) ||
            !QFileInfo(m_options.stickRoot).isDir()) {
        return refuse(stickFolderMessage());
    }
    for (const DlpContentRef& row : m_pDb->listContents()) {
        remember(row);
    }
    for (const auto& [imageId, stickPath] : m_pDb->listImages()) {
        m_imagePaths.insert(imageId, stickPath);
        m_nextImageNumber = std::max(m_nextImageNumber, artworkNumber(stickPath) + 1);
    }
    if (!m_pDb->lastErrorMessage().isEmpty() || !m_pDb->beginBatch()) {
        return failWithDbError(pErrorMessage);
    }
    return true;
}

DlpTrackResult DeviceLibraryPlusExporter::exportTrack(const DlpExportTrack& item) {
    ++m_offeredTrackCount;
    DlpTrackResult result;
    const auto skip = [this, &result, &item](const QString& reason) {
        qDebug() << "Device Library Plus export: skipped a track:" << reason;
        result.contentId.reset();
        result.skipReason = reason;
        // Ruling R20: the playlists keep a row that the stick has for the
        // track already.
        if (item.pTrack && item.pTrack->getId().isValid()) {
            if (const std::optional<qint64> rowId = olderRowOf(*item.pTrack)) {
                m_exportedContentIds.insert(item.pTrack->getId(), *rowId);
                result.warnings.append(olderCopyMessage());
            }
        }
        return result;
    };
    if (!m_pDb->lastErrorMessage().isEmpty()) {
        // The export cannot finish; write nothing more to the stick.
        return skip(m_pDb->lastErrorMessage());
    }

    // 1. The checks of the file.
    if (!item.pTrack) {
        return skip(missingFileMessage());
    }
    if (!item.pTrack->getId().isValid()) {
        // The ID is the masterContentId of the row.
        return skip(notInLibraryMessage());
    }
    const Track& track = *item.pTrack;
    const QString location = QDir::fromNativeSeparators(track.getLocation());
    const QFileInfo source(location);
    if (location.isEmpty() || !source.isFile()) {
        return skip(missingFileMessage());
    }
    const int fileType = dlpFileType(source.suffix());
    if (fileType == 0) {
        return skip(fileTypeMessage());
    }
    if (source.size() > m_options.maxFileSize) {
        return skip(largeFileMessage());
    }

    // 2. The place of the audio file on the stick.
    const Placement placement = placementOf(
            track.getId().toVariant().toLongLong(), location, track.getArtist(), track.getAlbum());
    if (placement.otherRowId) {
        // Rulings R8 and R19: the file is on the stick, and another program
        // has a row for it. The export uses that row as it is and writes
        // nothing.
        result.warnings.append(otherProgramMessage());
        result.contentId = *placement.otherRowId;
        m_exportedContentIds.insert(track.getId(), *placement.otherRowId);
        return result;
    }
    const std::optional<DlpContentRef>& ownRow = placement.ownRow;
    const Target& target = placement.target;

    // 3. The analysis data. A refusal skips the track before anything is
    // written.
    QString error;
    std::optional<AnlzTrackData> data = anlzTrackDataFrom(track,
            item.pWaveform,
            target.stickPath,
            rekordboxTimingOffsetMs(location),
            &result.warnings,
            &error);
    if (!data) {
        return skip(error);
    }
    if (fileType == kMp3FileType) {
        data->mp3PcmFrames = dlpMpegPcmFrames(location).value_or(0);
    }

    // 4. The audio file. A copy never replaces a file (targetOf()).
    const QString audioFile = absolutePath(target.stickPath);
    if (target.needsCopy) {
        const QString reason = copyAudio(location, audioFile);
        if (!reason.isEmpty()) {
            return skip(copyFailedMessage(reason));
        }
    }

    // 5. The ANLZ files. Ruling R17: always a new folder, also for a track
    // that has a row. The files of the row stay until the new library is on
    // the stick; finish() removes them after the save.
    const QString anlzFolder = dlpAnlzFolder(target.stickPath, m_usedAnlzFolderKeys, m_root);
    if (!writeAnlzFiles(*data, absolutePath(anlzFolder), &error)) {
        return skip(error);
    }

    // 6. The artwork.
    const std::optional<qint64> imageId = imageOf(item, ownRow, &result.warnings);

    // 7. The rows.
    DlpContent content;
    content.title = track.getTitle();
    content.artistId = idOfName(m_pDb, &DeviceLibraryPlusDb::findOrAddArtist, track.getArtist());
    content.composerId =
            idOfName(m_pDb, &DeviceLibraryPlusDb::findOrAddArtist, track.getComposer());
    content.genreId = idOfName(m_pDb, &DeviceLibraryPlusDb::findOrAddGenre, track.getGenre());
    content.keyId = idOfName(m_pDb, &DeviceLibraryPlusDb::findOrAddKey, dlpKeyName(track.getKey()));
    const QString album = track.getAlbum();
    if (!album.isEmpty()) {
        const QString albumArtist = track.getAlbumArtist();
        const std::optional<qint64> albumArtistId = albumArtist.isEmpty()
                ? content.artistId
                : idOfName(m_pDb, &DeviceLibraryPlusDb::findOrAddArtist, albumArtist);
        content.albumId = idOrNone(m_pDb->findOrAddAlbum(album, albumArtistId));
    }
    content.imageId = imageId;
    const RgbColor::optional_t color = track.getColor();
    content.colorId = dlpColorId(color
                    ? std::optional<QRgb>(static_cast<RgbColor::code_t>(*color))
                    : std::nullopt);
    content.bpmx100 = dlpBpmx100(track.getBpm());
    content.lengthSeconds = static_cast<int>(std::lround(track.getDuration()));
    content.rating = std::clamp(track.getRating(), 0, 5);
    content.releaseYear = TrackMetadata::parseCalendarYear(track.getYear());
    content.playCount = track.getTimesPlayed();
    content.djComment = track.getComment();
    content.dateAdded = dlpDate(track.getDateAdded());
    content.dateCreated = content.dateAdded;
    content.path = target.stickPath;
    content.fileName = fileNameOf(target.stickPath);
    content.fileSize = QFileInfo(audioFile).size();
    content.fileType = fileType;
    content.bitrate = track.getBitrate();
    content.bitDepth = dlpBitDepth(location, fileType);
    content.samplingRate = static_cast<int>(track.getSampleRate().value());
    content.masterDbId = m_options.masterDbId;
    content.masterContentId = track.getId().toVariant().toLongLong();
    content.analysisDataFilePath = anlzFolder + QLatin1Char('/') + kAnlzFileName;

    qint64 contentId = -1;
    if (ownRow) {
        contentId = ownRow->id;
        m_pDb->updateContent(contentId, content);
    } else {
        contentId = m_pDb->addContent(content);
    }
    if (contentId < 0 || !m_pDb->lastErrorMessage().isEmpty()) {
        return skip(m_pDb->lastErrorMessage());
    }

    // Rulings R17 and R18: the files that the row used before. finish()
    // removes those that no row uses after a good save.
    if (ownRow) {
        if (!ownRow->analysisDataFilePath.isEmpty() &&
                dlpPathKey(folderOf(ownRow->analysisDataFilePath)) != dlpPathKey(anlzFolder)) {
            m_replaced.anlzFolders.append(folderOf(ownRow->analysisDataFilePath));
        }
        if (ownRow->imageId && ownRow->imageId != content.imageId) {
            const QString oldImage = m_imagePaths.value(*ownRow->imageId);
            if (isArtworkPath(oldImage)) {
                m_replaced.imagePaths.append(oldImage);
            }
        }
    }
    if (target.replacedPath) {
        m_replaced.audioPaths.append(*target.replacedPath);
    }

    DlpContentRef row;
    row.id = contentId;
    row.path = content.path;
    row.masterDbId = content.masterDbId;
    row.masterContentId = content.masterContentId;
    row.analysisDataFilePath = content.analysisDataFilePath;
    row.imageId = content.imageId;
    remember(row);
    m_exportedContentIds.insert(track.getId(), contentId);
    qDebug() << "Device Library Plus export: wrote" << content.path;
    result.contentId = contentId;
    return result;
}

bool DeviceLibraryPlusExporter::exportPlaylists(const QList<DlpExportPlaylist>& playlists,
        const QList<DlpExportPlaylist>& crates,
        QString* pErrorMessage) {
    if (!m_pDb->lastErrorMessage().isEmpty()) {
        return failWithDbError(pErrorMessage);
    }
    for (const DlpExportPlaylist& playlist : playlists) {
        if (!writePlaylist(playlist, 0)) {
            return failWithDbError(pErrorMessage);
        }
    }
    if (crates.isEmpty()) {
        return true;
    }
    std::optional<qint64> folderId = m_pDb->findFolder(kCratesFolderName, 0);
    if (!folderId && m_pDb->lastErrorMessage().isEmpty()) {
        folderId = idOrNone(m_pDb->addPlaylist(kCratesFolderName, 0, true));
    }
    if (!folderId || !m_pDb->lastErrorMessage().isEmpty()) {
        return failWithDbError(pErrorMessage);
    }
    for (const DlpExportPlaylist& crate : crates) {
        if (!writePlaylist(crate, *folderId)) {
            return failWithDbError(pErrorMessage);
        }
    }
    return true;
}

bool DeviceLibraryPlusExporter::finish(QString* pErrorMessage) {
    if (m_offeredTrackCount > 0 && m_exportedContentIds.isEmpty()) {
        // Ruling R20: every track was skipped, and the stick has no row of
        // any of them. The library on the stick does not change.
        if (pErrorMessage) {
            *pErrorMessage = dlpMessageNoTrackExported();
        }
        return false;
    }
    if (m_pDb->lastErrorMessage().isEmpty()) {
        m_pDb->updateContentCount();
    }
    if (!m_pDb->lastErrorMessage().isEmpty() || !m_pDb->commitBatch()) {
        return failWithDbError(pErrorMessage);
    }
    if (!m_pDb->save(pErrorMessage)) {
        // Nothing is removed: the library on the stick uses the old files.
        return false;
    }
    // Rulings R17 and R18: the new library is on the stick. Remove the
    // replaced files that no row uses now. This never fails the export.
    const QList<DlpContentRef> rows = m_pDb->listContents();
    if (!m_pDb->lastErrorMessage().isEmpty()) {
        qWarning() << "Device Library Plus export: cannot read the rows; no old file is removed";
        return true;
    }
    const int removed = dlpRemoveReplacedFiles(m_root, m_replaced, rows, m_imagePaths);
    qDebug() << "Device Library Plus export: removed" << removed << "replaced files";
    return true;
}

qint64 DeviceLibraryPlusExporter::bytesToCopy(const QList<DlpCopyCandidate>& tracks) const {
    qint64 total = 0;
    for (const DlpCopyCandidate& candidate : tracks) {
        // The same checks as the start of exportTrack(): a track that it skips
        // copies nothing.
        const QString location = QDir::fromNativeSeparators(candidate.location);
        const QFileInfo source(location);
        if (!candidate.trackId.isValid() || location.isEmpty() || !source.isFile() ||
                dlpFileType(source.suffix()) == 0 || source.size() > m_options.maxFileSize) {
            continue;
        }
        const Placement placement = placementOf(candidate.trackId.toVariant().toLongLong(),
                location,
                candidate.artist,
                candidate.album);
        if (!placement.otherRowId && placement.target.needsCopy) {
            total += source.size();
        }
    }
    return total;
}

DeviceLibraryPlusExporter::Placement DeviceLibraryPlusExporter::placementOf(qint64 trackId,
        const QString& location,
        const QString& artist,
        const QString& album) const {
    Placement placement;
    if (const auto it = m_ownRows.constFind(trackId); it != m_ownRows.constEnd()) {
        placement.ownRow = *it;
    }
    const std::optional<QString> ownStickPath = stickPathOf(location);
    if (ownStickPath && !placement.ownRow) {
        const auto it = m_otherRowIdsByPath.constFind(dlpPathKey(*ownStickPath));
        if (it != m_otherRowIdsByPath.constEnd()) {
            placement.otherRowId = *it;
            return placement;
        }
    }
    placement.target = targetOf(artist, album, location, ownStickPath, placement.ownRow);
    if (placement.target.otherRowId) {
        placement.otherRowId = placement.target.otherRowId;
    }
    return placement;
}

std::optional<QString> DeviceLibraryPlusExporter::stickPathOf(const QString& location) const {
    if (!dlpPathKey(location).startsWith(dlpPathKey(m_root) + QLatin1Char('/'))) {
        return std::nullopt;
    }
    return location.mid(m_root.size());
}

DeviceLibraryPlusExporter::Target DeviceLibraryPlusExporter::targetOf(const QString& artist,
        const QString& album,
        const QString& location,
        const std::optional<QString>& ownStickPath,
        const std::optional<DlpContentRef>& ownRow) const {
    // A file that is on the stick already stays where it is. The check comes
    // first, so that such a file is never copied.
    if (ownStickPath) {
        if (ownRow && dlpPathKey(ownRow->path) == dlpPathKey(*ownStickPath)) {
            // The same file; keep the letter case of the row.
            return Target{ownRow->path, false};
        }
        return Target{*ownStickPath, false};
    }
    const qint64 size = QFileInfo(location).size();
    Target result;
    // The path of an own row passed dlpIsSafeStickPath() (remember()); a row
    // with a path that did not has no path here.
    if (ownRow && !ownRow->path.isEmpty()) {
        const QFileInfo onStick(absolutePath(ownRow->path));
        if (!isTaken(onStick)) {
            return Target{ownRow->path, true};
        }
        if (!isContentsPath(ownRow->path) ||
                (onStick.isFile() && !onStick.isSymLink() && onStick.size() == size)) {
            // The same size: the same file. A file outside /Contents is the
            // DJ's own file; the export never copies over it.
            return Target{ownRow->path, false};
        }
        // Ruling R18: the copy in /Contents is not the file of the track now.
        // The new copy gets a new name below; the old one stays until the
        // save.
        result.replacedPath = ownRow->path;
    }
    // A new file in /Contents. A file there that no row uses and that has the
    // size of the track is the track (spec 7.3).
    const QString safeName = fatSafeName(QFileInfo(location).fileName());
    for (int number = 1;; ++number) {
        const QString candidate = dlpAudioStickPath(artist, album, numberedFileName(safeName, number));
        const QString key = dlpPathKey(candidate);
        const QFileInfo onStick(absolutePath(candidate));
        if (m_usedPathKeys.contains(key)) {
            // Ruling R19: rekordbox put the track at the same place. The file
            // there has the size of the track, so it is the track: use the
            // row of the other program (as Ruling R8).
            const auto it = m_otherRowIdsByPath.constFind(key);
            if (!ownRow && it != m_otherRowIdsByPath.constEnd() && onStick.isFile() &&
                    !onStick.isSymLink() && onStick.size() == size) {
                result.stickPath = candidate;
                result.otherRowId = *it;
                return result;
            }
            continue;
        }
        if (!isTaken(onStick)) {
            result.stickPath = candidate;
            result.needsCopy = true;
            return result;
        }
        if (onStick.isFile() && !onStick.isSymLink() && onStick.size() == size) {
            result.stickPath = candidate;
            result.needsCopy = false;
            return result;
        }
    }
}

QString DeviceLibraryPlusExporter::absolutePath(const QString& stickPath) const {
    return m_root + stickPath;
}

std::optional<qint64> DeviceLibraryPlusExporter::olderRowOf(const Track& track) const {
    const auto own = m_ownRows.constFind(track.getId().toVariant().toLongLong());
    if (own != m_ownRows.constEnd()) {
        return own->id;
    }
    const std::optional<QString> stickPath =
            stickPathOf(QDir::fromNativeSeparators(track.getLocation()));
    if (stickPath) {
        const auto other = m_otherRowIdsByPath.constFind(dlpPathKey(*stickPath));
        if (other != m_otherRowIdsByPath.constEnd()) {
            return *other;
        }
    }
    return std::nullopt;
}

QString DeviceLibraryPlusExporter::freeArtworkPath() {
    for (;; ++m_nextImageNumber) {
        const QString smallPath = kArtworkFolder + QStringLiteral("/b%1.jpg").arg(m_nextImageNumber);
        if (!isTaken(QFileInfo(absolutePath(smallPath))) &&
                !isTaken(QFileInfo(absolutePath(mediumArtworkPath(smallPath))))) {
            return smallPath;
        }
    }
}

std::optional<qint64> DeviceLibraryPlusExporter::imageOf(const DlpExportTrack& item,
        const std::optional<DlpContentRef>& ownRow,
        QStringList* pWarnings) {
    if (item.cover.isNull()) {
        return std::nullopt;
    }
    if (item.coverHash) {
        // This export has an image of this cover already.
        const auto it = m_imageIdsByCoverHash.constFind(*item.coverHash);
        if (it != m_imageIdsByCoverHash.constEnd()) {
            return *it;
        }
    }
    const auto rememberCover = [this, &item](qint64 imageId) {
        if (item.coverHash) {
            m_imageIdsByCoverHash.insert(*item.coverHash, imageId);
        }
        return std::optional<qint64>(imageId);
    };
    std::optional<qint64> rowImageId;
    QString rowImagePath;
    if (ownRow && ownRow->imageId) {
        const QString path = m_imagePaths.value(*ownRow->imageId);
        if (isArtworkPath(path)) {
            rowImageId = ownRow->imageId;
            rowImagePath = path;
        }
    }

    const ArtworkBytes bytes = artworkBytes(item.cover);
    if (bytes.smallJpeg.isEmpty() || bytes.mediumJpeg.isEmpty()) {
        pWarnings->append(artworkFailedMessage());
        return rowImageId;
    }
    // Ruling R17: the files of an image are never written again. The same
    // bytes: the track keeps its image.
    if (rowImageId &&
            readAllOf(absolutePath(rowImagePath)) == bytes.smallJpeg &&
            readAllOf(absolutePath(mediumArtworkPath(rowImagePath))) == bytes.mediumJpeg) {
        return rememberCover(*rowImageId);
    }
    // Other bytes: a new image, with a number that no row and no file has.
    const QString stickPath = freeArtworkPath();
    if (!writeArtwork(bytes, absolutePath(stickPath))) {
        pWarnings->append(artworkFailedMessage());
        // The row keeps its image; its files did not change.
        return rowImageId;
    }
    const std::optional<qint64> imageId = idOrNone(m_pDb->addImage(stickPath));
    if (!imageId) {
        return std::nullopt;
    }
    ++m_nextImageNumber;
    m_imagePaths.insert(*imageId, stickPath);
    return rememberCover(*imageId);
}

void DeviceLibraryPlusExporter::remember(const DlpContentRef& row) {
    if (row.masterDbId == m_options.masterDbId) {
        // Important 4: a path of the database that does not pass
        // dlpIsSafeStickPath() is not used. The track gets a new place, and
        // the files of the bad path are never touched.
        DlpContentRef own = row;
        if (!dlpIsSafeStickPath(own.path)) {
            own.path.clear();
        }
        if (!dlpIsSafeStickPath(own.analysisDataFilePath)) {
            own.analysisDataFilePath.clear();
        }
        // The first row of a track wins, as in findContentByMaster(); a row of
        // this export replaces the one that it updated.
        const auto it = m_ownRows.constFind(own.masterContentId);
        if (it == m_ownRows.constEnd() || it->id == own.id) {
            m_ownRows.insert(own.masterContentId, own);
        }
    } else if (!m_otherRowIdsByPath.contains(dlpPathKey(row.path))) {
        m_otherRowIdsByPath.insert(dlpPathKey(row.path), row.id);
    }
    // The paths of all rows count as used, also those that are not safe.
    if (!row.path.isEmpty()) {
        m_usedPathKeys.insert(dlpPathKey(row.path));
    }
    if (!row.analysisDataFilePath.isEmpty()) {
        m_usedAnlzFolderKeys.insert(dlpPathKey(folderOf(row.analysisDataFilePath)));
    }
}

bool DeviceLibraryPlusExporter::writePlaylist(const DlpExportPlaylist& playlist, qint64 parentId) {
    QList<qint64> contentIds;
    for (const TrackId& trackId : playlist.trackIds) {
        const auto it = m_exportedContentIds.constFind(trackId);
        if (it != m_exportedContentIds.constEnd()) {
            contentIds.append(*it);
        }
    }
    std::optional<qint64> playlistId = m_pDb->findPlaylistOnly(playlist.name, parentId);
    if (!playlistId && m_pDb->lastErrorMessage().isEmpty()) {
        playlistId = idOrNone(m_pDb->addPlaylist(playlist.name, parentId, false));
    }
    if (!playlistId || !m_pDb->lastErrorMessage().isEmpty()) {
        return false;
    }
    m_pDb->setPlaylistContents(*playlistId, contentIds);
    if (!m_pDb->lastErrorMessage().isEmpty()) {
        return false;
    }
    ++m_playlistCount;
    return true;
}

bool DeviceLibraryPlusExporter::failWithDbError(QString* pErrorMessage) const {
    if (pErrorMessage) {
        *pErrorMessage = m_pDb->lastErrorMessage();
    }
    return false;
}

} // namespace rekordbox
} // namespace mixxx
