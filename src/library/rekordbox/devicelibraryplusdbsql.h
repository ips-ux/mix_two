#pragma once

// Mix Two: the SQLite helpers that devicelibraryplusdb.cpp and
// devicelibraryplusdbrows.cpp share. This header is private to the two files.

#include <sqlite3.h>

#include <QString>
#include <memory>
#include <optional>

namespace mixxx {
namespace rekordbox {
namespace dlpsql {

inline QString sqliteMessage(sqlite3* pDb) {
    return QString::fromUtf8(sqlite3_errmsg(pDb));
}

/// Runs SQL text. On an error it sets *pErrorMessage to the message of SQLite.
inline bool execSql(sqlite3* pDb, const char* sql, QString* pErrorMessage) {
    char* pMessage = nullptr;
    if (sqlite3_exec(pDb, sql, nullptr, nullptr, &pMessage) != SQLITE_OK) {
        *pErrorMessage = QString::fromUtf8(pMessage ? pMessage : sqlite3_errmsg(pDb));
        sqlite3_free(pMessage);
        return false;
    }
    return true;
}

struct StatementDeleter {
    void operator()(sqlite3_stmt* pStmt) const {
        sqlite3_finalize(pStmt);
    }
};
using StatementPtr = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

inline StatementPtr prepare(sqlite3* pDb, const char* sql) {
    sqlite3_stmt* pStmt = nullptr;
    if (sqlite3_prepare_v2(pDb, sql, -1, &pStmt, nullptr) != SQLITE_OK) {
        sqlite3_finalize(pStmt);
        return nullptr;
    }
    return StatementPtr(pStmt);
}

inline QString columnText(sqlite3_stmt* pStmt, int column) {
    return QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(pStmt, column)));
}

// The helpers of the typed row methods.

/// A prepared statement with named parameters (":name"). If the statement
/// cannot be prepared, or a value cannot be bound, step() gives the error.
class NamedStatement {
  public:
    enum class Result {
        Row,
        Done,
        Error,
    };

    NamedStatement(sqlite3* pDb, const char* sql)
            : m_pDb(pDb),
              m_pStmt(prepare(pDb, sql)) {
        if (!m_pStmt) {
            m_error = sqliteMessage(pDb);
        }
    }

    NamedStatement(const NamedStatement&) = delete;
    NamedStatement& operator=(const NamedStatement&) = delete;

    /// Binds UTF-8 text. An empty text is the empty string and not NULL.
    void bindText(const char* name, const QString& value) {
        if (!m_pStmt) {
            return;
        }
        const QByteArray utf8 = value.toUtf8();
        keepError(sqlite3_bind_text(m_pStmt.get(),
                indexOf(name),
                utf8.isEmpty() ? "" : utf8.constData(),
                static_cast<int>(utf8.size()),
                SQLITE_TRANSIENT));
    }

    void bindInt(const char* name, qint64 value) {
        if (m_pStmt) {
            keepError(sqlite3_bind_int64(m_pStmt.get(), indexOf(name), value));
        }
    }

    /// Binds NULL for an empty value.
    void bindOptionalInt(const char* name, std::optional<qint64> value) {
        if (!m_pStmt) {
            return;
        }
        keepError(value ? sqlite3_bind_int64(m_pStmt.get(), indexOf(name), *value)
                        : sqlite3_bind_null(m_pStmt.get(), indexOf(name)));
    }

    /// Runs the statement up to the next row.
    Result step() {
        if (m_error) {
            return Result::Error;
        }
        const int rc = sqlite3_step(m_pStmt.get());
        if (rc == SQLITE_ROW) {
            return Result::Row;
        }
        if (rc == SQLITE_DONE) {
            return Result::Done;
        }
        m_error = sqliteMessage(m_pDb);
        return Result::Error;
    }

    /// Makes the statement ready for new values.
    void reset() {
        sqlite3_reset(m_pStmt.get());
    }

    qint64 columnInt(int column) const {
        return sqlite3_column_int64(m_pStmt.get(), column);
    }

    /// The text of a column. A NULL value gives a null string.
    QString columnText(int column) const {
        return dlpsql::columnText(m_pStmt.get(), column);
    }

    bool columnIsNull(int column) const {
        return sqlite3_column_type(m_pStmt.get(), column) == SQLITE_NULL;
    }

    /// The rowid of the last INSERT in the database.
    qint64 insertedId() const {
        return sqlite3_last_insert_rowid(m_pDb);
    }

    /// The number of rows that the last INSERT, UPDATE or DELETE changed.
    int changes() const {
        return sqlite3_changes(m_pDb);
    }

    /// The first error, or an empty string.
    QString error() const {
        return m_error.value_or(QString());
    }

  private:
    int indexOf(const char* name) const {
        // 0 if the statement has no such parameter; the bind then fails.
        return sqlite3_bind_parameter_index(m_pStmt.get(), name);
    }

    void keepError(int rc) {
        if (rc != SQLITE_OK && !m_error) {
            m_error = sqliteMessage(m_pDb);
        }
    }

    sqlite3* const m_pDb;
    const StatementPtr m_pStmt;
    std::optional<QString> m_error;
};

/// Makes all changes between the constructor and release() one commit. If the
/// object is destroyed before release(), it undoes the changes.
class Savepoint {
  public:
    explicit Savepoint(sqlite3* pDb)
            : m_pDb(pDb),
              m_open(execSql(pDb, "SAVEPOINT mixtwo_change", &m_error)) {
    }

    ~Savepoint() {
        if (m_open) {
            sqlite3_exec(m_pDb, "ROLLBACK TO mixtwo_change", nullptr, nullptr, nullptr);
            sqlite3_exec(m_pDb, "RELEASE mixtwo_change", nullptr, nullptr, nullptr);
        }
    }

    Savepoint(const Savepoint&) = delete;
    Savepoint& operator=(const Savepoint&) = delete;

    /// False if the savepoint could not start; then error() tells why.
    bool isOpen() const {
        return m_open;
    }

    /// Ends the savepoint and keeps the changes. False if SQLite cannot do
    /// this; then error() tells why, and the destructor undoes the changes.
    bool release() {
        if (!execSql(m_pDb, "RELEASE mixtwo_change", &m_error)) {
            return false;
        }
        m_open = false;
        return true;
    }

    QString error() const {
        return m_error;
    }

  private:
    sqlite3* const m_pDb;
    QString m_error;
    bool m_open;
};

} // namespace dlpsql
} // namespace rekordbox
} // namespace mixxx
