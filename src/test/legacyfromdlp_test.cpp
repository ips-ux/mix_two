// AI-generated (Claude Code). Start of AI-generated text.

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <QTemporaryDir>

#include "library/rekordbox/devicelibraryplusdb.h"
#include "library/rekordbox/legacyfromdlp.h"

namespace mixxx::rekordbox {

class LegacyFromDlpTest : public testing::Test {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_stick.isValid());
        QString error;
        m_pDb = DeviceLibraryPlusDb::create(m_stick.path(), QStringLiteral("Test"), 42, &error);
        ASSERT_TRUE(m_pDb) << qPrintable(error);
    }

    void addRows(const char* sql) {
        auto* pHandle = m_pDb->handle();
        ASSERT_EQ(SQLITE_OK, sqlite3_exec(pHandle, sql, nullptr, nullptr, nullptr))
                << sqlite3_errmsg(pHandle);
    }

    QTemporaryDir m_stick;
    std::unique_ptr<DeviceLibraryPlusDb> m_pDb;
};

TEST_F(LegacyFromDlpTest, MapsTracksAndMetadataFromBothWriters) {
    ASSERT_NO_FATAL_FAILURE(addRows(R"sql(
        INSERT INTO artist(artist_id, name) VALUES (4294967297, 'Artist'),
            (8589934593, 'Composer'), (8589934597, 'Original'), (8589934599, 'Remixer');
        INSERT INTO album(album_id, artist_id, name) VALUES (4294967311, 8589934593, 'Album');
        INSERT INTO genre VALUES (4294967312, 'Genre');
        INSERT INTO key VALUES (4294967313, 'Key');
        INSERT INTO label VALUES (4294967314, 'Label');
        INSERT INTO image VALUES (4294967315, '/PIONEER/Artwork/a.jpg');
        INSERT INTO content(content_id, title, masterDbId, masterContentId, path, fileName,
            artist_id_artist, artist_id_composer, artist_id_originalArtist, artist_id_remixer,
            album_id, genre_id, key_id, label_id, image_id, color_id, analysisDataFilePath,
            bpmx100, length, rating, releaseYear, trackNo, discNo, djPlayCount, bitrate,
            samplingRate, bitDepth, fileSize, fileType, dateCreated, dateAdded, releaseDate,
            subtitle, djComment, isrc, kuvoDeliveryComment, isHotCueAutoLoadOn)
        VALUES (4294967500, 'rekordbox track', 17, 99, '/Contents/r.wav', 'r.wav',
            4294967297, 8589934593, 8589934597, 8589934599,
            4294967311, 4294967312, 4294967313, 4294967314, 4294967315, 8,
            '/PIONEER/USBANLZ/r/ANLZ0000.DAT', 12345, 62, 4, 2024, 9, 2, 3, 320,
            44100, 24, 123456, 11, '2024-10-02', '2024-10-03', '2024-10-01',
            'Mix', 'Comment', 'TEST123', 'Message', 0);
    )sql"));
    DlpContent content;
    content.title = QStringLiteral("Mix Two track");
    content.path = QStringLiteral("/Contents/m.mp3");
    content.analysisDataFilePath = QStringLiteral("/PIONEER/USBANLZ/m/ANLZ0000.DAT");
    content.artistId = 8589934593LL;
    content.albumId = 4294967311LL;
    content.imageId = 4294967315LL;
    content.masterDbId = 42;
    content.masterContentId = 99;
    ASSERT_EQ(4294967501LL, m_pDb->addContent(content));
    QString error;
    ASSERT_TRUE(m_pDb->save(&error)) << qPrintable(error);
    m_pDb = DeviceLibraryPlusDb::open(m_stick.path(), &error);
    ASSERT_TRUE(m_pDb) << qPrintable(error);

    const auto model = legacyFromDlp(*m_pDb, &error);
    ASSERT_TRUE(model) << qPrintable(error);
    ASSERT_EQ(2, model->tracks.size());
    ASSERT_EQ(4, model->artists.size());
    ASSERT_EQ(1, model->albums.size());
    ASSERT_EQ(1, model->genres.size());
    ASSERT_EQ(1, model->keys.size());
    ASSERT_EQ(1, model->labels.size());
    ASSERT_EQ(1, model->artwork.size());
    ASSERT_EQ(8, model->colors.size());
    EXPECT_EQ(QStringLiteral("Purple"), model->colors[7].name);
    EXPECT_EQ(8u, model->colors[7].id);
    const auto& first = model->tracks[0];
    const auto& second = model->tracks[1];
    EXPECT_EQ(1u, first.id);
    EXPECT_EQ(2u, second.id);
    EXPECT_EQ(QStringLiteral("rekordbox track"), first.title);
    EXPECT_EQ(content.title, second.title);
    EXPECT_EQ(QStringLiteral("/Contents/r.wav"), first.filePath);
    EXPECT_EQ(QStringLiteral("r.wav"), first.fileName);
    EXPECT_EQ(QStringLiteral("/PIONEER/USBANLZ/r/ANLZ0000.DAT"), first.analyzePath);
    EXPECT_EQ(content.analysisDataFilePath, second.analyzePath);
    EXPECT_EQ(1u, first.artistId);
    EXPECT_EQ(2u, first.composerId);
    EXPECT_EQ(3u, first.originalArtistId);
    EXPECT_EQ(4u, first.remixerId);
    EXPECT_EQ(2u, second.artistId);
    EXPECT_EQ(0u, second.composerId);
    EXPECT_EQ(QStringLiteral("Composer"), model->artists[1].name);
    EXPECT_EQ(2u, model->artists[1].id);
    EXPECT_EQ(1u, model->albums[0].id);
    EXPECT_EQ(2u, model->albums[0].artistId);
    EXPECT_EQ(1u, first.albumId);
    EXPECT_EQ(1u, second.albumId);
    EXPECT_EQ(1u, first.genreId);
    EXPECT_EQ(1u, first.keyId);
    EXPECT_EQ(1u, first.labelId);
    EXPECT_EQ(8u, first.colorId);
    EXPECT_EQ(0u, second.colorId);
    EXPECT_EQ(1u, first.artworkId);
    EXPECT_EQ(1u, second.artworkId);
    EXPECT_EQ(1u, model->artwork[0].id);
    EXPECT_EQ(QStringLiteral("/PIONEER/Artwork/a.jpg"), model->artwork[0].path);
    EXPECT_EQ(12345u, first.tempo);
    EXPECT_EQ(62u, first.duration);
    EXPECT_EQ(4u, first.rating);
    EXPECT_EQ(2024u, first.year);
    EXPECT_EQ(9u, first.trackNumber);
    EXPECT_EQ(2u, first.discNumber);
    EXPECT_EQ(3u, first.playCount);
    EXPECT_EQ(320u, first.bitrate);
    EXPECT_EQ(44100u, first.sampleRate);
    EXPECT_EQ(24u, first.sampleDepth);
    EXPECT_EQ(123456u, first.fileSize);
    EXPECT_EQ(11u, first.fileType);
    EXPECT_EQ(QStringLiteral("2024-10-02"), first.analyzeDate);
    EXPECT_EQ(QStringLiteral("2024-10-03"), first.dateAdded);
    EXPECT_EQ(QStringLiteral("2024-10-01"), first.releaseDate);
    EXPECT_EQ(QStringLiteral("Mix"), first.mixName);
    EXPECT_EQ(QStringLiteral("Comment"), first.comment);
    EXPECT_EQ(QStringLiteral("TEST123"), first.isrc);
    EXPECT_EQ(QStringLiteral("Message"), first.message);
    EXPECT_FALSE(first.autoLoadHotCues);
    EXPECT_TRUE(second.autoLoadHotCues);
}

TEST_F(LegacyFromDlpTest, PreservesNestedPlaylistOrderAndRepeatedEntries) {
    ASSERT_NO_FATAL_FAILURE(addRows(R"sql(
        INSERT INTO content(content_id) VALUES (200), (4294967496);
        INSERT INTO playlist VALUES
            (10, 3, 'Second child', NULL, 0, 5000000001),
            (20, 8, 'Last root', NULL, 0, 0),
            (30, 0, 'Nested playlist', NULL, 0, 5000000002),
            (5000000001, 2, 'Root folder', NULL, 1, NULL),
            (5000000002, 1, 'First child', NULL, 1, 5000000001);
        INSERT INTO playlist_content VALUES (30, 200, 100),
            (30, 4294967496, 0), (30, 4294967496, 5);
    )sql"));

    QString error;
    const auto model = legacyFromDlp(*m_pDb, &error);
    ASSERT_TRUE(model) << qPrintable(error);
    ASSERT_EQ(5, model->playlists.size());
    const auto& nodes = model->playlists;
    EXPECT_EQ(1u, nodes[0].id);
    EXPECT_EQ(4u, nodes[0].parentId);
    EXPECT_EQ(1u, nodes[0].sortOrder);
    EXPECT_EQ(1u, nodes[1].sortOrder);
    EXPECT_EQ(5u, nodes[2].parentId);
    EXPECT_EQ(4u, nodes[3].id);
    EXPECT_EQ(0u, nodes[3].parentId);
    EXPECT_EQ(0u, nodes[3].sortOrder);
    EXPECT_TRUE(nodes[3].isFolder);
    EXPECT_EQ(4u, nodes[4].parentId);
    EXPECT_EQ(0u, nodes[4].sortOrder);
    EXPECT_TRUE(nodes[4].isFolder);
    EXPECT_FALSE(nodes[2].isFolder);
    EXPECT_EQ(QStringLiteral("Nested playlist"), nodes[2].name);
    ASSERT_EQ(3, model->playlistEntries.size());
    for (const auto& entry : model->playlistEntries) {
        EXPECT_EQ(3u, entry.playlistId);
    }
    EXPECT_EQ(2u, model->playlistEntries[0].trackId);
    EXPECT_EQ(2u, model->playlistEntries[1].trackId);
    EXPECT_EQ(1u, model->playlistEntries[2].trackId);
    EXPECT_EQ(1u, model->playlistEntries[0].entryIndex);
    EXPECT_EQ(2u, model->playlistEntries[1].entryIndex);
    EXPECT_EQ(3u, model->playlistEntries[2].entryIndex);
}

TEST_F(LegacyFromDlpTest, MapsHistoriesAndPreservesPlayOrder) {
    ASSERT_NO_FATAL_FAILURE(addRows(R"sql(
        INSERT INTO content(content_id) VALUES (200), (4294967496);
        INSERT INTO history VALUES (9000000000, 2, 'Later', 0, 0),
            (9000000001, 1, 'Earlier', 0, 0);
        INSERT INTO history_content VALUES (9000000000, 200, 6),
            (9000000000, 4294967496, 2), (9000000001, 200, 1);
    )sql"));

    QString error;
    const auto model = legacyFromDlp(*m_pDb, &error);
    ASSERT_TRUE(model) << qPrintable(error);
    ASSERT_EQ(2, model->historyPlaylists.size());
    EXPECT_EQ(1u, model->historyPlaylists[0].id);
    EXPECT_EQ(QStringLiteral("Earlier"), model->historyPlaylists[0].name);
    EXPECT_EQ(2u, model->historyPlaylists[1].id);
    ASSERT_EQ(3, model->historyEntries.size());
    EXPECT_EQ(1u, model->historyEntries[0].playlistId);
    EXPECT_EQ(1u, model->historyEntries[0].trackId);
    EXPECT_EQ(2u, model->historyEntries[1].playlistId);
    EXPECT_EQ(2u, model->historyEntries[1].trackId);
    EXPECT_EQ(1u, model->historyEntries[1].entryIndex);
    EXPECT_EQ(2u, model->historyEntries[2].entryIndex);
}

TEST_F(LegacyFromDlpTest, ReadErrorDoesNotReturnAPartialLibrary) {
    ASSERT_NO_FATAL_FAILURE(addRows("DROP TABLE playlist_content;"));
    QString error;
    EXPECT_FALSE(legacyFromDlp(*m_pDb, &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(m_pDb->lastErrorMessage().isEmpty());
}

} // namespace mixxx::rekordbox

// End of AI-generated text.
