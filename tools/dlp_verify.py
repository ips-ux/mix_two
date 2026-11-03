#!/usr/bin/env python3
"""AI-generated (Claude Code).

Independent check of a Device Library Plus stick, with a parser Mix Two did not
write (pyrekordbox).

Opens PIONEER/rekordbox/exportLibrary.db of a stick and prints one line per
check: "PASS <name>" or "FAIL <name>: <reason>". The exit code is 0 when every
check passes and 1 otherwise. Usage:

    python -I tools/dlp_verify.py <stickRoot> [--reference <stickRoot>]

Checks:
  integrity_check      sqlcipher3 opens the file with pyrekordbox's key and
                       PRAGMA integrity_check gives "ok".
  onelibrary_tables    pyrekordbox OneLibrary reads every table through its
                       get_* method (the row count of each table is listed).
  property             "property" has one row and dbVersion is '1000'.
  schema_vs_reference  (only with --reference) the sqlite_master statements of
                       the two databases are the same. The tables of SQLite
                       itself (names that start with sqlite_, for example
                       sqlite_stat1) are not compared.
  wal                  (silent when it passes) the stick has no
                       exportLibrary.db-wal with content. If it has
                       one, the library in the main file is not
                       complete.

The script never writes to a stick: it copies exportLibrary.db (and nothing
else) into a temporary folder and opens only the copies. It never prints the
database key.

Set up (the environment goes outside the repository):

    py -m venv <venv>
    <venv>/Scripts/python -m pip install sqlcipher3-wheels==0.5.7 \\
        "git+https://github.com/dylanljones/pyrekordbox@02a208837434e4c05c0a222e8fe6583e6ac0547f"
"""

import argparse
import contextlib
import pathlib
import shutil
import sys
import tempfile

try:
    from pyrekordbox.onelibrary import OneLibrary
    from pyrekordbox.onelibrary.database import BLOB
    from pyrekordbox.utils import deobfuscate
    from sqlcipher3 import dbapi2 as sqlcipher
except ImportError as exc:
    sys.exit(
        f"dlp_verify: {exc}. Install sqlcipher3-wheels and pyrekordbox "
        "(see the docstring)."
    )

DB_PARTS = ("PIONEER", "rekordbox", "exportLibrary.db")
EXPECTED_DB_VERSION = "1000"
# Not a table of SQLite itself (sqlite_stat1 after ANALYZE, for example). The
# backslash is the escape character: the "_" is a letter and not a wildcard.
NOT_SQLITE_OWN = r"name NOT LIKE 'sqlite\_%' ESCAPE '\'"


class CheckFailed(Exception):
    """A check found a problem. The first line is the reason; more lines
    are details."""


class Report:
    """Prints the result of each check. Takes the key only to keep it out of
    the output."""

    def __init__(self, key):
        self._key = key
        self.failed = False

    def fail(self, name, text):
        self.failed = True
        reason, _, rest = text.replace(self._key, "<key>").partition("\n")
        print(f"FAIL {name}: {reason}")
        for line in rest.splitlines():
            print(f"  {line}")

    def run(self, name, check, *args):
        try:
            details = check(*args)
        except CheckFailed as exc:
            self.fail(name, str(exc))
        except Exception as exc:
            # Any other error in a check is a FAIL too, not a crash.
            # SQLAlchemy errors add the whole statement.
            lines = str(exc).splitlines()
            self.fail(
                name, f"{type(exc).__name__}: {lines[0] if lines else ''}"
            )
        else:
            print(f"PASS {name}")
            for line in details or []:
                print(f"  {line}")


def db_path(stick):
    return pathlib.Path(stick).joinpath(*DB_PARTS)


def copy_db(stick, folder):
    """Copy exportLibrary.db of the stick into folder. The stick is only
    read."""
    source = db_path(stick)
    if not source.is_file():
        raise CheckFailed(f"{source} does not exist")
    target = pathlib.Path(folder) / source.name
    with open(source, "rb") as src, open(target, "wb") as dst:
        shutil.copyfileobj(src, dst)
    return target


def query_rows(db_copy, key, sql):
    """Run one statement on the copy with sqlcipher3 (SQLCipher 4 defaults)
    and give the rows."""
    with contextlib.closing(sqlcipher.connect(str(db_copy))) as conn:
        # SQLCipher prints its own errors to stderr (in a form that is hard to
        # read); the FAIL line gives the reason. The setting is for the whole
        # process.
        conn.execute("PRAGMA cipher_log_level = NONE")
        # The key has no quote characters, so it goes into the PRAGMA as it is.
        conn.execute(f"PRAGMA key = '{key}'")
        return conn.execute(sql).fetchall()


def wal_problem(stick):
    """Give the reason when the stick has a write-ahead log with content,
    else None."""
    wal = db_path(stick).with_name(DB_PARTS[-1] + "-wal")
    if wal.exists() and wal.stat().st_size > 0:
        return (
            f"{wal} has {wal.stat().st_size} bytes: "
            "the library on the stick is not complete"
        )
    return None


