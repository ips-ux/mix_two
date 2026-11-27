#include "library/rekordbox/rekordboxfeature.h"

#include <rekordbox_anlz.h>
#include <rekordbox_pdb.h>

#include <QDir>
#include <QFile>
#include <QMenu>
#include <QPushButton>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QFileInfo>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTextCodec>
#include <QThread>
#include <QTimer>
#include <QtConcurrentRun>
#include <QtDebug>
#include <chrono>
#include <fstream>
#include <utility>

#include "engine/engine.h"
#include "library/dao/trackschema.h"
#include "library/library.h"
#include "library/queryutil.h"
#include "library/export/legacystickdialogs.h"
#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/devicelibraryplusmapping.h"
#include "library/rekordbox/devicelibraryplusstick.h"
#include "library/rekordbox/legacyfromdlp.h"
#include "library/rekordbox/rekordboxanlz.h"
#include "library/rekordbox/rekordboxbeats.h"
#include "library/rekordbox/rekordboxconstants.h"
#include "library/rekordbox/stickplaylistdelete.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/treeitem.h"
#include "moc_rekordboxfeature.cpp"
#include "track/beats.h"
#include "track/cue.h"
#include "track/keyfactory.h"
#include "track/track.h"
#include "util/color/color.h"
#include "util/db/dbconnectionpooled.h"
#include "util/db/dbconnectionpooler.h"
#include "util/sandbox.h"
#include "waveform/waveform.h"
#include "widget/wlibrary.h"
#include "widget/wlibrarytextbrowser.h"

#define IS_RECORDBOX_DEVICE "::isRecordboxDevice::"
#define IS_NOT_RECORDBOX_DEVICE "::isNotRecordboxDevice::"

namespace {

// AI-generated (Claude Code). Start of AI-generated text.
// A Windows drive letter can arrive before its filesystem is ready: after an
// arrival event, try for about three seconds.
#if defined(Q_OS_WIN)
constexpr int kVolumeReadinessRetries = 6;
#else
constexpr int kVolumeReadinessRetries = 0;
#endif
constexpr auto kVolumeReadinessRetryDelay = std::chrono::milliseconds(500);
constexpr int kEmptyPlaylistId = 0;
// End of AI-generated text.

const QString kRekordboxLibraryTable = QStringLiteral("rekordbox_library");
const QString kRekordboxPlaylistsTable = QStringLiteral("rekordbox_playlists");
const QString kRekordboxPlaylistTracksTable = QStringLiteral("rekordbox_playlist_tracks");

const QString kPLaylistPathDelimiter = QStringLiteral("-->");

enum class IDForColor : uint8_t {
    Pink = 1,
    Red,
    Orange,
    Yellow,
    Green,
    Aqua,
    Blue,
    Purple
};

constexpr mixxx::RgbColor kColorForIDPink(0xF870F8);
constexpr mixxx::RgbColor kColorForIDRed(0xF870900);
constexpr mixxx::RgbColor kColorForIDOrange(0xF8A030);
constexpr mixxx::RgbColor kColorForIDYellow(0xF8E331);
constexpr mixxx::RgbColor kColorForIDGreen(0x1EE000);
constexpr mixxx::RgbColor kColorForIDAqua(0x16C0F8);
constexpr mixxx::RgbColor kColorForIDBlue(0x0150F8);
constexpr mixxx::RgbColor kColorForIDPurple(0x9808F8);
constexpr mixxx::RgbColor kColorForIDNoColor(0x0);

struct memory_cue_loop_t {
    mixxx::audio::FramePos startPosition;
    mixxx::audio::FramePos endPosition;
    QString comment;
    mixxx::RgbColor::optional_t color;
};

bool createLibraryTable(QSqlDatabase& database, const QString& tableName) {
    qDebug() << "Creating Rekordbox library table: " << tableName;

    QSqlQuery query(database);
    query.prepare(
            "CREATE TABLE IF NOT EXISTS " + tableName +
            " ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    rb_id INTEGER,"
            "    artist TEXT,"
            "    title TEXT,"
            "    album TEXT,"
            "    year INTEGER,"
            "    genre TEXT,"
            "    tracknumber TEXT,"
            "    location TEXT UNIQUE,"
            "    comment TEXT,"
            "    duration INTEGER,"
            "    bitrate TEXT,"
            "    bpm FLOAT,"
            "    key TEXT,"
            "    rating INTEGER,"
            "    analyze_path TEXT UNIQUE,"
            "    device TEXT,"
            "    color INTEGER"
            ");");

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }

    return true;
}

bool createPlaylistsTable(QSqlDatabase& database, const QString& tableName) {
    qDebug() << "Creating Rekordbox playlists table: " << tableName;

    QSqlQuery query(database);
    query.prepare(
            "CREATE TABLE IF NOT EXISTS " + tableName +
            " ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    name TEXT UNIQUE,"
            "    device TEXT"
            ");");

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }

    return true;
}

bool createPlaylistTracksTable(QSqlDatabase& database, const QString& tableName) {
    qDebug() << "Creating Rekordbox playlist tracks table: " << tableName;

    QSqlQuery query(database);
    query.prepare(
            "CREATE TABLE IF NOT EXISTS " + tableName +
            " ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    playlist_id INTEGER REFERENCES rekordbox_playlists(id),"
            "    track_id INTEGER REFERENCES rekordbox_library(id),"
            "    position INTEGER"
            ");");

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }

    return true;
}

bool dropTable(QSqlDatabase& database, const QString& tableName) {
    qDebug() << "Dropping Rekordbox table: " << tableName;

    QSqlQuery query(database);
    query.prepare("DROP TABLE IF EXISTS " + tableName);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }

    return true;
}

template<typename Base, typename T>
inline bool instanceof (const T* ptr) {
    return dynamic_cast<const Base*>(ptr) != nullptr;
}

QString fromUtf16LeString(const std::string& toConvert) {
    // Kaitai uses std::string as single container for all string encodings.
    return QTextCodec::codecForName("UTF-16LE")
            ->toUnicode(toConvert.data(), static_cast<int>(toConvert.length()));
}

QString fromUtf16BeString(const std::string& toConvert) {
    // Kaitai uses std::string as single container for all string encodings.
    int length = static_cast<int>(toConvert.length()) - 2; // strip off trailing nullbyte
    return QTextCodec::codecForName("UTF-16BE")->toUnicode(toConvert.data(), length);
}

// Functions getText and parseDeviceDB are roughly based on the following Java file:
// https://github.com/Deep-Symmetry/crate-digger/commit/f09fa9fc097a2a428c43245ddd542ac1370c1adc
// getText is needed because the strings in the PDB file "have a variety of obscure representations".

QString getText(rekordbox_pdb_t::device_sql_string_t* deviceString) {
    QString text;

    if (instanceof <rekordbox_pdb_t::device_sql_short_ascii_t>(deviceString->body())) {
        rekordbox_pdb_t::device_sql_short_ascii_t* shortAsciiString =
                static_cast<rekordbox_pdb_t::device_sql_short_ascii_t*>(deviceString->body());
        text = QString::fromStdString(shortAsciiString->text());
    } else if (instanceof <rekordbox_pdb_t::device_sql_long_ascii_t>(deviceString->body())) {
        rekordbox_pdb_t::device_sql_long_ascii_t* longAsciiString =
                static_cast<rekordbox_pdb_t::device_sql_long_ascii_t*>(deviceString->body());
        text = QString::fromStdString(longAsciiString->text());
    } else if (instanceof <rekordbox_pdb_t::device_sql_long_utf16le_t>(deviceString->body())) {
        rekordbox_pdb_t::device_sql_long_utf16le_t* longUtf16leString =
                static_cast<rekordbox_pdb_t::device_sql_long_utf16le_t*>(deviceString->body());
        text = fromUtf16LeString(longUtf16leString->text());
    }

    // Some strings read from Rekordbox *.PDB files contain random null characters
    // which if not removed cause Mixxx to crash when attempting to read file paths
    return text.remove(QChar('\x0'));
}

int createDevicePlaylist(QSqlDatabase& database, const QString& devicePath) {
    int playlistID = kInvalidPlaylistId;

    QSqlQuery queryInsertIntoDevicePlaylist(database);
    queryInsertIntoDevicePlaylist.prepare(
            "INSERT INTO " + kRekordboxPlaylistsTable +
            " (name, device) "
            "VALUES (:name, :device)");

    queryInsertIntoDevicePlaylist.bindValue(":name", devicePath);
    queryInsertIntoDevicePlaylist.bindValue(":device", devicePath);

    if (!queryInsertIntoDevicePlaylist.exec()) {
        LOG_FAILED_QUERY(queryInsertIntoDevicePlaylist)
                << "devicePath: " << devicePath;
        return playlistID;
    }

    QSqlQuery idQuery(database);
    idQuery.prepare("select id from " + kRekordboxPlaylistsTable + " where name=:path");
    idQuery.bindValue(":path", devicePath);

    if (!idQuery.exec()) {
        LOG_FAILED_QUERY(idQuery)
                << "devicePath: " << devicePath;
        return playlistID;
    }

    while (idQuery.next()) {
        playlistID = idQuery.value(idQuery.record().indexOf("id")).toInt();
    }

    return playlistID;
}

mixxx::RgbColor colorFromID(int colorID) {
    switch (static_cast<IDForColor>(colorID)) {
    case IDForColor::Pink:
        return kColorForIDPink;
    case IDForColor::Red:
        return kColorForIDRed;
    case IDForColor::Orange:
        return kColorForIDOrange;
    case IDForColor::Yellow:
        return kColorForIDYellow;
    case IDForColor::Green:
        return kColorForIDGreen;
    case IDForColor::Aqua:
        return kColorForIDAqua;
    case IDForColor::Blue:
        return kColorForIDBlue;
    case IDForColor::Purple:
        return kColorForIDPurple;
    }
    return kColorForIDNoColor;
}

// AI-generated (Claude Code). Start of AI-generated text.
/// The values of one track of a stick, as the app table holds them. Both the
/// legacy reader and the Device Library Plus reader fill it. The paths are
/// stick paths (from the root of the stick, with a leading '/').
struct DeviceTrackRow {
    int rbId = 0;
    QString title;
    QString artist;
    QString album;
    QString genre;
    QString year;
    QString filePath;
    float bpm = 0;
    int bitrate = 0;
    QString key;
    int duration = 0;
    int rating = 0;
    QString comment;
    QString trackNumber;
    QString analyzePath;
    int colorId = 0;
};

