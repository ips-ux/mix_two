// AI-generated (Claude Code). Start of AI-generated text.

#include "library/rekordbox/legacylibrary.h"

#include <QDate>
#include <QObject>
#include <QSet>
#include <QStringView>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace mixxx {
namespace rekordbox {
namespace {
constexpr int kPageSize = 4096;
constexpr int kPageHeader = 40;
constexpr int kRowsPerGroup = 16;
constexpr int kGroupSize = 36;
constexpr int kMaxRowSize = kPageSize - kPageHeader - kGroupSize;

enum Table : quint32 {
    Tracks, Genres, Artists, Albums, Labels, Keys, Colors, PlaylistTree, PlaylistEntries,
    Unknown9, Unknown10, HistoryPlaylists, HistoryEntries, Artwork,
    Unknown14, Unknown15, Columns, Categories, SortOrder, HistorySync, TableCount,
};

void put16(QByteArray& bytes, qsizetype offset, quint16 value) {
    qToLittleEndian(value, bytes.data() + offset);
}

void put32(QByteArray& bytes, qsizetype offset, quint32 value) {
    qToLittleEndian(value, bytes.data() + offset);
}

bool isAscii(QStringView text) {
    return std::all_of(text.begin(), text.end(), [](QChar c) { return c.unicode() <= 0x7f; });
}

class Row {
  public:
    void clear() {
        m_bytes.resize(0);
        m_valid = true;
    }

    void u8(quint8 value) {
        m_bytes.append(static_cast<char>(value));
    }

    void u16(quint16 value) {
        char bytes[2];
        qToLittleEndian(value, bytes);
        m_bytes.append(bytes, sizeof(bytes));
    }

    void u32(quint32 value) {
        char bytes[4];
        qToLittleEndian(value, bytes);
        m_bytes.append(bytes, sizeof(bytes));
    }

    void setOffset(int field) {
        put16(m_bytes, field, static_cast<quint16>(m_bytes.size()));
    }

    void string(QStringView text) {
        if (text.size() > kMaxRowSize) {
            m_valid = false;
            return;
        }
        if (!isAscii(text)) {
            wideString(text);
            return;
        }
        if (text.size() <= 126) {
            u8(static_cast<quint8>(text.size() * 2 + 3));
        } else {
            longHeader(0x40, text.size());
        }
        asciiPayload(text);
    }

    void wideString(QStringView text, bool column = false) {
        longHeader(0x90, text.size() * 2 + (column ? 4 : 0));
        if (column) {
            u16(0xfffa);
        }
        for (const QChar c : text) {
            u16(c.unicode());
        }
        if (column) {
            u16(0xfffb);
        }
    }

    void isrc(QStringView text) {
        if (text.isEmpty()) {
            u8(3);
        } else {
            // ISRC uses the UTF-16 marker but stores a prefix, ASCII, and a terminator.
            longHeader(0x90, text.size() + 2);
            u8(3);
            asciiPayload(text);
            u8(0);
        }
    }

    bool fits() const {
        return m_valid && m_bytes.size() <= kMaxRowSize;
    }

    const QByteArray& bytes() const {
        return m_bytes;
    }

  private:
    void asciiPayload(QStringView text) {
        for (const QChar c : text) {
            u8(static_cast<quint8>(c.unicode()));
        }
    }

    void longHeader(quint8 kind, qsizetype payloadSize) {
        u8(kind);
        u16(static_cast<quint16>(payloadSize + 4));
        u8(0);
    }

    QByteArray m_bytes;
    bool m_valid = true;
};

class PdbWriter {
  public:
    PdbWriter()
            : m_bytes(kPageSize * (1 + 2 * TableCount), '\0') {
        put32(m_bytes, 4, kPageSize);
        put32(m_bytes, 8, TableCount);
        put32(m_bytes, 16, 5);
    }

    void begin(Table type) {
        m_type = type;
        m_first = 1 + 2 * type;
        m_last = m_first;
        m_used = m_count = 0;
    }

