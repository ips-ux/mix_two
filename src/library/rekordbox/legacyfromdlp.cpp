// AI-generated (Claude Code). Start of AI-generated text.

#include "library/rekordbox/legacyfromdlp.h"

#include <QCoreApplication>
#include <QHash>
#include <algorithm>
#include <limits>
#include <utility>

#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/devicelibraryplusdbsql.h"

namespace mixxx::rekordbox {

namespace {

using dlpsql::NamedStatement;
using IdMap = QHash<qint64, quint32>;

quint32 addId(IdMap& ids, qint64 sourceId) {
    const quint32 id = static_cast<quint32>(ids.size() + 1);
    ids.insert(sourceId, id);
    return id;
}

quint32 reference(const NamedStatement& query, int column, const IdMap& ids) {
    return query.columnIsNull(column) ? 0 : ids.value(query.columnInt(column));
}

quint32 number(const NamedStatement& query, int column) {
    return static_cast<quint32>(std::clamp<qint64>(
            query.columnInt(column), 0, std::numeric_limits<quint32>::max()));
}

class LibraryReader {
  public:
    LibraryReader(sqlite3* pDb, QString* pError)
            : m_pDb(pDb), m_pError(pError) {
    }

    template<typename ReadRow>
    bool read(const char* sql, ReadRow readRow) const {
        NamedStatement query(m_pDb, sql);
        for (;;) {
            switch (query.step()) {
            case NamedStatement::Result::Row:
                if (!readRow(query)) {
                    return missingReference();
                }
                break;
            case NamedStatement::Result::Done:
                return true;
            case NamedStatement::Result::Error:
                if (m_pError) {
                    *m_pError = QCoreApplication::translate("DeviceLibraryPlus",
                            "Mix Two cannot read Device Library Plus: %1")
                                        .arg(query.error());
                }
                return false;
            }
        }
    }

    template<typename Row>
    bool readNamed(const char* sql, QList<Row>& rows, IdMap& ids) const {
        return read(sql, [&](const NamedStatement& query) {
            rows.append(Row{addId(ids, query.columnInt(0)), query.columnText(1)});
            return true;
        });
    }

    template<typename Entry>
    bool readEntries(const char* sql,
            const IdMap& listIds,
            const IdMap& trackIds,
            QList<Entry>& entries) const {
        quint32 previousListId = 0;
        quint32 index = 0;
        return read(sql, [&](const NamedStatement& query) {
            const quint32 listId = reference(query, 0, listIds);
            const quint32 trackId = reference(query, 1, trackIds);
            if (listId == 0 || trackId == 0) {
                return false;
            }
            if (listId != previousListId) {
                previousListId = listId;
                index = 0;
            }
            entries.append(Entry{listId, trackId, ++index});
            return true;
        });
    }

    bool missingReference() const {
        if (m_pError) {
            *m_pError = QCoreApplication::translate("DeviceLibraryPlus",
                    "A list in Device Library Plus refers to a missing row.");
        }
        return false;
    }

