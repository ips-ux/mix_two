// AI-generated (Claude Code). Start of AI-generated text.

#include <gtest/gtest.h>
#include <rekordbox_pdb.h>

#include <QElapsedTimer>
#include <QtEndian>
#include <cstdio>
#include <sstream>

#include "library/rekordbox/legacylibrary.h"

namespace {
using namespace mixxx::rekordbox;
using Pdb = rekordbox_pdb_t;

class ParsedPdb {
  public:
    explicit ParsedPdb(const QByteArray& bytes)
            : m_input(bytes.toStdString()), m_stream(&m_input), db(&m_stream), m_size(bytes.size()) {
    }

    template<typename Row>
    QList<Row*> rows(unsigned type) {
        QList<Row*> result;
        visit(type, [&](Pdb::row_ref_t* ref) { result.append(static_cast<Row*>(ref->body())); });
        return result;
    }

    template<typename Visitor>
    int visit(unsigned type, Visitor visitor) {
        auto* table = db.tables()->at(type);
        EXPECT_EQ(static_cast<unsigned>(table->type()), type);
        auto* ref = table->first_page();
        int dataPages = 0;
        for (unsigned steps = 0; steps < m_size / db.len_page(); ++steps) {
            if (ref->index() == 0 || ref->index() >= m_size / db.len_page()) {
                break;
            }
            auto* page = ref->body();
            EXPECT_EQ(ref->index(), page->page_index());
            EXPECT_EQ(table->type(), page->type());
            if (page->is_data_page()) {
                ++dataPages;
                unsigned count = 0;
                unsigned groups = 0;
                for (auto* group : *page->row_groups()) {
                    ++groups;
                    for (auto* row : *group->rows()) {
                        if (!row->present()) {
                            continue;
                        }
                        EXPECT_LT(static_cast<unsigned>(row->ofs_row()), page->used_size());
                        visitor(row);
                        ++count;
                    }
                }
                EXPECT_EQ(count, page->num_rows());
                EXPECT_EQ(count, page->num_rows_valid());
                if (count > 255) {
                    // Older DeviceSQL readers fall back to this count for dense pages.
                    EXPECT_EQ(count, page->unknown_not_num_rows_large());
                }
                EXPECT_EQ(page->free_size() + page->used_size() + 36 * groups + 40, db.len_page());
            } else {
                EXPECT_EQ(ref->index(), table->first_page()->index());
            }
            if (ref->index() == table->last_page()->index()) {
                EXPECT_EQ(page->next_page()->index(), table->empty_candidate());
                return dataPages;
            }
            ref = page->next_page();
        }
        ADD_FAILURE() << "Broken page chain for table " << type;
        return dataPages;
    }

  private:
    std::istringstream m_input;
    kaitai::kstream m_stream;

  public:
    Pdb db;

