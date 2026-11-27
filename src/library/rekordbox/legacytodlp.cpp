// AI-generated (Claude Code). Start of AI-generated text.

#include "library/rekordbox/legacytodlp.h"

#include <rekordbox_pdb.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QtEndian>
#include <algorithm>
#include <exception>
#include <memory>
#include <sstream>

#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/devicelibraryplusstick.h"

namespace mixxx::rekordbox {

namespace {

using Pdb = rekordbox_pdb_t;

QString tr(const char* text) {
    return QCoreApplication::translate("LegacyToDlp", text);
}

QString setError(QString* pError, const QString& message) {
    if (pError) {
        *pError = message;
    }
    return message;
}

/// The text of a string of the legacy file. Mix Two builds the table reader
/// without encoding conversion, so a UTF-16 string has the bytes of the file.
QString pdbText(Pdb::device_sql_string_t* pString) {
    if (!pString || !pString->body()) {
        return {};
    }
    QString text;
    switch (pString->length_and_kind()) {
    case 0x40:
        text = QString::fromStdString(
                static_cast<Pdb::device_sql_long_ascii_t*>(pString->body())->text());
        break;
    case 0x90: {
        const std::string bytes =
                static_cast<Pdb::device_sql_long_utf16le_t*>(pString->body())->text();
        for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
            text.append(QChar(qFromLittleEndian<quint16>(bytes.data() + i)));
        }
        break;
    }
    default:
        text = QString::fromStdString(
                static_cast<Pdb::device_sql_short_ascii_t*>(pString->body())->text());
        break;
    }
    return text.remove(QChar(0));
}

/// The ISRC of a track. A long ISRC has the UTF-16 marker, but its bytes are
/// a prefix byte 3, ASCII text and a NUL.
QString pdbIsrc(Pdb::device_sql_string_t* pString) {
    if (pString && pString->body() && pString->length_and_kind() == 0x90) {
        std::string bytes = static_cast<Pdb::device_sql_long_utf16le_t*>(pString->body())->text();
        if (!bytes.empty() && bytes.front() == 3) {
            bytes.erase(0, 1);
            return QString::fromLatin1(bytes.data(), static_cast<int>(bytes.size()))
                    .remove(QChar(0));
        }
    }
    return pdbText(pString);
}

/// Calls `visit` for each row of the table of this type. Returns false when
/// the page chain is broken.
template<typename Visit>
bool visitRows(Pdb& db, qsizetype pageCount, Pdb::page_type_t type, Visit visit) {
    Pdb::table_t* pTable = nullptr;
    for (auto* pCandidate : *db.tables()) {
        if (pCandidate->type() == type) {
            pTable = pCandidate;
            break;
        }
    }
    if (!pTable) {
        // A file without this table has no rows of it.
        return true;
    }
    auto* pRef = pTable->first_page();
    for (qsizetype steps = 0; steps < pageCount; ++steps) {
        if (pRef->index() == 0 || pRef->index() >= pageCount) {
            return false;
        }
        auto* pPage = pRef->body();
        if (pPage->is_data_page()) {
            for (auto* pGroup : *pPage->row_groups()) {
                for (auto* pRow : *pGroup->rows()) {
                    if (pRow->present() && pRow->body()) {
                        visit(pRow->body());
                    }
                }
            }
        }
        if (pRef->index() == pTable->last_page()->index()) {
            return true;
        }
        pRef = pPage->next_page();
    }
    return false;
}

template<typename Row>
void sortById(QList<Row>& rows) {
    std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return a.id < b.id;
    });
}

template<typename Entry>
void sortEntries(QList<Entry>& entries) {
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        return a.playlistId != b.playlistId ? a.playlistId < b.playlistId
                                            : a.entryIndex < b.entryIndex;
    });
}

template<typename Row>
QSet<quint32> idsOf(const QList<Row>& rows) {
    QSet<quint32> ids;
    for (const Row& row : rows) {
        ids.insert(row.id);
    }
    return ids;
}