def check_integrity(db_copy, key):
    """sqlcipher3 opens the file with the key and integrity_check gives ok."""
    try:
        rows = query_rows(db_copy, key, "PRAGMA integrity_check")
    except sqlcipher.DatabaseError as exc:
        # On a page that does not decrypt SQLite says only "SQL logic error".
        # The SQLCipher check names the pages.
        pages = [
            r[0]
            for r in query_rows(db_copy, key, "PRAGMA cipher_integrity_check")
        ]
        raise CheckFailed(
            "; ".join([f"integrity_check raised: {exc}", *pages[:5]])
        ) from exc
    if rows != [("ok",)]:
        raise CheckFailed(
            "integrity_check gave: " + "; ".join(str(r[0]) for r in rows[:5])
        )


def check_onelibrary(db_copy, key):
    """OneLibrary reads every table through its get_* method."""
    counts = {}
    db = OneLibrary(db_copy)
    try:
        for method in sorted(
            n for n in dir(OneLibrary) if n.startswith("get_")
        ):
            query = getattr(db, method)()
            table = query.column_descriptions[0]["entity"].__tablename__
            counts[table] = len(query.all())
    finally:
        db.close()
        # Else the copy stays open and the temporary folder cannot go.
        db.engine.dispose()
    sql = (
        "SELECT name FROM sqlite_master WHERE type = 'table' "
        f"AND {NOT_SQLITE_OWN}"
    )
    unread = {r[0] for r in query_rows(db_copy, key, sql)} - counts.keys()
    if unread:
        raise CheckFailed(
            "tables that no get_* method reads: " + ", ".join(sorted(unread))
        )
    return [f"{table:<20} {counts[table]}" for table in sorted(counts)]


def check_property(db_copy, key):
    """property has one row and dbVersion is the text '1000'."""
    rows = query_rows(db_copy, key, "SELECT dbVersion FROM property")
    if len(rows) != 1:
        raise CheckFailed(f"property has {len(rows)} rows, expected 1")
    if rows[0][0] != EXPECTED_DB_VERSION:
        raise CheckFailed(
            f"dbVersion is {rows[0][0]!r}, expected {EXPECTED_DB_VERSION!r}"
        )


def check_schema(db_copy, reference_copy, key):
    """The sqlite_master statements are the same in the stick and the
    reference.

    The tables of SQLite itself (names that start with sqlite_) are not
    compared.
    """
    sql = (
        "SELECT sql FROM sqlite_master WHERE sql IS NOT NULL "
        f"AND {NOT_SQLITE_OWN}"
    )
    ours = {r[0] for r in query_rows(db_copy, key, sql)}
    theirs = {r[0] for r in query_rows(reference_copy, key, sql)}
    if ours == theirs:
        return
    lines = [
        f"{len(ours - theirs)} statement(s) only in the stick, "
        f"{len(theirs - ours)} only in the reference"
    ]
    for label, only in (
        ("stick", ours - theirs),
        ("reference", theirs - ours),
    ):
        for statement in sorted(only):
            lines.append(f"only in the {label}:")
            lines.extend("    " + line for line in statement.splitlines())
    raise CheckFailed("\n".join(lines))


def main():
    parser = argparse.ArgumentParser(
        description="Check a Device Library Plus stick with pyrekordbox."
    )
    parser.add_argument(
        "stick", help="root folder of the stick (the folder with PIONEER)"
    )
    parser.add_argument(
        "--reference",
        metavar="STICK",
        help="root folder of a stick that rekordbox wrote; "
        "compare the schema with it",
    )
    args = parser.parse_args()

    sticks = {"stick": args.stick}
    if args.reference:
        sticks["reference"] = args.reference

    def name_of(base, label):
        return base if label == "stick" else f"{base} ({label})"

    key = deobfuscate(BLOB)
    report = Report(key)
    for label, stick in sticks.items():
        # A PASS line for this check would be noise.
        problem = wal_problem(stick)
        if problem:
            report.fail(name_of("wal", label), problem)

    with tempfile.TemporaryDirectory() as tmp:
        copies = {}
        for label, stick in sticks.items():
            folder = pathlib.Path(tmp) / label
            folder.mkdir()
            try:
                copies[label] = copy_db(stick, folder)
            except (CheckFailed, OSError) as exc:
                report.fail(name_of("copy", label), str(exc))

        if "stick" in copies:
            report.run(
                "integrity_check", check_integrity, copies["stick"], key
            )
            report.run(
                "onelibrary_tables", check_onelibrary, copies["stick"], key
            )
            report.run("property", check_property, copies["stick"], key)
        if "stick" in copies and "reference" in copies:
            report.run(
                "schema_vs_reference",
                check_schema,
                copies["stick"],
                copies["reference"],
                key,
            )

    return 1 if report.failed else 0


if __name__ == "__main__":
    sys.exit(main())
