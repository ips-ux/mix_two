#include "library/rekordbox/sqlciphercodec.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <QCoreApplication>
#include <QtEndian>
#include <array>
#include <memory>

namespace mixxx {
namespace rekordbox {
namespace sqlcipher {

namespace {

constexpr int kKeySize = 32;
// A page is: [salt, page 1 only] | cipher text | IV | HMAC.
constexpr int kCipherEnd = kPageSize - kReserveBytes;
constexpr int kIvEnd = kPageSize - kHmacSize;
constexpr int kPageNumberSize = 4;
// The field "bytes reserved on each page" in the plain SQLite header.
constexpr int kReserveOffset = 20;

static_assert(kReserveBytes == kIvSize + kHmacSize);

const QByteArray kPlainMagic = QByteArrayLiteral("SQLite format 3\0");
static_assert(kSaltSize == 16, "The salt replaces the 16 byte SQLite header text.");

void setError(QString* pErrorMessage, const QString& text) {
    if (pErrorMessage) {
        *pErrorMessage = text;
    }
}

/// The two keys of one database. The destructor wipes them.
struct Keys {
    Keys() = default;
    Keys(const Keys&) = delete;
    Keys& operator=(const Keys&) = delete;
    ~Keys() {
        OPENSSL_cleanse(cipher.data(), cipher.size());
        OPENSSL_cleanse(hmac.data(), hmac.size());
    }