/// True when each reference of the library is 0 or the ID of a row.
bool referencesAreComplete(const LegacyLibrary& library) {
    const auto artists = idsOf(library.artists);
    const auto albums = idsOf(library.albums);
    const auto genres = idsOf(library.genres);
    const auto keys = idsOf(library.keys);
    const auto labels = idsOf(library.labels);
    const auto artwork = idsOf(library.artwork);
    const auto tracks = idsOf(library.tracks);
    const auto histories = idsOf(library.historyPlaylists);
    const auto known = [](const QSet<quint32>& ids, quint32 id) {
        return id == 0 || ids.contains(id);
    };
    for (const auto& album : library.albums) {
        if (!known(artists, album.artistId)) {
            return false;
        }
    }
    for (const auto& track : library.tracks) {
        if (!known(artists, track.artistId) || !known(artists, track.composerId) ||
                !known(artists, track.originalArtistId) || !known(artists, track.remixerId) ||
                !known(albums, track.albumId) || !known(genres, track.genreId) ||
                !known(labels, track.labelId) || !known(keys, track.keyId) ||
                !known(artwork, track.artworkId) || track.colorId > 8) {
            return false;
        }
    }
    QSet<quint32> folders;
    QSet<quint32> lists;
    for (const auto& node : library.playlists) {
        (node.isFolder ? folders : lists).insert(node.id);
    }
    for (const auto& node : library.playlists) {
        if (!known(folders, node.parentId)) {
            return false;
        }
    }
    for (const auto& entry : library.playlistEntries) {
        if (!lists.contains(entry.playlistId) || !tracks.contains(entry.trackId)) {
            return false;
        }
    }
    for (const auto& entry : library.historyEntries) {
        if (!histories.contains(entry.playlistId) || !tracks.contains(entry.trackId)) {
            return false;
        }
    }
    return true;
}

QString missingReferenceMessage() {
    return tr("A list in the older device library refers to a missing row.");
}