    bool add(const Row& row, bool indexed = false) {
        if (!row.fits()) {
            return false;
        }
        const auto& bytes = row.bytes();
        const int padded = (static_cast<int>(bytes.size()) + 3) & ~3;
        if (m_last == m_first) {
            ++m_last;
            pageHeader(m_last, 0x24, ++m_sequence);
        } else if (m_used + padded + indexSize(m_count + 1) > kPageSize - kPageHeader) {
            const auto next = allocatePage();
            if (!next) {
                return false;
            }
            finishPage(next);
            m_last = next;
            m_used = m_count = 0;
            pageHeader(m_last, 0x24, ++m_sequence);
        }
        const auto page = pageOffset(m_last);
        const auto start = page + kPageHeader + m_used;
        std::memcpy(m_bytes.data() + start, bytes.constData(), bytes.size());
        if (indexed) {
            put16(m_bytes, start + 2, static_cast<quint16>(m_count * 32));
        }
        const auto groupEnd = page + kPageSize - (m_count / kRowsPerGroup) * kGroupSize;
        put16(m_bytes, groupEnd - 6 - (m_count % kRowsPerGroup) * 2, m_used);
        m_used += padded;
        ++m_count;
        return true;
    }

    void end() {
        if (m_last != m_first) {
            finishPage(0); // The empty candidate is assigned once all physical pages exist.
        }
        m_lastPages[m_type] = m_last;
    }

    QByteArray finish() && {
        auto candidate = static_cast<quint32>(m_bytes.size() / kPageSize);
        for (quint32 type = 0; type < TableCount; ++type) {
            m_type = static_cast<Table>(type);
            const quint32 first = 1 + 2 * type;
            const quint32 last = m_lastPages[type];
            const bool hasRows = first != last;
            const quint32 empty = hasRows ? candidate++ : first + 1;
            const auto directory = 28 + 16 * type;
            put32(m_bytes, directory, type);
            put32(m_bytes, directory + 4, empty);
            put32(m_bytes, directory + 8, first);
            put32(m_bytes, directory + 12, last);
            indexPage(first, hasRows ? first + 1 : empty, hasRows);
            if (hasRows) {
                put32(m_bytes, pageOffset(last) + 12, empty);
            }
        }
        put32(m_bytes, 12, candidate);
        put32(m_bytes, 20, m_sequence + 1);
        return std::move(m_bytes);
    }

  private:
    static qsizetype pageOffset(quint32 index) {
        return static_cast<qsizetype>(index) * kPageSize;
    }

    static int indexSize(int count) {
        return (count + kRowsPerGroup - 1) / kRowsPerGroup * kGroupSize;
    }

    quint32 allocatePage() {
        // Some readers calculate page offsets with 32-bit arithmetic.
        constexpr auto limit = std::min<qint64>(
                std::numeric_limits<qsizetype>::max(), std::numeric_limits<quint32>::max());
        if (m_bytes.size() > limit - kPageSize) {
            return 0;
        }
        const auto index = static_cast<quint32>(m_bytes.size() / kPageSize);
        m_bytes.append(QByteArray(kPageSize, '\0'));
        return index;
    }

    void pageHeader(quint32 index, char flags, quint32 sequence) {
        const auto page = pageOffset(index);
        put32(m_bytes, page + 4, index);
        put32(m_bytes, page + 8, m_type);
        put32(m_bytes, page + 16, sequence);
        m_bytes[page + 27] = flags;
    }

    void indexPage(quint32 index, quint32 next, bool hasRows) {
        constexpr quint32 kNoPage = 0x03ffffff;
        pageHeader(index, 0x64, 1);
        const auto page = pageOffset(index);
        put32(m_bytes, page + 12, next);
        put16(m_bytes, page + 32, 0x1fff);
        put16(m_bytes, page + 34, 0x1fff);
        put16(m_bytes, page + 36, 0x03ec);
        put32(m_bytes, page + 40, index);
        // AI-generated (Claude Code). Start of AI-generated text.
        // As rekordbox does: copy the next-page link for a populated table, and
        // write the no-page sentinel for an empty table.
        // End of AI-generated text.
        put32(m_bytes, page + 44, hasRows ? next : kNoPage);
        put32(m_bytes, page + 48, kNoPage);
        put16(m_bytes, page + 58, 0x1fff);
        for (int i = 60; i < kPageSize - 20; i += 4) {
            put32(m_bytes, page + i, 0x1ffffff8); // Empty index entry.
        }
    }