/// Inserts the track into the app table and into the playlist of all the
/// tracks of the device, at this position.
// End of AI-generated text.
void insertDeviceTrack(
        QSqlDatabase& database,
        const DeviceTrackRow& track,
        QSqlQuery& query,
        QSqlQuery& queryInsertIntoDevicePlaylistTracks,
        const QString& devicePath,
        const QString& device,
        int audioFilesCount) {
    const int rbID = track.rbId;
    QString anlzPath = devicePath + track.analyzePath;
    // AI-generated (Claude Code). Start of AI-generated text.
    // A stick can have the USBANLZ folders in any letter case. The search
    // runs only when the path of the database does not exist.
    if (!QFileInfo::exists(anlzPath)) {
        const QString found = mixxx::rekordbox::dlpFindPathNoCase(
                devicePath, track.analyzePath);
        if (!found.isEmpty()) {
            anlzPath = found;
        }
    }
    // End of AI-generated text.

    query.bindValue(":rb_id", rbID);
    query.bindValue(":artist", track.artist);
    query.bindValue(":title", track.title);
    query.bindValue(":album", track.album);
    query.bindValue(":genre", track.genre);
    query.bindValue(":year", track.year);
    query.bindValue(":duration", track.duration);
    query.bindValue(":location", QString(devicePath + track.filePath));
    query.bindValue(":rating", track.rating);
    query.bindValue(":comment", track.comment);
    query.bindValue(":tracknumber", track.trackNumber);
    query.bindValue(":key", track.key);
    query.bindValue(":bpm", track.bpm);
    query.bindValue(":bitrate", track.bitrate);
    query.bindValue(":analyze_path", anlzPath);
    query.bindValue(":device", device);
    query.bindValue(":color",
            mixxx::RgbColor::toQVariant(colorFromID(track.colorId)));

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }

    int trackID = -1;
    QSqlQuery finderQuery(database);
    finderQuery.prepare("select id from " + kRekordboxLibraryTable +
            " where rb_id=:rb_id and device=:device");
    finderQuery.bindValue(":rb_id", rbID);
    finderQuery.bindValue(":device", device);

    if (!finderQuery.exec()) {
        LOG_FAILED_QUERY(finderQuery)
                << "rbID:" << rbID;
    }

    if (finderQuery.next()) {
        trackID = finderQuery.value(finderQuery.record().indexOf("id")).toInt();
    }

    // Insert into device all tracks playlist
    queryInsertIntoDevicePlaylistTracks.bindValue(":track_id", trackID);
    queryInsertIntoDevicePlaylistTracks.bindValue(":position", audioFilesCount);

    if (!queryInsertIntoDevicePlaylistTracks.exec()) {
        LOG_FAILED_QUERY(queryInsertIntoDevicePlaylistTracks)
                << "trackID:" << trackID
                << "position:" << audioFilesCount;
    }
}

void insertTrack(
        QSqlDatabase& database,
        rekordbox_pdb_t::track_row_t* track,
        QSqlQuery& query,
        QSqlQuery& queryInsertIntoDevicePlaylistTracks,
        QMap<uint32_t, QString>& artistsMap,
        QMap<uint32_t, QString>& albumsMap,
        QMap<uint32_t, QString>& genresMap,
        QMap<uint32_t, QString>& keysMap,
        const QString& devicePath,
        const QString& device,
        int audioFilesCount) {
    DeviceTrackRow row;
    row.rbId = static_cast<int>(track->id());
    row.title = getText(track->title());
    row.artist = artistsMap[track->artist_id()];
    row.album = albumsMap[track->album_id()];
    row.year = QString::number(track->year());
    row.genre = genresMap[track->genre_id()];
    row.filePath = getText(track->file_path());
    row.bpm = static_cast<float>(track->tempo() / 100.0);
    row.bitrate = static_cast<int>(track->bitrate());
    row.key = keysMap[track->key_id()];
    row.duration = static_cast<int>(track->duration());
    row.rating = static_cast<int>(track->rating());
    row.comment = getText(track->comment());
    row.trackNumber = QString::number(track->track_number());
    row.analyzePath = getText(track->analyze_path());
    row.colorId = static_cast<int>(track->color_id());
    insertDeviceTrack(database,
            row,
            query,
            queryInsertIntoDevicePlaylistTracks,
            devicePath,
            device,
            audioFilesCount);
}

void buildPlaylistTree(
        QSqlDatabase& database,
        TreeItem* parent,
        uint32_t parentID,
        QMap<uint32_t, QString>& playlistNameMap,
        QMap<uint32_t, bool>& playlistIsFolderMap,
        QMap<uint32_t, QMap<uint32_t, uint32_t>>& playlistTreeMap,
        QMap<uint32_t, QMap<uint32_t, uint32_t>>& playlistTrackMap,
        const QString& playlistPath,
        const QString& device);