std::optional<LegacyLibrary> readPdb(const QByteArray& bytes, QString* pError) {
    std::istringstream input(bytes.toStdString());
    kaitai::kstream stream(&input);
    Pdb db(&stream);
    if (db.len_page() == 0) {
        return std::nullopt;
    }
    const qsizetype pageCount = bytes.size() / db.len_page();
    LegacyLibrary library;
    const auto named = [](QList<LegacyNamedRow>& rows) {
        return [&rows](kaitai::kstruct* pBody) {
            auto* pRow = static_cast<Pdb::genre_row_t*>(pBody);
            rows.append(LegacyNamedRow{pRow->id(), pdbText(pRow->name())});
        };
    };
    bool ok = visitRows(db, pageCount, Pdb::PAGE_TYPE_TRACKS, [&](kaitai::kstruct* pBody) {
        auto* pRow = static_cast<Pdb::track_row_t*>(pBody);
        LegacyTrack track;
        track.id = pRow->id();
        track.title = pdbText(pRow->title());
        track.artistId = pRow->artist_id();
        track.composerId = pRow->composer_id();
        track.originalArtistId = pRow->original_artist_id();
        track.remixerId = pRow->remixer_id();
        track.albumId = pRow->album_id();
        track.genreId = pRow->genre_id();
        track.labelId = pRow->label_id();
        track.keyId = pRow->key_id();
        track.colorId = pRow->color_id();
        track.artworkId = pRow->artwork_id();
        track.tempo = pRow->tempo();
        track.duration = pRow->duration();
        track.rating = pRow->rating();
        track.year = pRow->year();
        track.trackNumber = pRow->track_number();
        track.discNumber = pRow->disc_number();
        track.playCount = pRow->play_count();
        track.bitrate = pRow->bitrate();
        track.sampleRate = pRow->sample_rate();
        track.sampleDepth = pRow->sample_depth();
        track.fileSize = pRow->file_size();
        // The u16 after the rating is the file type.
        track.fileType = pRow->_unnamed29();
        track.filePath = pdbText(pRow->file_path());
        track.fileName = pdbText(pRow->filename());
        track.analyzePath = pdbText(pRow->analyze_path());
        track.analyzeDate = pdbText(pRow->analyze_date());
        track.dateAdded = pdbText(pRow->date_added());
        track.releaseDate = pdbText(pRow->release_date());
        track.mixName = pdbText(pRow->mix_name());
        track.comment = pdbText(pRow->comment());
        track.isrc = pdbIsrc(pRow->isrc());
        track.message = pdbText(pRow->message());
        track.autoLoadHotCues = pdbText(pRow->autoload_hot_cues()) == QStringLiteral("ON");
        library.tracks.append(std::move(track));
    });
    ok = ok && visitRows(db, pageCount, Pdb::PAGE_TYPE_GENRES, named(library.genres));
    ok = ok && visitRows(db, pageCount, Pdb::PAGE_TYPE_ARTISTS, [&](kaitai::kstruct* pBody) {
        auto* pRow = static_cast<Pdb::artist_row_t*>(pBody);
        library.artists.append(LegacyNamedRow{pRow->id(), pdbText(pRow->name())});
    });
    ok = ok && visitRows(db, pageCount, Pdb::PAGE_TYPE_ALBUMS, [&](kaitai::kstruct* pBody) {
        auto* pRow = static_cast<Pdb::album_row_t*>(pBody);
        library.albums.append(LegacyAlbum{pRow->id(), pRow->artist_id(), pdbText(pRow->name())});
    });
    ok = ok && visitRows(db, pageCount, Pdb::PAGE_TYPE_LABELS, [&](kaitai::kstruct* pBody) {
        auto* pRow = static_cast<Pdb::label_row_t*>(pBody);
        library.labels.append(LegacyNamedRow{pRow->id(), pdbText(pRow->name())});
    });
    ok = ok && visitRows(db, pageCount, Pdb::PAGE_TYPE_KEYS, [&](kaitai::kstruct* pBody) {
        auto* pRow = static_cast<Pdb::key_row_t*>(pBody);
        library.keys.append(LegacyNamedRow{pRow->id(), pdbText(pRow->name())});
    });
    ok = ok && visitRows(db, pageCount, Pdb::PAGE_TYPE_COLORS, [&](kaitai::kstruct* pBody) {
        auto* pRow = static_cast<Pdb::color_row_t*>(pBody);
        library.colors.append(LegacyNamedRow{pRow->id(), pdbText(pRow->name())});
    });
    ok = ok && visitRows(db, pageCount, Pdb::PAGE_TYPE_ARTWORK, [&](kaitai::kstruct* pBody) {
        auto* pRow = static_cast<Pdb::artwork_row_t*>(pBody);
        library.artwork.append(LegacyArtwork{pRow->id(), pdbText(pRow->path())});
    });
    ok = ok && visitRows(db, pageCount, Pdb::PAGE_TYPE_PLAYLIST_TREE, [&](kaitai::kstruct* pBody) {
        auto* pRow = static_cast<Pdb::playlist_tree_row_t*>(pBody);
        library.playlists.append(LegacyPlaylistNode{pRow->id(),
                pRow->parent_id(),
                pRow->sort_order(),
                pRow->is_folder(),
                pdbText(pRow->name())});
    });
    ok = ok &&
            visitRows(db, pageCount, Pdb::PAGE_TYPE_PLAYLIST_ENTRIES, [&](kaitai::kstruct* pBody) {
                auto* pRow = static_cast<Pdb::playlist_entry_row_t*>(pBody);
                library.playlistEntries.append(LegacyPlaylistEntry{
                        pRow->playlist_id(), pRow->track_id(), pRow->entry_index()});
            });
    ok = ok &&
            visitRows(db, pageCount, Pdb::PAGE_TYPE_HISTORY_PLAYLISTS, [&](kaitai::kstruct* pBody) {
                auto* pRow = static_cast<Pdb::history_playlist_row_t*>(pBody);
                library.historyPlaylists.append(
                        LegacyHistoryPlaylist{pRow->id(), pdbText(pRow->name())});
            });
    ok = ok &&
            visitRows(db, pageCount, Pdb::PAGE_TYPE_HISTORY_ENTRIES, [&](kaitai::kstruct* pBody) {
                auto* pRow = static_cast<Pdb::history_entry_row_t*>(pBody);
                library.historyEntries.append(LegacyHistoryEntry{
                        pRow->playlist_id(), pRow->track_id(), pRow->entry_index()});
            });
    if (!ok) {
        setError(pError, tr("The pages of the older device library are not complete."));
        return std::nullopt;
    }
    sortById(library.tracks);
    sortById(library.artists);
    sortById(library.albums);
    sortById(library.genres);
    sortById(library.keys);
    sortById(library.labels);
    sortById(library.colors);
    sortById(library.artwork);
    sortById(library.playlists);
    sortById(library.historyPlaylists);
    sortEntries(library.playlistEntries);
    sortEntries(library.historyEntries);
    if (!referencesAreComplete(library)) {
        setError(pError, missingReferenceMessage());
        return std::nullopt;
    }
    return library;
}