    void finishPage(quint32 next) {
        const auto page = pageOffset(m_last);
        put32(m_bytes, page + 12, next);
        // One insertion transaction, no deleted rows. The two counts share 24 bits.
        put32(m_bytes, page + 24, static_cast<quint32>(m_count | (m_count << 13)) | 0x24000000);
        put16(m_bytes, page + 28, kPageSize - kPageHeader - m_used - indexSize(m_count));
        put16(m_bytes, page + 30, m_used);
        put16(m_bytes, page + 32, m_count);
        if (m_count > 255) {
            // Older readers use the low byte of the count unless this field is larger.
            put16(m_bytes, page + 34, m_count);
        }
        if (m_type == HistoryPlaylists || m_type == HistoryEntries) {
            put16(m_bytes, page + 38, 1);
        }
        for (int start = 0; start < m_count; start += kRowsPerGroup) {
            const auto groupEnd = page + kPageSize - (start / kRowsPerGroup) * kGroupSize;
            const auto mask = static_cast<quint16>((1u << std::min(kRowsPerGroup, m_count - start)) - 1);
            put16(m_bytes, groupEnd - 4, mask); // Present rows.
            put16(m_bytes, groupEnd - 2, mask); // Rows touched by the transaction.
        }
    }

    QByteArray m_bytes;
    std::array<quint32, TableCount> m_lastPages{};
    Table m_type = Tracks;
    quint32 m_sequence = 1;
    quint32 m_first = 0;
    quint32 m_last = 0;
    int m_used = 0;
    int m_count = 0;
};

bool fitsTrackFields(const LegacyTrack& track) {
    constexpr auto max = std::numeric_limits<quint16>::max();
    return track.discNumber <= max && track.playCount <= max && track.year <= max &&
            track.sampleDepth <= max && track.duration <= max && track.fileType <= max &&
            track.colorId <= 8 && track.rating <= 5 && track.isrc.size() <= kMaxRowSize &&
            isAscii(track.isrc) && !track.isrc.contains(QChar(0));
}

void writeTrack(Row& row, const LegacyTrack& track) {
    row.u16(0x24);
    row.u16(0); // Filled with the index in the destination page.
    row.u32(0x000c0700);
    row.u32(track.sampleRate);
    row.u32(track.composerId);
    row.u32(track.fileSize);
    row.u32(0); // Unknown, varies between tracks.
    row.u32(0x05e7fa80);
    row.u32(track.artworkId);
    row.u32(track.keyId);
    row.u32(track.originalArtistId);
    row.u32(track.labelId);
    row.u32(track.remixerId);
    row.u32(track.bitrate);
    row.u32(track.trackNumber);
    row.u32(track.tempo);
    row.u32(track.genreId);
    row.u32(track.albumId);
    row.u32(track.artistId);
    row.u32(track.id);
    row.u16(track.discNumber);
    row.u16(track.playCount);
    row.u16(track.year);
    row.u16(track.sampleDepth);
    row.u16(track.duration);
    row.u16(0x29);
    row.u8(track.colorId);
    row.u8(track.rating);
    row.u16(track.fileType);
    row.u16(3);

    const std::array<QStringView, 20> strings{{
            {}, // Lyricist.
            u"1", // Initial export revision.
            {}, {}, track.message, {}, // Publish track information is off.
            track.autoLoadHotCues ? QStringView(u"ON") : QStringView(),
            {}, {}, track.dateAdded, track.releaseDate, track.mixName, {},
            track.analyzePath, track.analyzeDate, track.comment, track.title, {},
            track.fileName, track.filePath,
    }};
    constexpr int kOffsets = 94;
    for (size_t i = 0; i <= strings.size(); ++i) {
        row.u16(0);
    }
    row.setOffset(kOffsets);
    row.isrc(track.isrc);
    for (size_t i = 0; i < strings.size(); ++i) {
        row.setOffset(kOffsets + 2 * (static_cast<int>(i) + 1));
        row.string(strings[i]);
    }
}

void writeNamed(Row& row, quint32 id, QStringView name) {
    row.u32(id);
    row.string(name);
}

void writeArtistOrAlbum(Row& row, quint32 id, QStringView name, const LegacyAlbum* album = nullptr) {
    const bool farOffset = name.size() > 200 || (name.size() > 100 && !isAscii(name));
    row.u16((album ? 0x80 : 0x60) | (farOffset ? 4 : 0));
    row.u16(0);
    if (album) {
        row.u32(0);
        row.u32(album->artistId);
    }
    row.u32(id);
    if (album) {
        row.u32(0);
    }
    if (farOffset) {
        row.u16(3);
        row.u16(album ? 24 : 12);
    } else {
        row.u8(3);
        row.u8(album ? 22 : 10);
    }
    row.string(name);
}

void writeDefaults(PdbWriter& writer) {
    Row row;
    constexpr std::array<QStringView, 8> colors{{
            u"Pink", u"Red", u"Orange", u"Yellow", u"Green", u"Aqua", u"Blue", u"Purple",
    }};
    writer.begin(Colors);
    quint8 colorId = 0;
    for (const auto color : colors) {
        row.clear();
        row.u32(0);
        row.u8(++colorId);
        row.u16(colorId);
        row.u8(0);
        row.string(color);
        writer.add(row);
    }
    writer.end();

    struct Column {
        quint16 id;
        QStringView name;
    };
    constexpr Column columns[] = {
            {0x80, u"GENRE"}, {0x81, u"ARTIST"}, {0x82, u"ALBUM"}, {0x83, u"TRACK"},
            {0x85, u"BPM"}, {0x86, u"RATING"}, {0x87, u"YEAR"}, {0x88, u"REMIXER"},
            {0x89, u"LABEL"}, {0x8a, u"ORIGINAL ARTIST"}, {0x8b, u"KEY"}, {0x8d, u"CUE"},
            {0x8e, u"COLOR"}, {0x92, u"TIME"}, {0x93, u"BITRATE"}, {0x94, u"FILE NAME"},
            {0x84, u"PLAYLIST"}, {0x98, u"HOT CUE BANK"}, {0x95, u"HISTORY"}, {0x91, u"SEARCH"},
            {0x96, u"COMMENTS"}, {0x8c, u"DATE ADDED"}, {0x97, u"DJ PLAY COUNT"}, {0x90, u"FOLDER"},
            {0xa1, u"DEFAULT"}, {0xa2, u"ALPHABET"}, {0xaa, u"MATCHING"},
    };
    writer.begin(Columns);
    quint16 position = 0;
    for (const auto& column : columns) {
        row.clear();
        row.u16(++position);
        row.u16(column.id);
        row.wideString(column.name, true);
        writer.add(row);
    }
    writer.end();

    // Default category and sorting settings exist on a freshly formatted stick.
    constexpr quint16 categories[][4] = {
            {1, 1, 0x163, 0}, {5, 6, 0x105, 0}, {6, 7, 0x163, 0}, {7, 8, 0x163, 0},
            {8, 9, 0x163, 0}, {9, 10, 0x163, 0}, {10, 11, 0x163, 0}, {13, 15, 0x163, 0},
            {14, 19, 0x104, 0}, {15, 20, 0x106, 0}, {16, 21, 0x163, 0}, {18, 23, 0x163, 0},
            {2, 2, 2, 1}, {3, 3, 3, 2}, {4, 4, 1, 3}, {11, 12, 0x63, 4}, {17, 5, 0x63, 5},
            {19, 22, 0x63, 6}, {20, 18, 0x63, 7}, {27, 26, 0x263, 8}, {24, 17, 0x63, 9},
            {22, 27, 0x63, 10},
    };
    constexpr quint16 sortOrder[][4] = {
            {1, 6, 1, 0}, {21, 7, 1, 0}, {14, 8, 1, 0}, {8, 9, 1, 0}, {9, 10, 1, 0},
            {10, 11, 1, 0}, {15, 13, 1, 0}, {13, 15, 1, 0}, {23, 16, 1, 0}, {22, 17, 1, 0},
            {25, 0, 0x100, 0}, {26, 1, 0x200, 0}, {2, 2, 0x300, 0}, {3, 3, 0x400, 0},
            {5, 4, 0x500, 0}, {6, 5, 0x600, 0}, {11, 12, 0x700, 0},
    };
    const auto writeSettings = [&](Table table, const auto& values) {
        writer.begin(table);
        for (const auto& valuesRow : values) {
            row.clear();
            for (quint16 value : valuesRow) {
                row.u16(value);
            }
            writer.add(row);
        }
        writer.end();
    };
    writeSettings(Categories, categories);
    writeSettings(SortOrder, sortOrder);

    writer.begin(HistorySync);
    row.clear();
    row.u16(0x280);
    row.u16(0);
    row.u32(0);
    row.u32(0);
    const auto date = QDate::currentDate().toString(Qt::ISODate);
    row.string(date);
    row.u8(25);
    row.u8(30);
    row.string(u"1000");
    row.string({});
    row.u8(0);
    row.u32(0);
    row.u32(0);
    writer.add(row, true);
    writer.end();
}

class LibraryWriter {
  public:
    explicit LibraryWriter(QString* pError) : m_pError(pError) {
        if (m_pError) {
            m_pError->clear();
        }
    }

