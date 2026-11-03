#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <functional>
#include <optional>

namespace mixxx {
namespace rekordbox {
namespace sqlcipher {

// Mix Two: the SQLCipher 4 default settings, which rekordbox uses for the
// Device Library Plus database (exportLibrary.db).
//
// A file is a row of pages of kPageSize bytes. The first kSaltSize bytes of
// page 1 are the salt. All other bytes of a page are AES-256-CBC cipher text
// (no padding), except the last kReserveBytes: a kIvSize byte IV and a
// kHmacSize byte HMAC-SHA512 of the cipher text, the IV and the page number.
inline constexpr int kPageSize = 4096;
inline constexpr int kReserveBytes = 80;
inline constexpr int kSaltSize = 16;
inline constexpr int kIvSize = 16;
inline constexpr int kHmacSize = 64;
inline constexpr int kKdfIterations = 256000;

/// Returns kIvSize bytes for the 1-based page number. An empty function means
/// random IVs.
using IvSource = std::function<QByteArray(quint32 pageNumber)>;

/// Returns count random bytes from OpenSSL's generator (empty if it fails).
QByteArray randomBytes(int count);

/// Returns the salt of an encrypted file: its first kSaltSize bytes.
QByteArray saltOf(const QByteArray& encrypted);

/// Decrypts an encrypted file to a plain SQLite file with the same size. Each
/// plain page ends with kReserveBytes zero bytes. Checks the HMAC of every
/// page. On an error it returns an empty value and sets *pErrorMessage (if it
/// is not null) to a message for the user.
std::optional<QByteArray> decrypt(const QByteArray& encrypted,
        const QByteArray& passphrase,
        QString* pErrorMessage);

/// Encrypts a plain SQLite file (page size kPageSize, kReserveBytes reserved
/// on each page) with the given salt. The reserved bytes of the plain pages
/// are ignored. ivSource gives the IV of each page; with the IVs of an
/// existing file, encrypt() gives back that file. On an error it returns an
/// empty value and sets *pErrorMessage (if it is not null).
std::optional<QByteArray> encrypt(const QByteArray& plain,
        const QByteArray& passphrase,
        const QByteArray& salt,
        const IvSource& ivSource,
        QString* pErrorMessage);

} // namespace sqlcipher
} // namespace rekordbox
} // namespace mixxx