/// The nodes of the playlist tree in ID order, but each folder before the
/// nodes in it. Empty when a parent is missing or the tree has a loop.
QList<LegacyPlaylistNode> parentsFirst(QList<LegacyPlaylistNode> nodes) {
    sortById(nodes);
    QList<LegacyPlaylistNode> ordered;
    QSet<quint32> done;
    while (!nodes.isEmpty()) {
        const qsizetype before = nodes.size();
        for (qsizetype i = 0; i < nodes.size();) {
            if (nodes[i].parentId == 0 || done.contains(nodes[i].parentId)) {
                done.insert(nodes[i].id);
                ordered.append(nodes.takeAt(i));
            } else {
                ++i;
            }
        }
        if (nodes.size() == before) {
            return {};
        }
    }
    return ordered;
}

std::optional<qint64> mapped(const QHash<quint32, qint64>& ids, quint32 id) {
    if (id == 0) {
        return std::nullopt;
    }
    return ids.value(id);
}

LegacyToDlpResult failed(LegacyToDlpResult result, const QString& reason) {
    result.ok = false;
    result.errorMessage = tr("Mix Two cannot update the stick. %1").arg(reason);
    return result;
}

const QString kRekordboxFolder = QStringLiteral("PIONEER/rekordbox");

QString stickPathOf(const QString& stickRoot, const QString& path) {
    return QLatin1Char('/') + QDir(stickRoot).relativeFilePath(path);
}

bool removeStickFile(const QString& stickRoot,
        const QString& relativePath,
        QStringList* pRemovedFiles,
        QString* pError) {
    const QString path = dlpFindPathNoCase(stickRoot, relativePath);
    if (path.isEmpty() || !QFileInfo(path).isFile()) {
        return true;
    }
    if (!QFile::remove(path)) {
        setError(pError,
                tr("Mix Two cannot remove the file %1 from the stick.")
                        .arg(stickPathOf(stickRoot, path)));
        return false;
    }
    if (pRemovedFiles) {
        pRemovedFiles->append(stickPathOf(stickRoot, path));
    }
    return true;
}

} // namespace

std::optional<LegacyLibrary> legacyLibraryFromPdb(const QByteArray& bytes, QString* pError) {
    if (pError) {
        pError->clear();
    }
    try {
        auto library = readPdb(bytes, pError);
        if (!library && pError && pError->isEmpty()) {
            *pError = tr("The file is not an older device library.");
        }
        return library;
    } catch (const std::exception&) {
        setError(pError, tr("The file is not an older device library."));
        return std::nullopt;
    }
}

