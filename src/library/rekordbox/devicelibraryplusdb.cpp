#include "library/rekordbox/devicelibraryplusdb.h"

#include <sqlite3.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringList>
#include <optional>

#include "library/rekordbox/devicelibrarypluskey.h"
#include "library/rekordbox/devicelibraryplusdbsql.h"
#include "library/rekordbox/devicelibraryplusschema.h"
#include "library/rekordbox/sqlciphercodec.h"

namespace mixxx {
namespace rekordbox {

const QString DeviceLibraryPlusDb::kRelativePath =
        QStringLiteral("PIONEER/rekordbox/exportLibrary.db");

namespace {

using namespace dlpsql;

/// The dbVersion of the databases that this code knows.
const QString kDbVersion = QStringLiteral("1000");

/// The place of the library file on a stick.
QString stickPathOf(const QString& stickRoot) {
    return QDir(stickRoot).filePath(DeviceLibraryPlusDb::kRelativePath);
}

/// True if the path is absolute. On Windows, "E:" and "E:folder" are relative
/// to the current folder on drive E, and Qt can call them absolute.
bool isAbsolutePath(const QString& path) {
    if (path.isEmpty() || !QFileInfo(path).isAbsolute()) {
        return false;
    }
#ifdef Q_OS_WIN
    if (path.size() >= 2 && path.at(0).isLetter() && path.at(1) == QLatin1Char(':')) {
        return path.size() >= 3 &&
                (path.at(2) == QLatin1Char('/') || path.at(2) == QLatin1Char('\\'));
    }
#endif
    return true;
}

/// True if the stick folder is an absolute path to a folder that exists. An
/// empty or a relative path is the folder of the program, not of a stick.
bool isStickRootAvailable(const QString& stickRoot) {
    return isAbsolutePath(stickRoot) && QFileInfo(stickRoot).isDir();
}

/// True if a WAL file with data is next to the library file. It holds changes
/// that are not in the library file yet. Only SQLite can apply them, and that
/// needs a write to the stick.
bool hasPendingWal(const QString& stickPath) {
    const QFileInfo walInfo(stickPath + QStringLiteral("-wal"));
    return walInfo.exists() && walInfo.size() > 0;
}

QByteArray sha256Of(const QByteArray& data) {
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

/// Makes the folders of the library file below the stick folder, which must
/// exist. It never makes the stick folder: QDir::mkdir() does not make parents.
bool makeLibraryFolders(const QString& stickRoot) {
    const QStringList names =
            QFileInfo(DeviceLibraryPlusDb::kRelativePath).path().split(QLatin1Char('/'));
    QDir dir(stickRoot);
    for (const QString& name : names) {
        const QString path = dir.filePath(name);
        // Another program can make the folder at the same time.
        if (!QFileInfo(path).isDir() && !dir.mkdir(name) && !QFileInfo(path).isDir()) {
            return false;
        }
        dir.setPath(path);
    }
    return true;
}

// The messages are for the user.

QString alreadyExistsError() {
    return QCoreApplication::translate(
            "DeviceLibraryPlusDb", "The stick already has a Device Library Plus.");
}

QString noLibraryError() {
    return QCoreApplication::translate(
            "DeviceLibraryPlusDb", "The stick has no Device Library Plus.");
}

QString incompleteError() {
    return QCoreApplication::translate("DeviceLibraryPlusDb",
            "The library on the stick is not complete. Put the stick in the "
            "player or in rekordbox again, then eject it correctly.");
}

QString stickRootError(const QString& stickRoot) {
    return QCoreApplication::translate("DeviceLibraryPlusDb",
            "The stick folder is not available: %1")
            .arg(stickRoot);
}

QString changedError() {
    return QCoreApplication::translate("DeviceLibraryPlusDb",
            "The library on the stick changed after Mix Two read it. Read the "
            "stick again.");
}

QString versionError(const QString& version) {
    return QCoreApplication::translate("DeviceLibraryPlusDb",
            "The library on the stick has an unknown version (%1).")
            .arg(version);
}

QString damagedError() {
    return QCoreApplication::translate(
            "DeviceLibraryPlusDb", "The library on the stick is damaged.");
}

QString structureError() {
    return QCoreApplication::translate("DeviceLibraryPlusDb",
            "The library on the stick has an unknown structure.");
}

QString readError(const QString& reason) {
    return QCoreApplication::translate("DeviceLibraryPlusDb",
            "Cannot read the library on the stick: %1")
            .arg(reason);
}

QString writeError(const QString& reason) {
    return QCoreApplication::translate("DeviceLibraryPlusDb",
            "Cannot write the library to the stick: %1")
            .arg(reason);
}

QString folderError(const QString& folder) {
    return QCoreApplication::translate("DeviceLibraryPlusDb", "Cannot make the folder %1.")
            .arg(QDir::toNativeSeparators(folder));
}

QString copyError(const QString& reason) {
    return QCoreApplication::translate("DeviceLibraryPlusDb",
            "Cannot make a temporary copy of the library: %1")
            .arg(reason);
}

QString databaseError(const QString& reason) {
    return QCoreApplication::translate("DeviceLibraryPlusDb",
            "Cannot use the temporary copy of the library: %1")
            .arg(reason);
}

QString randomError() {
    return QCoreApplication::translate("DeviceLibraryPlusDb",
            "The computer cannot make random numbers for the library.");
}

QString batchOpenError() {
    return QCoreApplication::translate("DeviceLibraryPlusDb",
            "The export did not finish. The library on the stick did not change.");
}

// The reasons below are for a change that the caller did in the wrong order.

QString batchIsOpenReason() {
    return QCoreApplication::translate("DeviceLibraryPlusDb", "A batch is open already.");
}

QString noBatchReason() {
    return QCoreApplication::translate("DeviceLibraryPlusDb", "No batch is open.");
}

void setError(QString* pErrorMessage, const QString& message) {
    if (pErrorMessage) {
        *pErrorMessage = message;
    }
}

/// The first column of the first row. Empty if the query fails or has no row.
std::optional<QString> firstText(sqlite3* pDb, const char* sql) {
    const StatementPtr pStmt = prepare(pDb, sql);
    if (!pStmt || sqlite3_step(pStmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    return columnText(pStmt.get(), 0);
}

/// The SQL text of all objects of the database, except the tables of SQLite
/// itself. Empty if the query fails.
std::optional<QStringList> schemaOf(sqlite3* pDb) {
    // The indexes that SQLite makes itself have no SQL text. The tables of
    // SQLite itself have names that start with "sqlite_" (sqlite_stat1 after
    // ANALYZE, for example): they are not part of the structure of rekordbox.
    // The escape character makes the "_" a letter and not a wildcard.
    const StatementPtr pStmt = prepare(pDb,
            "SELECT sql FROM sqlite_master WHERE sql IS NOT NULL "
            "AND name NOT LIKE 'sqlite\\_%' ESCAPE '\\'");
    if (!pStmt) {
        return std::nullopt;
    }
    QStringList schema;
    int rc;
    while ((rc = sqlite3_step(pStmt.get())) == SQLITE_ROW) {
        schema.append(columnText(pStmt.get(), 0));
    }
    if (rc != SQLITE_DONE) {
        return std::nullopt;
    }
    return schema;
}

} // namespace

DeviceLibraryPlusDb::DeviceLibraryPlusDb(const QString& stickRoot)
        : m_stickRoot(stickRoot),
          // The name is the prefix that the export uses to remove the folders
          // of runs that did not finish.
          m_workDir(QDir::tempPath() + QStringLiteral("/mixtwo-dlp-XXXXXX")),
          m_pDb(nullptr) {
}

DeviceLibraryPlusDb::~DeviceLibraryPlusDb() {
    if (m_pDb) {
        if (m_batchOpen) {
            sqlite3_exec(m_pDb, "ROLLBACK", nullptr, nullptr, nullptr);
            m_batchOpen = false;
        }
        // This also removes the WAL files of the temporary copy.
        sqlite3_close_v2(m_pDb);
        m_pDb = nullptr;
    }
    // m_workDir removes the folder after this.
}

bool DeviceLibraryPlusDb::prepareWorkingFile(QString* pErrorMessage) {
    if (!m_workDir.isValid()) {
        setError(pErrorMessage, copyError(m_workDir.errorString()));
        return false;
    }
    m_workingFilePath = m_workDir.filePath(QStringLiteral("exportLibrary.db"));
    return true;
}

bool DeviceLibraryPlusDb::openWorkingFile(QString* pErrorMessage) {
    const int rc = sqlite3_open_v2(m_workingFilePath.toUtf8().constData(),
            &m_pDb,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
            nullptr);
    if (rc != SQLITE_OK) {
        const QString reason =
                m_pDb ? sqliteMessage(m_pDb) : QString::fromUtf8(sqlite3_errstr(rc));
        setError(pErrorMessage, databaseError(reason));
        return false;
    }
    // The copy is thrown away after save(), so a crash needs no recovery.
    QString reason;
    if (!execSql(m_pDb, "PRAGMA synchronous = OFF;", &reason)) {
        setError(pErrorMessage, databaseError(reason));
        return false;
    }
    return true;
}

bool DeviceLibraryPlusDb::buildNewLibrary(
        const QString& deviceName, qint64 masterDbId, QString* pErrorMessage) {
    QString reason;
    // The reserved bytes must be set before the first write.
    int reserveBytes = sqlcipher::kReserveBytes;
    if (sqlite3_file_control(m_pDb, "main", SQLITE_FCNTL_RESERVE_BYTES, &reserveBytes) !=
            SQLITE_OK) {
        setError(pErrorMessage, databaseError(sqliteMessage(m_pDb)));
        return false;
    }
    if (!execSql(m_pDb, "PRAGMA page_size = 4096;", &reason)) {
        setError(pErrorMessage, databaseError(reason));
        return false;
    }
    {
        // Journal mode WAL: bytes 18 and 19 of the file are 2.
        const auto mode = firstText(m_pDb, "PRAGMA journal_mode = WAL;");
        if (!mode || mode->compare(QLatin1String("wal"), Qt::CaseInsensitive) != 0) {
            setError(pErrorMessage,
                    databaseError(mode ? QStringLiteral("WAL") : sqliteMessage(m_pDb)));
            return false;
        }
    }

    if (!execSql(m_pDb, "BEGIN;", &reason)) {
        setError(pErrorMessage, databaseError(reason));
        return false;
    }
    for (const QString& statement : dlpschema::statements()) {
        if (!execSql(m_pDb, statement.toUtf8().constData(), &reason)) {
            sqlite3_exec(m_pDb, "ROLLBACK;", nullptr, nullptr, nullptr);
            setError(pErrorMessage, databaseError(reason));
            return false;
        }
    }
    if (!execSql(m_pDb, "COMMIT;", &reason)) {
        setError(pErrorMessage, databaseError(reason));
        return false;
    }

    if (!dlpschema::insertDefaultRows(m_pDb, &reason)) {
        setError(pErrorMessage, databaseError(reason));
        return false;
    }

    // NamedStatement checks the result of every bind; step() gives the first
    // error.
    NamedStatement insert(m_pDb,
            "INSERT INTO property(deviceName, dbVersion, numberOfContents, "
            "createdDate, backGroundColorType, myTagMasterDBID) "
            "VALUES (:deviceName, :dbVersion, 0, :createdDate, 0, :masterDbId)");
    insert.bindText(":deviceName", deviceName);
    insert.bindText(":dbVersion", kDbVersion);
    insert.bindText(":createdDate", QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")));
    insert.bindInt(":masterDbId", masterDbId);
    if (insert.step() != NamedStatement::Result::Done) {
        setError(pErrorMessage, databaseError(insert.error()));
        return false;
    }

    m_deviceName = deviceName;
    return true;
}

bool DeviceLibraryPlusDb::checkExistingLibrary(QString* pErrorMessage) {
    // The pages have a correct HMAC, but the b-trees can still be wrong. The
    // check reads every page. A library that is damaged gives this message and
    // not the message of a structure that is not known.
    const auto quickCheck = firstText(m_pDb, "PRAGMA quick_check");
    if (!quickCheck || *quickCheck != QLatin1String("ok")) {
        setError(pErrorMessage, damagedError());
        return false;
    }

    // The version first: another version has other tables, and the version is
    // the more useful message.
    const auto version = firstText(m_pDb, "SELECT dbVersion FROM property LIMIT 1");
    if (!version) {
        setError(pErrorMessage, structureError());
        return false;
    }
    if (*version != kDbVersion) {
        setError(pErrorMessage, versionError(*version));
        return false;
    }

    auto schema = schemaOf(m_pDb);
    QStringList expected = dlpschema::statements();
    if (!schema) {
        setError(pErrorMessage, structureError());
        return false;
    }
    // The order of the objects in sqlite_master does not matter.
    schema->sort();
    expected.sort();
    if (*schema != expected) {
        setError(pErrorMessage, structureError());
        return false;
    }

    const auto deviceName = firstText(m_pDb, "SELECT deviceName FROM property LIMIT 1");
    if (!deviceName) {
        setError(pErrorMessage, structureError());
        return false;
    }
    m_deviceName = *deviceName;
    return true;
}

// static
std::unique_ptr<DeviceLibraryPlusDb> DeviceLibraryPlusDb::create(const QString& stickRoot,
        const QString& deviceName,
        qint64 masterDbId,
        QString* pErrorMessage) {
    if (!isStickRootAvailable(stickRoot)) {
        setError(pErrorMessage, stickRootError(stickRoot));
        return nullptr;
    }
    const QString stickPath = stickPathOf(stickRoot);
    if (QFileInfo::exists(stickPath)) {
        setError(pErrorMessage, alreadyExistsError());
        return nullptr;
    }
    // A new library would hide the changes in the WAL file.
    if (hasPendingWal(stickPath)) {
        setError(pErrorMessage, incompleteError());
        return nullptr;
    }
    std::unique_ptr<DeviceLibraryPlusDb> pDb(new DeviceLibraryPlusDb(stickRoot));
    QString error;
    if (!pDb->prepareWorkingFile(&error) || !pDb->openWorkingFile(&error) ||
            !pDb->buildNewLibrary(deviceName, masterDbId, &error)) {
        setError(pErrorMessage, error);
        return nullptr;
    }
    pDb->m_salt = sqlcipher::randomBytes(sqlcipher::kSaltSize);
    if (pDb->m_salt.size() != sqlcipher::kSaltSize) {
        setError(pErrorMessage, randomError());
        return nullptr;
    }
    return pDb;
}

// static
std::unique_ptr<DeviceLibraryPlusDb> DeviceLibraryPlusDb::open(
        const QString& stickRoot, QString* pErrorMessage) {
    if (!isStickRootAvailable(stickRoot)) {
        setError(pErrorMessage, stickRootError(stickRoot));
        return nullptr;
    }
    const QString stickPath = stickPathOf(stickRoot);
    if (!QFileInfo::exists(stickPath)) {
        setError(pErrorMessage, noLibraryError());
        return nullptr;
    }
    if (hasPendingWal(stickPath)) {
        setError(pErrorMessage, incompleteError());
        return nullptr;
    }

    std::unique_ptr<DeviceLibraryPlusDb> pDb(new DeviceLibraryPlusDb(stickRoot));
    QFile stickFile(stickPath);
    if (!stickFile.open(QIODevice::ReadOnly)) {
        setError(pErrorMessage, readError(stickFile.errorString()));
        return nullptr;
    }
    const QByteArray encrypted = stickFile.readAll();
    if (stickFile.error() != QFileDevice::NoError) {
        setError(pErrorMessage, readError(stickFile.errorString()));
        return nullptr;
    }
    stickFile.close();
    // The values of the bytes that were read: save() refuses if the file on the
    // stick is not the same.
    pDb->m_knownStickFile = StickFileState{
            encrypted.size(), QFileInfo(stickPath).lastModified(), sha256Of(encrypted)};

    QString error;
    const auto plain = sqlcipher::decrypt(encrypted, deviceLibraryPlusPassphrase(), &error);
    if (!plain) {
        setError(pErrorMessage, error);
        return nullptr;
    }
    pDb->m_salt = sqlcipher::saltOf(encrypted);

    if (!pDb->prepareWorkingFile(&error)) {
        setError(pErrorMessage, error);
        return nullptr;
    }
    {
        QFile workingFile(pDb->m_workingFilePath);
        if (!workingFile.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
                workingFile.write(*plain) != plain->size() || !workingFile.flush()) {
            setError(pErrorMessage, copyError(workingFile.errorString()));
            return nullptr;
        }
    }
    if (!pDb->openWorkingFile(&error) || !pDb->checkExistingLibrary(&error)) {
        setError(pErrorMessage, error);
        return nullptr;
    }
    return pDb;
}

bool DeviceLibraryPlusDb::save(QString* pErrorMessage) {
    // The changes of an open batch are not in the file yet.
    if (m_batchOpen) {
        setError(pErrorMessage, batchOpenError());
        return false;
    }
    if (!m_lastError.isEmpty()) {
        setError(pErrorMessage, writeError(m_lastError));
        return false;
    }

    // Move all changes from the WAL file into the database file.
    {
        const StatementPtr pCheckpoint = prepare(m_pDb, "PRAGMA wal_checkpoint(TRUNCATE)");
        // The first column is 1 if the checkpoint could not finish.
        if (!pCheckpoint || sqlite3_step(pCheckpoint.get()) != SQLITE_ROW ||
                sqlite3_column_int(pCheckpoint.get(), 0) != 0) {
            setError(pErrorMessage, writeError(sqliteMessage(m_pDb)));
            return false;
        }
    }

    QFile workingFile(m_workingFilePath);
    if (!workingFile.open(QIODevice::ReadOnly)) {
        setError(pErrorMessage, writeError(workingFile.errorString()));
        return false;
    }
    const QByteArray plain = workingFile.readAll();
    if (workingFile.error() != QFileDevice::NoError) {
        setError(pErrorMessage, writeError(workingFile.errorString()));
        return false;
    }
    workingFile.close();

    QString error;
    const auto encrypted = sqlcipher::encrypt(
            plain, deviceLibraryPlusPassphrase(), m_salt, {}, &error);
    if (!encrypted) {
        setError(pErrorMessage, writeError(error));
        return false;
    }

    // The encryption takes time, so the stick is checked again here, just
    // before the write.
    if (!checkStickBeforeWrite(pErrorMessage)) {
        return false;
    }
    const QString stickPath = stickPathOf(m_stickRoot);
    const QString folder = QFileInfo(stickPath).absolutePath();
    if (!makeLibraryFolders(m_stickRoot)) {
        setError(pErrorMessage, writeError(folderError(folder)));
        return false;
    }
    // QSaveFile writes a temporary file next to the library file. commit()
    // syncs it to the disk and renames it over the library file. The old file
    // stays until the rename.
    QSaveFile stickFile(stickPath);
    stickFile.setDirectWriteFallback(false);
    if (!stickFile.open(QIODevice::WriteOnly)) {
        setError(pErrorMessage, writeError(stickFile.errorString()));
        return false;
    }
    if (stickFile.write(*encrypted) != encrypted->size()) {
        setError(pErrorMessage, writeError(stickFile.errorString()));
        stickFile.cancelWriting();
        return false;
    }
    if (!stickFile.commit()) {
        setError(pErrorMessage, writeError(stickFile.errorString()));
        return false;
    }
    m_knownStickFile = StickFileState{
            encrypted->size(), QFileInfo(stickPath).lastModified(), sha256Of(*encrypted)};
    return true;
}

bool DeviceLibraryPlusDb::checkStickBeforeWrite(QString* pErrorMessage) const {
    // A removed stick must not come back as a folder on the disk of the computer.
    if (!isStickRootAvailable(m_stickRoot)) {
        setError(pErrorMessage, stickRootError(m_stickRoot));
        return false;
    }
    const QString stickPath = stickPathOf(m_stickRoot);
    if (hasPendingWal(stickPath)) {
        setError(pErrorMessage, incompleteError());
        return false;
    }

    const QFileInfo info(stickPath);
    if (!m_knownStickFile) {
        // The library is new. A file now is the library of another program.
        if (info.exists()) {
            setError(pErrorMessage, changedError());
            return false;
        }
        return true;
    }
    // The cheap values first, then the bytes.
    if (!info.isFile() || info.size() != m_knownStickFile->size ||
            info.lastModified() != m_knownStickFile->modified) {
        setError(pErrorMessage, changedError());
        return false;
    }
    QFile stickFile(stickPath);
    if (!stickFile.open(QIODevice::ReadOnly)) {
        setError(pErrorMessage, writeError(stickFile.errorString()));
        return false;
    }
    const QByteArray bytes = stickFile.readAll();
    if (stickFile.error() != QFileDevice::NoError) {
        setError(pErrorMessage, writeError(stickFile.errorString()));
        return false;
    }
    if (sha256Of(bytes) != m_knownStickFile->sha256) {
        setError(pErrorMessage, changedError());
        return false;
    }
    return true;
}

bool DeviceLibraryPlusDb::beginBatch() {
    if (m_batchOpen) {
        failChange(batchIsOpenReason());
        return false;
    }
    QString reason;
    if (!execSql(m_pDb, "BEGIN", &reason)) {
        failChange(reason);
        return false;
    }
    m_batchOpen = true;
    return true;
}

bool DeviceLibraryPlusDb::commitBatch() {
    if (!m_batchOpen) {
        failChange(noBatchReason());
        return false;
    }
    QString reason;
    if (!execSql(m_pDb, "COMMIT", &reason)) {
        failChange(reason);
        // A COMMIT that fails can leave the transaction open, for example for
        // a constraint that SQLite checks at the commit. The library is not
        // complete, so the batch ends here.
        sqlite3_exec(m_pDb, "ROLLBACK", nullptr, nullptr, nullptr);
        m_batchOpen = false;
        clearNameCaches();
        return false;
    }
    m_batchOpen = false;
    return true;
}

bool DeviceLibraryPlusDb::rollbackBatch() {
    if (!m_batchOpen) {
        failChange(noBatchReason());
        return false;
    }
    // The IDs of the rows that are gone must not stay in the caches.
    m_batchOpen = false;
    clearNameCaches();
    QString reason;
    if (!execSql(m_pDb, "ROLLBACK", &reason)) {
        failChange(reason);
        return false;
    }
    return true;
}

QString DeviceLibraryPlusDb::deviceName() const {
    return m_deviceName;
}

QString DeviceLibraryPlusDb::lastErrorMessage() const {
    return m_lastError;
}

sqlite3* DeviceLibraryPlusDb::handle() const {
    return m_pDb;
}

void DeviceLibraryPlusDb::setLastError(const QString& message) {
    if (m_lastError.isEmpty()) {
        m_lastError = message;
    }
}

} // namespace rekordbox
} // namespace mixxx
