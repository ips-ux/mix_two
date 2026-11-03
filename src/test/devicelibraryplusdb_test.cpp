#include "library/rekordbox/devicelibraryplusdb.h"

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QStringList>
#include <QTemporaryDir>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <string>

#include "library/rekordbox/devicelibrarypluskey.h"
#include "library/rekordbox/devicelibraryplusschema.h"
#include "library/rekordbox/sqlciphercodec.h"
#include "test/mixxxtest.h"

namespace mixxx {
namespace rekordbox {

namespace {

const QString kDeviceName = QString::fromUtf8("MIX TWO 日本");

/// Reads a whole file. The result is empty if the file cannot be read.
QByteArray readFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

bool writeFile(const QString& path, const QByteArray& data) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(data) == data.size();
}

QByteArray sha256(const QByteArray& data) {
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

/// The relative paths of all files and folders below a folder.
QStringList treeOf(const QString& root) {
    QStringList paths;
    QDirIterator it(root,
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
            QDirIterator::Subdirectories);
    while (it.hasNext()) {
        paths.append(QDir(root).relativeFilePath(it.next()));
    }
    paths.sort();
    return paths;
}

/// A track with a different value in each column, so that a value that goes to
/// a wrong column shows in a test.
DlpContent sampleContent() {
    DlpContent content;
    content.title = QStringLiteral("Sample Title");
    content.artistId = 11;
    content.composerId = 12;
    content.albumId = 13;
    content.genreId = 14;
    content.keyId = 15;
    content.imageId = 16;
    content.colorId = 3;
    content.bpmx100 = 12850;
    content.lengthSeconds = 254;
    content.rating = 4;
    content.releaseYear = 2019;
    content.playCount = 7;
    content.djComment = QStringLiteral("Comment text");
    content.dateAdded = QStringLiteral("2026-10-01");
    content.dateCreated = QStringLiteral("2026-09-02");
    content.path = QStringLiteral("/Contents/A/X/t.mp3");
    content.fileName = QStringLiteral("t.mp3");
    // More than 32 bits.
    content.fileSize = 5000000123;
    content.fileType = 1;
    content.bitrate = 320;
    content.bitDepth = 16;
    content.samplingRate = 44100;
    content.masterDbId = 77;
    content.masterContentId = 5;
    content.analysisDataFilePath = QStringLiteral("/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT");
    return content;
}

/// A column of a table and the SQL literal that the column must have, as
/// quote() writes it: NULL, 0, '' and 'text' are different.
struct ColumnValue {
    const char* column;
    const char* literal;
};

/// Gives a file its write permission back when the test ends, also when an
/// assertion fails. QTemporaryDir cannot remove a read-only file on Windows.
class WritableAtExit {
  public:
    explicit WritableAtExit(const QString& path)
            : m_path(path) {
    }
    ~WritableAtExit() {
        QFile::setPermissions(m_path, QFile::ReadOwner | QFile::WriteOwner);
    }

  private:
    QString m_path;
};

/// The device name of the library that WritesSampleStickWhenAsked writes.
const QString kSampleDeviceName = QStringLiteral("MIX TWO SAMPLE");

/// The guard of WritesSampleStickWhenAsked: it prepares the folder for a new
/// sample stick. The test must never delete or overwrite a real library, so
/// the folder must be an empty scratch folder. The guard refuses a folder that
/// has the file export.pdb or the folder USBANLZ of a rekordbox stick. It
/// removes exportLibrary.db only if Mix Two can open the file and the device
/// name is the name of the sample, which is the library of an earlier run of
/// the test.
/// It returns an empty string if the folder can take the sample stick. If it
/// cannot, it changes nothing and returns the reason.
QString prepareSampleFolder(const QString& sampleDir) {
    const QDir dir(sampleDir);
    if (QFileInfo::exists(dir.filePath(QStringLiteral("PIONEER/rekordbox/export.pdb"))) ||
            QFileInfo::exists(dir.filePath(QStringLiteral("PIONEER/USBANLZ")))) {
        return QStringLiteral(
                "MIXTWO_DLP_SAMPLE_DIR points at a stick with a rekordbox library. "
                "Use an empty folder.");
    }

    const QString libraryFile = dir.filePath(DeviceLibraryPlusDb::kRelativePath);
    if (!QFileInfo::exists(libraryFile)) {
        return QString();
    }
    {
        // Mix Two reads the file and does not write to it.
        QString error;
        const auto pDb = DeviceLibraryPlusDb::open(sampleDir, &error);
        if (!pDb || pDb->deviceName() != kSampleDeviceName) {
            return QStringLiteral("This test does not delete a library that it did not write.");
        }
    }
    if (!QFile::remove(libraryFile)) {
        return QStringLiteral("Cannot delete the library of the earlier run: %1").arg(libraryFile);
    }
    return QString();
}

} // namespace

TEST(DeviceLibraryPlusKeyTest, MatchesPublishedPassphrase) {
    const QByteArray passphrase = deviceLibraryPlusPassphrase();
    EXPECT_EQ(64, passphrase.size());
    EXPECT_TRUE(passphrase.startsWith("r8gd"));
    EXPECT_EQ(QByteArrayLiteral("1226445ceceabd206daf103ef3bd1a0ec5ac4a328651f75728b854898f4ceac8"),
            sha256(passphrase));
}

class DeviceLibraryPlusDbTest : public MixxxTest {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_stick.isValid());
    }

    QString root() const {
        return m_stick.path();
    }

    QString stickFile() const {
        return QDir(root()).filePath(DeviceLibraryPlusDb::kRelativePath);
    }

    /// DeviceLibraryPlusDb::handle() is not public.
    static sqlite3* handleOf(DeviceLibraryPlusDb& db) {
        return db.handle();
    }

    static void setErrorOf(DeviceLibraryPlusDb& db, const QString& message) {
        db.setLastError(message);
    }

    /// Makes a new library, as create() does, but fails the test if it cannot.
    std::unique_ptr<DeviceLibraryPlusDb> createDb(qint64 masterDbId = 1234) {
        QString error;
        auto pDb = DeviceLibraryPlusDb::create(root(), kDeviceName, masterDbId, &error);
        EXPECT_TRUE(pDb) << qPrintable(error);
        return pDb;
    }

    /// Saves the library, closes it and opens it again from the stick.
    std::unique_ptr<DeviceLibraryPlusDb> saveAndReopen(std::unique_ptr<DeviceLibraryPlusDb> pDb) {
        QString error;
        EXPECT_TRUE(pDb->save(&error)) << qPrintable(error);
        pDb.reset();
        auto pOpened = DeviceLibraryPlusDb::open(root(), &error);
        EXPECT_TRUE(pOpened) << qPrintable(error);
        return pOpened;
    }

    /// Checks the columns of the row of a table that the WHERE clause gives.
    static void expectRow(sqlite3* pDb,
            const char* table,
            const char* where,
            std::initializer_list<ColumnValue> expected) {
        for (const ColumnValue& value : expected) {
            const std::string sql = std::string("SELECT quote(") + value.column + ") FROM " +
                    table + " WHERE " + where;
            EXPECT_EQ(QString::fromLatin1(value.literal), scalar(pDb, sql.c_str()))
                    << table << "." << value.column;
        }
    }

    /// Sets the time of the last change of a file.
    static bool setModifiedTime(const QString& path, const QDateTime& time) {
        QFile file(path);
        return file.open(QIODevice::ReadWrite) &&
                file.setFileTime(time, QFileDevice::FileModificationTime);
    }

    /// The IDs and sequence numbers of the tracks of a playlist, in the order
    /// of the playlist, for example "7:1,3:2". Empty for a playlist with no
    /// tracks.
    static QString playlistRows(sqlite3* pDb, qint64 playlistId) {
        const std::string sql =
                "SELECT ifnull(group_concat(content_id || ':' || sequenceNo, ','), '') FROM "
                "(SELECT content_id, sequenceNo FROM playlist_content WHERE playlist_id = " +
                std::to_string(playlistId) + " ORDER BY sequenceNo)";
        return scalar(pDb, sql.c_str());
    }

    /// Makes a library on the stick and returns its bytes.
    QByteArray createAndSave(const QString& deviceName = kDeviceName, qint64 masterDbId = 1234) {
        QString error;
        auto pDb = DeviceLibraryPlusDb::create(root(), deviceName, masterDbId, &error);
        EXPECT_TRUE(pDb) << qPrintable(error);
        if (!pDb) {
            return QByteArray();
        }
        EXPECT_TRUE(pDb->save(&error)) << qPrintable(error);
        return readFile(stickFile());
    }

    static void exec(sqlite3* pDb, const char* sql) {
        char* pMessage = nullptr;
        EXPECT_EQ(SQLITE_OK, sqlite3_exec(pDb, sql, nullptr, nullptr, &pMessage))
                << sql << ": " << (pMessage ? pMessage : "");
        sqlite3_free(pMessage);
    }

    /// The first column of all rows of a query, as text.
    static QStringList column(sqlite3* pDb, const char* sql) {
        QStringList values;
        sqlite3_stmt* pStmt = nullptr;
        EXPECT_EQ(SQLITE_OK, sqlite3_prepare_v2(pDb, sql, -1, &pStmt, nullptr))
                << sql << ": " << sqlite3_errmsg(pDb);
        while (pStmt && sqlite3_step(pStmt) == SQLITE_ROW) {
            values.append(QString::fromUtf8(
                    reinterpret_cast<const char*>(sqlite3_column_text(pStmt, 0))));
        }
        sqlite3_finalize(pStmt);
        return values;
    }

    /// The first column of the first row of a query, as text.
    static QString scalar(sqlite3* pDb, const char* sql) {
        const QStringList values = column(pDb, sql);
        return values.isEmpty() ? QStringLiteral("<no row>") : values.first();
    }

  private:
    QTemporaryDir m_stick;
};