bool dlpFromLegacy(DeviceLibraryPlusDb& db,
        const LegacyLibrary& library,
        qint64 masterDbId,
        QString* pError) {
    if (pError) {
        pError->clear();
    }
    const QList<LegacyPlaylistNode> nodes = parentsFirst(library.playlists);
    if (!referencesAreComplete(library) || nodes.size() != library.playlists.size()) {
        setError(pError, missingReferenceMessage());
        return false;
    }
    if (!db.beginBatch()) {
        setError(pError, db.lastErrorMessage());
        return false;
    }
    QHash<quint32, qint64> artists, albums, genres, keys, labels, images, tracks, playlists,
            histories;
    const auto addNamed = [](const QList<LegacyNamedRow>& rows,
                                  QHash<quint32, qint64>& ids,
                                  const auto& add) {
        QList<LegacyNamedRow> sorted = rows;
        sortById(sorted);
        for (const auto& row : std::as_const(sorted)) {
            ids.insert(row.id, add(row.name));
        }
    };
    addNamed(library.artists, artists, [&](const QString& name) { return db.addArtist(name); });
    addNamed(library.genres, genres, [&](const QString& name) { return db.addGenre(name); });
    addNamed(library.keys, keys, [&](const QString& name) { return db.addKey(name); });
    addNamed(library.labels, labels, [&](const QString& name) { return db.addLabel(name); });
    QList<LegacyAlbum> sortedAlbums = library.albums;
    sortById(sortedAlbums);
    for (const auto& album : std::as_const(sortedAlbums)) {
        albums.insert(album.id, db.addAlbum(album.name, mapped(artists, album.artistId)));
    }
    QList<LegacyArtwork> sortedArtwork = library.artwork;
    sortById(sortedArtwork);
    for (const auto& image : std::as_const(sortedArtwork)) {
        images.insert(image.id, db.addImage(image.path));
    }
    // The colour rows are those of a new library: IDs 1 to 8, as in the
    // legacy file.
    QList<LegacyTrack> sortedTracks = library.tracks;
    sortById(sortedTracks);
    for (const auto& track : std::as_const(sortedTracks)) {
        DlpContent content;
        content.title = track.title;
        content.artistId = mapped(artists, track.artistId);
        content.composerId = mapped(artists, track.composerId);
        content.originalArtistId = mapped(artists, track.originalArtistId);
        content.remixerId = mapped(artists, track.remixerId);
        content.albumId = mapped(albums, track.albumId);
        content.genreId = mapped(genres, track.genreId);
        content.labelId = mapped(labels, track.labelId);
        content.keyId = mapped(keys, track.keyId);
        content.imageId = mapped(images, track.artworkId);
        content.colorId = static_cast<int>(track.colorId);
        content.bpmx100 = static_cast<int>(track.tempo);
        content.lengthSeconds = static_cast<int>(track.duration);
        content.rating = static_cast<int>(track.rating);
        content.releaseYear = static_cast<int>(track.year);
        content.trackNo = static_cast<int>(track.trackNumber);
        content.discNo = static_cast<int>(track.discNumber);
        content.playCount = static_cast<int>(track.playCount);
        content.hotCueAutoLoad = track.autoLoadHotCues;
        content.djComment = track.comment;
        content.dateAdded = track.dateAdded;
        content.dateCreated = track.analyzeDate;
        content.releaseDate = track.releaseDate;
        content.subtitle = track.mixName;
        content.isrc = track.isrc;
        content.kuvoDeliveryComment = track.message;
        content.path = track.filePath;
        content.fileName = track.fileName;
        content.fileSize = track.fileSize;
        content.fileType = static_cast<int>(track.fileType);
        content.bitrate = static_cast<int>(track.bitrate);
        content.bitDepth = static_cast<int>(track.sampleDepth);
        content.samplingRate = static_cast<int>(track.sampleRate);
        content.masterDbId = masterDbId;
        content.masterContentId = track.id;
        content.analysisDataFilePath = track.analyzePath;
        tracks.insert(track.id, db.addContent(content));
    }
    for (const auto& node : nodes) {
        playlists.insert(node.id,
                db.addPlaylistAt(node.name,
                        node.parentId == 0 ? 0 : playlists.value(node.parentId),
                        node.isFolder,
                        static_cast<int>(node.sortOrder)));
    }
    const auto listsOf = [&tracks](auto entries) {
        sortEntries(entries);
        QHash<quint32, QList<qint64>> lists;
        for (const auto& entry : std::as_const(entries)) {
            lists[entry.playlistId].append(tracks.value(entry.trackId));
        }
        return lists;
    };
    const auto playlistEntries = listsOf(library.playlistEntries);
    for (auto it = playlistEntries.cbegin(); it != playlistEntries.cend(); ++it) {
        db.setPlaylistContents(playlists.value(it.key()), it.value());
    }
    QList<LegacyHistoryPlaylist> sortedHistories = library.historyPlaylists;
    sortById(sortedHistories);
    for (const auto& history : std::as_const(sortedHistories)) {
        histories.insert(history.id, db.addHistory(history.name));
    }
    const auto historyEntries = listsOf(library.historyEntries);
    for (auto it = historyEntries.cbegin(); it != historyEntries.cend(); ++it) {
        db.setHistoryContents(histories.value(it.key()), it.value());
    }
    db.updateContentCount();
    if (!db.lastErrorMessage().isEmpty()) {
        setError(pError, db.lastErrorMessage());
        db.rollbackBatch();
        return false;
    }
    if (!db.commitBatch()) {
        setError(pError, db.lastErrorMessage());
        return false;
    }
    return true;
}