bool parseDeviceDB(mixxx::DbConnectionPoolPtr dbConnectionPool,
        TreeItem* deviceItem,
        const QString& dbPath,
        const std::atomic_bool& canceled) {
    const QString devicePath = deviceItem->getData().toList().at(0).toString();
    const QString& device = devicePath;
    if (canceled.load()) {
        return false;
    }

    mixxx::FileInfo fileInfo(dbPath);
    if (!Sandbox::askForAccess(&fileInfo) || canceled.load()) {
        return false;
    }
    std::ifstream ifs(dbPath.toStdString(), std::ifstream::binary);
    if (!ifs) {
        return false;
    }
    kaitai::kstream ks(&ifs);
    rekordbox_pdb_t rekordboxDB(&ks);

    // The pooler limits the lifetime all thread-local connections,
    // that should be closed immediately before exiting this function.
    const mixxx::DbConnectionPooler dbConnectionPooler(dbConnectionPool);
    QSqlDatabase database = mixxx::DbConnectionPooled(dbConnectionPool);

    //Open the database connection in this thread.
    VERIFY_OR_DEBUG_ASSERT(database.isOpen()) {
        qDebug() << "Failed to open database for Rekordbox parser."
                 << database.lastError();
        return false;
    }

    //Give thread a low priority
    QThread* thisThread = QThread::currentThread();
    thisThread->setPriority(QThread::LowPriority);

    ScopedTransaction transaction(database);

    QSqlQuery query(database);
    query.prepare("INSERT INTO " + kRekordboxLibraryTable +
            " (rb_id, artist, title, album, year,"
            "genre,comment,tracknumber,bpm, bitrate,duration, location,"
            "rating,key,analyze_path,device,color) VALUES (:rb_id, :artist, "
            ":title, :album, :year,:genre,"
            ":comment, :tracknumber,:bpm, :bitrate,:duration, :location,"
            ":rating,:key,:analyze_path,:device,:color)");

    int audioFilesCount = 0;

    // Create a playlist for all the tracks on a device
    int playlistID = createDevicePlaylist(database, devicePath);
    if (playlistID == kInvalidPlaylistId) {
        return false;
    }

    QSqlQuery queryInsertIntoDevicePlaylistTracks(database);
    queryInsertIntoDevicePlaylistTracks.prepare(
            "INSERT INTO " + kRekordboxPlaylistTracksTable +
            " (playlist_id, track_id, position) "
            "VALUES (:playlist_id, :track_id, :position)");

    queryInsertIntoDevicePlaylistTracks.bindValue(":playlist_id", playlistID);

    // There are other types of tables (eg. COLOR), these are the only ones we are
    // interested at the moment. Perhaps when/if
    // https://github.com/mixxxdj/mixxx/issues/6852
    // is completed, this can be revisited.
    // Attempt was made to also recover HISTORY
    // playlists (which are found on removable Rekordbox devices), however
    // they didn't appear to contain valid row_ref_t structures.
    constexpr int totalTables = 8;

    rekordbox_pdb_t::page_type_t tableOrder[totalTables] = {
            rekordbox_pdb_t::PAGE_TYPE_KEYS,
            rekordbox_pdb_t::PAGE_TYPE_GENRES,
            rekordbox_pdb_t::PAGE_TYPE_ARTISTS,
            rekordbox_pdb_t::PAGE_TYPE_ALBUMS,
            rekordbox_pdb_t::PAGE_TYPE_PLAYLIST_ENTRIES,
            rekordbox_pdb_t::PAGE_TYPE_TRACKS,
            rekordbox_pdb_t::PAGE_TYPE_PLAYLIST_TREE,
            rekordbox_pdb_t::PAGE_TYPE_HISTORY};

    QMap<uint32_t, QString> keysMap;
    QMap<uint32_t, QString> genresMap;
    QMap<uint32_t, QString> artistsMap;
    QMap<uint32_t, QString> albumsMap;
    QMap<uint32_t, QString> playlistNameMap;
    QMap<uint32_t, bool> playlistIsFolderMap;
    QMap<uint32_t, QMap<uint32_t, uint32_t>> playlistTreeMap;
    QMap<uint32_t, QMap<uint32_t, uint32_t>> playlistTrackMap;

    bool folderOrPlaylistFound = false;

    for (int tableOrderIndex = 0; tableOrderIndex < totalTables; tableOrderIndex++) {
        for (const auto& table : *rekordboxDB.tables()) {
            if (table->type() == tableOrder[tableOrderIndex]) {
                uint16_t lastIndex = table->last_page()->index();
                rekordbox_pdb_t::page_ref_t* currentRef = table->first_page();

                while (true) {
                    if (canceled.load()) {
                        return false;
                    }
                    rekordbox_pdb_t::page_t* page = currentRef->body();

                    if (page->is_data_page()) {
                        for (const auto& rowgroup : *page->row_groups()) {
                            for (const auto& rowRef : *rowgroup->rows()) {
                                if (rowRef->present()) {
                                    switch (tableOrder[tableOrderIndex]) {
                                    case rekordbox_pdb_t::PAGE_TYPE_KEYS: {
                                        auto* key =
                                                static_cast<rekordbox_pdb_t::key_row_t*>(
                                                        rowRef->body());
                                        keysMap[key->id()] = getText(key->name());
                                    } break;
                                    case rekordbox_pdb_t::PAGE_TYPE_GENRES: {
                                        auto* genre =
                                                static_cast<rekordbox_pdb_t::genre_row_t*>(
                                                        rowRef->body());
                                        genresMap[genre->id()] = getText(genre->name());
                                    } break;
                                    case rekordbox_pdb_t::PAGE_TYPE_ARTISTS: {
                                        auto* artist =
                                                static_cast<rekordbox_pdb_t::artist_row_t*>(
                                                        rowRef->body());
                                        artistsMap[artist->id()] = getText(artist->name());
                                    } break;
                                    case rekordbox_pdb_t::PAGE_TYPE_ALBUMS: {
                                        auto* album =
                                                static_cast<rekordbox_pdb_t::album_row_t*>(
                                                        rowRef->body());
                                        albumsMap[album->id()] = getText(album->name());
                                    } break;
                                    case rekordbox_pdb_t::PAGE_TYPE_PLAYLIST_ENTRIES: {
                                        auto* playlistEntry =
                                                static_cast<rekordbox_pdb_t::playlist_entry_row_t*>(
                                                        rowRef->body());
                                        playlistTrackMap
                                                [playlistEntry->playlist_id()]
                                                [playlistEntry->entry_index()] =
                                                        playlistEntry
                                                                ->track_id();
                                    } break;
                                    case rekordbox_pdb_t::PAGE_TYPE_TRACKS: {
                                        insertTrack(database,
                                                static_cast<rekordbox_pdb_t::track_row_t*>(
                                                        rowRef->body()),
                                                query,
                                                queryInsertIntoDevicePlaylistTracks,
                                                artistsMap,
                                                albumsMap,
                                                genresMap,
                                                keysMap,
                                                devicePath,
                                                device,
                                                audioFilesCount);

                                        audioFilesCount++;
                                    } break;
                                    case rekordbox_pdb_t::PAGE_TYPE_PLAYLIST_TREE: {
                                        auto* playlistTree =
                                                static_cast<rekordbox_pdb_t::playlist_tree_row_t*>(
                                                        rowRef->body());

                                        playlistNameMap[playlistTree->id()] =
                                                getText(playlistTree->name());
                                        playlistIsFolderMap[playlistTree
                                                                    ->id()] =
                                                playlistTree->is_folder();
                                        playlistTreeMap
                                                [playlistTree->parent_id()]
                                                [playlistTree->sort_order()] =
                                                        playlistTree->id();

                                        folderOrPlaylistFound = true;
                                    } break;
                                    default:
                                        // we currently don't handle any other
                                        // data, even though there is more.
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    if (currentRef->index() == lastIndex) {
                        break;
                    } else {
                        currentRef = page->next_page();
                    }
                }
            }
        }
    }

    ifs.close();
    if (canceled.load()) {
        return false;
    }
    if (audioFilesCount > 0 || folderOrPlaylistFound) {
        // If we have found anything, recursively build playlist/folder TreeItem children
        // for the original device TreeItem
        buildPlaylistTree(database,
                deviceItem,
                0,
                playlistNameMap,
                playlistIsFolderMap,
                playlistTreeMap,
                playlistTrackMap,
                devicePath,
                device);
    }

    qDebug() << "Found: " << audioFilesCount << " audio files in Rekordbox device " << device;

    if (canceled.load()) {
        return false;
    }
    return transaction.commit();
}

// AI-generated (Claude Code). Start of AI-generated text.
/// Fills the app tables from the Device Library Plus database of a stick, as
/// parseDeviceDB() does from export.pdb. It reads a copy of the database file
/// and never writes to the stick. Returns false and sets *pError on a read
/// error; then the transaction undoes all its rows.
bool parseDeviceLibraryPlus(mixxx::DbConnectionPoolPtr dbConnectionPool,
        TreeItem* deviceItem,
        const QString& dbPath,
        const std::atomic_bool& canceled,
        QString* pError) {
    const QString devicePath = deviceItem->getData().toList().at(0).toString();
    const QString& device = devicePath;
    if (canceled.load()) {
        return false;
    }
    mixxx::FileInfo fileInfo(dbPath);
    if (!Sandbox::askForAccess(&fileInfo)) {
        *pError = QObject::tr("Mix Two has no access to %1.").arg(dbPath);
        return false;
    }
    if (canceled.load()) {
        return false;
    }
    // open() copies the file to a temporary folder. The copy goes when pDb
    // goes. This function never calls save(), so the stick does not change.
    const auto pDb = mixxx::rekordbox::DeviceLibraryPlusDb::open(devicePath, pError);
    if (!pDb || canceled.load()) {
        return false;
    }
    const auto library = mixxx::rekordbox::legacyFromDlp(*pDb, pError);
    if (!library || canceled.load()) {
        return false;
    }

    const mixxx::DbConnectionPooler dbConnectionPooler(dbConnectionPool);
    QSqlDatabase database = mixxx::DbConnectionPooled(dbConnectionPool);
    VERIFY_OR_DEBUG_ASSERT(database.isOpen()) {
        *pError = database.lastError().text();
        return false;
    }
    QThread::currentThread()->setPriority(QThread::LowPriority);

    ScopedTransaction transaction(database);
    QSqlQuery query(database);
    query.prepare("INSERT INTO " + kRekordboxLibraryTable +
            " (rb_id, artist, title, album, year,"
            "genre,comment,tracknumber,bpm, bitrate,duration, location,"
            "rating,key,analyze_path,device,color) VALUES (:rb_id, :artist, "
            ":title, :album, :year,:genre,"
            ":comment, :tracknumber,:bpm, :bitrate,:duration, :location,"
            ":rating,:key,:analyze_path,:device,:color)");

    const int playlistID = createDevicePlaylist(database, devicePath);
    if (playlistID == kInvalidPlaylistId) {
        *pError = QObject::tr("Mix Two cannot make the playlist of the stick.");
        return false;
    }
    QSqlQuery queryInsertIntoDevicePlaylistTracks(database);
    queryInsertIntoDevicePlaylistTracks.prepare(
            "INSERT INTO " + kRekordboxPlaylistTracksTable +
            " (playlist_id, track_id, position) "
            "VALUES (:playlist_id, :track_id, :position)");
    queryInsertIntoDevicePlaylistTracks.bindValue(":playlist_id", playlistID);

    QMap<uint32_t, QString> artistsMap;
    for (const auto& row : library->artists) {
        artistsMap[row.id] = row.name;
    }
    QMap<uint32_t, QString> albumsMap;
    for (const auto& row : library->albums) {
        albumsMap[row.id] = row.name;
    }
    QMap<uint32_t, QString> genresMap;
    for (const auto& row : library->genres) {
        genresMap[row.id] = row.name;
    }
    QMap<uint32_t, QString> keysMap;
    for (const auto& row : library->keys) {
        keysMap[row.id] = row.name;
    }

    int audioFilesCount = 0;
    for (const auto& track : library->tracks) {
        if (canceled.load()) {
            return false;
        }
        DeviceTrackRow row;
        row.rbId = static_cast<int>(track.id);
        row.title = track.title;
        row.artist = artistsMap.value(track.artistId);
        row.album = albumsMap.value(track.albumId);
        row.genre = genresMap.value(track.genreId);
        row.year = QString::number(track.year);
        row.filePath = track.filePath;
        row.bpm = static_cast<float>(track.tempo / 100.0);
        row.bitrate = static_cast<int>(track.bitrate);
        row.key = keysMap.value(track.keyId);
        row.duration = static_cast<int>(track.duration);
        row.rating = static_cast<int>(track.rating);
        row.comment = track.comment;
        row.trackNumber = QString::number(track.trackNumber);
        row.analyzePath = track.analyzePath;
        row.colorId = static_cast<int>(track.colorId);
        insertDeviceTrack(database,
                row,
                query,
                queryInsertIntoDevicePlaylistTracks,
                devicePath,
                device,
                audioFilesCount);
        audioFilesCount++;
    }

    // The same maps as the legacy reader fills, so that one function builds
    // the tree of both formats.
    QMap<uint32_t, QString> playlistNameMap;
    QMap<uint32_t, bool> playlistIsFolderMap;
    QMap<uint32_t, QMap<uint32_t, uint32_t>> playlistTreeMap;
    QMap<uint32_t, QMap<uint32_t, uint32_t>> playlistTrackMap;
    for (const auto& node : library->playlists) {
        playlistNameMap[node.id] = node.name;
        playlistIsFolderMap[node.id] = node.isFolder;
        playlistTreeMap[node.parentId][node.sortOrder] = node.id;
    }
    for (const auto& entry : library->playlistEntries) {
        playlistTrackMap[entry.playlistId][entry.entryIndex] = entry.trackId;
    }
    if (canceled.load()) {
        return false;
    }
    if (audioFilesCount > 0 || !library->playlists.isEmpty()) {
        buildPlaylistTree(database,
                deviceItem,
                0,
                playlistNameMap,
                playlistIsFolderMap,
                playlistTreeMap,
                playlistTrackMap,
                devicePath,
                device);
    }
    qDebug() << "Found:" << audioFilesCount
             << "audio files in the Device Library Plus of" << device;
    if (canceled.load()) {
        return false;
    }
    if (!transaction.commit()) {
        *pError = database.lastError().text();
        return false;
    }
    return true;
}
// End of AI-generated text.

void buildPlaylistTree(
        QSqlDatabase& database,
        TreeItem* parent,
        uint32_t parentID,
        QMap<uint32_t, QString>& playlistNameMap,
        QMap<uint32_t, bool>& playlistIsFolderMap,
        QMap<uint32_t, QMap<uint32_t, uint32_t>>& playlistTreeMap,
        QMap<uint32_t, QMap<uint32_t, uint32_t>>& playlistTrackMap,
        const QString& playlistPath,
        const QString& device) {
    for (uint32_t childIndex = 0;
            childIndex < (uint32_t)playlistTreeMap[parentID].size();
            childIndex++) {
        uint32_t childID = playlistTreeMap[parentID][childIndex];
        if (childID == 0) {
            continue;
        }
        QString playlistItemName = playlistNameMap[childID];

        QString currentPath = playlistPath + kPLaylistPathDelimiter + playlistItemName;

        TreeItem* child = parent->appendChild(playlistItemName,
                QVariant(QList<QString>{currentPath, IS_NOT_RECORDBOX_DEVICE}));

        // Create a playlist for this child
        QSqlQuery queryInsertIntoPlaylist(database);
        queryInsertIntoPlaylist.prepare(
                "INSERT INTO " + kRekordboxPlaylistsTable +
                " (name, device) "
                "VALUES (:name, :device)");

        queryInsertIntoPlaylist.bindValue(":name", currentPath);
        queryInsertIntoPlaylist.bindValue(":device", device);

        if (!queryInsertIntoPlaylist.exec()) {
            LOG_FAILED_QUERY(queryInsertIntoPlaylist)
                    << "currentPath" << currentPath;
            return;
        }

        QSqlQuery idQuery(database);
        idQuery.prepare("select id from " + kRekordboxPlaylistsTable + " where name=:path");
        idQuery.bindValue(":path", currentPath);

        if (!idQuery.exec()) {
            LOG_FAILED_QUERY(idQuery)
                    << "currentPath" << currentPath;
            return;
        }

        int playlistID = kInvalidPlaylistId;
        while (idQuery.next()) {
            playlistID = idQuery.value(idQuery.record().indexOf("id")).toInt();
        }

        QSqlQuery queryInsertIntoPlaylistTracks(database);
        queryInsertIntoPlaylistTracks.prepare(
                "INSERT INTO " + kRekordboxPlaylistTracksTable +
                " (playlist_id, track_id, position) "
                "VALUES (:playlist_id, :track_id, :position)");

        if (playlistTrackMap.contains(childID)) {
            // Add playlist tracks for children
            for (uint32_t trackIndex = 1; trackIndex <=
                    static_cast<uint32_t>(playlistTrackMap[childID].size());
                    trackIndex++) {
                uint32_t rbTrackID = playlistTrackMap[childID][trackIndex];

                int trackID = -1;
                QSqlQuery finderQuery(database);
                finderQuery.prepare("select id from " + kRekordboxLibraryTable +
                        " where rb_id=:rb_id and device=:device");
                finderQuery.bindValue(":rb_id", rbTrackID);
                finderQuery.bindValue(":device", device);

                if (!finderQuery.exec()) {
                    LOG_FAILED_QUERY(finderQuery)
                            << "rbTrackID:" << rbTrackID
                            << "device:" << device;
                    return;
                }

                if (finderQuery.next()) {
                    trackID = finderQuery.value(finderQuery.record().indexOf("id")).toInt();
                }

                queryInsertIntoPlaylistTracks.bindValue(":playlist_id", playlistID);
                queryInsertIntoPlaylistTracks.bindValue(":track_id", trackID);
                queryInsertIntoPlaylistTracks.bindValue(":position", static_cast<int>(trackIndex));

                if (!queryInsertIntoPlaylistTracks.exec()) {
                    LOG_FAILED_QUERY(queryInsertIntoPlaylistTracks)
                            << "playlistID:" << playlistID
                            << "trackID:" << trackID
                            << "trackIndex:" << trackIndex;

                    return;
                }
            }
        }

        if (playlistIsFolderMap[childID]) {
            // If this child is a folder (playlists are only leaf nodes), build playlist tree for it
            buildPlaylistTree(database,
                    child,
                    childID,
                    playlistNameMap,
                    playlistIsFolderMap,
                    playlistTreeMap,
                    playlistTrackMap,
                    currentPath,
                    device);
        }
    }
}

void clearDeviceTables(const mixxx::DbConnectionPoolPtr& pool, const QString& root) {
    const mixxx::DbConnectionPooler pooler(pool);
    QSqlDatabase database = mixxx::DbConnectionPooled(pool);
    ScopedTransaction transaction(database);
    const QStringList statements{
            "DELETE FROM " + kRekordboxPlaylistTracksTable + " WHERE playlist_id IN (SELECT id FROM " +
                    kRekordboxPlaylistsTable + " WHERE device=:root)",
            "DELETE FROM " + kRekordboxPlaylistsTable + " WHERE device=:root",
            "DELETE FROM " + kRekordboxLibraryTable + " WHERE device=:root"};
    for (const auto& statement : statements) {
        QSqlQuery query(database);
        query.prepare(statement);
        query.bindValue(":root", root);
        if (!query.exec()) {
            LOG_FAILED_QUERY(query) << root;
            return;
        }
    }
    transaction.commit();
}

std::unique_ptr<TreeItem> copyPlaylistTree(const TreeItem& source) {
    auto result = std::make_unique<TreeItem>(source.getLabel(), source.getData());
    for (const auto* child : source.children()) {
        result->insertChild(result->childRows(), copyPlaylistTree(*child));
    }
    return result;
}

void setHotCue(TrackPointer track,
        mixxx::audio::FramePos startPosition,
        mixxx::audio::FramePos endPosition,
        int id,
        const QString& label,
        mixxx::RgbColor::optional_t color) {
    CuePointer pCue;
    const QList<CuePointer> cuePoints = track->getCuePoints();
    for (const CuePointer& trackCue : cuePoints) {
        if (trackCue->getHotCue() == id) {
            pCue = trackCue;
            break;
        }
    }

    mixxx::CueType type = mixxx::CueType::HotCue;
    if (endPosition.isValid()) {
        type = mixxx::CueType::Loop;
    }

    if (pCue) {
        pCue->setStartAndEndPosition(startPosition, endPosition);
    } else {
        pCue = track->createAndAddCue(
                type,
                id,
                startPosition,
                endPosition);
    }
    pCue->setLabel(label);
    if (color) {
        pCue->setColor(*color);
    }
}

// Adds a memory cue (or memory loop), or updates the one already at that
// position, so that importing a track again does not duplicate it.
void setMemoryCue(TrackPointer track,
        mixxx::audio::FramePos startPosition,
        mixxx::audio::FramePos endPosition,
        const QString& label,
        mixxx::RgbColor::optional_t color) {
    CuePointer pCue;
    const QList<CuePointer> cuePoints = track->getCuePoints();
    for (const CuePointer& trackCue : cuePoints) {
        if (trackCue->getType() != mixxx::CueType::MemoryCue) {
            continue;
        }
        const Cue::StartAndEndPositions pos = trackCue->getStartAndEndPosition();
        if (std::abs(pos.startPosition - startPosition) < 1 &&
                pos.endPosition.isValid() == endPosition.isValid()) {
            pCue = trackCue;
            break;
        }
    }

    if (pCue) {
        pCue->setStartAndEndPosition(startPosition, endPosition);
    } else {
        pCue = track->createAndAddCue(
                mixxx::CueType::MemoryCue,
                Cue::kNoHotCue,
                startPosition,
                endPosition);
    }
    pCue->setLabel(label);
    if (color) {
        pCue->setColor(*color);
    }
}

// Mix Two: the beat grid and the cues are read separately (they used to share
// one "ignoreCues" flag, which skipped the grid of tracks without an .EXT).
void readAnalyze(TrackPointer track,
        mixxx::audio::SampleRate sampleRate,
        int timingOffset,
        bool readBeatGrid,
        bool readCues,
        const QString& anlzPath) {
    if (!QFile(anlzPath).exists()) {
        return;
    }

    qDebug() << "Rekordbox ANLZ path:" << anlzPath << " for: " << track->getTitle();

    std::ifstream ifs(anlzPath.toStdString(), std::ifstream::binary);
    kaitai::kstream ks(&ifs);

    rekordbox_anlz_t anlz = rekordbox_anlz_t(&ks);

    const double sampleRateKhz = sampleRate / 1000.0;

    QList<memory_cue_loop_t> memoryCuesAndLoops;
    int lastHotCueIndex = 0;

    for (const auto& section : *anlz.sections()) {
        switch (section->fourcc()) {
        case rekordbox_anlz_t::SECTION_TAGS_BEAT_GRID: {
            if (!readBeatGrid) {
                break;
            }

            auto* beatGridTag =
                    static_cast<rekordbox_anlz_t::beat_grid_tag_t*>(
                            section->body());

            QVector<mixxx::rekordbox::GridBeat> beats;

            for (const auto& beat : *beatGridTag->beats()) {
                int time = static_cast<int>(beat->time()) - timingOffset;
                // Ensure no offset times are less than 1
                if (time < 1) {
                    time = 1;
                }
                beats << mixxx::rekordbox::GridBeat{
                        mixxx::audio::FramePos(sampleRateKhz * static_cast<double>(time)),
                        static_cast<int>(beat->tempo())};
            }

            // Mix Two: tempo segments from rekordbox's stored tempo, not one
            // per millisecond-rounded beat (that made the BPM flicker).
            const auto pBeats = mixxx::rekordbox::beatsFromGrid(
                    sampleRate,
                    beats,
                    mixxx::rekordboxconstants::beatsSubversion);
            if (pBeats) {
                track->trySetBeats(pBeats);
            }
        } break;
        case rekordbox_anlz_t::SECTION_TAGS_CUES: {
            if (!readCues) {
                break;
            }

            auto* cuesTag =
                    static_cast<rekordbox_anlz_t::cue_tag_t*>(
                            section->body());

            for (const auto& cueEntry : *cuesTag->cues()) {
                int time = static_cast<int>(cueEntry->time()) - timingOffset;
                // Ensure no offset times are less than 1
                if (time < 1) {
                    time = 1;
                }
                const auto position = mixxx::audio::FramePos(
                        sampleRateKhz * static_cast<double>(time));

                switch (cuesTag->type()) {
                case rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES: {
                    switch (cueEntry->type()) {
                    case rekordbox_anlz_t::CUE_ENTRY_TYPE_MEMORY_CUE: {
                        memory_cue_loop_t memoryCue;
                        memoryCue.startPosition = position;
                        memoryCue.endPosition = mixxx::audio::kInvalidFramePos;
                        memoryCue.color = mixxx::RgbColor::nullopt();
                        memoryCuesAndLoops << memoryCue;
                    } break;
                    case rekordbox_anlz_t::CUE_ENTRY_TYPE_LOOP: {
                        int endTime = static_cast<int>(cueEntry->loop_time()) - timingOffset;
                        // Ensure no offset times are less than 1
                        if (endTime < 1) {
                            endTime = 1;
                        }

                        memory_cue_loop_t loop;
                        loop.startPosition = position;
                        loop.endPosition = mixxx::audio::FramePos(
                                sampleRateKhz * static_cast<double>(endTime));
                        loop.color = mixxx::RgbColor::nullopt();
                        memoryCuesAndLoops << loop;
                    } break;
                    }
                } break;
                case rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES: {
                    int hotCueIndex = static_cast<int>(cueEntry->hot_cue() - 1);
                    if (hotCueIndex > lastHotCueIndex) {
                        lastHotCueIndex = hotCueIndex;
                    }
                    setHotCue(
                            track,
                            position,
                            mixxx::audio::kInvalidFramePos,
                            hotCueIndex,
                            QString(),
                            mixxx::RgbColor::nullopt());
                } break;
                }
            }
        } break;
        case rekordbox_anlz_t::SECTION_TAGS_CUES_2: {
            if (!readCues) {
                break;
            }

            auto* cuesExtendedTag =
                    static_cast<rekordbox_anlz_t::cue_extended_tag_t*>(
                            section->body());

            for (const auto& cueExtendedEntry : *cuesExtendedTag->cues()) {
                int time = static_cast<int>(cueExtendedEntry->time()) - timingOffset;
                // Ensure no offset times are less than 1
                if (time < 1) {
                    time = 1;
                }
                const auto position = mixxx::audio::FramePos(
                        sampleRateKhz * static_cast<double>(time));

                switch (cuesExtendedTag->type()) {
                case rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES: {
                    switch (cueExtendedEntry->type()) {
                    case rekordbox_anlz_t::CUE_ENTRY_TYPE_MEMORY_CUE: {
                        memory_cue_loop_t memoryCue;
                        memoryCue.startPosition = position;
                        memoryCue.endPosition = mixxx::audio::kInvalidFramePos;
                        memoryCue.comment = fromUtf16BeString(cueExtendedEntry->comment());
                        memoryCue.color = colorFromID(static_cast<int>(
                                cueExtendedEntry->color_id()));
                        memoryCuesAndLoops << memoryCue;
                    } break;
                    case rekordbox_anlz_t::CUE_ENTRY_TYPE_LOOP: {
                        int endTime =
                                static_cast<int>(
                                        cueExtendedEntry->loop_time()) -
                                timingOffset;
                        // Ensure no offset times are less than 1
                        if (endTime < 1) {
                            endTime = 1;
                        }

                        memory_cue_loop_t loop;
                        loop.startPosition = position;
                        loop.endPosition = mixxx::audio::FramePos(
                                sampleRateKhz * static_cast<double>(endTime));
                        loop.comment = fromUtf16BeString(cueExtendedEntry->comment());
                        loop.color = colorFromID(static_cast<int>(cueExtendedEntry->color_id()));
                        memoryCuesAndLoops << loop;
                    } break;
                    }
                } break;
                case rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES: {
                    int hotCueIndex = static_cast<int>(cueExtendedEntry->hot_cue() - 1);
                    if (hotCueIndex > lastHotCueIndex) {
                        lastHotCueIndex = hotCueIndex;
                    }
                    setHotCue(track,
                            position,
                            mixxx::audio::kInvalidFramePos,
                            hotCueIndex,
                            fromUtf16BeString(cueExtendedEntry->comment()),
                            mixxx::RgbColor(qRgb(
                                    static_cast<int>(
                                            cueExtendedEntry->color_red()),
                                    static_cast<int>(
                                            cueExtendedEntry->color_green()),
                                    static_cast<int>(cueExtendedEntry
                                                    ->color_blue()))));
                } break;
                }
            }
        } break;
        default:
            break;
        }
    }

    if (memoryCuesAndLoops.size() > 0) {
        std::sort(memoryCuesAndLoops.begin(),
                memoryCuesAndLoops.end(),
                [](const memory_cue_loop_t& a, const memory_cue_loop_t& b)
                        -> bool { return a.startPosition < b.startPosition; });

        bool mainCueFound = false;

        // Add memory cues and loops
        for (int memoryCueOrLoopIndex = 0;
                memoryCueOrLoopIndex < memoryCuesAndLoops.size();
                memoryCueOrLoopIndex++) {
            memory_cue_loop_t memoryCueOrLoop = memoryCuesAndLoops[memoryCueOrLoopIndex];

            if (!mainCueFound && !memoryCueOrLoop.endPosition.isValid()) {
                // The first chronological memory cue is also the cue point,
                // where CDJs and rekordbox stop when loading the track.
                track->setMainCuePosition(memoryCueOrLoop.startPosition);
                CuePointer pMainCue = track->findCueByType(mixxx::CueType::MainCue);
                pMainCue->setLabel(memoryCueOrLoop.comment);
                // Legacy ANLZ memory cues have no color.
                if (memoryCueOrLoop.color) {
                    pMainCue->setColor(*memoryCueOrLoop.color);
                }
                mainCueFound = true;
            }
            setMemoryCue(track,
                    memoryCueOrLoop.startPosition,
                    memoryCueOrLoop.endPosition,
                    memoryCueOrLoop.comment,
                    memoryCueOrLoop.color);
        }
    }
}

} // anonymous namespace

void mixxx::rekordbox::readAnalysisFiles(TrackPointer track,
        mixxx::audio::SampleRate sampleRate,
        int timingOffset,
        const QString& anlzPath) {
    const QString anlzPathExt = anlzPath.left(anlzPath.length() - 3) + "EXT";
    if (QFile(anlzPathExt).exists()) {
        // Beatgrids appear to be only correct in legacy ANLZ file
        readAnalyze(track, sampleRate, timingOffset, true, false, anlzPath);
        readAnalyze(track, sampleRate, timingOffset, false, true, anlzPathExt);
    } else {
        // Only the legacy file: it holds both the grid and the cues.
        readAnalyze(track, sampleRate, timingOffset, true, true, anlzPath);
    }
}

RekordboxPlaylistModel::RekordboxPlaylistModel(QObject* parent,
        TrackCollectionManager* trackCollectionManager,
        QSharedPointer<BaseTrackCache> trackSource)
        : BaseExternalPlaylistModel(parent,
                  trackCollectionManager,
                  "mixxx.db.model.rekordbox.playlistmodel",
                  kRekordboxPlaylistsTable,
                  kRekordboxPlaylistTracksTable,
                  trackSource) {
}

void RekordboxPlaylistModel::initSortColumnMapping() {
    // Add a bijective mapping between the SortColumnIds and column indices
    for (int i = 0; i < static_cast<int>(TrackModel::SortColumnId::IdMax); ++i) {
        m_columnIndexBySortColumnId[i] = -1;
    }

    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Artist)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ARTIST);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Title)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TITLE);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Album)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ALBUM);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::AlbumArtist)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ALBUMARTIST);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Year)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_YEAR);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Genre)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_GENRE);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Composer)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COMPOSER);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Grouping)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_GROUPING);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::TrackNumber)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TRACKNUMBER);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::FileType)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_FILETYPE);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::NativeLocation)] =
            fieldIndex(ColumnCache::COLUMN_TRACKLOCATIONSTABLE_LOCATION);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Comment)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COMMENT);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Duration)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_DURATION);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::BitRate)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_BITRATE);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Bpm)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_BPM);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::ReplayGain)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_REPLAYGAIN);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::DateTimeAdded)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_DATETIMEADDED);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::TimesPlayed)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TIMESPLAYED);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::LastPlayedAt)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_LAST_PLAYED_AT);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Rating)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_RATING);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Key)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_KEY);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Preview)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_PREVIEW);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Color)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COLOR);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::CoverArt)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Position)] =
            fieldIndex(ColumnCache::COLUMN_PLAYLISTTRACKSTABLE_POSITION);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::PlaylistDateTimeAdded)] =
            fieldIndex(ColumnCache::COLUMN_PLAYLISTTRACKSTABLE_DATETIMEADDED);

    m_sortColumnIdByColumnIndex.clear();
    for (int i = static_cast<int>(TrackModel::SortColumnId::IdMin);
            i < static_cast<int>(TrackModel::SortColumnId::IdMax);
            ++i) {
        TrackModel::SortColumnId sortColumn = static_cast<TrackModel::SortColumnId>(i);
        m_sortColumnIdByColumnIndex.insert(
                m_columnIndexBySortColumnId[static_cast<int>(sortColumn)],
                sortColumn);
    }
}