TEST_F(DeviceLibraryPlusDbTest, CreateSaveOpen) {
    QString error;
    {
        auto pDb = DeviceLibraryPlusDb::create(root(), kDeviceName, 1234, &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        // create() works in a temporary copy; the stick has no file yet.
        EXPECT_FALSE(QFileInfo::exists(stickFile()));
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
        EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());
    }

    const QByteArray file = readFile(stickFile());
    ASSERT_FALSE(file.isEmpty());
    EXPECT_EQ(0, file.size() % sqlcipher::kPageSize);
    EXPECT_FALSE(file.startsWith("SQLite format 3"));
    // save() leaves no temporary file on the stick.
    EXPECT_EQ(QStringList() << QStringLiteral("PIONEER")
                            << QStringLiteral("PIONEER/rekordbox")
                            << DeviceLibraryPlusDb::kRelativePath,
            treeOf(root()));

    auto pDb = DeviceLibraryPlusDb::open(root(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    EXPECT_EQ(kDeviceName, pDb->deviceName());
    sqlite3* pHandle = handleOf(*pDb);

    EXPECT_EQ(dlpschema::statements(),
            column(pHandle,
                    "SELECT sql FROM sqlite_master WHERE sql IS NOT NULL ORDER BY rowid"));
    EXPECT_EQ(26, dlpschema::statements().size());

    EXPECT_EQ(QStringLiteral("27"), scalar(pHandle, "SELECT count(*) FROM menuItem"));
    EXPECT_EQ(QStringLiteral("8"), scalar(pHandle, "SELECT count(*) FROM color"));
    EXPECT_EQ(QStringLiteral("22"), scalar(pHandle, "SELECT count(*) FROM category"));
    EXPECT_EQ(QStringLiteral("17"), scalar(pHandle, "SELECT count(*) FROM sort"));
    EXPECT_EQ(QStringLiteral("1"), scalar(pHandle, "SELECT count(*) FROM property"));

    EXPECT_EQ(QStringLiteral("1000"), scalar(pHandle, "SELECT dbVersion FROM property"));
    EXPECT_EQ(QStringLiteral("0"), scalar(pHandle, "SELECT numberOfContents FROM property"));
    EXPECT_EQ(QStringLiteral("1234"), scalar(pHandle, "SELECT myTagMasterDBID FROM property"));
    EXPECT_EQ(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")),
            scalar(pHandle, "SELECT createdDate FROM property"));
    EXPECT_EQ(QStringLiteral("0"), scalar(pHandle, "SELECT backGroundColorType FROM property"));
    EXPECT_EQ(QStringLiteral("0"), scalar(pHandle, "SELECT count(*) FROM content"));
}

TEST_F(DeviceLibraryPlusDbTest, DefaultRowsHaveTheOwnerStickValues) {
    QString error;
    auto pDb = DeviceLibraryPlusDb::create(root(), kDeviceName, 1, &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    sqlite3* pHandle = handleOf(*pDb);

    // The menu item names are in the U+FFFA ... U+FFFB brackets.
    EXPECT_EQ(QString::fromUtf8("\xEF\xBF\xBA"
                                "GENRE"
                                "\xEF\xBF\xBB"),
            scalar(pHandle, "SELECT name FROM menuItem WHERE menuItem_id = 1"));
    EXPECT_EQ(QString::fromUtf8("\xEF\xBF\xBA"
                                "MATCHING"
                                "\xEF\xBF\xBB"),
            scalar(pHandle, "SELECT name FROM menuItem WHERE menuItem_id = 27"));
    EXPECT_EQ(QStringLiteral("170"),
            scalar(pHandle, "SELECT kind FROM menuItem WHERE menuItem_id = 27"));
    EXPECT_EQ(QStringLiteral("161"),
            scalar(pHandle, "SELECT kind FROM menuItem WHERE menuItem_id = 25"));

    EXPECT_EQ(QStringLiteral("Pink,Red,Orange,Yellow,Green,Aqua,Blue,Purple"),
            scalar(pHandle,
                    "SELECT group_concat(name, ',') FROM "
                    "(SELECT name FROM color ORDER BY color_id)"));

    // The visible categories, in the order of the browser.
    EXPECT_EQ(QStringLiteral("2,3,4,12,5,22,18,26,17"),
            scalar(pHandle,
                    "SELECT group_concat(category_id, ',') FROM "
                    "(SELECT category_id FROM category WHERE isVisible = 1 "
                    "ORDER BY sequenceNo)"));
    EXPECT_EQ(QStringLiteral("0,1,2,3,4,5,12"),
            scalar(pHandle,
                    "SELECT group_concat(sort_id, ',') FROM "
                    "(SELECT sort_id FROM sort WHERE isVisible = 1 ORDER BY sequenceNo)"));
    EXPECT_EQ(QStringLiteral("0"), scalar(pHandle, "SELECT sum(isSelectedAsSubColumn) FROM sort"));
}

TEST_F(DeviceLibraryPlusDbTest, SavedFileHasRekordboxSettings) {
    const QByteArray file = createAndSave();
    ASSERT_FALSE(file.isEmpty());

    QString error;
    const auto plain = sqlcipher::decrypt(file, deviceLibraryPlusPassphrase(), &error);
    ASSERT_TRUE(plain.has_value()) << qPrintable(error);
    ASSERT_GT(plain->size(), 21);
    EXPECT_EQ(0x10, static_cast<quint8>(plain->at(16)));
    EXPECT_EQ(0x00, static_cast<quint8>(plain->at(17)));
    // Journal mode WAL: file format version numbers 2 and 2.
    EXPECT_EQ(2, static_cast<quint8>(plain->at(18)));
    EXPECT_EQ(2, static_cast<quint8>(plain->at(19)));
    // 80 bytes reserved on each page.
    EXPECT_EQ(80, static_cast<quint8>(plain->at(20)));
}

TEST_F(DeviceLibraryPlusDbTest, SaveKeepsSalt) {
    const QByteArray first = createAndSave();
    ASSERT_GE(first.size(), sqlcipher::kSaltSize);
    const QByteArray salt = first.left(sqlcipher::kSaltSize);
    EXPECT_EQ(sqlcipher::kSaltSize, salt.size());

    QString error;
    {
        auto pDb = DeviceLibraryPlusDb::open(root(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    const QByteArray second = readFile(stickFile());
    ASSERT_GE(second.size(), sqlcipher::kSaltSize);
    EXPECT_EQ(salt, second.left(sqlcipher::kSaltSize));
    // The IVs are new on each save, so the pages are not the same bytes.
    EXPECT_NE(first, second);
    EXPECT_EQ(first.size(), second.size());

    // A different library gets a different salt.
    QTemporaryDir otherStick;
    ASSERT_TRUE(otherStick.isValid());
    auto pOther = DeviceLibraryPlusDb::create(otherStick.path(), kDeviceName, 1234, &error);
    ASSERT_TRUE(pOther) << qPrintable(error);
    ASSERT_TRUE(pOther->save(&error)) << qPrintable(error);
    const QByteArray other = readFile(QDir(otherStick.path())
                    .filePath(DeviceLibraryPlusDb::kRelativePath));
    EXPECT_NE(salt, other.left(sqlcipher::kSaltSize));
}

TEST_F(DeviceLibraryPlusDbTest, ChangesSurviveSaveAndOpen) {
    createAndSave();
    QString error;
    {
        auto pDb = DeviceLibraryPlusDb::open(root(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        exec(handleOf(*pDb), "INSERT INTO artist(artist_id, name) VALUES (7, 'Änderung')");
        exec(handleOf(*pDb), "UPDATE property SET numberOfContents = 3");
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    auto pDb = DeviceLibraryPlusDb::open(root(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    EXPECT_EQ(QString::fromUtf8("Änderung"),
            scalar(handleOf(*pDb), "SELECT name FROM artist WHERE artist_id = 7"));
    EXPECT_EQ(QStringLiteral("3"), scalar(handleOf(*pDb), "SELECT numberOfContents FROM property"));
}

TEST_F(DeviceLibraryPlusDbTest, SaveRefusesAfterError) {
    QString error;
    auto pDb = DeviceLibraryPlusDb::create(root(), kDeviceName, 1234, &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    setErrorOf(*pDb, QStringLiteral("Something went wrong."));
    EXPECT_EQ(QStringLiteral("Something went wrong."), pDb->lastErrorMessage());

    EXPECT_FALSE(pDb->save(&error));
    EXPECT_TRUE(error.startsWith(QStringLiteral("Cannot write the library to the stick: ")))
            << qPrintable(error);
    EXPECT_TRUE(error.contains(QStringLiteral("Something went wrong.")));
    EXPECT_FALSE(QFileInfo::exists(stickFile()));
    EXPECT_TRUE(treeOf(root()).isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, SaveReportsAStickThatCannotBeWritten) {
    // The place of the PIONEER folder is a file, so the folder cannot be made.
    ASSERT_TRUE(writeFile(QDir(root()).filePath(QStringLiteral("PIONEER")), QByteArray("x")));
    QString error;
    auto pDb = DeviceLibraryPlusDb::create(root(), kDeviceName, 1234, &error);
    // create() refuses nothing here: there is no library file.
    ASSERT_TRUE(pDb) << qPrintable(error);
    error.clear();
    EXPECT_FALSE(pDb->save(&error));
    EXPECT_TRUE(error.startsWith(QStringLiteral("Cannot write the library to the stick: ")))
            << qPrintable(error);
}

TEST_F(DeviceLibraryPlusDbTest, CreateRefusesExistingLibrary) {
    const QByteArray before = createAndSave();
    ASSERT_FALSE(before.isEmpty());

    QString error;
    auto pDb = DeviceLibraryPlusDb::create(root(), QStringLiteral("OTHER"), 99, &error);
    EXPECT_FALSE(pDb);
    EXPECT_EQ(QStringLiteral("The stick already has a Device Library Plus."), error);
    EXPECT_EQ(before, readFile(stickFile()));
}

TEST_F(DeviceLibraryPlusDbTest, OpenRefusesStickWithoutLibrary) {
    QString error;
    EXPECT_FALSE(DeviceLibraryPlusDb::open(root(), &error));
    EXPECT_EQ(QStringLiteral("The stick has no Device Library Plus."), error);
    EXPECT_TRUE(treeOf(root()).isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, OpenRefusesTruncatedFile) {
    const QByteArray file = createAndSave();
    ASSERT_GT(file.size(), 5000);
    ASSERT_TRUE(writeFile(stickFile(), file.left(5000)));

    QString error;
    EXPECT_FALSE(DeviceLibraryPlusDb::open(root(), &error));
    EXPECT_TRUE(error.contains(QStringLiteral("incorrect size"))) << qPrintable(error);
}

TEST_F(DeviceLibraryPlusDbTest, OpenRefusesChangedByte) {
    QByteArray file = createAndSave();
    ASSERT_GT(file.size(), 5000);
    file[5000] = static_cast<char>(file[5000] ^ 1);
    ASSERT_TRUE(writeFile(stickFile(), file));

    QString error;
    EXPECT_FALSE(DeviceLibraryPlusDb::open(root(), &error));
    EXPECT_TRUE(error.contains(QStringLiteral("page 2"))) << qPrintable(error);
}

TEST_F(DeviceLibraryPlusDbTest, OpenRefusesPendingWal) {
    ASSERT_FALSE(createAndSave().isEmpty());
    const QString walPath = stickFile() + QStringLiteral("-wal");
    ASSERT_TRUE(writeFile(walPath, QByteArray("x")));

    QString error;
    EXPECT_FALSE(DeviceLibraryPlusDb::open(root(), &error));
    EXPECT_TRUE(error.startsWith(QStringLiteral("The library on the stick is not complete.")))
            << qPrintable(error);

    // An empty WAL file has no changes in it.
    ASSERT_TRUE(writeFile(walPath, QByteArray()));
    error.clear();
    EXPECT_TRUE(DeviceLibraryPlusDb::open(root(), &error)) << qPrintable(error);
}

TEST_F(DeviceLibraryPlusDbTest, OpenDoesNotWriteToStick) {
    ASSERT_FALSE(createAndSave().isEmpty());
    const QString file = stickFile();
    const QByteArray bytesBefore = readFile(file);
    const QDateTime timeBefore = QFileInfo(file).lastModified();
    const QStringList treeBefore = treeOf(root());

    ASSERT_TRUE(QFile::setPermissions(file, QFile::ReadOwner));
    WritableAtExit restorePermissions(file);
    {
        QString error;
        auto pDb = DeviceLibraryPlusDb::open(root(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        // Reads of the library are possible.
        EXPECT_EQ(kDeviceName, pDb->deviceName());
        EXPECT_EQ(QStringLiteral("27"), scalar(handleOf(*pDb), "SELECT count(*) FROM menuItem"));
    }

    EXPECT_EQ(sha256(bytesBefore), sha256(readFile(file)));
    EXPECT_EQ(timeBefore, QFileInfo(file).lastModified());
    EXPECT_EQ(treeBefore, treeOf(root()));
}

TEST_F(DeviceLibraryPlusDbTest, OpenRefusesOtherVersion) {
    createAndSave();
    QString error;
    {
        auto pDb = DeviceLibraryPlusDb::open(root(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        exec(handleOf(*pDb), "UPDATE property SET dbVersion = '2000'");
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    EXPECT_FALSE(DeviceLibraryPlusDb::open(root(), &error));
    EXPECT_TRUE(error.contains(QStringLiteral("unknown version (2000)"))) << qPrintable(error);
}

TEST_F(DeviceLibraryPlusDbTest, OpenRefusesChangedSchema) {
    createAndSave();
    QString error;
    {
        auto pDb = DeviceLibraryPlusDb::open(root(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        exec(handleOf(*pDb), "ALTER TABLE content ADD COLUMN x integer");
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    EXPECT_FALSE(DeviceLibraryPlusDb::open(root(), &error));
    EXPECT_TRUE(error.contains(QStringLiteral("unknown structure"))) << qPrintable(error);
}

TEST_F(DeviceLibraryPlusDbTest, OpenRefusesDamagedLibrary) {
    QString error;
    const QByteArray file = createAndSave();
    ASSERT_FALSE(file.isEmpty());

    // The library is whole: it opens. The root page of the content table is the
    // page to damage.
    int rootPage = 0;
    {
        auto pDb = DeviceLibraryPlusDb::open(root(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        rootPage = scalar(handleOf(*pDb),
                "SELECT rootpage FROM sqlite_master WHERE name = 'content'")
                           .toInt();
    }
    ASSERT_GT(rootPage, 1);

    // Damage the page in the plain copy and encrypt it again, so that the HMAC
    // of every page is correct: only the checks of SQLite find the damage.
    auto plain = sqlcipher::decrypt(file, deviceLibraryPlusPassphrase(), &error);
    ASSERT_TRUE(plain.has_value()) << qPrintable(error);
    // The first byte of a b-tree page is its type: 2, 5, 10 or 13.
    (*plain)[(rootPage - 1) * sqlcipher::kPageSize] = static_cast<char>(0xFF);
    const auto damaged = sqlcipher::encrypt(
            *plain, deviceLibraryPlusPassphrase(), sqlcipher::saltOf(file), {}, &error);
    ASSERT_TRUE(damaged.has_value()) << qPrintable(error);
    ASSERT_TRUE(writeFile(stickFile(), *damaged));

    EXPECT_FALSE(DeviceLibraryPlusDb::open(root(), &error));
    EXPECT_EQ(QStringLiteral("The library on the stick is damaged."), error);
    EXPECT_EQ(*damaged, readFile(stickFile()));
}

TEST_F(DeviceLibraryPlusDbTest, OpenAcceptsTablesOfSqlite) {
    // ANALYZE makes the table sqlite_stat1. It is not a table of rekordbox, but
    // rekordbox or another program can make it.
    createAndSave();
    QString error;
    {
        auto pDb = DeviceLibraryPlusDb::open(root(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        exec(handleOf(*pDb), "ANALYZE");
        ASSERT_EQ(QStringLiteral("1"),
                scalar(handleOf(*pDb),
                        "SELECT count(*) FROM sqlite_master WHERE name = 'sqlite_stat1'"));
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    auto pDb = DeviceLibraryPlusDb::open(root(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    EXPECT_EQ(kDeviceName, pDb->deviceName());
}

TEST_F(DeviceLibraryPlusDbTest, OpenRefusesTableThatLooksLikeATableOfSqlite) {
    // Only the names that start with "sqlite_" are tables of SQLite. The "_" is
    // not a wildcard.
    createAndSave();
    QString error;
    {
        auto pDb = DeviceLibraryPlusDb::open(root(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        exec(handleOf(*pDb), "CREATE TABLE sqliteXother(a integer)");
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    EXPECT_FALSE(DeviceLibraryPlusDb::open(root(), &error));
    EXPECT_TRUE(error.contains(QStringLiteral("unknown structure"))) << qPrintable(error);
}

TEST_F(DeviceLibraryPlusDbTest, CreateRefusesPendingWal) {
    // A player or rekordbox left changes in the WAL file of a stick that has no
    // library file (yet). A new library would hide them.
    ASSERT_TRUE(QDir(root()).mkpath(QStringLiteral("PIONEER/rekordbox")));
    const QString walPath = stickFile() + QStringLiteral("-wal");
    ASSERT_TRUE(writeFile(walPath, QByteArray("x")));
    const QStringList treeBefore = treeOf(root());

    QString error;
    EXPECT_FALSE(DeviceLibraryPlusDb::create(root(), kDeviceName, 1234, &error));
    EXPECT_EQ(QStringLiteral("The library on the stick is not complete. Put the stick in the "
                             "player or in rekordbox again, then eject it correctly."),
            error);
    EXPECT_EQ(treeBefore, treeOf(root()));
    EXPECT_EQ(QByteArray("x"), readFile(walPath));

    // An empty WAL file has no changes in it.
    ASSERT_TRUE(writeFile(walPath, QByteArray()));
    error.clear();
    EXPECT_TRUE(DeviceLibraryPlusDb::create(root(), kDeviceName, 1234, &error))
            << qPrintable(error);
}

TEST_F(DeviceLibraryPlusDbTest, SaveRefusesPendingWal) {
    const QString incomplete =
            QStringLiteral(
                    "The library on the stick is not complete. Put the stick in the "
                    "player or in rekordbox again, then eject it correctly.");
    QString error;

    // A WAL file that comes after create().
    auto pCreated = createDb();
    ASSERT_TRUE(pCreated);
    ASSERT_TRUE(QDir(root()).mkpath(QStringLiteral("PIONEER/rekordbox")));
    const QString walPath = stickFile() + QStringLiteral("-wal");
    ASSERT_TRUE(writeFile(walPath, QByteArray("x")));
    EXPECT_FALSE(pCreated->save(&error));
    EXPECT_EQ(incomplete, error);
    EXPECT_FALSE(QFileInfo::exists(stickFile()));
    EXPECT_EQ(QByteArray("x"), readFile(walPath));
    pCreated.reset();
    ASSERT_TRUE(QFile::remove(walPath));

    // A WAL file that comes after open().
    ASSERT_FALSE(createAndSave().isEmpty());
    auto pOpened = DeviceLibraryPlusDb::open(root(), &error);
    ASSERT_TRUE(pOpened) << qPrintable(error);
    const QByteArray before = readFile(stickFile());
    ASSERT_TRUE(writeFile(walPath, QByteArray("x")));
    error.clear();
    EXPECT_FALSE(pOpened->save(&error));
    EXPECT_EQ(incomplete, error);
    EXPECT_EQ(before, readFile(stickFile()));
}

TEST_F(DeviceLibraryPlusDbTest, SaveRefusesFileChangedAfterOpen) {
    const QByteArray original = createAndSave();
    ASSERT_GT(original.size(), 5000);
    const QDateTime originalTime = QDateTime::fromSecsSinceEpoch(1772600767);
    const QString message = QStringLiteral(
            "The library on the stick changed after Mix Two read it. Read the stick again.");

    // Puts the original library on the stick, opens it, makes the change, and
    // checks that save() refuses and does not write.
    const auto expectRefused = [&](const char* what,
                                       const std::function<void()>& change,
                                       const std::function<QByteArray()>& expectedAfter) {
        SCOPED_TRACE(what);
        ASSERT_TRUE(writeFile(stickFile(), original));
        ASSERT_TRUE(setModifiedTime(stickFile(), originalTime));
        QString error;
        auto pDb = DeviceLibraryPlusDb::open(root(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);

        change();
        const QStringList treeBefore = treeOf(root());
        EXPECT_FALSE(pDb->save(&error));
        EXPECT_EQ(message, error);
        EXPECT_EQ(expectedAfter(), readFile(stickFile()));
        EXPECT_EQ(treeBefore, treeOf(root()));
    };

    // Only the bytes are different: the size and the time are the same.
    QByteArray flipped = original;
    flipped[5000] = static_cast<char>(flipped[5000] ^ 1);
    expectRefused(
            "same size and time, other bytes",
            [&] {
                ASSERT_TRUE(writeFile(stickFile(), flipped));
                ASSERT_TRUE(setModifiedTime(stickFile(), originalTime));
            },
            [&] { return flipped; });

    // Only the size is different.
    const QByteArray shorter = original.left(original.size() - sqlcipher::kPageSize);
    expectRefused(
            "other size",
            [&] {
                ASSERT_TRUE(writeFile(stickFile(), shorter));
                ASSERT_TRUE(setModifiedTime(stickFile(), originalTime));
            },
            [&] { return shorter; });

    // Only the time is different: the bytes are the same.
    expectRefused(
            "other time",
            [&] { ASSERT_TRUE(setModifiedTime(stickFile(), originalTime.addSecs(11))); },
            [&] { return original; });

    // The file is gone.
    expectRefused(
            "no file",
            [&] { ASSERT_TRUE(QFile::remove(stickFile())); },
            [&] { return QByteArray(); });
}

TEST_F(DeviceLibraryPlusDbTest, SaveRefusesFileCreatedAfterCreate) {
    QString error;
    auto pDb = createDb();
    ASSERT_TRUE(pDb);

    // Another program (rekordbox, for example) writes a library after create().
    ASSERT_TRUE(QDir(root()).mkpath(QStringLiteral("PIONEER/rekordbox")));
    const QByteArray other("the library of another program");
    ASSERT_TRUE(writeFile(stickFile(), other));
    const QStringList treeBefore = treeOf(root());

    EXPECT_FALSE(pDb->save(&error));
    EXPECT_EQ(QStringLiteral("The library on the stick changed after Mix Two read it. "
                             "Read the stick again."),
            error);
    EXPECT_EQ(other, readFile(stickFile()));
    EXPECT_EQ(treeBefore, treeOf(root()));
}

TEST_F(DeviceLibraryPlusDbTest, SaveRefusesMissingStickRoot) {
    const QString message = QStringLiteral("The stick folder is not available: %1");
    QString error;

    // After create().
    const QString createdRoot = QDir(root()).filePath(QStringLiteral("created"));
    ASSERT_TRUE(QDir().mkpath(createdRoot));
    auto pCreated = DeviceLibraryPlusDb::create(createdRoot, kDeviceName, 1234, &error);
    ASSERT_TRUE(pCreated) << qPrintable(error);
    ASSERT_TRUE(QDir(createdRoot).removeRecursively());
    EXPECT_FALSE(pCreated->save(&error));
    EXPECT_EQ(message.arg(createdRoot), error);
    // save() does not make the stick folder again.
    EXPECT_FALSE(QFileInfo::exists(createdRoot));

    // After open().
    const QString openedRoot = QDir(root()).filePath(QStringLiteral("opened"));
    ASSERT_TRUE(QDir().mkpath(openedRoot));
    {
        auto pDb = DeviceLibraryPlusDb::create(openedRoot, kDeviceName, 1234, &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    auto pOpened = DeviceLibraryPlusDb::open(openedRoot, &error);
    ASSERT_TRUE(pOpened) << qPrintable(error);
    ASSERT_TRUE(QDir(openedRoot).removeRecursively());
    error.clear();
    EXPECT_FALSE(pOpened->save(&error));
    EXPECT_EQ(message.arg(openedRoot), error);
    EXPECT_FALSE(QFileInfo::exists(openedRoot));
}

TEST_F(DeviceLibraryPlusDbTest, RejectsRelativeOrEmptyRoot) {
    const QString message = QStringLiteral("The stick folder is not available: %1");
    ASSERT_FALSE(createAndSave().isEmpty());

    const QString missing = QDir(root()).filePath(QStringLiteral("missing"));
    const QString aFile = QDir(root()).filePath(QStringLiteral("a_file"));
    ASSERT_TRUE(writeFile(aFile, QByteArray("x")));
    const QStringList treeBefore = treeOf(root());

    const QStringList badRoots = {
            QString(),
            QStringLiteral("."),
            QStringLiteral(".."),
            QStringLiteral("stick"),
            QStringLiteral("PIONEER/rekordbox"),
            // A drive without a folder is not an absolute path.
            QStringLiteral("C:"),
            QStringLiteral("C:stick"),
            // An absolute path that is not a folder that exists.
            missing,
            aFile,
    };
    for (const QString& badRoot : badRoots) {
        SCOPED_TRACE(qPrintable(badRoot));
        QString error;
        EXPECT_FALSE(DeviceLibraryPlusDb::create(badRoot, kDeviceName, 1234, &error));
        EXPECT_EQ(message.arg(badRoot), error);
        error.clear();
        EXPECT_FALSE(DeviceLibraryPlusDb::open(badRoot, &error));
        EXPECT_EQ(message.arg(badRoot), error);
    }
    EXPECT_FALSE(QFileInfo::exists(missing));
    EXPECT_EQ(treeBefore, treeOf(root()));
}

TEST_F(DeviceLibraryPlusDbTest, SaveTwiceOnSameObjectWorks) {
    QString error;
    const QString changedMessage = QStringLiteral(
            "The library on the stick changed after Mix Two read it. Read the stick again.");

    // After create().
    {
        auto pDb = createDb();
        ASSERT_TRUE(pDb);
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
        EXPECT_EQ(1, pDb->findOrAddArtist(QStringLiteral("First")));
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
        EXPECT_EQ(2, pDb->findOrAddArtist(QStringLiteral("Second")));
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    {
        auto pDb = DeviceLibraryPlusDb::open(root(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        EXPECT_EQ(QStringLiteral("First,Second"),
                scalar(handleOf(*pDb),
                        "SELECT group_concat(name, ',') FROM (SELECT name FROM artist ORDER BY "
                        "artist_id)"));

        // After open().
        EXPECT_EQ(3, pDb->findOrAddArtist(QStringLiteral("Third")));
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
        EXPECT_EQ(4, pDb->findOrAddArtist(QStringLiteral("Fourth")));
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);

        // A change of the file after the second save() is found by the third.
        const QByteArray other("the library of another program");
        ASSERT_TRUE(writeFile(stickFile(), other));
        error.clear();
        EXPECT_FALSE(pDb->save(&error));
        EXPECT_EQ(changedMessage, error);
        EXPECT_EQ(other, readFile(stickFile()));
    }
}

TEST_F(DeviceLibraryPlusDbTest, FindOrAddReusesNames) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);

    const qint64 artistA = pDb->findOrAddArtist(QStringLiteral("A"));
    EXPECT_EQ(1, artistA);
    EXPECT_EQ(artistA, pDb->findOrAddArtist(QStringLiteral("A")));
    const qint64 artistB = pDb->findOrAddArtist(QStringLiteral("B"));
    EXPECT_EQ(2, artistB);
    EXPECT_EQ(QStringLiteral("2"), scalar(pHandle, "SELECT count(*) FROM artist"));
    expectRow(pHandle, "artist", "artist_id = 2", {{"name", "'B'"}, {"nameForSearch", "NULL"}});

    // An album is the pair of a name and an album artist.
    const qint64 albumXA = pDb->findOrAddAlbum(QStringLiteral("X"), artistA);
    const qint64 albumXB = pDb->findOrAddAlbum(QStringLiteral("X"), artistB);
    const qint64 albumXNone = pDb->findOrAddAlbum(QStringLiteral("X"), std::nullopt);
    const qint64 albumYA = pDb->findOrAddAlbum(QStringLiteral("Y"), artistA);
    EXPECT_EQ(1, albumXA);
    EXPECT_EQ(2, albumXB);
    EXPECT_EQ(3, albumXNone);
    EXPECT_EQ(4, albumYA);
    EXPECT_EQ(albumXA, pDb->findOrAddAlbum(QStringLiteral("X"), artistA));
    EXPECT_EQ(albumXB, pDb->findOrAddAlbum(QStringLiteral("X"), artistB));
    EXPECT_EQ(albumXNone, pDb->findOrAddAlbum(QStringLiteral("X"), std::nullopt));
    EXPECT_EQ(QStringLiteral("4"), scalar(pHandle, "SELECT count(*) FROM album"));
    expectRow(pHandle,
            "album",
            "album_id = 1",
            {{"name", "'X'"},
                    {"artist_id", "1"},
                    {"image_id", "NULL"},
                    {"isComplation", "0"},
                    {"nameForSearch", "NULL"}});
    expectRow(pHandle, "album", "album_id = 3", {{"name", "'X'"}, {"artist_id", "NULL"}});

    const qint64 genreHouse = pDb->findOrAddGenre(QStringLiteral("House"));
    EXPECT_EQ(1, genreHouse);
    EXPECT_EQ(genreHouse, pDb->findOrAddGenre(QStringLiteral("House")));
    EXPECT_EQ(2, pDb->findOrAddGenre(QStringLiteral("Techno")));
    EXPECT_EQ(QStringLiteral("2"), scalar(pHandle, "SELECT count(*) FROM genre"));
    expectRow(pHandle, "genre", "genre_id = 2", {{"name", "'Techno'"}});

    const qint64 key8A = pDb->findOrAddKey(QStringLiteral("8A"));
    EXPECT_EQ(1, key8A);
    EXPECT_EQ(key8A, pDb->findOrAddKey(QStringLiteral("8A")));
    EXPECT_EQ(2, pDb->findOrAddKey(QStringLiteral("9A")));
    EXPECT_EQ(QStringLiteral("2"), scalar(pHandle, "SELECT count(*) FROM \"key\""));
    expectRow(pHandle, "\"key\"", "key_id = 2", {{"name", "'9A'"}});

    // An image is not looked up: each call adds a row.
    EXPECT_EQ(1, pDb->addImage(QStringLiteral("/PIONEER/Artwork/00001/b1.jpg")));
    EXPECT_EQ(2, pDb->addImage(QStringLiteral("/PIONEER/Artwork/00001/b2.jpg")));
    expectRow(pHandle, "image", "image_id = 2", {{"path", "'/PIONEER/Artwork/00001/b2.jpg'"}});

    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, NewIdsAreTheLargestIdPlusOne) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);

    // The tables are empty: the first ID is 1.
    EXPECT_EQ(1, pDb->addPlaylist(QStringLiteral("First"), 0, false));
    EXPECT_EQ(1, pDb->addContent(DlpContent()));

    // There are gaps in the IDs: the new ID follows the largest one.
    exec(pHandle, "INSERT INTO artist(artist_id, name) VALUES (7, 'Seven')");
    exec(pHandle, "INSERT INTO content(content_id) VALUES (41)");
    exec(pHandle, "INSERT INTO playlist(playlist_id) VALUES (9)");
    EXPECT_EQ(8, pDb->findOrAddArtist(QStringLiteral("Eight")));
    EXPECT_EQ(42, pDb->addContent(DlpContent()));
    EXPECT_EQ(10, pDb->addPlaylist(QStringLiteral("Ten"), 0, false));
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, IdsAboveInt32Survive) {
    // rekordbox writes IDs that are wider than 31 bits: the myTag_id values of
    // the library of the owner are above INT_MAX.
    constexpr qint64 kBigArtist = 3029741607;
    constexpr qint64 kBigPlaylist = 4020618460;
    constexpr qint64 kBigContent = 3500000000;
    static_assert(kBigArtist > std::numeric_limits<int>::max());

    std::unique_ptr<DeviceLibraryPlusDb> pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);
    exec(pHandle, "INSERT INTO artist(artist_id, name) VALUES (3029741607, 'Big Artist')");
    exec(pHandle,
            "INSERT INTO playlist(playlist_id, sequenceNo, name, attribute, "
            "playlist_id_parent) VALUES (4020618460, 0, 'Big Playlist', 1, 0)");
    exec(pHandle, "INSERT INTO content(content_id, path) VALUES (3500000000, '/Contents/big.mp3')");

    // The find methods give the whole ID, and a new row follows the largest ID.
    EXPECT_EQ(kBigArtist, pDb->findOrAddArtist(QStringLiteral("Big Artist")));
    EXPECT_EQ(kBigArtist + 1, pDb->findOrAddArtist(QStringLiteral("Next Artist")));
    EXPECT_EQ(std::optional<qint64>(kBigPlaylist),
            pDb->findPlaylist(QStringLiteral("Big Playlist"), 0));
    EXPECT_EQ(std::optional<qint64>(kBigContent),
            pDb->findContentByPath(QStringLiteral("/Contents/big.mp3")));

    // An ID above INT_MAX is the parent ID of a playlist and the artist ID of an
    // album, and the new IDs follow the largest IDs.
    EXPECT_EQ(kBigPlaylist + 1,
            pDb->addPlaylist(QStringLiteral("Child"), kBigPlaylist, false));
    EXPECT_EQ(std::optional<qint64>(kBigPlaylist + 1),
            pDb->findPlaylist(QStringLiteral("Child"), kBigPlaylist));
    expectRow(pHandle,
            "playlist",
            "name = 'Child'",
            {{"playlist_id", "4020618461"}, {"playlist_id_parent", "4020618460"}});
    const qint64 album = pDb->findOrAddAlbum(QStringLiteral("Album"), kBigArtist);
    EXPECT_EQ(1, album);
    EXPECT_EQ(album, pDb->findOrAddAlbum(QStringLiteral("Album"), kBigArtist));
    expectRow(pHandle, "album", "album_id = 1", {{"artist_id", "3029741607"}});

    // setPlaylistContents() writes the rows for exactly this playlist ID, with
    // track IDs above INT_MAX too.
    pDb->setPlaylistContents(kBigPlaylist, {kBigContent, 5, kBigContent + 1});
    EXPECT_EQ(QStringLiteral("3500000000:1,5:2,3500000001:3"), playlistRows(pHandle, kBigPlaylist));
    EXPECT_EQ(QStringLiteral("3"), scalar(pHandle, "SELECT count(*) FROM playlist_content"));
    EXPECT_EQ(QStringLiteral("3"),
            scalar(pHandle,
                    "SELECT count(*) FROM playlist_content WHERE playlist_id = 4020618460"));

    // addContent() and updateContent() round trip an artist ID above INT_MAX.
    DlpContent content = sampleContent();
    content.artistId = kBigArtist;
    content.composerId = kBigArtist + 1;
    content.path = QStringLiteral("/Contents/new.mp3");
    const qint64 contentId = pDb->addContent(content);
    EXPECT_EQ(kBigContent + 1, contentId);
    expectRow(pHandle,
            "content",
            "content_id = 3500000001",
            {{"artist_id_artist", "3029741607"}, {"artist_id_composer", "3029741608"}});
    content.title = QStringLiteral("Changed");
    pDb->updateContent(contentId, content);
    expectRow(pHandle,
            "content",
            "content_id = 3500000001",
            {{"title", "'Changed'"}, {"artist_id_artist", "3029741607"}});
    EXPECT_EQ(QStringLiteral("2"), scalar(pHandle, "SELECT count(*) FROM content"));
    EXPECT_EQ(std::optional<qint64>(contentId),
            pDb->findContentByMaster(content.masterDbId, content.masterContentId));
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty()) << qPrintable(pDb->lastErrorMessage());

    // The IDs are the same after save() and open().
    auto pOpened = saveAndReopen(std::move(pDb));
    ASSERT_TRUE(pOpened);
    EXPECT_EQ(kBigArtist, pOpened->findOrAddArtist(QStringLiteral("Big Artist")));
    EXPECT_EQ(std::optional<qint64>(contentId),
            pOpened->findContentByPath(QStringLiteral("/Contents/new.mp3")));
    EXPECT_EQ(QStringLiteral("3500000000:1,5:2,3500000001:3"),
            playlistRows(handleOf(*pOpened), kBigPlaylist));
}

TEST_F(DeviceLibraryPlusDbTest, ContentFindAndUpdate) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);

    DlpContent content = sampleContent();
    content.masterDbId = 77;
    content.masterContentId = 5;
    content.path = QStringLiteral("/Contents/A/X/t.mp3");
    const qint64 id = pDb->addContent(content);
    EXPECT_EQ(1, id);

    EXPECT_EQ(std::optional<qint64>(1), pDb->findContentByMaster(77, 5));
    EXPECT_EQ(std::optional<qint64>(1),
            pDb->findContentByPath(QStringLiteral("/Contents/A/X/t.mp3")));
    EXPECT_EQ(std::nullopt, pDb->findContentByMaster(77, 6));
    EXPECT_EQ(std::nullopt, pDb->findContentByMaster(78, 5));
    EXPECT_EQ(std::nullopt, pDb->findContentByPath(QStringLiteral("/Contents/A/X/other.mp3")));
    // The match is exact, also for the case of the letters.
    EXPECT_EQ(std::nullopt, pDb->findContentByPath(QStringLiteral("/Contents/a/x/t.mp3")));
    // A second track has the next ID.
    DlpContent second = content;
    second.masterContentId = 6;
    second.path = QStringLiteral("/Contents/A/X/u.mp3");
    EXPECT_EQ(2, pDb->addContent(second));
    EXPECT_EQ(std::optional<qint64>(2), pDb->findContentByMaster(77, 6));
    EXPECT_EQ(std::optional<qint64>(2),
            pDb->findContentByPath(QStringLiteral("/Contents/A/X/u.mp3")));

    content.title = QStringLiteral("New");
    content.bpmx100 = 9999;
    content.masterContentId = 8;
    pDb->updateContent(id, content);
    EXPECT_EQ(QStringLiteral("2"), scalar(pHandle, "SELECT count(*) FROM content"));
    expectRow(pHandle,
            "content",
            "content_id = 1",
            {{"title", "'New'"}, {"bpmx100", "9999"}, {"masterContentId", "8"}});
    // The other track does not change.
    expectRow(pHandle, "content", "content_id = 2", {{"title", "'Sample Title'"}});
    EXPECT_EQ(std::optional<qint64>(1), pDb->findContentByMaster(77, 8));
    EXPECT_EQ(std::nullopt, pDb->findContentByMaster(77, 5));
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, ContentColumnsRoundTrip) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    ASSERT_EQ(1, pDb->addContent(sampleContent()));

    expectRow(handleOf(*pDb),
            "content",
            "content_id = 1",
            {{"title", "'Sample Title'"},
                    {"bpmx100", "12850"},
                    {"length", "254"},
                    {"artist_id_artist", "11"},
                    {"artist_id_composer", "12"},
                    {"album_id", "13"},
                    {"genre_id", "14"},
                    {"key_id", "15"},
                    {"color_id", "3"},
                    {"image_id", "16"},
                    {"djComment", "'Comment text'"},
                    {"rating", "4"},
                    {"releaseYear", "2019"},
                    {"dateCreated", "'2026-09-02'"},
                    {"dateAdded", "'2026-10-01'"},
                    {"path", "'/Contents/A/X/t.mp3'"},
                    {"fileName", "'t.mp3'"},
                    {"fileSize", "5000000123"},
                    {"fileType", "1"},
                    {"bitrate", "320"},
                    {"bitDepth", "16"},
                    {"samplingRate", "44100"},
                    {"djPlayCount", "7"},
                    {"masterDbId", "77"},
                    {"masterContentId", "5"},
                    {"analysisDataFilePath", "'/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT'"}});

    // A value that is not set is NULL (the IDs) or empty (the texts).
    ASSERT_EQ(2, pDb->addContent(DlpContent()));
    expectRow(handleOf(*pDb),
            "content",
            "content_id = 2",
            {{"title", "''"},
                    {"artist_id_artist", "NULL"},
                    {"artist_id_composer", "NULL"},
                    {"album_id", "NULL"},
                    {"genre_id", "NULL"},
                    {"key_id", "NULL"},
                    {"image_id", "NULL"},
                    {"color_id", "0"},
                    {"djComment", "''"},
                    {"path", "''"},
                    {"fileSize", "0"}});
}

TEST_F(DeviceLibraryPlusDbTest, ContentFixedColumns) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);

    // The fixed values of spec section 6.
    const auto expectFixedColumns = [&](const char* where) {
        SCOPED_TRACE(where);
        expectRow(pHandle,
                "content",
                where,
                {{"isHotCueAutoLoadOn", "1"},
                        {"analysedBits", "41"},
                        {"contentLink", "788224"},
                        {"hasModified", "0"},
                        {"titleForSearch", "NULL"},
                        {"cueUpdateCount", "NULL"},
                        {"analysisDataUpdateCount", "NULL"},
                        {"informationUpdateCount", "NULL"},
                        {"subtitle", "''"},
                        {"isrc", "''"},
                        {"releaseDate", "''"},
                        {"kuvoDeliveryComment", "''"},
                        {"trackNo", "0"},
                        {"discNo", "0"},
                        {"isKuvoDeliverStatusOn", "0"},
                        {"artist_id_lyricist", "0"},
                        {"artist_id_remixer", "NULL"},
                        {"artist_id_originalArtist", "NULL"},
                        {"label_id", "NULL"}});
    };

    const qint64 id = pDb->addContent(sampleContent());
    ASSERT_EQ(1, id);
    expectFixedColumns("content_id = 1");

    // updateContent() writes them again.
    exec(pHandle,
            "UPDATE content SET isHotCueAutoLoadOn = 0, analysedBits = 0, contentLink = 0, "
            "hasModified = 1, titleForSearch = 'x', cueUpdateCount = 1, "
            "analysisDataUpdateCount = 2, informationUpdateCount = 3, subtitle = 's', "
            "isrc = 'i', releaseDate = 'r', kuvoDeliveryComment = 'k', trackNo = 4, "
            "discNo = 5, isKuvoDeliverStatusOn = 1, artist_id_lyricist = 6, "
            "artist_id_remixer = 7, artist_id_originalArtist = 8, label_id = 9");
    pDb->updateContent(id, sampleContent());
    expectFixedColumns("content_id = 1");
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, UpdateOfMissingContentIsAnError) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    ASSERT_EQ(1, pDb->addContent(sampleContent()));
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());

    pDb->updateContent(99, sampleContent());
    EXPECT_EQ(QStringLiteral("The library has no track with the ID 99."), pDb->lastErrorMessage());
    EXPECT_EQ(QStringLiteral("1"), scalar(handleOf(*pDb), "SELECT count(*) FROM content"));
    QString error;
    EXPECT_FALSE(pDb->save(&error));
    EXPECT_FALSE(QFileInfo::exists(stickFile()));
}

TEST_F(DeviceLibraryPlusDbTest, PlaylistsAndFolders) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);

    const qint64 folder = pDb->addPlaylist(QStringLiteral("Crates"), 0, true);
    EXPECT_EQ(1, folder);
    const qint64 house = pDb->addPlaylist(QStringLiteral("House"), folder, false);
    const qint64 techno = pDb->addPlaylist(QStringLiteral("Techno"), folder, false);
    const qint64 top = pDb->addPlaylist(QStringLiteral("Top"), 0, false);
    EXPECT_EQ(2, house);
    EXPECT_EQ(3, techno);
    EXPECT_EQ(4, top);

    expectRow(pHandle,
            "playlist",
            "playlist_id = 1",
            {{"name", "'Crates'"},
                    {"attribute", "1"},
                    {"sequenceNo", "0"},
                    {"playlist_id_parent", "0"},
                    {"image_id", "NULL"}});
    // The sequence number is the number of playlists and folders with the same parent.
    expectRow(pHandle,
            "playlist",
            "playlist_id = 2",
            {{"name", "'House'"},
                    {"attribute", "0"},
                    {"sequenceNo", "0"},
                    {"playlist_id_parent", "1"},
                    {"image_id", "NULL"}});
    expectRow(pHandle,
            "playlist",
            "playlist_id = 3",
            {{"attribute", "0"}, {"sequenceNo", "1"}, {"playlist_id_parent", "1"}});
    expectRow(pHandle,
            "playlist",
            "playlist_id = 4",
            {{"attribute", "0"}, {"sequenceNo", "1"}, {"playlist_id_parent", "0"}});

    EXPECT_EQ(std::optional<qint64>(folder), pDb->findPlaylist(QStringLiteral("Crates"), 0));
    EXPECT_EQ(std::optional<qint64>(house), pDb->findPlaylist(QStringLiteral("House"), folder));
    EXPECT_EQ(std::optional<qint64>(top), pDb->findPlaylist(QStringLiteral("Top"), 0));
    // The parent is part of the match.
    EXPECT_EQ(std::nullopt, pDb->findPlaylist(QStringLiteral("Crates"), folder));
    EXPECT_EQ(std::nullopt, pDb->findPlaylist(QStringLiteral("House"), 0));
    EXPECT_EQ(std::nullopt, pDb->findPlaylist(QStringLiteral("Missing"), 0));

    pDb->setPlaylistContents(house, {3, 1, 2});
    EXPECT_EQ(QStringLiteral("3:1,1:2,2:3"), playlistRows(pHandle, house));
    pDb->setPlaylistContents(techno, {9});
    // The second list replaces the first list, and only that list.
    pDb->setPlaylistContents(house, {2, 5});
    EXPECT_EQ(QStringLiteral("2:1,5:2"), playlistRows(pHandle, house));
    EXPECT_EQ(QStringLiteral("9:1"), playlistRows(pHandle, techno));
    EXPECT_EQ(QStringLiteral("3"), scalar(pHandle, "SELECT count(*) FROM playlist_content"));

    pDb->setPlaylistContents(house, {});
    EXPECT_EQ(QString(), playlistRows(pHandle, house));
    EXPECT_EQ(QStringLiteral("9:1"), playlistRows(pHandle, techno));
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, FailedPlaylistChangeIsUndone) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);
    const qint64 playlist = pDb->addPlaylist(QStringLiteral("P"), 0, false);
    pDb->setPlaylistContents(playlist, {1, 2});
    ASSERT_EQ(QStringLiteral("1:1,2:2"), playlistRows(pHandle, playlist));

    // The third insert fails.
    exec(pHandle,
            "CREATE TRIGGER fail_third BEFORE INSERT ON playlist_content "
            "WHEN NEW.sequenceNo = 3 BEGIN SELECT RAISE(ABORT, 'third row'); END");
    pDb->setPlaylistContents(playlist, {10, 11, 12, 13});
    EXPECT_TRUE(pDb->lastErrorMessage().contains(QStringLiteral("third row")))
            << qPrintable(pDb->lastErrorMessage());
    // The old list is still there, and no transaction stays open.
    EXPECT_EQ(QStringLiteral("1:1,2:2"), playlistRows(pHandle, playlist));
    EXPECT_NE(0, sqlite3_get_autocommit(pHandle));
}

TEST_F(DeviceLibraryPlusDbTest, ContentCountAndRoundTrip) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    for (int i = 1; i <= 3; ++i) {
        DlpContent content = sampleContent();
        content.masterContentId = i;
        content.path = QStringLiteral("/Contents/A/X/%1.mp3").arg(i);
        ASSERT_EQ(i, pDb->addContent(content));
    }
    // The number changes only when the caller asks for it.
    EXPECT_EQ(QStringLiteral("0"), scalar(handleOf(*pDb), "SELECT numberOfContents FROM property"));
    pDb->updateContentCount();
    EXPECT_EQ(QStringLiteral("3"), scalar(handleOf(*pDb), "SELECT numberOfContents FROM property"));
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());

    auto pOpened = saveAndReopen(std::move(pDb));
    ASSERT_TRUE(pOpened);
    sqlite3* pHandle = handleOf(*pOpened);
    EXPECT_EQ(QStringLiteral("3"), scalar(pHandle, "SELECT numberOfContents FROM property"));
    EXPECT_EQ(QStringLiteral("3"), scalar(pHandle, "SELECT count(*) FROM content"));
    EXPECT_EQ(std::optional<qint64>(2), pOpened->findContentByMaster(77, 2));
    // New rows go after the rows that were saved.
    EXPECT_EQ(4, pOpened->addContent(sampleContent()));
    pOpened->updateContentCount();
    EXPECT_EQ(QStringLiteral("4"), scalar(pHandle, "SELECT numberOfContents FROM property"));
}

TEST_F(DeviceLibraryPlusDbTest, UnicodeRoundTrip) {
    // Review Focus 4. The escapes keep the test independent of the encoding of
    // this file. The title is "Ünïcødé 日本語 🎧", which has a character
    // outside the Basic Multilingual Plane.
    const QString title =
            QString::fromUtf16(u"Ünïcødé 日本語 \U0001F3A7");
    const QString artistName = QString::fromUtf16(u"Björk");
    const QString albumName = QString::fromUtf16(u"アルバム é");
    const QString path = QString::fromUtf16(u"/Contents/Björk/日本/x.flac");
    const QString comment = QString::fromUtf16(u"ça va \U0001F3A7");
    const QString playlistName =
            QString::fromUtf16(u"プレイリスト \U0001F3A7");

    qint64 artistId = 0;
    qint64 contentId = 0;
    qint64 playlistId = 0;
    std::unique_ptr<DeviceLibraryPlusDb> pOpened;
    {
        auto pDb = createDb();
        ASSERT_TRUE(pDb);
        artistId = pDb->findOrAddArtist(artistName);
        const qint64 albumId = pDb->findOrAddAlbum(albumName, artistId);
        DlpContent content = sampleContent();
        content.title = title;
        content.artistId = artistId;
        content.albumId = albumId;
        content.djComment = comment;
        content.path = path;
        content.fileName = QStringLiteral("x.flac");
        contentId = pDb->addContent(content);
        playlistId = pDb->addPlaylist(playlistName, 0, false);
        pDb->setPlaylistContents(playlistId, {contentId});
        pDb->updateContentCount();
        ASSERT_TRUE(pDb->lastErrorMessage().isEmpty());
        pOpened = saveAndReopen(std::move(pDb));
    }
    ASSERT_TRUE(pOpened);
    sqlite3* pHandle = handleOf(*pOpened);

    EXPECT_EQ(title, scalar(pHandle, "SELECT title FROM content"));
    EXPECT_EQ(comment, scalar(pHandle, "SELECT djComment FROM content"));
    EXPECT_EQ(path, scalar(pHandle, "SELECT path FROM content"));
    EXPECT_EQ(artistName, scalar(pHandle, "SELECT name FROM artist"));
    EXPECT_EQ(albumName, scalar(pHandle, "SELECT name FROM album"));
    EXPECT_EQ(playlistName, scalar(pHandle, "SELECT name FROM playlist"));
    // The text is stored as UTF-8.
    EXPECT_EQ(QString::fromLatin1(title.toUtf8().toHex().toUpper()),
            scalar(pHandle, "SELECT hex(title) FROM content"));

    // The lookups find the saved rows: nothing is added again.
    EXPECT_EQ(artistId, pOpened->findOrAddArtist(artistName));
    EXPECT_EQ(QStringLiteral("1"), scalar(pHandle, "SELECT count(*) FROM artist"));
    EXPECT_EQ(std::optional<qint64>(contentId), pOpened->findContentByPath(path));
    EXPECT_EQ(std::optional<qint64>(playlistId), pOpened->findPlaylist(playlistName, 0));
    EXPECT_TRUE(pOpened->lastErrorMessage().isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, SqliteErrorsAreRecordedAndSaveRefuses) {
    // Drops a table that a method needs. The method must report the error, must
    // not leave a transaction open, and save() must refuse.
    const auto expectError = [&](const char* table,
                                     const std::function<void(DeviceLibraryPlusDb&)>& change) {
        SCOPED_TRACE(table);
        auto pDb = createDb();
        ASSERT_TRUE(pDb);
        exec(handleOf(*pDb), (std::string("DROP TABLE \"") + table + "\"").c_str());
        change(*pDb);
        EXPECT_TRUE(pDb->lastErrorMessage().startsWith(
                QStringLiteral("Cannot change the library: ")))
                << qPrintable(pDb->lastErrorMessage());
        EXPECT_NE(0, sqlite3_get_autocommit(handleOf(*pDb)));
        QString error;
        EXPECT_FALSE(pDb->save(&error));
        EXPECT_TRUE(error.startsWith(QStringLiteral("Cannot write the library to the stick: ")))
                << qPrintable(error);
        EXPECT_FALSE(QFileInfo::exists(stickFile()));
    };
    const QString name = QStringLiteral("N");

    expectError("artist", [&](DeviceLibraryPlusDb& db) {
        EXPECT_EQ(-1, db.findOrAddArtist(name));
    });
    expectError("album", [&](DeviceLibraryPlusDb& db) {
        EXPECT_EQ(-1, db.findOrAddAlbum(name, std::nullopt));
    });
    expectError("genre", [&](DeviceLibraryPlusDb& db) {
        EXPECT_EQ(-1, db.findOrAddGenre(name));
    });
    expectError("key", [&](DeviceLibraryPlusDb& db) {
        EXPECT_EQ(-1, db.findOrAddKey(name));
    });
    expectError("image", [&](DeviceLibraryPlusDb& db) {
        EXPECT_EQ(-1, db.addImage(name));
    });
    expectError("content", [&](DeviceLibraryPlusDb& db) {
        EXPECT_EQ(-1, db.addContent(sampleContent()));
    });
    expectError("content", [&](DeviceLibraryPlusDb& db) {
        db.updateContent(1, sampleContent());
    });
    expectError("content", [&](DeviceLibraryPlusDb& db) {
        EXPECT_EQ(std::nullopt, db.findContentByMaster(1, 2));
    });
    expectError("content", [&](DeviceLibraryPlusDb& db) {
        EXPECT_EQ(std::nullopt, db.findContentByPath(name));
    });
    expectError("content", [&](DeviceLibraryPlusDb& db) {
        db.updateContentCount();
    });
    expectError("property", [&](DeviceLibraryPlusDb& db) {
        db.updateContentCount();
    });
    expectError("playlist", [&](DeviceLibraryPlusDb& db) {
        EXPECT_EQ(-1, db.addPlaylist(name, 0, false));
    });
    expectError("playlist", [&](DeviceLibraryPlusDb& db) {
        EXPECT_EQ(std::nullopt, db.findPlaylist(name, 0));
    });
    expectError("playlist_content", [&](DeviceLibraryPlusDb& db) {
        db.setPlaylistContents(1, {1, 2});
    });
}

TEST_F(DeviceLibraryPlusDbTest, FirstErrorStays) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    exec(handleOf(*pDb), "DROP TABLE artist");
    exec(handleOf(*pDb), "DROP TABLE genre");
    EXPECT_EQ(-1, pDb->findOrAddArtist(QStringLiteral("A")));
    const QString first = pDb->lastErrorMessage();
    EXPECT_TRUE(first.contains(QStringLiteral("artist"))) << qPrintable(first);
    EXPECT_EQ(-1, pDb->findOrAddGenre(QStringLiteral("G")));
    EXPECT_EQ(first, pDb->lastErrorMessage());
}

TEST_F(DeviceLibraryPlusDbTest, WorkingCopyTemplate) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    const QFileInfo workingFile(QString::fromUtf8(sqlite3_db_filename(handleOf(*pDb), "main")));
    ASSERT_TRUE(workingFile.exists());
    // Mix Two removes the folders of earlier runs by this name.
    EXPECT_TRUE(workingFile.dir().dirName().startsWith(QStringLiteral("mixtwo-dlp-")))
            << qPrintable(workingFile.absoluteFilePath());
    EXPECT_EQ(QDir::cleanPath(QDir::tempPath()),
            QDir::cleanPath(workingFile.dir().absoluteFilePath(QStringLiteral(".."))));

    // The same for a library that open() reads.
    QString error;
    ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    auto pOpened = DeviceLibraryPlusDb::open(root(), &error);
    ASSERT_TRUE(pOpened) << qPrintable(error);
    EXPECT_TRUE(QFileInfo(QString::fromUtf8(sqlite3_db_filename(handleOf(*pOpened), "main")))
                        .dir()
                        .dirName()
                        .startsWith(QStringLiteral("mixtwo-dlp-")));
}

TEST_F(DeviceLibraryPlusDbTest, WorkingCopyDoesNotSync) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    // 0 is OFF.
    EXPECT_EQ(QStringLiteral("0"), scalar(handleOf(*pDb), "PRAGMA synchronous"));
    auto pOpened = saveAndReopen(std::move(pDb));
    ASSERT_TRUE(pOpened);
    EXPECT_EQ(QStringLiteral("0"), scalar(handleOf(*pOpened), "PRAGMA synchronous"));
}

TEST_F(DeviceLibraryPlusDbTest, BatchCommitsOnce) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);
    ASSERT_NE(0, sqlite3_get_autocommit(pHandle));

    ASSERT_TRUE(pDb->beginBatch());
    // The transaction stays open over all typed calls.
    EXPECT_EQ(0, sqlite3_get_autocommit(pHandle));
    for (int i = 1; i <= 100; ++i) {
        ASSERT_EQ(i, pDb->findOrAddArtist(QStringLiteral("Artist %1").arg(i)));
        EXPECT_EQ(0, sqlite3_get_autocommit(pHandle));
    }
    ASSERT_TRUE(pDb->commitBatch());
    EXPECT_NE(0, sqlite3_get_autocommit(pHandle));
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());

    auto pOpened = saveAndReopen(std::move(pDb));
    ASSERT_TRUE(pOpened);
    EXPECT_EQ(QStringLiteral("100"), scalar(handleOf(*pOpened), "SELECT count(*) FROM artist"));
}

TEST_F(DeviceLibraryPlusDbTest, BatchRollsBackOnDestroy) {
    {
        auto pDb = createDb();
        ASSERT_TRUE(pDb);
        QString error;
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    {
        QString error;
        auto pDb = DeviceLibraryPlusDb::open(root(), &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        ASSERT_TRUE(pDb->beginBatch());
        for (int i = 1; i <= 5; ++i) {
            ASSERT_EQ(i, pDb->findOrAddArtist(QStringLiteral("Artist %1").arg(i)));
        }
        // The object goes away with the batch open.
    }
    QString error;
    auto pOpened = DeviceLibraryPlusDb::open(root(), &error);
    ASSERT_TRUE(pOpened) << qPrintable(error);
    EXPECT_EQ(QStringLiteral("0"), scalar(handleOf(*pOpened), "SELECT count(*) FROM artist"));
}

TEST_F(DeviceLibraryPlusDbTest, RollbackBatchUndoesTheBatch) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);
    ASSERT_EQ(1, pDb->findOrAddArtist(QStringLiteral("Kept")));

    ASSERT_TRUE(pDb->beginBatch());
    EXPECT_EQ(2, pDb->findOrAddArtist(QStringLiteral("Undone")));
    EXPECT_GT(pDb->addPlaylist(QStringLiteral("Undone"), 0, false), 0);
    ASSERT_TRUE(pDb->rollbackBatch());
    EXPECT_NE(0, sqlite3_get_autocommit(pHandle));
    EXPECT_EQ(QStringLiteral("Kept"), scalar(pHandle, "SELECT group_concat(name) FROM artist"));
    EXPECT_EQ(QStringLiteral("0"), scalar(pHandle, "SELECT count(*) FROM playlist"));
    // A rollback is not an error: the library can still be saved.
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());
    QString error;
    EXPECT_TRUE(pDb->save(&error)) << qPrintable(error);
}

TEST_F(DeviceLibraryPlusDbTest, BatchCallsInTheWrongOrderFail) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    // No batch is open.
    EXPECT_FALSE(pDb->commitBatch());
    EXPECT_FALSE(pDb->rollbackBatch());
    EXPECT_FALSE(pDb->lastErrorMessage().isEmpty());

    auto pSecond = createDb();
    ASSERT_TRUE(pSecond);
    // A batch is open.
    ASSERT_TRUE(pSecond->beginBatch());
    EXPECT_FALSE(pSecond->beginBatch());
    EXPECT_FALSE(pSecond->lastErrorMessage().isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, FailedCommitSetsErrorAndRollsBack) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);
    // A deferred foreign key is checked at COMMIT.
    exec(pHandle, "PRAGMA foreign_keys = ON");
    exec(pHandle, "CREATE TABLE parent_t(id integer primary key)");
    exec(pHandle,
            "CREATE TABLE child_t(parent integer REFERENCES parent_t(id) "
            "DEFERRABLE INITIALLY DEFERRED)");

    ASSERT_TRUE(pDb->beginBatch());
    EXPECT_EQ(1, pDb->findOrAddArtist(QStringLiteral("A")));
    exec(pHandle, "INSERT INTO child_t(parent) VALUES (99)");
    EXPECT_FALSE(pDb->commitBatch());
    EXPECT_TRUE(pDb->lastErrorMessage().contains(QStringLiteral("FOREIGN KEY")))
            << qPrintable(pDb->lastErrorMessage());
    // No transaction stays open, and the batch is gone.
    EXPECT_NE(0, sqlite3_get_autocommit(pHandle));
    EXPECT_EQ(QStringLiteral("0"), scalar(pHandle, "SELECT count(*) FROM artist"));
    QString error;
    EXPECT_FALSE(pDb->save(&error));
    EXPECT_TRUE(error.startsWith(QStringLiteral("Cannot write the library to the stick: ")))
            << qPrintable(error);
}

TEST_F(DeviceLibraryPlusDbTest, SaveRefusesOpenBatch) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    ASSERT_TRUE(pDb->beginBatch());
    EXPECT_EQ(1, pDb->findOrAddArtist(QStringLiteral("A")));

    QString error;
    EXPECT_FALSE(pDb->save(&error));
    EXPECT_EQ(QStringLiteral(
                      "The export did not finish. The library on the stick did not change."),
            error);
    EXPECT_FALSE(QFileInfo::exists(stickFile()));
    // The refusal is not an error of a change, and the batch is still open.
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());
    EXPECT_EQ(0, sqlite3_get_autocommit(handleOf(*pDb)));
    ASSERT_TRUE(pDb->commitBatch());
    EXPECT_TRUE(pDb->save(&error)) << qPrintable(error);
    EXPECT_TRUE(QFileInfo::exists(stickFile()));
}

TEST_F(DeviceLibraryPlusDbTest, SavepointInsideBatchUndoesOnlyItsOwnChange) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);
    ASSERT_TRUE(pDb->beginBatch());
    const qint64 artist = pDb->findOrAddArtist(QStringLiteral("Before"));
    const qint64 playlist = pDb->addPlaylist(QStringLiteral("P"), 0, false);
    pDb->setPlaylistContents(playlist, {1, 2});
    ASSERT_EQ(QStringLiteral("1:1,2:2"), playlistRows(pHandle, playlist));

    // The third insert fails.
    exec(pHandle,
            "CREATE TRIGGER fail_third BEFORE INSERT ON playlist_content "
            "WHEN NEW.sequenceNo = 3 BEGIN SELECT RAISE(ABORT, 'third row'); END");
    pDb->setPlaylistContents(playlist, {10, 11, 12, 13});
    EXPECT_TRUE(pDb->lastErrorMessage().contains(QStringLiteral("third row")))
            << qPrintable(pDb->lastErrorMessage());
    // The old list is there, the earlier work of the batch is there, and the
    // batch is still open.
    EXPECT_EQ(QStringLiteral("1:1,2:2"), playlistRows(pHandle, playlist));
    EXPECT_EQ(QStringLiteral("1"), scalar(pHandle, "SELECT count(*) FROM artist"));
    EXPECT_EQ(artist, pDb->findOrAddArtist(QStringLiteral("Before")));
    EXPECT_EQ(0, sqlite3_get_autocommit(pHandle));
    EXPECT_TRUE(pDb->commitBatch());
    EXPECT_NE(0, sqlite3_get_autocommit(pHandle));
}

TEST_F(DeviceLibraryPlusDbTest, NameCacheGivesSameId) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);

    const qint64 artist = pDb->findOrAddArtist(QStringLiteral("A"));
    EXPECT_EQ(artist, pDb->findOrAddArtist(QStringLiteral("A")));
    // The match is case-sensitive.
    EXPECT_NE(artist, pDb->findOrAddArtist(QStringLiteral("a")));
    EXPECT_EQ(QStringLiteral("2"), scalar(pHandle, "SELECT count(*) FROM artist"));
    const qint64 album = pDb->findOrAddAlbum(QStringLiteral("X"), artist);
    EXPECT_EQ(album, pDb->findOrAddAlbum(QStringLiteral("X"), artist));
    EXPECT_NE(album, pDb->findOrAddAlbum(QStringLiteral("X"), std::nullopt));
    EXPECT_NE(album, pDb->findOrAddAlbum(QStringLiteral("X"), artist + 1));
    EXPECT_EQ(QStringLiteral("3"), scalar(pHandle, "SELECT count(*) FROM album"));
    const qint64 genre = pDb->findOrAddGenre(QStringLiteral("G"));
    EXPECT_EQ(genre, pDb->findOrAddGenre(QStringLiteral("G")));
    const qint64 key = pDb->findOrAddKey(QStringLiteral("8A"));
    EXPECT_EQ(key, pDb->findOrAddKey(QStringLiteral("8A")));
    // The tables do not mix.
    EXPECT_EQ(QStringLiteral("1"), scalar(pHandle, "SELECT count(*) FROM genre"));
    EXPECT_EQ(QStringLiteral("1"), scalar(pHandle, "SELECT count(*) FROM \"key\""));
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, NameCacheDoesNotKeepRolledBackIds) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);
    ASSERT_EQ(1, pDb->findOrAddArtist(QStringLiteral("Kept")));

    ASSERT_TRUE(pDb->beginBatch());
    ASSERT_EQ(2, pDb->findOrAddArtist(QStringLiteral("Gone 1")));
    ASSERT_EQ(3, pDb->findOrAddArtist(QStringLiteral("Gone 2")));
    ASSERT_EQ(1, pDb->findOrAddAlbum(QStringLiteral("Album 1"), 1));
    ASSERT_EQ(2, pDb->findOrAddAlbum(QStringLiteral("Album 2"), 1));
    ASSERT_EQ(1, pDb->findOrAddGenre(QStringLiteral("Genre 1")));
    ASSERT_EQ(2, pDb->findOrAddGenre(QStringLiteral("Genre 2")));
    ASSERT_EQ(1, pDb->findOrAddKey(QStringLiteral("Key 1")));
    ASSERT_EQ(2, pDb->findOrAddKey(QStringLiteral("Key 2")));
    ASSERT_TRUE(pDb->rollbackBatch());

    // A cache that kept the old IDs would give 3, 2, 2 and 2.
    EXPECT_EQ(2, pDb->findOrAddArtist(QStringLiteral("Gone 2")));
    EXPECT_EQ(1, pDb->findOrAddAlbum(QStringLiteral("Album 2"), 1));
    EXPECT_EQ(1, pDb->findOrAddGenre(QStringLiteral("Genre 2")));
    EXPECT_EQ(1, pDb->findOrAddKey(QStringLiteral("Key 2")));
    EXPECT_EQ(1, pDb->findOrAddArtist(QStringLiteral("Kept")));
    EXPECT_EQ(QStringLiteral("2"), scalar(pHandle, "SELECT count(*) FROM artist"));
    EXPECT_EQ(QStringLiteral("1"), scalar(pHandle, "SELECT count(*) FROM album"));
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, NameCacheIsFilledFromExistingRows) {
    {
        auto pDb = createDb();
        ASSERT_TRUE(pDb);
        sqlite3* pHandle = handleOf(*pDb);
        // A library of another program: the same name twice, a NULL name, a
        // name with a wider ID, and an album with no artist.
        exec(pHandle, "INSERT INTO artist(artist_id, name) VALUES (5, 'Twice')");
        exec(pHandle, "INSERT INTO artist(artist_id, name) VALUES (3, 'Twice')");
        exec(pHandle, "INSERT INTO artist(artist_id, name) VALUES (4, NULL)");
        exec(pHandle, "INSERT INTO artist(artist_id, name) VALUES (3029741607, 'Big')");
        exec(pHandle, "INSERT INTO album(album_id, name, artist_id) VALUES (8, 'Alone', NULL)");
        exec(pHandle, "INSERT INTO album(album_id, name, artist_id) VALUES (9, 'Alone', 5)");
        exec(pHandle, "INSERT INTO genre(genre_id, name) VALUES (2, 'House')");
        exec(pHandle, "INSERT INTO \"key\"(key_id, name) VALUES (6, '8A')");
        QString error;
        ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    }
    QString error;
    auto pDb = DeviceLibraryPlusDb::open(root(), &error);
    ASSERT_TRUE(pDb) << qPrintable(error);
    sqlite3* pHandle = handleOf(*pDb);

    // The first row with the name wins, as the SQL query gave it.
    EXPECT_EQ(3, pDb->findOrAddArtist(QStringLiteral("Twice")));
    EXPECT_EQ(3029741607, pDb->findOrAddArtist(QStringLiteral("Big")));
    EXPECT_EQ(8, pDb->findOrAddAlbum(QStringLiteral("Alone"), std::nullopt));
    EXPECT_EQ(9, pDb->findOrAddAlbum(QStringLiteral("Alone"), 5));
    EXPECT_EQ(2, pDb->findOrAddGenre(QStringLiteral("House")));
    EXPECT_EQ(6, pDb->findOrAddKey(QStringLiteral("8A")));
    EXPECT_EQ(QStringLiteral("4"), scalar(pHandle, "SELECT count(*) FROM artist"));
    EXPECT_EQ(QStringLiteral("2"), scalar(pHandle, "SELECT count(*) FROM album"));
    // A row with a NULL name is not a row with the empty name.
    const qint64 empty = pDb->findOrAddArtist(QString());
    EXPECT_EQ(3029741608, empty);
    EXPECT_EQ(empty, pDb->findOrAddArtist(QString()));
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, ListContentsAndImages) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);

    EXPECT_TRUE(pDb->listContents().isEmpty());
    EXPECT_TRUE(pDb->listImages().isEmpty());

    const qint64 image1 = pDb->addImage(QStringLiteral("/PIONEER/Artwork/00001/b1.jpg"));
    const qint64 image2 = pDb->addImage(QStringLiteral("/PIONEER/Artwork/00001/b2.jpg"));
    DlpContent first = sampleContent();
    first.imageId = image2;
    first.path = QStringLiteral("/Contents/A/X/1.mp3");
    first.masterDbId = 3029741607;
    first.masterContentId = 41;
    first.analysisDataFilePath = QStringLiteral("/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT");
    DlpContent second = sampleContent();
    second.imageId = std::nullopt;
    second.path = QStringLiteral("/Contents/B/Y/2.mp3");
    second.masterDbId = 0;
    second.masterContentId = 0;
    second.analysisDataFilePath = QStringLiteral("/PIONEER/USBANLZ/P001/00000002/ANLZ0000.DAT");
    const qint64 id1 = pDb->addContent(first);
    const qint64 id2 = pDb->addContent(second);
    ASSERT_GT(id1, 0);
    ASSERT_GT(id2, id1);

    const QList<DlpContentRef> contents = pDb->listContents();
    ASSERT_EQ(2, contents.size());
    EXPECT_EQ(id1, contents[0].id);
    EXPECT_EQ(first.path, contents[0].path);
    EXPECT_EQ(3029741607, contents[0].masterDbId);
    EXPECT_EQ(41, contents[0].masterContentId);
    EXPECT_EQ(first.analysisDataFilePath, contents[0].analysisDataFilePath);
    EXPECT_EQ(std::optional<qint64>(image2), contents[0].imageId);
    EXPECT_EQ(id2, contents[1].id);
    EXPECT_EQ(second.path, contents[1].path);
    EXPECT_EQ(0, contents[1].masterDbId);
    EXPECT_EQ(0, contents[1].masterContentId);
    EXPECT_EQ(second.analysisDataFilePath, contents[1].analysisDataFilePath);
    EXPECT_EQ(std::nullopt, contents[1].imageId);

    const QList<QPair<qint64, QString>> images = pDb->listImages();
    ASSERT_EQ(2, images.size());
    EXPECT_EQ(image1, images[0].first);
    EXPECT_EQ(QStringLiteral("/PIONEER/Artwork/00001/b1.jpg"), images[0].second);
    EXPECT_EQ(image2, images[1].first);
    EXPECT_EQ(QStringLiteral("/PIONEER/Artwork/00001/b2.jpg"), images[1].second);
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());

    // The lists come from the saved library too.
    auto pOpened = saveAndReopen(std::move(pDb));
    ASSERT_TRUE(pOpened);
    EXPECT_EQ(2, pOpened->listContents().size());
    EXPECT_EQ(2, pOpened->listImages().size());
}

