#pragma once

#include <QByteArray>

namespace mixxx {
namespace rekordbox {

/// Mix Two: the passphrase of rekordbox's Device Library Plus database
/// (exportLibrary.db). It is the same on every stick. The pyrekordbox project
/// (MIT license) publishes it. It is the SQLCipher passphrase: the codec in
/// sqlciphercodec.h derives the page keys from it and the salt of the file.
///
/// Never write the passphrase to a log, a message or a test output.
QByteArray deviceLibraryPlusPassphrase();

} // namespace rekordbox
} // namespace mixxx