  private:
    size_t m_size;
};

QString text(Pdb::device_sql_string_t* string) {
    switch (string->length_and_kind()) {
    case 0x40:
        return QString::fromStdString(static_cast<Pdb::device_sql_long_ascii_t*>(string->body())->text());
    case 0x90: {
        // Mix Two builds Kaitai without encoding conversion; text() returns the wire bytes.
        const auto payload = static_cast<Pdb::device_sql_long_utf16le_t*>(string->body())->text();
        EXPECT_EQ(payload.size() % 2, 0u);
        QString result;
        result.reserve(payload.size() / 2);
        for (size_t i = 0; i + 1 < payload.size(); i += 2) {
            result.append(QChar(qFromLittleEndian<quint16>(payload.data() + i)));
        }
        return result;
    }
    default:
        return QString::fromStdString(static_cast<Pdb::device_sql_short_ascii_t*>(string->body())->text());
    }
}

TEST(LegacyPdb, EmptyLibraryHasFixedTables) {
    QString error;
    const auto bytes = legacyPdbBytes({}, &error);
    ASSERT_FALSE(bytes.isEmpty()) << error.toStdString();
    ParsedPdb parsed(bytes);
    EXPECT_EQ(parsed.db.len_page(), 4096u);
    ASSERT_EQ(parsed.db.tables()->size(), 20u);
    EXPECT_EQ(bytes.size() % 4096, 0);
    const QStringList names{"Pink", "Red", "Orange", "Yellow", "Green", "Aqua", "Blue", "Purple"};
    const auto colors = parsed.rows<Pdb::color_row_t>(6);
    ASSERT_EQ(colors.size(), names.size());
    for (int i = 0; i < colors.size(); ++i) {
        EXPECT_EQ(colors[i]->id(), i + 1);
        EXPECT_EQ(text(colors[i]->name()), names[i]);
    }

    const QList<quint16> columnIds{0x80, 0x81, 0x82, 0x83, 0x85, 0x86, 0x87, 0x88,
            0x89, 0x8a, 0x8b, 0x8d, 0x8e, 0x92, 0x93, 0x94, 0x84, 0x98, 0x95,
            0x91, 0x96, 0x8c, 0x97, 0x90, 0xa1, 0xa2, 0xaa};
    const QStringList columns{"GENRE", "ARTIST", "ALBUM", "TRACK", "BPM", "RATING",
            "YEAR", "REMIXER", "LABEL", "ORIGINAL ARTIST", "KEY", "CUE", "COLOR", "TIME",
            "BITRATE", "FILE NAME", "PLAYLIST", "HOT CUE BANK", "HISTORY", "SEARCH",
            "COMMENTS", "DATE ADDED", "DJ PLAY COUNT", "FOLDER", "DEFAULT", "ALPHABET", "MATCHING"};
    const auto* table = parsed.db.tables()->at(16);
    int i = 0;
    parsed.visit(16, [&](auto* ref) {
        ASSERT_LT(i, columns.size());
        const char* row = bytes.constData() + table->last_page()->index() * 4096 + ref->row_base();
        EXPECT_EQ(qFromLittleEndian<quint16>(row), i + 1);
        EXPECT_EQ(qFromLittleEndian<quint16>(row + 2), columnIds[i]);
        EXPECT_EQ(static_cast<unsigned char>(row[4]), 0x90);
        EXPECT_EQ(qFromLittleEndian<quint16>(row + 8), 0xfffa);
        for (int j = 0; j < columns[i].size(); ++j) {
            EXPECT_EQ(qFromLittleEndian<quint16>(row + 10 + j * 2), columns[i][j].unicode());
        }
        EXPECT_EQ(qFromLittleEndian<quint16>(row + 10 + columns[i].size() * 2), 0xfffb);
        ++i;
    });
    EXPECT_EQ(i, columns.size());
    for (unsigned type = 0; type < 20; ++type) {
        if (type != 6 && type != 16) {
            EXPECT_EQ(parsed.visit(type, [](auto*) {}), type >= 17 ? 1 : 0);
        }
    }
}

TEST(LegacyPdb, RoundTripsLibraryRows) {
    LegacyLibrary library;
    library.artists = {{2, "Artist"}, {3, "Composer"}, {4, "Original"}, {5, "Remixer"}};
    library.genres = {{6, "House"}};
    library.albums = {{7, 2, "Album"}};
    library.labels = {{8, "Label"}};
    library.keys = {{9, "Dm"}};
    library.artwork = {{10, "/PIONEER/Artwork/00001/a1.jpg"}};
    library.playlists = {{11, 0, 0, true, "Folder"}, {12, 11, 0, false, "First"},
            {13, 11, 1, false, "Second"}};
    library.playlistEntries = {{12, 42, 1}, {12, 44, 2}, {13, 43, 1}};
    library.historyPlaylists = {{14, "HISTORY 001"}};
    library.historyEntries = {{14, 43, 1}, {14, 42, 2}};
    LegacyTrack track;
    track.id = 42;
    track.title = "Title";
    track.artistId = 2;
    track.composerId = 3;
    track.originalArtistId = 4;
    track.remixerId = 5;
    track.genreId = 6;
    track.albumId = 7;
    track.labelId = 8;
    track.keyId = 9;
    track.artworkId = 10;
    track.colorId = 2;
    track.tempo = 12843;
    track.duration = 207;
    track.rating = 4;
    track.year = 2001;
    track.trackNumber = 3;
    track.discNumber = 2;
    track.playCount = 11;
    track.bitrate = 320;
    track.sampleRate = 44100;
    track.sampleDepth = 16;
    track.fileSize = 12345678;
    track.fileType = 1;
    track.filePath = "/Contents/Artist/Album/a.mp3";
    track.fileName = "a.mp3";
    track.analyzePath = "/PIONEER/USBANLZ/P016/0000875E/ANLZ0000.DAT";
    track.analyzeDate = "2026-11-23";
    track.dateAdded = "2026-11-24";
    track.releaseDate = "2001-01-02";
    track.mixName = "Club Mix";
    track.comment = "Test comment";
    track.isrc = "USABC0100001";
    track.message = "Message";
    library.tracks.append(track);
    track.id = 43;
    track.title = QStringLiteral("\u65e5\u672c\U0001F3B5");
    track.autoLoadHotCues = false;
    track.fileType = 11;
    library.tracks.append(track);
    track = {};
    track.id = 44;
    library.tracks.append(track);

    const auto bytes = legacyPdbBytes(library);
    ASSERT_FALSE(bytes.isEmpty());
    ParsedPdb parsed(bytes);
    const auto tracks = parsed.rows<Pdb::track_row_t>(0);
    ASSERT_EQ(tracks.size(), library.tracks.size());
    for (int i = 0; i < tracks.size(); ++i) {
        auto* row = tracks[i];
        const auto& expected = library.tracks[i];
        EXPECT_EQ(row->id(), expected.id);
        EXPECT_EQ(row->artist_id(), expected.artistId);
        EXPECT_EQ(row->composer_id(), expected.composerId);
        EXPECT_EQ(row->original_artist_id(), expected.originalArtistId);
        EXPECT_EQ(row->remixer_id(), expected.remixerId);
        EXPECT_EQ(row->album_id(), expected.albumId);
        EXPECT_EQ(row->genre_id(), expected.genreId);
        EXPECT_EQ(row->label_id(), expected.labelId);
        EXPECT_EQ(row->key_id(), expected.keyId);
        EXPECT_EQ(row->artwork_id(), expected.artworkId);
        EXPECT_EQ(row->color_id(), expected.colorId);
        EXPECT_EQ(row->tempo(), expected.tempo);
        EXPECT_EQ(row->duration(), expected.duration);
        EXPECT_EQ(row->rating(), expected.rating);
        EXPECT_EQ(row->year(), expected.year);
        EXPECT_EQ(row->track_number(), expected.trackNumber);
        EXPECT_EQ(row->disc_number(), expected.discNumber);
        EXPECT_EQ(row->play_count(), expected.playCount);
        EXPECT_EQ(row->bitrate(), expected.bitrate);
        EXPECT_EQ(row->sample_rate(), expected.sampleRate);
        EXPECT_EQ(row->sample_depth(), expected.sampleDepth);
        EXPECT_EQ(row->file_size(), expected.fileSize);
        EXPECT_EQ(text(row->title()), expected.title);
        EXPECT_EQ(text(row->filename()), expected.fileName);
        EXPECT_EQ(text(row->file_path()), expected.filePath);
        EXPECT_EQ(text(row->analyze_path()), expected.analyzePath);
        EXPECT_EQ(text(row->analyze_date()), expected.analyzeDate);
        EXPECT_EQ(text(row->date_added()), expected.dateAdded);
        EXPECT_EQ(text(row->release_date()), expected.releaseDate);
        EXPECT_EQ(text(row->mix_name()), expected.mixName);
        EXPECT_EQ(text(row->comment()), expected.comment);
        EXPECT_EQ(text(row->message()), expected.message);
        if (expected.isrc.isEmpty()) {
            EXPECT_TRUE(text(row->isrc()).isEmpty());
        }
        EXPECT_EQ(text(row->autoload_hot_cues()), expected.autoLoadHotCues ? "ON" : "");
        EXPECT_EQ(row->index_shift(), i * 32);
    }
    parsed.visit(0, [&](auto* ref) {
        const auto* page = ref->_parent()->_parent();
        const auto* start = bytes.constData() + page->page_index() * 4096 + ref->row_base();
        auto* row = static_cast<Pdb::track_row_t*>(ref->body());
        ASSERT_GE(row->id(), 42u);
        ASSERT_LE(row->id(), 44u);
        const auto& track = library.tracks.at(row->id() - 42);
        EXPECT_EQ(qFromLittleEndian<quint16>(start + 90), track.fileType);
        if (!track.isrc.isEmpty()) {
            auto* isrc = row->isrc();
            ASSERT_EQ(isrc->length_and_kind(), 0x90);
            const auto payload = static_cast<Pdb::device_sql_long_utf16le_t*>(isrc->body())->text();
            EXPECT_EQ(payload, std::string("\x03") + track.isrc.toStdString() + '\0');
        }
    });

    const auto verifyNamed = [](const auto& rows, const auto& expected) {
        ASSERT_EQ(rows.size(), expected.size());
        for (int i = 0; i < rows.size(); ++i) {
            EXPECT_EQ(rows[i]->id(), expected[i].id);
            EXPECT_EQ(text(rows[i]->name()), expected[i].name);
        }
    };
    verifyNamed(parsed.rows<Pdb::artist_row_t>(2), library.artists);
    const auto albums = parsed.rows<Pdb::album_row_t>(3);
    verifyNamed(albums, library.albums);
    ASSERT_EQ(albums.size(), 1);
    EXPECT_EQ(albums[0]->artist_id(), library.albums[0].artistId);
    verifyNamed(parsed.rows<Pdb::genre_row_t>(1), library.genres);
    verifyNamed(parsed.rows<Pdb::label_row_t>(4), library.labels);
    const auto keys = parsed.rows<Pdb::key_row_t>(5);
    verifyNamed(keys, library.keys);
    ASSERT_EQ(keys.size(), 1);
    EXPECT_EQ(keys[0]->id2(), 9u);
    const auto artwork = parsed.rows<Pdb::artwork_row_t>(13);
    ASSERT_EQ(artwork.size(), 1);
    EXPECT_EQ(artwork[0]->id(), library.artwork[0].id);
    EXPECT_EQ(text(artwork[0]->path()), library.artwork[0].path);
    const auto playlists = parsed.rows<Pdb::playlist_tree_row_t>(7);
    verifyNamed(playlists, library.playlists);
    ASSERT_EQ(playlists.size(), library.playlists.size());
    for (int i = 0; i < playlists.size(); ++i) {
        EXPECT_EQ(playlists[i]->parent_id(), library.playlists[i].parentId);
        EXPECT_EQ(playlists[i]->sort_order(), library.playlists[i].sortOrder);
        EXPECT_EQ(playlists[i]->is_folder(), library.playlists[i].isFolder);
    }
    verifyNamed(parsed.rows<Pdb::history_playlist_row_t>(11), library.historyPlaylists);
    const auto verifyEntries = [](const auto& rows, const auto& expected) {
        ASSERT_EQ(rows.size(), expected.size());
        for (int i = 0; i < rows.size(); ++i) {
            EXPECT_EQ(rows[i]->playlist_id(), expected[i].playlistId);
            EXPECT_EQ(rows[i]->track_id(), expected[i].trackId);
            EXPECT_EQ(rows[i]->entry_index(), expected[i].entryIndex);
        }
    };
    verifyEntries(parsed.rows<Pdb::playlist_entry_row_t>(8), library.playlistEntries);
    verifyEntries(parsed.rows<Pdb::history_entry_row_t>(12), library.historyEntries);
}

TEST(LegacyPdb, SelectsStringEncodingWithoutLosingText) {
    LegacyLibrary library;
    library.genres = {{1, QString(126, 'a')}, {2, QString(127, 'b')},
            {3, QStringLiteral("Caf\u00e9 \u65e5\u672c")}};
    const auto bytes = legacyPdbBytes(library);
    ASSERT_FALSE(bytes.isEmpty());
    ParsedPdb parsed(bytes);
    const auto rows = parsed.rows<Pdb::genre_row_t>(1);
    ASSERT_EQ(rows.size(), 3);
    EXPECT_EQ(rows[0]->name()->length_and_kind(), 0xff);
    EXPECT_EQ(rows[1]->name()->length_and_kind(), 0x40);
    EXPECT_EQ(rows[2]->name()->length_and_kind(), 0x90);
    for (int i = 0; i < rows.size(); ++i) {
        EXPECT_EQ(text(rows[i]->name()), library.genres[i].name);
    }
}

TEST(LegacyPdb, FollowsMultiplePagesIncludingDenseRows) {
    LegacyLibrary library;
    library.playlists = {{1, 0, 0, false, "All"}};
    for (quint32 id = 1; id <= 2000; ++id) {
        LegacyTrack track;
        track.id = id;
        library.tracks.append(track);
        library.playlistEntries.append({1, id, id});
    }
    const auto bytes = legacyPdbBytes(library);
    ASSERT_FALSE(bytes.isEmpty());
    ParsedPdb parsed(bytes);
    unsigned id = 0;
    EXPECT_GT(parsed.visit(0, [&](auto* ref) {
        EXPECT_EQ(static_cast<Pdb::track_row_t*>(ref->body())->id(), ++id);
    }), 1);
    EXPECT_EQ(id, 2000u);
    id = 0;
    EXPECT_GT(parsed.visit(8, [&](auto* ref) {
        auto* row = static_cast<Pdb::playlist_entry_row_t*>(ref->body());
        EXPECT_EQ(row->entry_index(), ++id);
        EXPECT_EQ(row->track_id(), id);
        EXPECT_EQ(row->playlist_id(), 1u);
    }), 1);
    EXPECT_EQ(id, 2000u);
}

TEST(LegacyPdb, RejectsZeroAndDuplicateTrackIds) {
    LegacyLibrary library;
    LegacyTrack track;
    library.tracks.append(track);
    QString zeroError;
    EXPECT_TRUE(legacyPdbBytes(library, &zeroError).isEmpty());
    EXPECT_FALSE(zeroError.isEmpty());
    library.tracks[0].id = track.id = 1;
    library.tracks.append(track);
    QString duplicateError;
    EXPECT_TRUE(legacyPdbBytes(library, &duplicateError).isEmpty());
    EXPECT_FALSE(duplicateError.isEmpty());
    EXPECT_NE(zeroError, duplicateError);
}

TEST(LegacyPdb, RejectsRowsThatCannotFit) {
    LegacyLibrary library;
    library.genres = {{1, QString(4096, 'a')}};
    QString error;
    EXPECT_TRUE(legacyPdbBytes(library, &error).isEmpty());
    EXPECT_FALSE(error.isEmpty());
    library.genres.clear();
    LegacyTrack track;
    track.id = 1;
    track.duration = 65536;
    library.tracks.append(track);
    EXPECT_TRUE(legacyPdbBytes(library, &error).isEmpty());
    EXPECT_FALSE(error.isEmpty());
}

TEST(LegacyPdb, WritesFiveThousandTracks) {
    LegacyLibrary library;
    for (quint32 id = 1; id <= 5000; ++id) {
        LegacyTrack track;
        track.id = id;
        track.title = QStringLiteral("Track %1").arg(id);
        track.fileName = track.title + ".mp3";
        track.filePath = "/Contents/Artist/Album/" + track.fileName;
        library.tracks.append(track);
    }
    QElapsedTimer timer;
    timer.start();
    const auto bytes = legacyPdbBytes(library);
    const auto elapsed = timer.elapsed();
    std::printf("5000 tracks: %lld ms\n", static_cast<long long>(elapsed));
    ASSERT_FALSE(bytes.isEmpty());
    EXPECT_LT(elapsed, 2000);
}
} // namespace

// End of AI-generated text.
