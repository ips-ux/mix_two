#pragma once

#include <QString>
#include <QStringList>

struct sqlite3;

namespace mixxx {
namespace rekordbox {
namespace dlpschema {

/// Mix Two: the 22 tables and 4 indexes of rekordbox's Device Library Plus
/// database (exportLibrary.db), as rekordbox wrote them (design section 10):
/// the SQL text of the 26 statements, tables first, then indexes. SQLite
/// stores this text in sqlite_master, so an existing library has this schema
/// when its text is the same.
const QStringList& statements();

/// Inserts the fixed rows of a new library: menuItem, color, category and
/// sort. The values are those of a library that rekordbox wrote. Does it in
/// one transaction. On an error it changes nothing and sets *pErrorMessage (if
/// it is not null).
bool insertDefaultRows(sqlite3* pDb, QString* pErrorMessage);

} // namespace dlpschema
} // namespace rekordbox
} // namespace mixxx