bool removeLegacyLibraryFiles(
        const QString& stickRoot, QStringList* pRemovedFiles, QString* pError) {
    bool ok = removeStickFile(stickRoot,
            kRekordboxFolder + QStringLiteral("/export.pdb"),
            pRemovedFiles,
            pError);
    ok = removeStickFile(stickRoot,
                 kRekordboxFolder + QStringLiteral("/exportExt.pdb"),
                 pRemovedFiles,
                 pError) &&
            ok;
    return ok;
}

bool removePlusLibraryFile(const QString& stickRoot, QStringList* pRemovedFiles, QString* pError) {
    return removeStickFile(stickRoot, DeviceLibraryPlusDb::kRelativePath, pRemovedFiles, pError);
}

LegacyToDlpResult updateLegacyStickToDlp(
        const QString& stickRoot, const LegacyToDlpOptions& options) {
    LegacyToDlpResult result;
    if (stickRoot.trimmed().isEmpty() || !QFileInfo(stickRoot).isDir()) {
        return failed(result, tr("Mix Two cannot find the stick."));
    }
    if (!dlpFindPathNoCase(stickRoot, DeviceLibraryPlusDb::kRelativePath).isEmpty()) {
        return failed(result, tr("The stick has a Device Library Plus already."));
    }
    const QString legacyPath =
            dlpFindPathNoCase(stickRoot, kRekordboxFolder + QStringLiteral("/export.pdb"));
    if (legacyPath.isEmpty()) {
        return failed(result, tr("The stick has no older device library."));
    }

    QString error;
    result.backupFolder = dlpBackupLegacyLibrary(
            stickRoot, options.backupBaseDir, options.deviceName, options.now, &error);
    if (result.backupFolder.isEmpty()) {
        return failed(result, error);
    }

    QFile file(legacyPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return failed(result, tr("Mix Two cannot read the older device library."));
    }
    const auto library = legacyLibraryFromPdb(file.readAll(), &error);
    file.close();
    if (!library) {
        return failed(result, error);
    }

    auto pDb = DeviceLibraryPlusDb::create(
            stickRoot, options.deviceName, options.masterDbId, &error);
    if (!pDb) {
        return failed(result, error);
    }
    if (!dlpFromLegacy(*pDb, *library, options.masterDbId, &error) || !pDb->save(&error)) {
        return failed(result, error);
    }
    result.ok = true;
    result.writtenFiles.append(stickPathOf(stickRoot, pDb->stickFilePath()));
    result.trackCount = static_cast<int>(library->tracks.size());
    result.playlistCount = static_cast<int>(std::count_if(library->playlists.cbegin(),
            library->playlists.cend(),
            [](const LegacyPlaylistNode& node) { return !node.isFolder; }));
    result.historyCount = static_cast<int>(library->historyPlaylists.size());
    if (!options.keepLegacy) {
        // The new library is on the stick, and the backup has the old files.
        if (!removeLegacyLibraryFiles(stickRoot, &result.removedFiles, &error)) {
            result.errorMessage = error;
        }
    }
    result.legacyKept = QFileInfo(legacyPath).exists();
    return result;
}

} // namespace mixxx::rekordbox

// End of AI-generated text.