TrackPointer RekordboxPlaylistModel::getTrack(const QModelIndex& index) const {
    qDebug() << "RekordboxTrackModel::getTrack";

    TrackPointer track = BaseExternalPlaylistModel::getTrack(index);
    QString location = getFieldVariant(
            index, ColumnCache::COLUMN_TRACKLOCATIONSTABLE_LOCATION)
                               .toString();

    if (!QFile(location).exists()) {
        return track;
    }

    // The timing offsets that align cue points, loops and beatgrids with
    // Mix Two's decoders are in rekordboxTimingOffsetMs().
    const int timingOffset = mixxx::rekordbox::rekordboxTimingOffsetMs(location);

    mixxx::audio::SampleRate sampleRate = track->getSampleRate();

    QString anlzPath =
            getFieldVariant(index, ColumnCache::COLUMN_REKORDBOX_ANALYZE_PATH)
                    .toString();
    mixxx::rekordbox::readAnalysisFiles(track, sampleRate, timingOffset, anlzPath);

    // Assume that the key of the file the has been analyzed in Recordbox is correct
    // and prevent the AnalyzerKey from re-analyzing.
    // Form 5.4.3 Key format depends on the preferences option:
    // Classic: Abm,B,Ebm,F#,Bbm,Db,Fm,Ab,…,F#m,A,Dbm,E
    // Alphanumeric (Camelot): 1A,1B,2A,2B,3A,3B,4A,4B,…,11A,11B,12A,12B
    // Not reckognized: 1m, 01A
    // Earlier versions allow any format
    // Decision: We normalize the KeyText here to not write garbage to the
    // file metadata and it is unlikely to loose extra info.
    track->setKeys(KeyFactory::makeBasicKeysNormalized(
            getFieldVariant(index, ColumnCache::COLUMN_LIBRARYTABLE_KEY).toString(),
            mixxx::track::io::key::USER));

    track->setColor(mixxx::RgbColor::fromQVariant(
            getFieldVariant(index, ColumnCache::COLUMN_LIBRARYTABLE_COLOR)));

    return track;
}

