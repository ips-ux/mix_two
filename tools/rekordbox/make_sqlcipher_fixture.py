#!/usr/bin/env python3
"""Make the SQLCipher 4 test fixture for the Device Library Plus codec.

Mix Two: writes a small database with the SQLCipher 4 default settings (the
settings rekordbox uses for exportLibrary.db). The codec tests decrypt this
file and encrypt it again, and compare the bytes.

Usage:
    python -I tools/rekordbox/make_sqlcipher_fixture.py <out.db>

Needs the sqlcipher3 module, for example in a scratch virtual environment:
    py -m venv "$SCRATCH/venv-sqlcipher"
    "$SCRATCH/venv-sqlcipher/Scripts/python" -m pip install \\
        sqlcipher3-wheels==0.5.7

The passphrase is a test value. The script refuses to overwrite a file.
"""

import os
import sys

from sqlcipher3 import dbapi2 as sqlcipher

PASSPHRASE = "mix-two-test-passphrase"
NAME = "\u00dcn\u00efc\u00f8d\u00e9 \u65e5\u672c\u8a9e \U0001f3a7"


def main(argv):
    if len(argv) != 2:
        print("Usage: make_sqlcipher_fixture.py <out.db>", file=sys.stderr)
        return 2
    path = argv[1]
    if os.path.exists(path):
        print(f"Refusing to overwrite {path}", file=sys.stderr)
        return 1

    connection = sqlcipher.connect(path)
    try:
        connection.execute(f"PRAGMA key = '{PASSPHRASE}'")
        connection.execute("PRAGMA cipher_compatibility = 4")
        connection.execute("PRAGMA journal_mode = DELETE")
        connection.execute(
            "CREATE TABLE t(id integer primary key, name varchar)"
        )
        connection.execute("INSERT INTO t VALUES (1, ?)", (NAME,))
        connection.commit()
    finally:
        connection.close()
    print(f"Wrote {path} ({os.path.getsize(path)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
