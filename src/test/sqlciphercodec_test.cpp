#include "library/rekordbox/sqlciphercodec.h"

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include "test/mixxxtest.h"

namespace {

using namespace mixxx::rekordbox::sqlcipher;

const QByteArray kPassphrase = QByteArrayLiteral("mix-two-test-passphrase");
const QByteArray kPlainMagic = QByteArrayLiteral("SQLite format 3\0");

/// Reads a whole file. The result is empty if the file cannot be read.
QByteArray readFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

class SqlCipherCodecTest : public MixxxTest {
  protected:
    /// tools/rekordbox/make_sqlcipher_fixture.py wrote this file with the
    /// real SQLCipher 4 library.
    QByteArray fixture() const {
        return readFile(getTestDir().filePath(QStringLiteral("rekordbox/sqlcipher4_fixture.db")));
    }

    /// The IVs of an existing encrypted file.
    static IvSource ivsOf(const QByteArray& encrypted) {
        return [&encrypted](quint32 pageNumber) {
            return encrypted.mid(
                    static_cast<int>(pageNumber - 1) * kPageSize + kPageSize - kReserveBytes,
                    kIvSize);
        };
    }
};

TEST_F(SqlCipherCodecTest, DecryptsRealSqlCipherFile) {
    const QByteArray encrypted = fixture();
    ASSERT_FALSE(encrypted.isEmpty());
    ASSERT_EQ(0, encrypted.size() % kPageSize);
    // An encrypted file does not look like a SQLite file.
    ASSERT_FALSE(encrypted.startsWith(kPlainMagic));

    QString error;
    const auto plain = decrypt(encrypted, kPassphrase, &error);
    ASSERT_TRUE(plain.has_value()) << qPrintable(error);
    EXPECT_EQ(encrypted.size(), plain->size());
    EXPECT_TRUE(plain->startsWith(kPlainMagic));
    EXPECT_EQ(kReserveBytes, static_cast<quint8>(plain->at(20)));
    // The reserved bytes of every plain page are zero.
    for (int page = 1; page <= plain->size() / kPageSize; ++page) {
        EXPECT_EQ(QByteArray(kReserveBytes, '\0'),
                plain->mid(page * kPageSize - kReserveBytes, kReserveBytes))
                << "page " << page;
    }

    // SQLite itself reads the plain file.
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString path = tempDir.filePath(QStringLiteral("plain.db"));
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        ASSERT_EQ(plain->size(), file.write(*plain));
    }
    sqlite3* pDb = nullptr;
    ASSERT_EQ(SQLITE_OK,
            sqlite3_open_v2(path.toUtf8().constData(), &pDb, SQLITE_OPEN_READONLY, nullptr));
    sqlite3_stmt* pStmt = nullptr;
    ASSERT_EQ(SQLITE_OK,
            sqlite3_prepare_v2(pDb, "SELECT name FROM t WHERE id = 1", -1, &pStmt, nullptr))
            << sqlite3_errmsg(pDb);
    ASSERT_EQ(SQLITE_ROW, sqlite3_step(pStmt));
    EXPECT_EQ(QString::fromUtf8("Ünïcødé 日本語 🎧"),
            QString::fromUtf8(
                    reinterpret_cast<const char*>(sqlite3_column_text(pStmt, 0))));
    sqlite3_finalize(pStmt);
    sqlite3_close(pDb);
}

TEST_F(SqlCipherCodecTest, ReencryptIsByteIdentical) {
    const QByteArray encrypted = fixture();
    ASSERT_FALSE(encrypted.isEmpty());
    QString error;
    const auto plain = decrypt(encrypted, kPassphrase, &error);
    ASSERT_TRUE(plain.has_value()) << qPrintable(error);

    const auto again = encrypt(*plain, kPassphrase, saltOf(encrypted), ivsOf(encrypted), &error);
    ASSERT_TRUE(again.has_value()) << qPrintable(error);
    EXPECT_EQ(encrypted, *again);
}