bool RekordboxPlaylistModel::isColumnHiddenByDefault(int column) {
    if (column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_BITRATE)) {
        return true;
    }
    return BaseSqlTableModel::isColumnHiddenByDefault(column);
}

bool RekordboxPlaylistModel::isColumnInternal(int column) {
    return column == fieldIndex(ColumnCache::COLUMN_REKORDBOX_ANALYZE_PATH) ||
            BaseExternalPlaylistModel::isColumnInternal(column);
}

RekordboxFeature::RekordboxFeature(
        Library* pLibrary,
        UserSettingsPointer pConfig)
        : BaseExternalLibraryFeature(pLibrary, pConfig, QStringLiteral("rekordbox")),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_stickWatcher(this) {
    QString tableName = kRekordboxLibraryTable;
    QString idColumn = LIBRARYTABLE_ID;
    QStringList columns = {
            LIBRARYTABLE_ID,
            LIBRARYTABLE_ARTIST,
            LIBRARYTABLE_TITLE,
            LIBRARYTABLE_ALBUM,
            LIBRARYTABLE_YEAR,
            LIBRARYTABLE_GENRE,
            LIBRARYTABLE_TRACKNUMBER,
            TRACKLOCATIONSTABLE_LOCATION,
            LIBRARYTABLE_COMMENT,
            LIBRARYTABLE_RATING,
            LIBRARYTABLE_DURATION,
            LIBRARYTABLE_BITRATE,
            LIBRARYTABLE_BPM,
            LIBRARYTABLE_KEY,
            LIBRARYTABLE_COLOR,
            REKORDBOX_ANALYZE_PATH};

    const QStringList searchColumns = {
            LIBRARYTABLE_ARTIST,
            LIBRARYTABLE_TITLE,
            LIBRARYTABLE_ALBUM,
            LIBRARYTABLE_GENRE,
            LIBRARYTABLE_TRACKNUMBER,
            TRACKLOCATIONSTABLE_LOCATION,
            LIBRARYTABLE_COMMENT};

    m_trackSource = QSharedPointer<BaseTrackCache>::create(
            m_pTrackCollection,
            tableName,
            std::move(idColumn),
            std::move(columns),
            std::move(searchColumns),
            false);
    m_pRekordboxPlaylistModel = make_parented<RekordboxPlaylistModel>(
            this, pLibrary->trackCollectionManager(), m_trackSource);

    QSqlDatabase database = m_pTrackCollection->database();
    ScopedTransaction transaction(database);
    // Drop any leftover temporary Rekordbox database tables if they exist
    dropTable(database, kRekordboxPlaylistTracksTable);
    dropTable(database, kRekordboxPlaylistsTable);
    dropTable(database, kRekordboxLibraryTable);

    // Create new temporary Rekordbox database tables
    createLibraryTable(database, kRekordboxLibraryTable);
    createPlaylistsTable(database, kRekordboxPlaylistsTable);
    createPlaylistTracksTable(database, kRekordboxPlaylistTracksTable);
    transaction.commit();

    m_pSidebarModel->setRootItem(TreeItem::newRoot(this));
    // AI-generated (Claude Code). Start of AI-generated text.
    // Only an arrival event retries. The startup scan and a refresh probe each
    // drive letter once, so fixed, network and optical drives do not occupy
    // the thread pool for seconds.
    connect(&m_stickWatcher,
            &mixxx::rekordbox::StickWatcher::volumeAdded,
            this,
            [this](const QString& root, bool arrivalEvent) {
                probeVolume(root, arrivalEvent ? kVolumeReadinessRetries : 0);
            });
    // End of AI-generated text.
    connect(&m_stickWatcher, &mixxx::rekordbox::StickWatcher::volumeRecheckRequested,
            this, [this](const QString& root) { probeVolume(root, 0); });
    connect(&m_stickWatcher, &mixxx::rekordbox::StickWatcher::volumeRemoved,
            this, &RekordboxFeature::removeVolume);
    connect(&m_databaseJob, &QFutureWatcherBase::finished,
            this, &RekordboxFeature::onDatabaseJobFinished);
    connect(pLibrary, &Library::showTrackModel, this, [this](QAbstractItemModel* model) {
        m_modelVisible = model == m_pRekordboxPlaylistModel.get();
        if (!m_modelVisible) {
            m_pendingOpen.clear();
        }
    });
    connect(pLibrary, &Library::switchToView, this, [this](const QString& view) {
        if (view != QStringLiteral("WTrackTableView")) {
            m_modelVisible = false;
            m_pendingOpen.clear();
        }
    });
    // AI-generated (Claude Code). Start of AI-generated text.
    m_pDeleteFromStickAction = make_parented<QAction>(tr("Delete from stick"), this);
    connect(m_pDeleteFromStickAction,
            &QAction::triggered,
            this,
            &RekordboxFeature::slotDeleteFromStick);
    // End of AI-generated text.
    m_stickWatcher.start();
}

