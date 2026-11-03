#include "library/rekordbox/devicelibraryplusschema.h"

#include <sqlite3.h>

#include <QCoreApplication>
#include <QLatin1String>

namespace mixxx {
namespace rekordbox {
namespace dlpschema {

namespace {

// Mix Two: the fixed rows of a new library. Provisional: they are the rows of
// a library on the owner's stick (design open item O7). Replace them when
// that item closes.

struct MenuItemRow {
    int id;
    int kind;
    const char* label;
};

constexpr MenuItemRow kMenuItems[] = {
        {1, 128, "GENRE"},
        {2, 129, "ARTIST"},
        {3, 130, "ALBUM"},
        {4, 131, "TRACK"},
        {5, 133, "BPM"},
        {6, 134, "RATING"},
        {7, 135, "YEAR"},
        {8, 136, "REMIXER"},
        {9, 137, "LABEL"},
        {10, 138, "ORIGINAL ARTIST"},
        {11, 139, "KEY"},
        {12, 141, "CUE"},
        {13, 142, "COLOR"},
        {14, 146, "TIME"},
        {15, 147, "BITRATE"},
        {16, 148, "FILE NAME"},
        {17, 132, "PLAYLIST"},
        {18, 152, "HOT CUE BANK"},
        {19, 149, "HISTORY"},
        {20, 145, "SEARCH"},
        {21, 150, "COMMENTS"},
        {22, 140, "DATE ADDED"},
        {23, 151, "DJ PLAY COUNT"},
        {24, 144, "FOLDER"},
        {25, 161, "DEFAULT"},
        {26, 162, "ALPHABET"},
        {27, 170, "MATCHING"},
};

constexpr const char* kColors[] = {
        "Pink",
        "Red",
        "Orange",
        "Yellow",
        "Green",
        "Aqua",
        "Blue",
        "Purple",
};

struct CategoryRow {
    int id;
    int menuItemId;
    int sequenceNo;
    int isVisible;
};

constexpr CategoryRow kCategories[] = {
        {1, 1, 0, 0},
        {2, 2, 1, 1},
        {3, 3, 2, 1},
        {4, 4, 3, 1},
        {5, 17, 5, 1},
        {6, 5, 0, 0},
        {7, 6, 0, 0},
        {8, 7, 0, 0},
        {9, 8, 0, 0},
        {10, 9, 0, 0},
        {11, 10, 0, 0},
        {12, 11, 4, 1},
        {15, 13, 0, 0},
        {17, 24, 9, 1},
        {18, 20, 7, 1},
        {19, 14, 0, 0},
        {20, 15, 0, 0},
        {21, 16, 0, 0},
        {22, 19, 6, 1},
        {23, 18, 0, 0},
        {26, 27, 8, 1},
        {27, 22, 10, 0},
};

struct SortRow {
    int id;
    int menuItemId;
    int sequenceNo;
    int isVisible;
    int isSelectedAsSubColumn;
};

constexpr SortRow kSorts[] = {
        {0, 25, 1, 1, 0},
        {1, 26, 2, 1, 0},
        {2, 2, 3, 1, 0},
        {3, 3, 4, 1, 0},
        {4, 5, 5, 1, 0},
        {5, 6, 6, 1, 0},
        {6, 1, 0, 0, 0},
        {7, 21, 0, 0, 0},
        {8, 14, 0, 0, 0},
        {9, 8, 0, 0, 0},
        {10, 9, 0, 0, 0},
        {11, 10, 0, 0, 0},
        {12, 11, 7, 1, 0},
        {13, 15, 0, 0, 0},
        {15, 13, 0, 0, 0},
        {16, 23, 0, 0, 0},
        {17, 22, 0, 0, 0},
};

/// The SQL text of all default rows: one transaction.
QString defaultRowsSql() {
    // The menu item names are in brackets of two special characters.
    const QString bracketOpen(QChar(0xFFFA));
    const QString bracketClose(QChar(0xFFFB));

    QString sql = QStringLiteral("BEGIN;\n");
    for (const MenuItemRow& row : kMenuItems) {
        sql += QStringLiteral(
                "INSERT INTO menuItem(menuItem_id, kind, name) "
                "VALUES (%1, %2, '%3%4%5');\n")
                       .arg(row.id)
                       .arg(row.kind)
                       .arg(bracketOpen, QLatin1String(row.label), bracketClose);
    }
    int colorId = 1;
    for (const char* name : kColors) {
        sql += QStringLiteral("INSERT INTO color(color_id, name) VALUES (%1, '%2');\n")
                       .arg(colorId++)
                       .arg(QLatin1String(name));
    }
    for (const CategoryRow& row : kCategories) {
        sql += QStringLiteral(
                "INSERT INTO category(category_id, menuItem_id, sequenceNo, isVisible) "
                "VALUES (%1, %2, %3, %4);\n")
                       .arg(row.id)
                       .arg(row.menuItemId)
                       .arg(row.sequenceNo)
                       .arg(row.isVisible);
    }
    for (const SortRow& row : kSorts) {
        sql += QStringLiteral(
                "INSERT INTO sort(sort_id, menuItem_id, sequenceNo, isVisible, "
                "isSelectedAsSubColumn) VALUES (%1, %2, %3, %4, %5);\n")
                       .arg(row.id)
                       .arg(row.menuItemId)
                       .arg(row.sequenceNo)
                       .arg(row.isVisible)
                       .arg(row.isSelectedAsSubColumn);
    }
    sql += QStringLiteral("COMMIT;\n");
    return sql;
}

} // namespace

const QStringList& statements() {
    // Mix Two: design section 10, verbatim (rekordbox's spelling included,
    // for example "OutFileOffsetInBlock" with a capital O).
    // clang-format off
    static const QStringList kStatements = {
        QStringLiteral(R"(CREATE TABLE album(album_id integer primary key, name varchar, artist_id integer, image_id integer, isComplation integer, nameForSearch varchar))"),
        QStringLiteral(R"(CREATE TABLE artist(artist_id integer primary key, name varchar, nameForSearch varchar))"),
        QStringLiteral(R"(CREATE TABLE category(category_id integer primary key, menuItem_id integer, sequenceNo integer, isVisible integer))"),
        QStringLiteral(R"(CREATE TABLE color(color_id integer primary key, name varchar))"),
        QStringLiteral(R"(CREATE TABLE content(content_id integer primary key, title varchar, titleForSearch varchar, subtitle varchar, bpmx100 integer, length integer, trackNo integer, discNo integer, artist_id_artist integer, artist_id_remixer integer, artist_id_originalArtist integer, artist_id_composer integer, artist_id_lyricist integer, album_id integer, genre_id integer, label_id integer, key_id integer, color_id integer, image_id integer, djComment varchar, rating integer, releaseYear integer, releaseDate varchar, dateCreated varchar, dateAdded varchar, path varchar, fileName varchar, fileSize integer, fileType integer, bitrate integer, bitDepth integer, samplingRate integer, isrc varchar, djPlayCount integer, isHotCueAutoLoadOn integer, isKuvoDeliverStatusOn integer, kuvoDeliveryComment varchar, masterDbId integer, masterContentId integer, analysisDataFilePath varchar, analysedBits integer, contentLink integer, hasModified integer, cueUpdateCount integer, analysisDataUpdateCount integer, informationUpdateCount integer))"),
        QStringLiteral(R"(CREATE TABLE cue(cue_id integer primary key, content_id integer, kind integer, colorTableIndex integer, cueComment varchar, isActiveLoop integer, beatLoopNumerator integer, beatLoopDenominator integer, inUsec integer, outUsec integer, in150FramePerSec integer, out150FramePerSec integer, inMpegFrameNumber integer, outMpegFrameNumber integer, inMpegAbs integer, outMpegAbs integer, inDecodingStartFramePosition integer, outDecodingStartFramePosition integer, inFileOffsetInBlock integer, OutFileOffsetInBlock integer, inNumberOfSampleInBlock integer, outNumberOfSampleInBlock integer))"),
        QStringLiteral(R"(CREATE TABLE genre(genre_id integer primary key, name varchar))"),
        QStringLiteral(R"(CREATE TABLE history(history_id integer primary key, sequenceNo integer, name varchar, attribute integer, history_id_parent integer))"),
        QStringLiteral(R"(CREATE TABLE history_content(history_id integer, content_id integer, sequenceNo integer))"),
        QStringLiteral(R"(CREATE TABLE hotCueBankList(hotCueBankList_id integer primary key, sequenceNo integer, name varchar, image_id integer, attribute integer, hotCueBankList_id_parent integer))"),
        QStringLiteral(R"(CREATE TABLE hotCueBankList_cue(hotCueBankList_id integer, cue_id integer, sequenceNo integer))"),
        QStringLiteral(R"(CREATE TABLE image(image_id integer primary key, path varchar))"),
        QStringLiteral(R"(CREATE TABLE key(key_id integer primary key, name varchar))"),
        QStringLiteral(R"(CREATE TABLE label(label_id integer primary key, name varchar))"),
        QStringLiteral(R"(CREATE TABLE menuItem(menuItem_id integer primary key, kind integer, name varchar))"),
        QStringLiteral(R"(CREATE TABLE myTag(myTag_id integer primary key, sequenceNo integer, name varchar, attribute integer, myTag_id_parent integer))"),
        QStringLiteral(R"(CREATE TABLE myTag_content(myTag_id integer, content_id integer))"),
        QStringLiteral(R"(CREATE TABLE playlist(playlist_id integer primary key, sequenceNo integer, name varchar, image_id integer, attribute integer, playlist_id_parent integer))"),
        QStringLiteral(R"(CREATE TABLE playlist_content(playlist_id integer, content_id integer, sequenceNo integer))"),
        QStringLiteral(R"(CREATE TABLE property(deviceName varchar, dbVersion varchar, numberOfContents integer, createdDate varchar, backGroundColorType integer, myTagMasterDBID integer))"),
        QStringLiteral(R"(CREATE TABLE recommendedLike(content_id_1 integer, content_id_2 integer, rating integer, createdDate integer))"),
        QStringLiteral(R"(CREATE TABLE sort(sort_id integer primary key, menuItem_id integer, sequenceNo integer, isVisible integer, isSelectedAsSubColumn integer))"),
        QStringLiteral(R"(CREATE INDEX index_hotCueBankList_cue_hotCueBankList_id on hotCueBankList_cue(hotCueBankList_id))"),
        QStringLiteral(R"(CREATE INDEX index_myTag_content_content_id on myTag_content(content_id))"),
        QStringLiteral(R"(CREATE INDEX index_myTag_content_myTag_id on myTag_content(myTag_id))"),
        QStringLiteral(R"(CREATE INDEX index_playlist_content_playlist_id on playlist_content(playlist_id))"),
    };
    // clang-format on
    return kStatements;
}

bool insertDefaultRows(sqlite3* pDb, QString* pErrorMessage) {
    const QByteArray sql = defaultRowsSql().toUtf8();
    char* pMessage = nullptr;
    if (sqlite3_exec(pDb, sql.constData(), nullptr, nullptr, &pMessage) != SQLITE_OK) {
        if (pErrorMessage) {
            *pErrorMessage = QCoreApplication::translate("DeviceLibraryPlusSchema",
                    "Cannot write the standard rows of the library: %1")
                                     .arg(QString::fromUtf8(pMessage ? pMessage : "unknown error"));
        }
        sqlite3_free(pMessage);
        // Nothing stays when a statement of the transaction failed. This
        // fails (and does nothing) when the transaction is no longer open.
        sqlite3_exec(pDb, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    return true;
}

} // namespace dlpschema
} // namespace rekordbox
} // namespace mixxx