    std::array<unsigned char, kKeySize> cipher{};
    std::array<unsigned char, kKeySize> hmac{};
};

bool deriveKeys(const QByteArray& passphrase, const QByteArray& salt, Keys* pKeys) {
    if (PKCS5_PBKDF2_HMAC(passphrase.constData(),
                static_cast<int>(passphrase.size()),
                reinterpret_cast<const unsigned char*>(salt.constData()),
                static_cast<int>(salt.size()),
                kKdfIterations,
                EVP_sha512(),
                kKeySize,
                pKeys->cipher.data()) != 1) {
        return false;
    }
    // The HMAC key comes from the cipher key and the salt with each byte
    // XOR 0x3a, in 2 iterations.
    QByteArray hmacSalt = salt;
    for (char& c : hmacSalt) {
        c = static_cast<char>(c ^ 0x3a);
    }
    return PKCS5_PBKDF2_HMAC(reinterpret_cast<const char*>(pKeys->cipher.data()),
                   static_cast<int>(pKeys->cipher.size()),
                   reinterpret_cast<const unsigned char*>(hmacSalt.constData()),
                   static_cast<int>(hmacSalt.size()),
                   2,
                   EVP_sha512(),
                   kKeySize,
                   pKeys->hmac.data()) == 1;
}

/// AES-256-CBC without padding. The length must be a multiple of 16.
/// Returns an empty array on an error.
QByteArray aesCbc(bool doEncrypt,
        const Keys& keys,
        const unsigned char* pIv,
        const unsigned char* pIn,
        int length) {
    const std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> pCtx(
            EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!pCtx ||
            EVP_CipherInit_ex(pCtx.get(),
                    EVP_aes_256_cbc(),
                    nullptr,
                    keys.cipher.data(),
                    pIv,
                    doEncrypt ? 1 : 0) != 1 ||
            EVP_CIPHER_CTX_set_padding(pCtx.get(), 0) != 1) {
        return QByteArray();
    }
    QByteArray out(length, '\0');
    auto* pOut = reinterpret_cast<unsigned char*>(out.data());
    int written = 0;
    int finalWritten = 0;
    if (EVP_CipherUpdate(pCtx.get(), pOut, &written, pIn, length) != 1 ||
            EVP_CipherFinal_ex(pCtx.get(), pOut + written, &finalWritten) != 1 ||
            written + finalWritten != length) {
        return QByteArray();
    }
    return out;
}

/// Writes the HMAC-SHA512 of the bytes at pCipherAndIv (cipher text, then IV)
/// and the page number (32 bits, little endian) to pOut (kHmacSize bytes).
bool pageHmac(const Keys& keys,
        const char* pCipherAndIv,
        int length,
        quint32 pageNumber,
        unsigned char* pOut) {
    QByteArray data(pCipherAndIv, length);
    char number[kPageNumberSize];
    qToLittleEndian(pageNumber, number);
    data.append(number, kPageNumberSize);
    unsigned int outLength = 0;
    return HMAC(EVP_sha512(),
                   keys.hmac.data(),
                   static_cast<int>(keys.hmac.size()),
                   reinterpret_cast<const unsigned char*>(data.constData()),
                   static_cast<size_t>(data.size()),
                   pOut,
                   &outLength) != nullptr &&
            outLength == kHmacSize;
}

bool isWholePages(const QByteArray& data) {
    return !data.isEmpty() && data.size() % kPageSize == 0;
}

// The messages are for the user.
QString sizeError() {
    return QCoreApplication::translate("SqlCipherCodec", "The library file has an incorrect size.");
}

QString damagedError(quint32 pageNumber) {
    return QCoreApplication::translate("SqlCipherCodec",
            "The library file is damaged or has an unknown key (page %1).")
            .arg(pageNumber);
}

QString plainFormatError() {
    return QCoreApplication::translate("SqlCipherCodec",
            "The data is not a SQLite database with 80 reserved bytes on each page.");
}

QString saltError() {
    return QCoreApplication::translate("SqlCipherCodec", "The salt has an incorrect size.");
}

QString ivError() {
    return QCoreApplication::translate("SqlCipherCodec", "The IV has an incorrect size.");
}

QString cryptoError() {
    return QCoreApplication::translate(
            "SqlCipherCodec", "The encryption library did not work correctly.");
}

} // namespace

QByteArray randomBytes(int count) {
    if (count <= 0) {
        return QByteArray();
    }
    QByteArray bytes(count, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(bytes.data()), count) != 1) {
        return QByteArray();
    }
    return bytes;
}

QByteArray saltOf(const QByteArray& encrypted) {
    return encrypted.left(kSaltSize);
}

std::optional<QByteArray> decrypt(const QByteArray& encrypted,
        const QByteArray& passphrase,
        QString* pErrorMessage) {
    if (!isWholePages(encrypted)) {
        setError(pErrorMessage, sizeError());
        return std::nullopt;
    }
    Keys keys;
    if (!deriveKeys(passphrase, saltOf(encrypted), &keys)) {
        setError(pErrorMessage, cryptoError());
        return std::nullopt;
    }

    const qsizetype pageCount = encrypted.size() / kPageSize;
    QByteArray plain;
    plain.reserve(encrypted.size());
    for (qsizetype index = 0; index < pageCount; ++index) {
        const quint32 pageNumber = static_cast<quint32>(index + 1);
        const char* pPage = encrypted.constData() + index * kPageSize;
        // Page 1 starts with the salt in place of the SQLite header text.
        const int start = (index == 0) ? kSaltSize : 0;

        unsigned char expected[kHmacSize];
        if (!pageHmac(keys, pPage + start, kIvEnd - start, pageNumber, expected)) {
            setError(pErrorMessage, cryptoError());
            return std::nullopt;
        }
        if (CRYPTO_memcmp(expected, pPage + kIvEnd, kHmacSize) != 0) {
            setError(pErrorMessage, damagedError(pageNumber));
            return std::nullopt;
        }

        const QByteArray decrypted = aesCbc(false,
                keys,
                reinterpret_cast<const unsigned char*>(pPage + kCipherEnd),
                reinterpret_cast<const unsigned char*>(pPage + start),
                kCipherEnd - start);
        if (decrypted.isEmpty()) {
            setError(pErrorMessage, cryptoError());
            return std::nullopt;
        }
        if (index == 0) {
            plain.append(kPlainMagic);
        }
        plain.append(decrypted);
        plain.append(QByteArray(kReserveBytes, '\0'));
    }
    return plain;
}

std::optional<QByteArray> encrypt(const QByteArray& plain,
        const QByteArray& passphrase,
        const QByteArray& salt,
        const IvSource& ivSource,
        QString* pErrorMessage) {
    if (!isWholePages(plain)) {
        setError(pErrorMessage, sizeError());
        return std::nullopt;
    }
    if (!plain.startsWith(kPlainMagic) ||
            static_cast<quint8>(plain.at(kReserveOffset)) != kReserveBytes) {
        setError(pErrorMessage, plainFormatError());
        return std::nullopt;
    }
    if (salt.size() != kSaltSize) {
        setError(pErrorMessage, saltError());
        return std::nullopt;
    }
    Keys keys;
    if (!deriveKeys(passphrase, salt, &keys)) {
        setError(pErrorMessage, cryptoError());
        return std::nullopt;
    }

    const qsizetype pageCount = plain.size() / kPageSize;
    QByteArray encrypted;
    encrypted.reserve(plain.size());
    for (qsizetype index = 0; index < pageCount; ++index) {
        const quint32 pageNumber = static_cast<quint32>(index + 1);
        const char* pPage = plain.constData() + index * kPageSize;
        const int start = (index == 0) ? kSaltSize : 0;

        const QByteArray iv = ivSource ? ivSource(pageNumber) : randomBytes(kIvSize);
        if (iv.size() != kIvSize) {
            setError(pErrorMessage, ivSource ? ivError() : cryptoError());
            return std::nullopt;
        }
        const QByteArray cipherText = aesCbc(true,
                keys,
                reinterpret_cast<const unsigned char*>(iv.constData()),
                reinterpret_cast<const unsigned char*>(pPage + start),
                kCipherEnd - start);
        if (cipherText.isEmpty()) {
            setError(pErrorMessage, cryptoError());
            return std::nullopt;
        }

        const qsizetype pageStart = encrypted.size();
        if (index == 0) {
            encrypted.append(salt);
        }
        encrypted.append(cipherText);
        encrypted.append(iv);
        unsigned char hmac[kHmacSize];
        if (!pageHmac(keys,
                    encrypted.constData() + pageStart + start,
                    kIvEnd - start,
                    pageNumber,
                    hmac)) {
            setError(pErrorMessage, cryptoError());
            return std::nullopt;
        }
        encrypted.append(reinterpret_cast<const char*>(hmac), kHmacSize);
    }
    return encrypted;
}

} // namespace sqlcipher
} // namespace rekordbox
} // namespace mixxx