RekordboxFeature::~RekordboxFeature() {
    if (m_importCanceled) {
        m_importCanceled->store(true);
    }
    m_databaseJob.waitForFinished();

    // Drop temporary Rekordbox database tables on shutdown
    QSqlDatabase database = m_pTrackCollection->database();
    ScopedTransaction transaction(database);
    dropTable(database, kRekordboxPlaylistTracksTable);
    dropTable(database, kRekordboxPlaylistsTable);
    dropTable(database, kRekordboxLibraryTable);
    transaction.commit();
}

void RekordboxFeature::bindLibraryWidget(WLibrary* pLibraryWidget,
        KeyboardEventFilter* keyboard) {
    Q_UNUSED(keyboard);
    parented_ptr<WLibraryTextBrowser> pEdit = make_parented<WLibraryTextBrowser>(pLibraryWidget);
    pEdit->setHtml(formatRootViewHtml());
    pEdit->setOpenLinks(false);
    connect(pEdit, &WLibraryTextBrowser::anchorClicked, this, &RekordboxFeature::htmlLinkClicked);
    pLibraryWidget->registerView("REKORDBOXHOME", pEdit);
    // AI-generated (Claude Code). Start of AI-generated text.
    m_pLibraryWidget = pLibraryWidget;
    // End of AI-generated text.
}

void RekordboxFeature::htmlLinkClicked(const QUrl& link) {
    if (QString(link.path()) == "refresh") {
        m_stickWatcher.refresh();
    } else {
        qDebug() << "Unknown link clicked" << link;
    }
}

std::unique_ptr<BaseSqlTableModel>
RekordboxFeature::createPlaylistModelForPlaylist(const QVariant& data) {
    VERIFY_OR_DEBUG_ASSERT(data.canConvert<QVariantList>()) {
        return {};
    }
    QVariantList playlists = data.toList();
    VERIFY_OR_DEBUG_ASSERT(playlists.size() > 0) {
        return {};
    }
    auto pModel = std::make_unique<RekordboxPlaylistModel>(
            this, m_pLibrary->trackCollectionManager(), m_trackSource);
    pModel->setPlaylist(playlists.at(0).toString());
    return pModel;
}

QVariant RekordboxFeature::title() {
    return tr("Rekordbox");
}

bool RekordboxFeature::isSupported() {
    return true;
}

TreeItemModel* RekordboxFeature::sidebarModel() const {
    return m_pSidebarModel;
}

QString RekordboxFeature::formatRootViewHtml() const {
    QString title = tr("Rekordbox");
    QString summary = tr(
            "Reads databases exported for Pioneer CDJ / XDJ players using "
            "the Rekordbox Export mode.<br/>"
            "Rekordbox can only export to USB or SD devices with a FAT or "
            "HFS file system.<br/>"
            "Mixxx can read a database from any device that contains the "
            "database folders (<tt>PIONEER</tt> and <tt>Contents</tt>).<br/>"
            "Not supported are Rekordbox databases that have been moved to "
            "an external device via<br/>"
            "<i>Preferences > Advanced > Database management</i>.<br/>"
            "<br/>"
            "The following data is read:");

    QStringList items;

    items
            << tr("Folders")
            << tr("Playlists")
            << tr("Beatgrids")
            << tr("Hot cues")
            << tr("Memory cues")
            << tr("Loops (only the first loop is currently usable in Mixxx)");

    QString html;
    QString refreshLink = tr("Check for attached Rekordbox USB / SD devices (refresh)");
    html.append(QString("<h2>%1</h2>").arg(title));
    html.append(QString("<p>%1</p>").arg(summary));
    html.append(QString("<ul>"));
    for (const auto& item : std::as_const(items)) {
        html.append(QString("<li>%1</li>").arg(item));
    }
    html.append(QString("</ul>"));

    //Colorize links in lighter blue, instead of QT default dark blue.
    //Links are still different from regular text, but readable on dark/light backgrounds.
    //https://github.com/mixxxdj/mixxx/issues/9103
    html.append(QString("<a style=\"color:#0496FF;\" href=\"refresh\">%1</a>")
                        .arg(refreshLink));
    return html;
}

void RekordboxFeature::refreshLibraryModels() {
}

void RekordboxFeature::activate() {
    m_pendingOpen.clear();
    m_modelVisible = false;
    emit enableCoverArtDisplay(true);
    emit switchToView("REKORDBOXHOME");
    emit disableSearch();
}

void RekordboxFeature::activateChild(const QModelIndex& index) {
    if (!index.isValid()) {
        return;
    }
    const auto* item = m_pSidebarModel->getItem(index);
    if (!item || !item->getData().isValid()) {
        return;
    }
    const auto* device = item;
    while (device->parent() && device->parent()->hasParent()) {
        device = device->parent();
    }
    const auto root = device->getData().toList().at(0).toString();
    const auto data = item->getData().toList();
    if (data.at(1).toString() == IS_RECORDBOX_DEVICE) {
        m_pendingOpen = root;
        startNextDatabaseJob();
    } else {
        m_pendingOpen.clear();
        showDevicePlaylist(root, data.at(0).toString());
    }
}

// AI-generated (Claude Code). Start of AI-generated text.
void RekordboxFeature::probeVolume(const QString& root, int retriesLeft) {
    if (m_probes.contains(root)) {
        return;
    }
    auto* job = new QFutureWatcher<mixxx::rekordbox::StickFormats>(this);
    m_probes.insert(root, job);
    connect(job, &QFutureWatcherBase::finished, this, [this, job, root, retriesLeft] {
        if (m_probes.value(root) != job) {
            job->deleteLater();
            return;
        }
        const auto formats = job->result();
        if (!formats.hasLegacy() && !formats.hasDeviceLibraryPlus() && retriesLeft > 0) {
            // A drive letter can arrive before its filesystem is ready. Do not
            // occupy a worker between attempts. Removal deletes the timer context.
            QTimer::singleShot(kVolumeReadinessRetryDelay, job, [this, job, root, retriesLeft] {
                if (m_probes.value(root) == job) {
                    m_probes.remove(root);
                    probeVolume(root, retriesLeft - 1);
                }
                job->deleteLater();
            });
            return;
        }
        m_probes.remove(root);
        job->deleteLater();
#if defined(Q_OS_WIN)
        if (!formats.hasLegacy() && !formats.hasDeviceLibraryPlus()) {
            m_stickWatcher.forgetVolume(root);
            m_formats.remove(root);
            removeDevice(root);
            return;
        }
#endif
        m_formats.insert(root, formats);
        if (!formats.hasLegacy() && !formats.hasDeviceLibraryPlus()) {
            removeDevice(root);
            return;
        }
        // AI-generated (Claude Code). Start of AI-generated text.
        if (mixxx::isLegacyOnlyStick(formats.hasLegacy(), formats.hasDeviceLibraryPlus())) {
            offerLegacyStickUpdate(root);
        }
        // End of AI-generated text.
        if (deviceIndex(root).isValid()) {
            return;
        }
        std::vector<std::unique_ptr<TreeItem>> rows;
        QString label = QFileInfo(root).fileName();
        if (label.isEmpty()) {
            label = root.endsWith(QLatin1Char('/')) && root.size() > 1 ? root.chopped(1) : root;
        }
        rows.push_back(std::make_unique<TreeItem>(label,
                QVariant(QList<QString>{root, IS_RECORDBOX_DEVICE})));
        m_pSidebarModel->insertTreeItemRows(std::move(rows), m_pSidebarModel->rowCount());
    });
    job->setFuture(QtConcurrent::run([root] {
        return mixxx::rekordbox::detectStickFormats(root);
    }));
}

QModelIndex RekordboxFeature::deviceIndex(const QString& root) const {
    for (int row = 0; row < m_pSidebarModel->rowCount(); ++row) {
        const auto index = m_pSidebarModel->index(row, 0);
        if (m_pSidebarModel->getItem(index)->getData().toList().at(0).toString() == root) {
            return index;
        }
    }
    return {};
}

