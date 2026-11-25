#pragma once
// AI-generated (Claude Code). Start of AI-generated text.
// The artwork of a playlist (rekordbox: right-click a playlist > Add Artwork).
// Mix Two keeps its own copy of the picture in the library database (column
// Playlists.artwork, as PNG bytes), as it does for other data that it owns
// (for example the wave summary of a track). It never keeps a link to the
// file of the user, so the picture stays when the user moves or deletes the
// file.

#include <QByteArray>
#include <QImage>
#include <QString>

namespace mixxx {
namespace library {

/// A picture with a longer edge is scaled down before it is stored. The
/// aspect ratio stays. A smaller picture keeps its size.
constexpr int kMaxPlaylistArtworkEdge = 512;

/// Reads a JPEG or PNG file and returns the PNG bytes to store. The file is
/// opened for reading only; it is never changed. Returns an empty array if
/// the file cannot be read or is not a JPEG or PNG picture.
QByteArray makePlaylistArtworkFromFile(const QString& filePath);

/// The picture for stored bytes. Returns a null image for empty or bad bytes.
QImage playlistArtworkImage(const QByteArray& storedBytes);

} // namespace library
} // namespace mixxx

// End of AI-generated text.
