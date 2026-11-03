#include <sqlite3.h>

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <optional>

#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/devicelibraryplusdbsql.h"

namespace mixxx {
namespace rekordbox {

namespace {

using namespace dlpsql;

QString changeError(const QString& reason) {
    return QCoreApplication::translate("DeviceLibraryPlusDb", "Cannot change the library: %1")
            .arg(reason);
}

QString noContentError(qint64 contentId) {
    return QCoreApplication::translate("DeviceLibraryPlusDb",
            "The library has no track with the ID %1.")
            .arg(contentId);
}

/// The values of playlist.attribute.
constexpr int kPlaylistAttribute = 0;
constexpr int kFolderAttribute = 1;

/// The album artist in the key of the album cache for an album with no artist.
constexpr qint64 kNoAlbumArtist = -1;

/// The first column of the first row as an ID. Empty if there is no row or if
/// the statement fails; then statement.error() is set.
std::optional<qint64> firstId(NamedStatement& statement) {
    if (statement.step() != NamedStatement::Result::Row) {
        return std::nullopt;
    }
    return statement.columnInt(0);
}

/// A column of the content table, and the SQL text of the value that
/// addContent() and updateContent() write to it: a named parameter or a fixed
/// value (spec section 6).
struct ContentColumn {
    const char* column;
    const char* value;
};

// All columns of the content table except content_id, in the order of the
// table. bindContent() binds the parameters.
constexpr ContentColumn kContentColumns[] = {
        {"title", ":title"},
        {"titleForSearch", "NULL"},
        {"subtitle", "''"},
        {"bpmx100", ":bpmx100"},
        {"length", ":length"},
        {"trackNo", "0"},
        {"discNo", "0"},
        {"artist_id_artist", ":artist"},
        {"artist_id_remixer", "NULL"},
        {"artist_id_originalArtist", "NULL"},
        {"artist_id_composer", ":composer"},
        {"artist_id_lyricist", "0"},
        {"album_id", ":album"},
        {"genre_id", ":genre"},
        {"label_id", "NULL"},
        {"key_id", ":key"},
        {"color_id", ":color"},
        {"image_id", ":image"},
        {"djComment", ":djComment"},
        {"rating", ":rating"},
        {"releaseYear", ":releaseYear"},
        {"releaseDate", "''"},
        {"dateCreated", ":dateCreated"},
        {"dateAdded", ":dateAdded"},
        {"path", ":path"},
        {"fileName", ":fileName"},
        {"fileSize", ":fileSize"},
        {"fileType", ":fileType"},
        {"bitrate", ":bitrate"},
        {"bitDepth", ":bitDepth"},
        {"samplingRate", ":samplingRate"},
        {"isrc", "''"},
        {"djPlayCount", ":playCount"},
        {"isHotCueAutoLoadOn", "1"},
        {"isKuvoDeliverStatusOn", "0"},
        {"kuvoDeliveryComment", "''"},
        {"masterDbId", ":masterDbId"},
        {"masterContentId", ":masterContentId"},
        {"analysisDataFilePath", ":analysisDataFilePath"},
        {"analysedBits", "41"},
        {"contentLink", "788224"},
        {"hasModified", "0"},
        {"cueUpdateCount", "NULL"},
        {"analysisDataUpdateCount", "NULL"},
        {"informationUpdateCount", "NULL"},
};

/// SQL for a new track. SQLite gives the new row the ID MAX(content_id) + 1,
/// or 1 for an empty table.
QByteArray insertContentSql() {
    QByteArray columns;
    QByteArray values;
    for (const ContentColumn& entry : kContentColumns) {
        if (!columns.isEmpty()) {
            columns.append(", ");
            values.append(", ");
        }
        columns.append(entry.column);
        values.append(entry.value);
    }
    QByteArray sql("INSERT INTO content(");
    sql.append(columns).append(") VALUES (").append(values).append(')');
    return sql;
}

/// SQL for a track that exists. The parameter :id is the ID of the track.
QByteArray updateContentSql() {
    QByteArray assignments;
    for (const ContentColumn& entry : kContentColumns) {
        if (!assignments.isEmpty()) {
            assignments.append(", ");
        }
        assignments.append(entry.column).append(" = ").append(entry.value);
    }
    QByteArray sql("UPDATE content SET ");
    sql.append(assignments).append(" WHERE content_id = :id");
    return sql;
}

/// Binds the parameters of kContentColumns.
void bindContent(NamedStatement& statement, const DlpContent& content) {
    statement.bindText(":title", content.title);
    statement.bindInt(":bpmx100", content.bpmx100);
    statement.bindInt(":length", content.lengthSeconds);
    statement.bindOptionalInt(":artist", content.artistId);
    statement.bindOptionalInt(":composer", content.composerId);
    statement.bindOptionalInt(":album", content.albumId);
    statement.bindOptionalInt(":genre", content.genreId);
    statement.bindOptionalInt(":key", content.keyId);
    statement.bindInt(":color", content.colorId);
    statement.bindOptionalInt(":image", content.imageId);
    statement.bindText(":djComment", content.djComment);
    statement.bindInt(":rating", content.rating);
    statement.bindInt(":releaseYear", content.releaseYear);
    statement.bindText(":dateCreated", content.dateCreated);
    statement.bindText(":dateAdded", content.dateAdded);
    statement.bindText(":path", content.path);
    statement.bindText(":fileName", content.fileName);
    statement.bindInt(":fileSize", content.fileSize);
    statement.bindInt(":fileType", content.fileType);
    statement.bindInt(":bitrate", content.bitrate);
    statement.bindInt(":bitDepth", content.bitDepth);
    statement.bindInt(":samplingRate", content.samplingRate);
    statement.bindInt(":playCount", content.playCount);
    statement.bindInt(":masterDbId", content.masterDbId);
    statement.bindInt(":masterContentId", content.masterContentId);
    statement.bindText(":analysisDataFilePath", content.analysisDataFilePath);
}

} // namespace

// The typed row methods. A table with an integer primary key gives a new row
// the ID MAX(id) + 1 (1 for an empty table) when the INSERT sets no ID.

qint64 DeviceLibraryPlusDb::failChange(const QString& reason) {
    setLastError(changeError(reason));
    return -1;
}

bool DeviceLibraryPlusDb::loadNameCache(
        const char* table, const char* idColumn, NameCache& cache) {
    QByteArray sql("SELECT ");
    sql.append(idColumn).append(", name FROM \"").append(table);
    sql.append("\" ORDER BY ").append(idColumn);
    NamedStatement select(m_pDb, sql.constData());
    cache.ids.clear();
    while (true) {
        switch (select.step()) {
        case NamedStatement::Result::Row:
            // A row with no name never matches a name, also not the empty one.
            if (!select.columnIsNull(1)) {
                const QString name = select.columnText(1);
                // The first row of a name wins.
                if (!cache.ids.contains(name)) {
                    cache.ids.insert(name, select.columnInt(0));
                }
            }
            break;
        case NamedStatement::Result::Done:
            cache.loaded = true;
            return true;
        case NamedStatement::Result::Error:
            cache.ids.clear();
            failChange(select.error());
            return false;
        }
    }
}

bool DeviceLibraryPlusDb::loadAlbumCache() {
    NamedStatement select(m_pDb,
            "SELECT album_id, name, artist_id FROM album ORDER BY album_id");
    m_albumIds.clear();
    while (true) {
        switch (select.step()) {
        case NamedStatement::Result::Row:
            if (!select.columnIsNull(1)) {
                const QPair<QString, qint64> key(select.columnText(1),
                        select.columnIsNull(2) ? kNoAlbumArtist : select.columnInt(2));
                if (!m_albumIds.contains(key)) {
                    m_albumIds.insert(key, select.columnInt(0));
                }
            }
            break;
        case NamedStatement::Result::Done:
            m_albumCacheLoaded = true;
            return true;
        case NamedStatement::Result::Error:
            m_albumIds.clear();
            failChange(select.error());
            return false;
        }
    }
}

void DeviceLibraryPlusDb::clearNameCaches() {
    for (NameCache* pCache : {&m_artistCache, &m_genreCache, &m_keyCache}) {
        pCache->loaded = false;
        pCache->ids.clear();
    }
    m_albumCacheLoaded = false;
    m_albumIds.clear();
}

qint64 DeviceLibraryPlusDb::findOrAddNamed(const char* table,
        const char* idColumn,
        NameCache& cache,
        const QString& name) {
    if (!cache.loaded && !loadNameCache(table, idColumn, cache)) {
        return -1;
    }
    const auto it = cache.ids.constFind(name);
    if (it != cache.ids.constEnd()) {
        return it.value();
    }
    QByteArray addSql("INSERT INTO \"");
    addSql.append(table).append("\"(name) VALUES (:name)");
    NamedStatement add(m_pDb, addSql.constData());
    add.bindText(":name", name);
    if (add.step() != NamedStatement::Result::Done) {
        return failChange(add.error());
    }
    const qint64 id = add.insertedId();
    cache.ids.insert(name, id);
    return id;
}

qint64 DeviceLibraryPlusDb::findOrAddArtist(const QString& name) {
    return findOrAddNamed("artist", "artist_id", m_artistCache, name);
}

qint64 DeviceLibraryPlusDb::findOrAddAlbum(
        const QString& name, std::optional<qint64> artistId) {
    if (!m_albumCacheLoaded && !loadAlbumCache()) {
        return -1;
    }
    const QPair<QString, qint64> key(name, artistId.value_or(kNoAlbumArtist));
    const auto it = m_albumIds.constFind(key);
    if (it != m_albumIds.constEnd()) {
        return it.value();
    }
    NamedStatement add(m_pDb,
            "INSERT INTO album(name, artist_id, image_id, isComplation, nameForSearch) "
            "VALUES (:name, :artist, NULL, 0, NULL)");
    add.bindText(":name", name);
    add.bindOptionalInt(":artist", artistId);
    if (add.step() != NamedStatement::Result::Done) {
        return failChange(add.error());
    }
    const qint64 id = add.insertedId();
    m_albumIds.insert(key, id);
    return id;
}

qint64 DeviceLibraryPlusDb::findOrAddGenre(const QString& name) {
    return findOrAddNamed("genre", "genre_id", m_genreCache, name);
}

qint64 DeviceLibraryPlusDb::findOrAddKey(const QString& name) {
    return findOrAddNamed("key", "key_id", m_keyCache, name);
}

qint64 DeviceLibraryPlusDb::addImage(const QString& stickPath) {
    NamedStatement add(m_pDb, "INSERT INTO image(path) VALUES (:path)");
    add.bindText(":path", stickPath);
    if (add.step() != NamedStatement::Result::Done) {
        return failChange(add.error());
    }
    return add.insertedId();
}

qint64 DeviceLibraryPlusDb::addContent(const DlpContent& content) {
    NamedStatement add(m_pDb, insertContentSql().constData());
    bindContent(add, content);
    if (add.step() != NamedStatement::Result::Done) {
        return failChange(add.error());
    }
    return add.insertedId();
}

void DeviceLibraryPlusDb::updateContent(qint64 contentId, const DlpContent& content) {
    NamedStatement update(m_pDb, updateContentSql().constData());
    bindContent(update, content);
    update.bindInt(":id", contentId);
    if (update.step() != NamedStatement::Result::Done) {
        failChange(update.error());
    } else if (update.changes() == 0) {
        // The library would not have the new values of the track.
        setLastError(noContentError(contentId));
    }
}

std::optional<qint64> DeviceLibraryPlusDb::findContentByMaster(
        qint64 masterDbId, qint64 masterContentId) {
    NamedStatement find(m_pDb,
            "SELECT content_id FROM content WHERE masterDbId = :masterDbId "
            "AND masterContentId = :masterContentId ORDER BY content_id LIMIT 1");
    find.bindInt(":masterDbId", masterDbId);
    find.bindInt(":masterContentId", masterContentId);
    const auto id = firstId(find);
    if (!id && !find.error().isEmpty()) {
        failChange(find.error());
    }
    return id;
}

std::optional<qint64> DeviceLibraryPlusDb::findContentByPath(const QString& stickPath) {
    NamedStatement find(m_pDb,
            "SELECT content_id FROM content WHERE path = :path "
            "ORDER BY content_id LIMIT 1");
    find.bindText(":path", stickPath);
    const auto id = firstId(find);
    if (!id && !find.error().isEmpty()) {
        failChange(find.error());
    }
    return id;
}

qint64 DeviceLibraryPlusDb::addPlaylist(
        const QString& name, qint64 parentId, bool isFolder) {
    // The sequence number is the number of playlists and folders that have
    // the same parent. attribute: 0 for a playlist, 1 for a folder.
    NamedStatement add(m_pDb,
            "INSERT INTO playlist(sequenceNo, name, image_id, attribute, "
            "playlist_id_parent) VALUES ((SELECT COUNT(*) FROM playlist "
            "WHERE playlist_id_parent = :parent), :name, NULL, :attribute, :parent)");
    add.bindText(":name", name);
    add.bindInt(":attribute", isFolder ? kFolderAttribute : kPlaylistAttribute);
    add.bindInt(":parent", parentId);
    if (add.step() != NamedStatement::Result::Done) {
        return failChange(add.error());
    }
    return add.insertedId();
}

std::optional<qint64> DeviceLibraryPlusDb::findPlaylistRow(
        const QString& name, qint64 parentId, std::optional<int> attribute) {
    NamedStatement find(m_pDb,
            "SELECT playlist_id FROM playlist WHERE name = :name "
            "AND playlist_id_parent = :parent "
            "AND (:attribute IS NULL OR attribute = :attribute) "
            "ORDER BY playlist_id LIMIT 1");
    find.bindText(":name", name);
    find.bindInt(":parent", parentId);
    find.bindOptionalInt(":attribute", attribute);
    const auto id = firstId(find);
    if (!id && !find.error().isEmpty()) {
        failChange(find.error());
    }
    return id;
}

std::optional<qint64> DeviceLibraryPlusDb::findPlaylist(
        const QString& name, qint64 parentId) {
    return findPlaylistRow(name, parentId, std::nullopt);
}

std::optional<qint64> DeviceLibraryPlusDb::findFolder(
        const QString& name, qint64 parentId) {
    return findPlaylistRow(name, parentId, kFolderAttribute);
}

std::optional<qint64> DeviceLibraryPlusDb::findPlaylistOnly(
        const QString& name, qint64 parentId) {
    return findPlaylistRow(name, parentId, kPlaylistAttribute);
}

QList<DlpContentRef> DeviceLibraryPlusDb::listContents() {
    NamedStatement select(m_pDb,
            "SELECT content_id, path, masterDbId, masterContentId, "
            "analysisDataFilePath, image_id FROM content ORDER BY content_id");
    QList<DlpContentRef> contents;
    while (true) {
        switch (select.step()) {
        case NamedStatement::Result::Row: {
            DlpContentRef ref;
            ref.id = select.columnInt(0);
            ref.path = select.columnText(1);
            // The columns of a row that another program wrote can be NULL; a
            // NULL ID is 0.
            ref.masterDbId = select.columnInt(2);
            ref.masterContentId = select.columnInt(3);
            ref.analysisDataFilePath = select.columnText(4);
            if (!select.columnIsNull(5)) {
                ref.imageId = select.columnInt(5);
            }
            contents.append(ref);
            break;
        }
        case NamedStatement::Result::Done:
            return contents;
        case NamedStatement::Result::Error:
            failChange(select.error());
            return QList<DlpContentRef>();
        }
    }
}

QList<QPair<qint64, QString>> DeviceLibraryPlusDb::listImages() {
    NamedStatement select(m_pDb, "SELECT image_id, path FROM image ORDER BY image_id");
    QList<QPair<qint64, QString>> images;
    while (true) {
        switch (select.step()) {
        case NamedStatement::Result::Row:
            images.append(qMakePair(select.columnInt(0), select.columnText(1)));
            break;
        case NamedStatement::Result::Done:
            return images;
        case NamedStatement::Result::Error:
            failChange(select.error());
            return QList<QPair<qint64, QString>>();
        }
    }
}

void DeviceLibraryPlusDb::setPlaylistContents(
        qint64 playlistId, const QList<qint64>& contentIds) {
    // One savepoint makes the delete and all inserts one commit to the file,
    // and a failed insert leaves the old list as it was.
    Savepoint savepoint(m_pDb);
    if (!savepoint.isOpen()) {
        failChange(savepoint.error());
        return;
    }
    {
        NamedStatement clear(m_pDb, "DELETE FROM playlist_content WHERE playlist_id = :playlist");
        clear.bindInt(":playlist", playlistId);
        if (clear.step() != NamedStatement::Result::Done) {
            failChange(clear.error());
            return;
        }

        NamedStatement insert(m_pDb,
                "INSERT INTO playlist_content(playlist_id, content_id, sequenceNo) "
                "VALUES (:playlist, :content, :sequence)");
        int sequenceNo = 0;
        for (const qint64 contentId : contentIds) {
            insert.reset();
            insert.bindInt(":playlist", playlistId);
            insert.bindInt(":content", contentId);
            insert.bindInt(":sequence", ++sequenceNo);
            if (insert.step() != NamedStatement::Result::Done) {
                failChange(insert.error());
                return;
            }
        }
    }
    if (!savepoint.release()) {
        failChange(savepoint.error());
    }
}

void DeviceLibraryPlusDb::updateContentCount() {
    NamedStatement update(m_pDb,
            "UPDATE property SET numberOfContents = (SELECT COUNT(*) FROM content)");
    if (update.step() != NamedStatement::Result::Done) {
        failChange(update.error());
    }
}

} // namespace rekordbox
} // namespace mixxx