    QByteArray write(const LegacyLibrary& library) {
        if (!table(Tracks, library.tracks, [&](const LegacyTrack& track) {
                    if (!fitsTrackFields(track)) {
                        return error(QObject::tr("A track has a value that the Device Library cannot store."));
                    }
                    writeTrack(m_row, track);
                    return add(true);
                }) ||
                !namedTable(Genres, library.genres) ||
                !table(Artists, library.artists, [&](const LegacyNamedRow& artist) {
                    writeArtistOrAlbum(m_row, artist.id, artist.name);
                    return add(true);
                }) ||
                !table(Albums, library.albums, [&](const LegacyAlbum& album) {
                    writeArtistOrAlbum(m_row, album.id, album.name, &album);
                    return add(true);
                }) ||
                !namedTable(Labels, library.labels) ||
                !table(Keys, library.keys, [&](const LegacyNamedRow& key) {
                    m_row.u32(key.id);
                    writeNamed(m_row, key.id, key.name);
                    return add();
                }) ||
                !table(PlaylistTree, library.playlists, [&](const LegacyPlaylistNode& node) {
                    m_row.u32(node.parentId);
                    m_row.u32(0);
                    m_row.u32(node.sortOrder);
                    m_row.u32(node.id);
                    m_row.u32(node.isFolder);
                    m_row.string(node.name);
                    return add();
                }) ||
                !entries(PlaylistEntries, library.playlistEntries) ||
                !namedTable(HistoryPlaylists, library.historyPlaylists) ||
                !entries(HistoryEntries, library.historyEntries) ||
                !table(Artwork, library.artwork, [&](const LegacyArtwork& art) {
                    writeNamed(m_row, art.id, art.path);
                    return add();
                })) {
            return {};
        }
        for (const auto type : {Unknown9, Unknown10, Unknown14, Unknown15}) {
            m_writer.begin(type);
            m_writer.end();
        }
        writeDefaults(m_writer);
        return std::move(m_writer).finish();
    }