TEST_F(SqlCipherCodecTest, RoundTripWithRandomIvs) {
    const QByteArray encrypted = fixture();
    ASSERT_FALSE(encrypted.isEmpty());
    QString error;
    const auto plain = decrypt(encrypted, kPassphrase, &error);
    ASSERT_TRUE(plain.has_value()) << qPrintable(error);

    const QByteArray salt = randomBytes(kSaltSize);
    ASSERT_EQ(kSaltSize, salt.size());
    const auto other = encrypt(*plain, kPassphrase, salt, {}, &error);
    ASSERT_TRUE(other.has_value()) << qPrintable(error);
    EXPECT_EQ(plain->size(), other->size());
    EXPECT_EQ(salt, saltOf(*other));
    EXPECT_NE(encrypted, *other);

    const auto back = decrypt(*other, kPassphrase, &error);
    ASSERT_TRUE(back.has_value()) << qPrintable(error);
    EXPECT_EQ(*plain, *back);
}

TEST_F(SqlCipherCodecTest, RejectsWrongPassphrase) {
    const QByteArray encrypted = fixture();
    ASSERT_FALSE(encrypted.isEmpty());
    QString error;
    EXPECT_FALSE(decrypt(encrypted, QByteArrayLiteral("wrong"), &error).has_value());
    EXPECT_TRUE(error.contains(QStringLiteral("page 1"))) << qPrintable(error);
}

TEST_F(SqlCipherCodecTest, RejectsChangedByte) {
    QByteArray encrypted = fixture();
    ASSERT_GT(encrypted.size(), 5000);
    // Byte 5000 is in page 2.
    encrypted[5000] = static_cast<char>(encrypted[5000] ^ 1);
    QString error;
    EXPECT_FALSE(decrypt(encrypted, kPassphrase, &error).has_value());
    EXPECT_TRUE(error.contains(QStringLiteral("page 2"))) << qPrintable(error);
}

TEST_F(SqlCipherCodecTest, RejectsBadSize) {
    const QByteArray encrypted = fixture();
    ASSERT_GT(encrypted.size(), 5000);
    QString error;
    EXPECT_FALSE(decrypt(encrypted.left(5000), kPassphrase, &error).has_value());
    EXPECT_FALSE(error.isEmpty());

    EXPECT_FALSE(decrypt(QByteArray(), kPassphrase, &error).has_value());

    QByteArray plain(5000, '\0');
    plain.replace(0, kPlainMagic.size(), kPlainMagic);
    plain[20] = static_cast<char>(kReserveBytes);
    error.clear();
    EXPECT_FALSE(encrypt(plain, kPassphrase, randomBytes(kSaltSize), {}, &error).has_value());
    EXPECT_FALSE(error.isEmpty());
}

TEST_F(SqlCipherCodecTest, EncryptRejectsPlainWithoutReserve) {
    const QByteArray encrypted = fixture();
    ASSERT_FALSE(encrypted.isEmpty());
    QString error;
    const auto plain = decrypt(encrypted, kPassphrase, &error);
    ASSERT_TRUE(plain.has_value()) << qPrintable(error);
    const QByteArray salt = saltOf(encrypted);

    QByteArray noReserve = *plain;
    noReserve[20] = 0;
    error.clear();
    EXPECT_FALSE(encrypt(noReserve, kPassphrase, salt, {}, &error).has_value());
    EXPECT_FALSE(error.isEmpty());

    // Not a SQLite file at all (the file is still encrypted).
    error.clear();
    EXPECT_FALSE(encrypt(encrypted, kPassphrase, salt, {}, &error).has_value());
    EXPECT_FALSE(error.isEmpty());

    // The same plain pages are accepted when the header is correct.
    EXPECT_TRUE(encrypt(*plain, kPassphrase, salt, {}, &error).has_value()) << qPrintable(error);
}

} // namespace