void RekordboxFeature::removeVolume(const QString& root) {
    if (auto* job = m_probes.take(root)) {
        job->deleteLater();
    }
    m_formats.remove(root);
    m_plusReadErrors.remove(root);
    // AI-generated (Claude Code). Start of AI-generated text.
    m_legacyUpdateOffers.remove(root);
    // End of AI-generated text.
    removeDevice(root);
}

void RekordboxFeature::removeDevice(const QString& root) {
    const auto index = deviceIndex(root);
    if (!index.isValid()) {
        return;
    }
    if (m_pendingOpen == root) {
        m_pendingOpen.clear();
    }
    if (m_modelRoot == root) {
        m_modelRoot.clear();
        // Playlist IDs start at one, so this playlist is always empty. The
        // invalid ID gives a view name that the select cannot parse.
        m_pRekordboxPlaylistModel->setPlaylistById(kEmptyPlaylistId);
        m_pRekordboxPlaylistModel->select();
        if (m_modelVisible) {
            m_modelVisible = false;
            emit enableCoverArtDisplay(true);
            emit switchToView("REKORDBOXHOME");
            emit disableSearch();
        }
    }
    if (m_importRoot == root) {
        m_importCanceled->store(true);
        m_pendingCleanup.insert(root);
    } else if (m_pSidebarModel->getItem(index)->getData().toList().at(1).toString() ==
            IS_NOT_RECORDBOX_DEVICE) {
        m_pendingCleanup.insert(root);
    }
    clearLastRightClickedIndex();
    m_pSidebarModel->removeRows(index.row(), 1);
    startNextDatabaseJob();
}

void RekordboxFeature::showDevicePlaylist(const QString& root, const QString& playlist) {
    m_modelRoot = root;
    m_pRekordboxPlaylistModel->setPlaylist(playlist);
    emit showTrackModel(m_pRekordboxPlaylistModel);
}

void RekordboxFeature::startNextDatabaseJob() {
    if (m_databaseBusy) {
        return;
    }
    const auto pool = m_pLibrary->dbConnectionPool();
    if (!m_pendingCleanup.isEmpty()) {
        const QString root = *m_pendingCleanup.begin();
        m_pendingCleanup.remove(root);
        m_databaseBusy = true;
        m_databaseJob.setFuture(QtConcurrent::run([pool, root]() -> std::shared_ptr<TreeItem> {
            clearDeviceTables(pool, root);
            return {};
        }));
        return;
    }
    const auto index = deviceIndex(m_pendingOpen);
    if (!index.isValid()) {
        return;
    }
    const auto* item = m_pSidebarModel->getItem(index);
    m_importRoot = m_pendingOpen;
    m_importCanceled = std::make_shared<std::atomic_bool>(false);
    const auto formats = m_formats.value(m_importRoot);
    m_importPlusError = std::make_shared<QString>();
    m_databaseBusy = true;
    // The worker owns a detached tree. A removal can delete the sidebar row immediately.
    auto result = std::make_shared<TreeItem>(item->getLabel(), item->getData());
    m_databaseJob.setFuture(QtConcurrent::run(
            [pool, result, formats, canceled = m_importCanceled, plusError = m_importPlusError]()
                    -> std::shared_ptr<TreeItem> {
                auto tree = result;
                try {
                    // A stick with both formats shows its Device Library Plus.
                    if (formats.hasDeviceLibraryPlus()) {
                        if (parseDeviceLibraryPlus(pool,
                                    tree.get(),
                                    formats.deviceLibraryPlusPath,
                                    *canceled,
                                    plusError.get())) {
                            return tree;
                        }
                        if (canceled->load() || !formats.hasLegacy()) {
                            return nullptr;
                        }
                        qWarning() << "Reads export.pdb, because the Device Library Plus"
                                   << "cannot be read:" << *plusError;
                        // The failed read can have added rows to its tree.
                        tree = std::make_shared<TreeItem>(tree->getLabel(), tree->getData());
                    }
                    return parseDeviceDB(pool, tree.get(), formats.legacyPath, *canceled)
                            ? tree
                            : nullptr;
                } catch (const std::exception& e) {
                    qWarning() << "Failed to load Rekordbox database:" << e.what();
                    return nullptr;
                }
            }));
}

void RekordboxFeature::onDatabaseJobFinished() {
    const auto root = std::exchange(m_importRoot, {});
    const auto result = m_databaseJob.result();
    const bool canceled = m_importCanceled && m_importCanceled->load();
    const auto plusError = std::exchange(m_importPlusError, {});
    if (!canceled && plusError && !plusError->isEmpty() && m_formats.contains(root)) {
        showPlusReadError(root, *plusError);
    }
    if (result && !canceled) {
        const auto index = deviceIndex(root);
        if (index.isValid()) {
            auto* item = m_pSidebarModel->getItem(index);
            item->setData(QVariant(QList<QString>{root, IS_NOT_RECORDBOX_DEVICE}));
            std::vector<std::unique_ptr<TreeItem>> children;
            for (const auto* child : result->children()) {
                children.push_back(copyPlaylistTree(*child));
            }
            m_pSidebarModel->insertTreeItemRows(std::move(children), 0, index);
            if (m_pendingOpen == root) {
                m_pendingOpen.clear();
                showDevicePlaylist(root, root);
            }
        }
    }
    // A new device at this root may already have a pending open.
    if (!canceled && m_pendingOpen == root) {
        m_pendingOpen.clear();
    }
    m_importCanceled.reset();
    m_databaseBusy = false;
    startNextDatabaseJob();
}

// AI-generated (Claude Code). Start of AI-generated text.
namespace {

using mixxx::rekordbox::DlpDeletePlaylistResult;
using mixxx::rekordbox::DlpRemovedTrack;
using mixxx::rekordbox::LibraryFileRef;
using mixxx::rekordbox::StickDeleteHooks;
using mixxx::rekordbox::StickDeleteOutcome;
using mixxx::rekordbox::StickDeleteRequest;
using mixxx::rekordbox::StickDeleteStatus;

/// The same key as the export: the library ID of Mix Two on a stick.
const ConfigKey kMasterDbIdConfigKey(
        QStringLiteral("[Library]"), QStringLiteral("DeviceLibraryPlusMasterDbId"));
/// The same folder as the export, below the settings folder.
const QString kDeviceBackupFolder = QStringLiteral("/device-backups");
/// The tracks that the second question names; the rest is a count.
constexpr int kMaxTracksInQuestion = 10;

/// The device item of a sidebar item.
const TreeItem* deviceItemOf(const TreeItem* item) {
    const TreeItem* device = item;
    while (device->parent() && device->parent()->hasParent()) {
        device = device->parent();
    }
    return device;
}

/// The library files with a size, for the library check. Runs on the worker
/// thread that holds a DbConnectionPooler.
mixxx::rekordbox::LibraryFilesOfSize libraryFilesOfSize(const mixxx::DbConnectionPoolPtr& pool) {
    return [pool](qint64 size) {
        QList<LibraryFileRef> files;
        QSqlDatabase database = mixxx::DbConnectionPooled(pool);
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
                "SELECT track_locations.location, track_locations.filename "
                "FROM track_locations INNER JOIN library "
                "ON library.location = track_locations.id "
                "WHERE track_locations.filesize = :size "
                "AND track_locations.fs_deleted = 0 AND library.mixxx_deleted = 0"));
        query.bindValue(QStringLiteral(":size"), size);
        if (!query.exec()) {
            LOG_FAILED_QUERY(query);
            return files;
        }
        while (query.next()) {
            files.append({query.value(0).toString(), query.value(1).toString()});
        }
        return files;
    };
}

/// Copies a file of the stick into `folder`, with a new name when the name
/// is used. Gives the path of the copy, or an empty string.
QString copyFromStick(const QString& stickFilePath, const QString& folder) {
    const QFileInfo source(stickFilePath);
    if (!source.isFile() || !QDir().mkpath(folder)) {
        return {};
    }
    const QDir dir(folder);
    QString target = dir.filePath(source.fileName());
    for (int n = 2; QFileInfo::exists(target); ++n) {
        target = dir.filePath(QStringLiteral("%1 (%2).%3")
                        .arg(source.completeBaseName(), QString::number(n), source.suffix()));
    }
    if (!QFile::copy(stickFilePath, target) || QFileInfo(target).size() != source.size()) {
        QFile::remove(target);
        return {};
    }
    return target;
}

struct StickDeleteJobResult {
    StickDeleteOutcome outcome;
    /// The copies of the stick files, for the import into the library.
    QStringList copies;
};

} // namespace

RekordboxFeature::DeleteFromStickMenuState RekordboxFeature::deleteFromStickMenuState(
        const QModelIndex& index) const {
    DeleteFromStickMenuState state;
    if (!index.isValid()) {
        return state;
    }
    const auto* item = m_pSidebarModel->getItem(index);
    if (!item || !item->getData().isValid() || !item->parent() ||
            !item->parent()->hasParent()) {
        // No item, or the item of the stick itself.
        return state;
    }
    const QString root = deviceItemOf(item)->getData().toList().value(0).toString();
    const auto data = item->getData().toList();
    if (data.value(1).toString() != IS_NOT_RECORDBOX_DEVICE ||
            !data.value(0).toString().startsWith(root + kPLaylistPathDelimiter)) {
        return state;
    }
    state.visible = true;
    if (item->childRows() > 0) {
        state.toolTip = tr("You cannot delete a folder from the stick.");
        return state;
    }
    if (!m_formats.value(root).hasDeviceLibraryPlus()) {
        state.toolTip = tr(
                "This stick has no Device Library Plus. Mix Two can delete only "
                "from a Device Library Plus.");
        return state;
    }
    if (m_stickDeleteBusy) {
        state.toolTip = tr("Mix Two deletes a playlist from a stick now.");
        return state;
    }
    state.enabled = true;
    return state;
}

void RekordboxFeature::addMenuActions(QMenu* pMenu, const QModelIndex& index) {
    const auto state = deleteFromStickMenuState(index);
    if (!state.visible) {
        return;
    }
    m_pDeleteFromStickAction->setEnabled(state.enabled);
    m_pDeleteFromStickAction->setToolTip(state.toolTip);
    pMenu->setToolTipsVisible(true);
    pMenu->addSeparator();
    pMenu->addAction(m_pDeleteFromStickAction);
}