  private:
    bool error(const QString& reason) {
        if (m_pError) {
            *m_pError = reason;
        }
        return false;
    }

    bool add(bool indexed = false) {
        return m_writer.add(m_row, indexed) ||
                error(QObject::tr("The Device Library has too much data."));
    }

    template<typename T, typename Encode>
    bool table(Table type, const QList<T>& rows, const Encode& encode) {
        m_writer.begin(type);
        QSet<quint32> ids;
        ids.reserve(rows.size());
        for (const T& row : rows) {
            if (row.id == 0) {
                return error(QObject::tr("The Device Library has a row with ID 0."));
            }
            if (ids.contains(row.id)) {
                return error(QObject::tr("The Device Library has a duplicate row ID."));
            }
            ids.insert(row.id);
            m_row.clear();
            if (!encode(row)) {
                return false;
            }
        }
        m_writer.end();
        return true;
    }

    template<typename T>
    bool namedTable(Table type, const QList<T>& rows) {
        return table(type, rows, [&](const T& row) {
            writeNamed(m_row, row.id, row.name);
            return add();
        });
    }

    template<typename T>
    bool entries(Table type, const QList<T>& rows) {
        m_writer.begin(type);
        QSet<quint64> positions;
        positions.reserve(rows.size());
        for (const T& row : rows) {
            if (!row.trackId || !row.playlistId || !row.entryIndex) {
                return error(QObject::tr("The Device Library has a playlist entry with ID or position 0."));
            }
            const auto position = (static_cast<quint64>(row.playlistId) << 32) | row.entryIndex;
            if (positions.contains(position)) {
                return error(QObject::tr("The Device Library has a duplicate playlist position."));
            }
            positions.insert(position);
            m_row.clear();
            if (type == PlaylistEntries) {
                m_row.u32(row.entryIndex);
            }
            m_row.u32(row.trackId);
            m_row.u32(row.playlistId);
            if (type == HistoryEntries) {
                m_row.u32(row.entryIndex);
            }
            if (!add()) {
                return false;
            }
        }
        m_writer.end();
        return true;
    }

    QString* m_pError;
    PdbWriter m_writer;
    Row m_row;
};
} // namespace

QByteArray legacyPdbBytes(const LegacyLibrary& library, QString* pError) {
    return LibraryWriter(pError).write(library);
}

} // namespace rekordbox
} // namespace mixxx

// End of AI-generated text.