  private:
    sqlite3* const m_pDb;
    QString* const m_pError;
};

} // namespace

std::optional<LegacyLibrary> legacyFromDlp(const DeviceLibraryPlusDb& db, QString* pError) {
    if (pError) {
        pError->clear();
    }
    const LibraryReader reader(db.handle(), pError);
    LegacyLibrary library;
    IdMap artists, albums, genres, keys, labels, colors, artwork, tracks, playlists, histories;

    if (!reader.readNamed("SELECT artist_id, name FROM artist ORDER BY artist_id",
                library.artists,
                artists) ||
            !reader.readNamed("SELECT genre_id, name FROM genre ORDER BY genre_id",
                    library.genres,
                    genres) ||
            !reader.readNamed("SELECT key_id, name FROM key ORDER BY key_id", library.keys, keys) ||
            !reader.readNamed("SELECT label_id, name FROM label ORDER BY label_id",
                    library.labels,
                    labels) ||
            !reader.readNamed("SELECT color_id, name FROM color ORDER BY color_id",
                    library.colors,
                    colors) ||
            !reader.readNamed("SELECT image_id, path FROM image ORDER BY image_id",
                    library.artwork,
                    artwork) ||
            !reader.read("SELECT album_id, artist_id, name FROM album ORDER BY album_id",
                    [&](const NamedStatement& query) {
                        library.albums.append(LegacyAlbum{
                                addId(albums, query.columnInt(0)),
                                reference(query, 1, artists),
                                query.columnText(2)});
                        return true;
                    })) {
        return std::nullopt;
    }

    if (!reader.read(
                "SELECT content_id, title, artist_id_artist, artist_id_composer, "
                "artist_id_originalArtist, artist_id_remixer, album_id, genre_id, label_id, "
                "key_id, color_id, image_id, bpmx100, length, rating, releaseYear, trackNo, "
                "discNo, djPlayCount, bitrate, samplingRate, bitDepth, fileSize, fileType, "
                "path, fileName, analysisDataFilePath, dateCreated, dateAdded, releaseDate, "
                "subtitle, djComment, isrc, kuvoDeliveryComment, isHotCueAutoLoadOn "
                "FROM content ORDER BY content_id",
                [&](const NamedStatement& query) {
                    LegacyTrack track;
                    track.id = addId(tracks, query.columnInt(0));
                    track.title = query.columnText(1);
                    track.artistId = reference(query, 2, artists);
                    track.composerId = reference(query, 3, artists);
                    track.originalArtistId = reference(query, 4, artists);
                    track.remixerId = reference(query, 5, artists);
                    track.albumId = reference(query, 6, albums);
                    track.genreId = reference(query, 7, genres);
                    track.labelId = reference(query, 8, labels);
                    track.keyId = reference(query, 9, keys);
                    track.colorId = reference(query, 10, colors);
                    track.artworkId = reference(query, 11, artwork);
                    track.tempo = number(query, 12);
                    track.duration = number(query, 13);
                    track.rating = std::min(number(query, 14), 5u);
                    track.year = number(query, 15);
                    track.trackNumber = number(query, 16);
                    track.discNumber = number(query, 17);
                    track.playCount = number(query, 18);
                    track.bitrate = number(query, 19);
                    track.sampleRate = number(query, 20);
                    track.sampleDepth = number(query, 21);
                    track.fileSize = number(query, 22);
                    track.fileType = number(query, 23);
                    track.filePath = query.columnText(24);
                    track.fileName = query.columnText(25);
                    track.analyzePath = query.columnText(26);
                    track.analyzeDate = query.columnText(27);
                    track.dateAdded = query.columnText(28);
                    track.releaseDate = query.columnText(29);
                    track.mixName = query.columnText(30);
                    track.comment = query.columnText(31);
                    track.isrc = query.columnText(32);
                    track.message = query.columnText(33);
                    track.autoLoadHotCues = query.columnIsNull(34) || query.columnInt(34) != 0;
                    library.tracks.append(std::move(track));
                    return true;
                })) {
        return std::nullopt;
    }

    // Map parents after all nodes have IDs. A NULL parent and 0 both mean the root.
    QList<qint64> parentIds;
    if (!reader.read(
                "SELECT playlist_id, playlist_id_parent, attribute, name, "
                "ROW_NUMBER() OVER (PARTITION BY COALESCE(playlist_id_parent, 0) "
                "ORDER BY sequenceNo, playlist_id) - 1 FROM playlist ORDER BY playlist_id",
                [&](const NamedStatement& query) {
                    library.playlists.append(LegacyPlaylistNode{
                            addId(playlists, query.columnInt(0)),
                            0,
                            number(query, 4),
                            query.columnInt(2) == 1,
                            query.columnText(3)});
                    parentIds.append(query.columnInt(1));
                    return true;
                })) {
        return std::nullopt;
    }
    for (qsizetype i = 0; i < parentIds.size(); ++i) {
        if (parentIds[i] != 0) {
            const quint32 parentId = playlists.value(parentIds[i]);
            if (parentId == 0) {
                reader.missingReference();
                return std::nullopt;
            }
            library.playlists[i].parentId = parentId;
        }
    }

    if (!reader.readEntries(
                "SELECT playlist_id, content_id FROM playlist_content "
                "ORDER BY playlist_id, sequenceNo, rowid",
                playlists,
                tracks,
                library.playlistEntries) ||
            !reader.readNamed("SELECT history_id, name FROM history ORDER BY sequenceNo, history_id",
                    library.historyPlaylists,
                    histories) ||
            !reader.readEntries(
                    "SELECT entry.history_id, entry.content_id FROM history_content AS entry "
                    "LEFT JOIN history ON history.history_id = entry.history_id "
                    "ORDER BY history.sequenceNo, entry.history_id, entry.sequenceNo, entry.rowid",
                    histories,
                    tracks,
                    library.historyEntries)) {
        return std::nullopt;
    }
    return library;
}

} // namespace mixxx::rekordbox

// End of AI-generated text.