void RekordboxFeature::slotDeleteFromStick() {
    const QModelIndex index = lastRightClickedIndex();
    if (!deleteFromStickMenuState(index).enabled) {
        return;
    }
    const auto* item = m_pSidebarModel->getItem(index);
    const auto* device = deviceItemOf(item);
    const QString root = device->getData().toList().value(0).toString();
    const QString path = item->getData().toList().value(0).toString();
    const QStringList namePath =
            path.mid(root.size() + kPLaylistPathDelimiter.size()).split(kPLaylistPathDelimiter);
    const QString label = device->getLabel();

    // Question 1, on the GUI thread. The audio has its own thread.
    if (QMessageBox::question(m_pLibraryWidget,
                tr("Delete from stick"),
                tr("Delete the playlist \"%1\" from the stick? Mix Two also deletes "
                   "the tracks that no other list on the stick uses.")
                        .arg(namePath.last()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    StickDeleteRequest request;
    request.stickRoot = root;
    request.namePath = namePath;
    request.backupBaseDir = m_pConfig->getSettingsPath() + kDeviceBackupFolder;
    // The same library ID as the export. The export makes it when it is
    // missing; so does the delete.
    request.masterDbId = m_pConfig->getValue<int>(kMasterDbIdConfigKey, 0);
    if (request.masterDbId <= 0) {
        request.masterDbId = QRandomGenerator::global()->bounded(1, 0x7fffffff);
        m_pConfig->setValue<int>(kMasterDbIdConfigKey, static_cast<int>(request.masterDbId));
        m_pConfig->save();
    }

    // The preview and the library check run on a worker thread.
    m_stickDeleteBusy = true;
    const auto pool = m_pLibrary->dbConnectionPool();
    auto* pWatcher = new QFutureWatcher<DlpDeletePlaylistResult>(this);
    connect(pWatcher, &QFutureWatcherBase::finished, this, [this, pWatcher, request, label] {
        const auto preview = pWatcher->result();
        pWatcher->deleteLater();
        continueStickDelete(request, preview, label);
    });
    pWatcher->setFuture(QtConcurrent::run([request, pool] {
        const mixxx::DbConnectionPooler pooler(pool);
        return mixxx::rekordbox::dlpPreviewDeletePlaylistByPath(request.stickRoot,
                request.masterDbId,
                request.namePath,
                mixxx::rekordbox::dlpFileNameAndSizeCheck(
                        request.stickRoot, libraryFilesOfSize(pool)));
    }));
}

void RekordboxFeature::continueStickDelete(const StickDeleteRequest& request,
        const DlpDeletePlaylistResult& preview,
        const QString& label) {
    const auto showResult = [this](QMessageBox::Icon icon, const QString& text) {
        // Not modal: the library stays usable.
        auto* pBox = new QMessageBox(
                icon, tr("Delete from stick"), text, QMessageBox::Ok, m_pLibraryWidget);
        pBox->setAttribute(Qt::WA_DeleteOnClose);
        pBox->setModal(false);
        pBox->show();
    };
    if (!preview.ok) {
        m_stickDeleteBusy = false;
        showResult(QMessageBox::Warning, preview.errorMessage);
        return;
    }

    // Question 2, when tracks that go are not in the library.
    QStringList notInLibrary;
    for (const DlpRemovedTrack& track : preview.removedTracks) {
        if (!track.inMixTwoLibrary) {
            notInLibrary.append(track.artist.isEmpty()
                            ? track.title
                            : track.artist + QStringLiteral(" - ") + track.title);
        }
    }
    bool addTracks = false;
    if (!notInLibrary.isEmpty()) {
        const int count = static_cast<int>(notInLibrary.size());
        QMessageBox box(QMessageBox::Question,
                tr("Delete from stick"),
                tr("%n track(s) of the playlist are not in the Mix Two library. "
                   "Mix Two deletes them from the stick. Add them to the library first?",
                        nullptr,
                        count),
                QMessageBox::NoButton,
                m_pLibraryWidget);
        QStringList lines = notInLibrary.mid(0, kMaxTracksInQuestion);
        if (count > kMaxTracksInQuestion) {
            lines.append(tr("%n more track(s)", nullptr, count - kMaxTracksInQuestion));
        }
        box.setInformativeText(lines.join(QLatin1Char('\n')));
        auto* pAdd = box.addButton(tr("Add the tracks to the library"), QMessageBox::AcceptRole);
        auto* pNoAdd = box.addButton(tr("Do not add"), QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(pAdd);
        box.exec();
        if (box.clickedButton() != pAdd && box.clickedButton() != pNoAdd) {
            m_stickDeleteBusy = false;
            return;
        }
        addTracks = box.clickedButton() == pAdd;
    }

    // The copies go to the first music folder of the library; the import
    // adds them as "Import Track" does.
    const QStringList musicFolders = m_pTrackCollection->getRootDirStrings();
    const QString baseFolder = musicFolders.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::MusicLocation)
            : musicFolders.first();
    const QString copyFolder = QDir(baseFolder).filePath(
            QStringLiteral("From stick/") + mixxx::rekordbox::fatSafeName(label));

    // The delete and the rewrite of export.pdb run on a worker thread.
    const auto pool = m_pLibrary->dbConnectionPool();
    auto* pWatcher = new QFutureWatcher<StickDeleteJobResult>(this);
    connect(pWatcher, &QFutureWatcherBase::finished, this, [this, pWatcher, request, showResult] {
        const auto result = pWatcher->result();
        pWatcher->deleteLater();
        m_stickDeleteBusy = false;
        if (!result.copies.isEmpty()) {
            m_pLibrary->importFiles(result.copies);
        }
        if (result.outcome.status == StickDeleteStatus::Deleted) {
            reloadStick(request.stickRoot);
        }
        showResult(result.outcome.status == StickDeleteStatus::Deleted
                        ? QMessageBox::Information
                        : QMessageBox::Warning,
                mixxx::rekordbox::stickDeleteSummary(result.outcome));
    });
    pWatcher->setFuture(QtConcurrent::run([request, pool, addTracks, copyFolder] {
        const mixxx::DbConnectionPooler pooler(pool);
        StickDeleteJobResult result;
        StickDeleteRequest checked = request;
        checked.isInLibrary = mixxx::rekordbox::dlpFileNameAndSizeCheck(
                request.stickRoot, libraryFilesOfSize(pool));
        StickDeleteHooks hooks;
        // The user answered both questions on the GUI thread.
        hooks.offerImport = [addTracks](const QList<DlpRemovedTrack>&) { return addTracks; };
        hooks.importTrack = [&result, copyFolder](
                                    const QString& stickFilePath, const DlpRemovedTrack&) {
            const QString copy = copyFromStick(stickFilePath, copyFolder);
            if (copy.isEmpty()) {
                return false;
            }
            result.copies.append(copy);
            return true;
        };
        result.outcome = mixxx::rekordbox::runStickPlaylistDelete(checked, hooks);
        return result;
    }));
}

void RekordboxFeature::reloadStick(const QString& root) {
    // The stick shows again closed; a click reads the changed library.
    removeDevice(root);
    m_formats.remove(root);
    probeVolume(root, 0);
}
// End of AI-generated text.

void RekordboxFeature::showPlusReadError(const QString& root, const QString& reason) {
    // One message for each stick while it stays connected.
    if (m_plusReadErrors.contains(root)) {
        return;
    }
    m_plusReadErrors.insert(root, reason);
    QString text = tr("Mix Two cannot read the Device Library Plus of the stick %1.").arg(root);
    if (m_formats.value(root).hasLegacy()) {
        text += QLatin1Char(' ') +
                tr("Mix Two shows the older device library (export.pdb) of the stick.");
    }
    text += QStringLiteral("\n\n") + reason;
    qWarning() << text;
    if (!m_pLibraryWidget) {
        return;
    }
    // Not modal: the tracks of the stick stay usable.
    auto* pBox = new QMessageBox(QMessageBox::Warning,
            tr("Rekordbox"),
            text,
            QMessageBox::Ok,
            m_pLibraryWidget);
    pBox->setAttribute(Qt::WA_DeleteOnClose);
    pBox->setModal(false);
    pBox->show();
}
// End of AI-generated text.

// AI-generated (Claude Code). Start of AI-generated text.
void RekordboxFeature::offerLegacyStickUpdate(const QString& root) {
    // One notice for each stick while it stays connected.
    if (m_legacyUpdateOffers.contains(root) || !m_pLibraryWidget) {
        return;
    }
    m_legacyUpdateOffers.insert(root);
    if (mixxx::legacyStickUpdateStopAsking(m_pConfig)) {
        // Later: the event loop is free again, and the probe job is gone.
        QTimer::singleShot(0, this, [this, root] { updateLegacyStick(root); });
        return;
    }
    // Not modal: the tracks of the stick stay usable.
    auto* pBox = new QMessageBox(QMessageBox::Information,
            tr("Rekordbox"),
            tr("The stick %1 has only the older device library (export.pdb). Mix Two can "
               "update the stick to Device Library Plus with the same tracks and playlists.")
                    .arg(root),
            QMessageBox::Close,
            m_pLibraryWidget);
    QPushButton* pUpdate = pBox->addButton(tr("Update stick..."), QMessageBox::ActionRole);
    pBox->setAttribute(Qt::WA_DeleteOnClose);
    pBox->setModal(false);
    connect(pUpdate, &QPushButton::clicked, this, [this, root] {
        // Start after the notice closed.
        QTimer::singleShot(0, this, [this, root] { updateLegacyStick(root); });
    });
    pBox->show();
}

void RekordboxFeature::updateLegacyStick(const QString& root) {
    // The stick can be gone, or another program can have changed it.
    if (!m_formats.contains(root)) {
        return;
    }
    const auto formats = mixxx::rekordbox::detectStickFormats(root);
    if (!mixxx::isLegacyOnlyStick(formats.hasLegacy(), formats.hasDeviceLibraryPlus())) {
        return;
    }
    const mixxx::LegacyStickFlowOutcome outcome =
            mixxx::runLegacyStickUpdate(m_pLibraryWidget, m_pConfig, root);
    if (outcome.applyCount == 0) {
        return;
    }
    if (mixxx::legacyStickUpdateStopAsking(m_pConfig) && outcome.lastResult.ok &&
            m_pLibraryWidget) {
        // An update with no question gets a short note.
        auto* pNote = new QMessageBox(QMessageBox::Information,
                tr("Rekordbox"),
                tr("Mix Two updated the stick %1 to Device Library Plus.").arg(root),
                QMessageBox::Ok,
                m_pLibraryWidget);
        pNote->setAttribute(Qt::WA_DeleteOnClose);
        pNote->setModal(false);
        pNote->show();
    }
    // Read the stick again, so that the view shows the library on it now.
    removeVolume(root);
    m_legacyUpdateOffers.insert(root);
    probeVolume(root, 0);
}
// End of AI-generated text.