TEST_F(DeviceLibraryPlusDbTest, ListFailuresAreRecorded) {
    {
        auto pDb = createDb();
        ASSERT_TRUE(pDb);
        exec(handleOf(*pDb), "DROP TABLE content");
        EXPECT_TRUE(pDb->listContents().isEmpty());
        EXPECT_TRUE(pDb->lastErrorMessage().startsWith(
                QStringLiteral("Cannot change the library: ")))
                << qPrintable(pDb->lastErrorMessage());
    }
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    exec(handleOf(*pDb), "DROP TABLE image");
    EXPECT_TRUE(pDb->listImages().isEmpty());
    EXPECT_FALSE(pDb->lastErrorMessage().isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, FindFolderAndPlaylistOnly) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);

    // A folder and a playlist with the same name at the same level.
    const qint64 folder = pDb->addPlaylist(QStringLiteral("Same"), 0, true);
    const qint64 playlist = pDb->addPlaylist(QStringLiteral("Same"), 0, false);
    const qint64 child = pDb->addPlaylist(QStringLiteral("Child"), folder, false);

    EXPECT_EQ(std::optional<qint64>(folder), pDb->findFolder(QStringLiteral("Same"), 0));
    EXPECT_EQ(std::optional<qint64>(playlist), pDb->findPlaylistOnly(QStringLiteral("Same"), 0));
    // findPlaylist() gives the first of both.
    EXPECT_EQ(std::optional<qint64>(folder), pDb->findPlaylist(QStringLiteral("Same"), 0));

    // A name that only the other kind has.
    EXPECT_EQ(std::nullopt, pDb->findFolder(QStringLiteral("Child"), folder));
    EXPECT_EQ(std::optional<qint64>(child),
            pDb->findPlaylistOnly(QStringLiteral("Child"), folder));
    // The parent is part of the match.
    EXPECT_EQ(std::nullopt, pDb->findFolder(QStringLiteral("Same"), folder));
    EXPECT_EQ(std::nullopt, pDb->findPlaylistOnly(QStringLiteral("Same"), folder));
    EXPECT_EQ(std::nullopt, pDb->findPlaylistOnly(QStringLiteral("Missing"), 0));
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());

    exec(handleOf(*pDb), "DROP TABLE playlist");
    EXPECT_EQ(std::nullopt, pDb->findFolder(QStringLiteral("Same"), 0));
    EXPECT_FALSE(pDb->lastErrorMessage().isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, UpdateContentClearsOptionalId) {
    auto pDb = createDb();
    ASSERT_TRUE(pDb);
    sqlite3* pHandle = handleOf(*pDb);

    DlpContent content = sampleContent();
    content.artistId = 5;
    content.imageId = 16;
    const qint64 id = pDb->addContent(content);
    ASSERT_GT(id, 0);
    expectRow(pHandle,
            "content",
            "content_id = 1",
            {{"image_id", "16"}, {"artist_id_artist", "5"}});

    // An empty ID in the struct is NULL in the row, not the old value.
    content.imageId = std::nullopt;
    content.artistId = std::nullopt;
    pDb->updateContent(id, content);
    expectRow(pHandle,
            "content",
            "content_id = 1",
            {{"image_id", "NULL"}, {"artist_id_artist", "NULL"}});
    EXPECT_EQ(std::nullopt, pDb->listContents().first().imageId);
    EXPECT_TRUE(pDb->lastErrorMessage().isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, SampleFolderGuardAcceptsEmptyFolder) {
    EXPECT_EQ(QString(), prepareSampleFolder(root()));
    EXPECT_TRUE(treeOf(root()).isEmpty());
}

TEST_F(DeviceLibraryPlusDbTest, SampleFolderGuardRemovesOnlyItsOwnLibrary) {
    // The library of an earlier run of the test: create() would refuse the
    // stick, so the guard removes it.
    ASSERT_FALSE(createAndSave(kSampleDeviceName).isEmpty());
    ASSERT_TRUE(QFileInfo::exists(stickFile()));

    EXPECT_EQ(QString(), prepareSampleFolder(root()));
    EXPECT_FALSE(QFileInfo::exists(stickFile()));
}

TEST_F(DeviceLibraryPlusDbTest, SampleFolderGuardKeepsForeignLibrary) {
    // A library of another device is a real library. The test did not write it.
    const QByteArray before = createAndSave(kDeviceName);
    ASSERT_FALSE(before.isEmpty());
    const QDateTime timeBefore = QFileInfo(stickFile()).lastModified();
    const QString message =
            QStringLiteral("This test does not delete a library that it did not write.");

    EXPECT_EQ(message, prepareSampleFolder(root()));
    EXPECT_EQ(before, readFile(stickFile()));
    EXPECT_EQ(timeBefore, QFileInfo(stickFile()).lastModified());

    // A file that is not a library at all is not deleted either.
    const QByteArray notALibrary("This is not a database.");
    ASSERT_TRUE(writeFile(stickFile(), notALibrary));
    EXPECT_EQ(message, prepareSampleFolder(root()));
    EXPECT_EQ(notALibrary, readFile(stickFile()));
}

TEST_F(DeviceLibraryPlusDbTest, SampleFolderGuardRefusesRekordboxStick) {
    const QString message = QStringLiteral(
            "MIXTWO_DLP_SAMPLE_DIR points at a stick with a rekordbox library. "
            "Use an empty folder.");

    // The library has the device name of the sample, so only the other files of
    // the stick are the reason to refuse.
    const QByteArray library = createAndSave(kSampleDeviceName);
    ASSERT_FALSE(library.isEmpty());
    const QString pdbFile =
            QDir(root()).filePath(QStringLiteral("PIONEER/rekordbox/export.pdb"));
    const QByteArray pdbBytes("export.pdb of a player");
    ASSERT_TRUE(writeFile(pdbFile, pdbBytes));

    EXPECT_EQ(message, prepareSampleFolder(root()));
    EXPECT_EQ(library, readFile(stickFile()));
    EXPECT_EQ(pdbBytes, readFile(pdbFile));

    // The same with only the export.pdb file in the folder.
    ASSERT_TRUE(QFile::remove(stickFile()));
    EXPECT_EQ(message, prepareSampleFolder(root()));
    EXPECT_EQ(pdbBytes, readFile(pdbFile));
    EXPECT_FALSE(QFileInfo::exists(stickFile()));
}

TEST_F(DeviceLibraryPlusDbTest, SampleFolderGuardRefusesAnalysisFolder) {
    const QString message = QStringLiteral(
            "MIXTWO_DLP_SAMPLE_DIR points at a stick with a rekordbox library. "
            "Use an empty folder.");

    const QByteArray library = createAndSave(kSampleDeviceName);
    ASSERT_FALSE(library.isEmpty());
    ASSERT_TRUE(QDir(root()).mkpath(QStringLiteral("PIONEER/USBANLZ/P001")));

    EXPECT_EQ(message, prepareSampleFolder(root()));
    EXPECT_EQ(library, readFile(stickFile()));
}

TEST_F(DeviceLibraryPlusDbTest, WritesSampleStickWhenAsked) {
    // For the independent check (tools/dlp_verify.py). The folder must be an
    // empty scratch folder that exists. The test fails, and changes nothing,
    // if the folder has a rekordbox library. It deletes only a library that an
    // earlier run of this test wrote (the device name "MIX TWO SAMPLE"):
    //   MIXTWO_DLP_SAMPLE_DIR=<empty folder> mixxx-test --gtest_filter=*WritesSampleStickWhenAsked*
    const QString sampleDir = qEnvironmentVariable("MIXTWO_DLP_SAMPLE_DIR");
    if (sampleDir.isEmpty()) {
        GTEST_SKIP() << "MIXTWO_DLP_SAMPLE_DIR is not set.";
    }
    // create() refuses a stick that has a library, so a run again needs a new start.
    const QString problem = prepareSampleFolder(sampleDir);
    if (!problem.isEmpty()) {
        FAIL() << qPrintable(problem);
    }

    QString error;
    auto pDb = DeviceLibraryPlusDb::create(sampleDir, kSampleDeviceName, 20260211, &error);
    ASSERT_TRUE(pDb) << qPrintable(error);

    const qint64 artistOne = pDb->findOrAddArtist(QStringLiteral("Artist One"));
    const qint64 artistTwo = pDb->findOrAddArtist(QString::fromUtf16(u"Björk"));
    const qint64 albumOne = pDb->findOrAddAlbum(QStringLiteral("Album One"), artistOne);
    const qint64 albumTwo = pDb->findOrAddAlbum(QString::fromUtf16(u"日本"), artistTwo);
    const qint64 genre = pDb->findOrAddGenre(QStringLiteral("House"));
    const qint64 key = pDb->findOrAddKey(QStringLiteral("8A"));
    const qint64 image = pDb->addImage(QStringLiteral("/PIONEER/Artwork/00001/b1.jpg"));

    DlpContent first = sampleContent();
    first.title = QStringLiteral("Sample One");
    first.artistId = artistOne;
    first.albumId = albumOne;
    first.genreId = genre;
    first.keyId = key;
    first.imageId = image;
    first.path = QStringLiteral("/Contents/Artist One/Album One/sample-one.mp3");
    first.fileName = QStringLiteral("sample-one.mp3");
    first.masterContentId = 1;
    first.analysisDataFilePath = QStringLiteral("/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT");
    DlpContent second = sampleContent();
    second.title = QString::fromUtf16(u"Sample Two 日本語");
    second.artistId = artistTwo;
    second.albumId = albumTwo;
    second.genreId = genre;
    second.keyId = std::nullopt;
    second.imageId = std::nullopt;
    second.path = QString::fromUtf16(u"/Contents/Björk/日本/sample-two.flac");
    second.fileName = QStringLiteral("sample-two.flac");
    second.fileType = 5;
    second.bitrate = 900;
    second.bitDepth = 24;
    second.samplingRate = 48000;
    second.masterContentId = 2;
    second.analysisDataFilePath = QStringLiteral("/PIONEER/USBANLZ/P002/00000002/ANLZ0000.DAT");
    const qint64 idOne = pDb->addContent(first);
    const qint64 idTwo = pDb->addContent(second);

    const qint64 folder = pDb->addPlaylist(QStringLiteral("Crates"), 0, true);
    const qint64 crate = pDb->addPlaylist(QStringLiteral("Crate A"), folder, false);
    const qint64 playlist = pDb->addPlaylist(QStringLiteral("Playlist A"), 0, false);
    pDb->setPlaylistContents(crate, {idOne, idTwo});
    pDb->setPlaylistContents(playlist, {idTwo, idOne});
    pDb->updateContentCount();
    ASSERT_TRUE(pDb->lastErrorMessage().isEmpty()) << qPrintable(pDb->lastErrorMessage());
    ASSERT_TRUE(pDb->save(&error)) << qPrintable(error);
    pDb.reset();

    // The stick opens again with the rows in it.
    auto pOpened = DeviceLibraryPlusDb::open(sampleDir, &error);
    ASSERT_TRUE(pOpened) << qPrintable(error);
    EXPECT_EQ(QStringLiteral("2"),
            scalar(handleOf(*pOpened), "SELECT numberOfContents FROM property"));
    EXPECT_EQ(QStringLiteral("3"), scalar(handleOf(*pOpened), "SELECT count(*) FROM playlist"));
}

TEST_F(DeviceLibraryPlusDbTest, ReferenceStickOpens) {
    // Opens a stick that rekordbox made, to prove that open() accepts a real
    // library, also when its tables were made in another order than the order of
    // the design. The stick is only read:
    //   MIXTWO_DLP_REFERENCE=<stick root> mixxx-test --gtest_filter=*ReferenceStickOpens*
    // The test reads only the folder PIONEER/rekordbox of the stick.
    const QString referenceRoot = qEnvironmentVariable("MIXTWO_DLP_REFERENCE");
    if (referenceRoot.isEmpty()) {
        GTEST_SKIP() << "MIXTWO_DLP_REFERENCE is not set.";
    }
    const QString libraryFile = QDir(referenceRoot).filePath(DeviceLibraryPlusDb::kRelativePath);
    const QString libraryFolder = QFileInfo(libraryFile).absolutePath();
    ASSERT_TRUE(QFileInfo::exists(libraryFile)) << qPrintable(libraryFile);

    const QByteArray hashBefore = sha256(readFile(libraryFile));
    const QDateTime timeBefore = QFileInfo(libraryFile).lastModified();
    const QStringList treeBefore = treeOf(libraryFolder);
    ASSERT_FALSE(treeBefore.isEmpty());

    {
        QString error;
        auto pDb = DeviceLibraryPlusDb::open(referenceRoot, &error);
        ASSERT_TRUE(pDb) << qPrintable(error);
        EXPECT_FALSE(pDb->deviceName().isEmpty());

        sqlite3* pHandle = handleOf(*pDb);
        const QString contentRows = scalar(pHandle, "SELECT COUNT(*) FROM content");
        const QString counted = scalar(pHandle, "SELECT numberOfContents FROM property");
        EXPECT_NE(QStringLiteral("<no row>"), counted);
        EXPECT_EQ(contentRows, counted);
    }

    // The stick is as it was.
    EXPECT_EQ(hashBefore, sha256(readFile(libraryFile)));
    EXPECT_EQ(timeBefore, QFileInfo(libraryFile).lastModified());
    EXPECT_EQ(treeBefore, treeOf(libraryFolder));
}

} // namespace rekordbox
} // namespace mixxx
