#pragma once

// AI-generated (Claude Code). Start of AI-generated text.

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

namespace mixxx {
namespace rekordbox {

/// Mix Two: the content of a legacy device library (`export.pdb`) as plain
/// data. One part of Mix Two makes it from the Device Library Plus database
/// of the stick, so that both formats hold the same library; the PDB writer
/// (legacyPdbBytes()) turns it into the bytes of `export.pdb`.
///
/// All IDs are the IDs of the legacy file: 32 bit, 1 and up. An ID of 0 means
/// "none". IDs are unique in each list. Paths are stick paths: from the root
/// of the stick, with '/' and a leading '/'.

/// A row with an ID and a name: artists, genres, keys, labels and colours.
struct LegacyNamedRow {
    quint32 id = 0;
    QString name;
};

struct LegacyAlbum {
    quint32 id = 0;
    quint32 artistId = 0;
    QString name;
};

struct LegacyArtwork {
    quint32 id = 0;
    /// For example "/PIONEER/Artwork/00001/a1.jpg".
    QString path;
};

struct LegacyTrack {
    quint32 id = 0;
    QString title;
    quint32 artistId = 0;
    quint32 composerId = 0;
    quint32 originalArtistId = 0;
    quint32 remixerId = 0;
    quint32 albumId = 0;
    quint32 genreId = 0;
    quint32 labelId = 0;
    quint32 keyId = 0;
    /// 0 for none, 1 to 8 for the rekordbox colours.
    quint32 colorId = 0;
    quint32 artworkId = 0;
    /// The BPM times 100.
    quint32 tempo = 0;
    /// In seconds.
    quint32 duration = 0;
    /// 0 to 5.
    quint32 rating = 0;
    quint32 year = 0;
    quint32 trackNumber = 0;
    quint32 discNumber = 0;
    quint32 playCount = 0;
    /// In kbit/s.
    quint32 bitrate = 0;
    /// In Hz.
    quint32 sampleRate = 0;
    /// In bits.
    quint32 sampleDepth = 0;
    /// In bytes.
    quint32 fileSize = 0;
    /// The rekordbox code of the file type (mp3 1, m4a 4, flac 5, wav 11,
    /// aiff 12), as in the Device Library Plus database.
    quint32 fileType = 0;
    /// The path of the audio file, for example "/Contents/Artist/Album/a.mp3".
    QString filePath;
    /// The file name only, for example "a.mp3".
    QString fileName;
    /// The path of ANLZ0000.DAT, for example
    /// "/PIONEER/USBANLZ/P016/0000875E/ANLZ0000.DAT". Both formats use the
    /// same ANLZ files.
    QString analyzePath;
    /// Dates are "yyyy-MM-dd"; an empty string for none.
    QString analyzeDate;
    QString dateAdded;
    QString releaseDate;
    QString mixName;
    QString comment;
    QString isrc;
    QString message;
    /// "Allow to auto load HotCue on CDJ / XDJ".
    bool autoLoadHotCues = true;
};

/// A playlist or a folder of the playlist tree.
struct LegacyPlaylistNode {
    quint32 id = 0;
    /// 0 for the root.
    quint32 parentId = 0;
    /// The position among the nodes with the same parent, 0 and up.
    quint32 sortOrder = 0;
    bool isFolder = false;
    QString name;
};

struct LegacyPlaylistEntry {
    quint32 playlistId = 0;
    quint32 trackId = 0;
    /// The position in the playlist, 1 and up.
    quint32 entryIndex = 0;
};

struct LegacyHistoryPlaylist {
    quint32 id = 0;
    QString name;
};

struct LegacyHistoryEntry {
    quint32 playlistId = 0;
    quint32 trackId = 0;
    /// The position in the history, 1 and up.
    quint32 entryIndex = 0;
};

struct LegacyLibrary {
    QList<LegacyTrack> tracks;
    QList<LegacyNamedRow> artists;
    QList<LegacyAlbum> albums;
    QList<LegacyNamedRow> genres;
    QList<LegacyNamedRow> keys;
    QList<LegacyNamedRow> labels;
    QList<LegacyNamedRow> colors;
    QList<LegacyArtwork> artwork;
    QList<LegacyPlaylistNode> playlists;
    QList<LegacyPlaylistEntry> playlistEntries;
    QList<LegacyHistoryPlaylist> historyPlaylists;
    QList<LegacyHistoryEntry> historyEntries;
};

/// The bytes of `export.pdb` for the library, as rekordbox 6.8.5 writes the
/// file. An empty array when the library cannot be written; then *pError
/// (if not null) tells why. Any thread can call it; it does no file I/O.
QByteArray legacyPdbBytes(const LegacyLibrary& library, QString* pError = nullptr);

} // namespace rekordbox
} // namespace mixxx

// End of AI-generated text.
