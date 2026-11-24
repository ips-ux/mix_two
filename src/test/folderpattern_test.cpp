// AI-generated (Claude Code). Start of AI-generated text.
// Tests for the folder pattern of the library.
#include "library/consolidate/folderpattern.h"

#include <gtest/gtest.h>

#include "track/trackrecord.h"

namespace {

using mixxx::library::defaultFolderPattern;
using mixxx::library::libraryFolderFor;
using mixxx::library::sanitizePathPart;

mixxx::TrackRecord makeRecord(const QString& artist,
        const QString& album = QString(),
        const QString& year = QString(),
        const QString& genre = QString()) {
    mixxx::TrackRecord record;
    auto& metadata = record.refMetadata();
    metadata.refTrackInfo().setArtist(artist);
    metadata.refTrackInfo().setYear(year);
    metadata.refTrackInfo().setGenre(genre);
    metadata.refAlbumInfo().setTitle(album);
    return record;
}

TEST(FolderPatternTest, DefaultPatternIsArtistAndFile) {
    EXPECT_EQ(QStringLiteral("<artist>/<file>"), defaultFolderPattern());
    EXPECT_EQ(QStringLiteral("Daft Punk"),
            libraryFolderFor(makeRecord(QStringLiteral("Daft Punk")), defaultFolderPattern()));
}

TEST(FolderPatternTest, BlankPatternIsTheDefault) {
    EXPECT_EQ(QStringLiteral("Daft Punk"),
            libraryFolderFor(makeRecord(QStringLiteral("Daft Punk")), QString()));
    EXPECT_EQ(QStringLiteral("Daft Punk"),
            libraryFolderFor(makeRecord(QStringLiteral("Daft Punk")), QStringLiteral("  ")));
}

TEST(FolderPatternTest, AlbumAndYearPattern) {
    const auto record = makeRecord(QStringLiteral("Daft Punk"),
            QStringLiteral("Discovery"),
            QStringLiteral("2001"),
            QStringLiteral("House"));
    EXPECT_EQ(QStringLiteral("Daft Punk/Discovery"),
            libraryFolderFor(record, QStringLiteral("<artist>/<album>/<file>")));
    EXPECT_EQ(QStringLiteral("Daft Punk/2001 - Discovery"),
            libraryFolderFor(record, QStringLiteral("<artist>/<year> - <album>/<file>")));
    EXPECT_EQ(QStringLiteral("House/Daft Punk"),
            libraryFolderFor(record, QStringLiteral("<genre>/<artist>/<file>")));
    // Back slashes also separate the parts.
    EXPECT_EQ(QStringLiteral("House/Daft Punk"),
            libraryFolderFor(record, QStringLiteral("<genre>\\<artist>\\<file>")));
}

TEST(FolderPatternTest, PatternWithoutFileTokenIsAllFolder) {
    EXPECT_EQ(QStringLiteral("Daft Punk/Discovery"),
            libraryFolderFor(makeRecord(QStringLiteral("Daft Punk"), QStringLiteral("Discovery")),
                    QStringLiteral("<artist>/<album>")));
}

TEST(FolderPatternTest, NothingAfterTheFileTokenCounts) {
    EXPECT_EQ(QStringLiteral("Daft Punk"),
            libraryFolderFor(makeRecord(QStringLiteral("Daft Punk"), QStringLiteral("Discovery")),
                    QStringLiteral("<artist>/<file>/<album>")));
}

TEST(FolderPatternTest, IllegalCharactersBecomeUnderscores) {
    EXPECT_EQ(QStringLiteral("AC_DC"),
            libraryFolderFor(makeRecord(QStringLiteral("AC/DC")), defaultFolderPattern()));
    EXPECT_EQ(QStringLiteral("What_ Who_ _Why_"),
            sanitizePathPart(QStringLiteral("What? Who* \"Why\"")));
    EXPECT_EQ(QStringLiteral("a_b_c_d_e_f_g_h_i"),
            sanitizePathPart(QStringLiteral("a\\b/c:d*e?f\"g<h>i")));
    EXPECT_EQ(QStringLiteral("a_b"), sanitizePathPart(QStringLiteral("a|b")));
    EXPECT_EQ(QStringLiteral("a_b"), sanitizePathPart(QStringLiteral("a\tb")));
}

TEST(FolderPatternTest, TrailingDotsAndSpacesAreCut) {
    EXPECT_EQ(QStringLiteral("Mr. Oizo"), sanitizePathPart(QStringLiteral("  Mr. Oizo. . ")));
    EXPECT_EQ(QString(), sanitizePathPart(QStringLiteral("...")));
    EXPECT_EQ(QString(), sanitizePathPart(QStringLiteral("   ")));
}

TEST(FolderPatternTest, DotNamesAndReservedNamesAreSafe) {
    EXPECT_EQ(QStringLiteral("_CON"), sanitizePathPart(QStringLiteral("CON")));
    EXPECT_EQ(QStringLiteral("_nul"), sanitizePathPart(QStringLiteral("nul")));
    EXPECT_EQ(QStringLiteral("_COM1"), sanitizePathPart(QStringLiteral("COM1")));
    EXPECT_EQ(QStringLiteral("Console"), sanitizePathPart(QStringLiteral("Console")));
    // A folder name never walks out of the library folder: ".." has only
    // dots, so it is cut to nothing and the artist counts as empty.
    EXPECT_EQ(QStringLiteral("Various-Uncategorized"),
            libraryFolderFor(makeRecord(QStringLiteral("..")), defaultFolderPattern()));
    EXPECT_EQ(QStringLiteral("a/b"),
            libraryFolderFor(makeRecord(QStringLiteral("a")),
                    QStringLiteral("<artist>/./b/<file>")));
    EXPECT_EQ(QStringLiteral("a/b"),
            libraryFolderFor(makeRecord(QStringLiteral("a")),
                    QStringLiteral("<artist>/../b/<file>")));
}

TEST(FolderPatternTest, EmptyArtistGoesToVariousUncategorized) {
    EXPECT_EQ(QStringLiteral("Various-Uncategorized"),
            libraryFolderFor(makeRecord(QString()), defaultFolderPattern()));
    EXPECT_EQ(QStringLiteral("Various-Uncategorized"),
            libraryFolderFor(makeRecord(QStringLiteral("  ")), defaultFolderPattern()));
    EXPECT_EQ(QStringLiteral("Various-Uncategorized"),
            libraryFolderFor(makeRecord(QString(), QStringLiteral("Discovery")),
                    QStringLiteral("<artist>/<album>/<file>")));
}

TEST(FolderPatternTest, EmptyArtistDoesNotMatterWhenPatternHasNoArtist) {
    EXPECT_EQ(QStringLiteral("House"),
            libraryFolderFor(makeRecord(QString(), QString(), QString(), QStringLiteral("House")),
                    QStringLiteral("<genre>/<file>")));
}

TEST(FolderPatternTest, EmptyOtherTokensLeaveTheirFolderOut) {
    EXPECT_EQ(QStringLiteral("Daft Punk"),
            libraryFolderFor(makeRecord(QStringLiteral("Daft Punk")),
                    QStringLiteral("<artist>/<album>/<file>")));
    EXPECT_EQ(QStringLiteral("Daft Punk/Discovery"),
            libraryFolderFor(makeRecord(QStringLiteral("Daft Punk"), QStringLiteral("Discovery")),
                    QStringLiteral("<artist>/<genre>/<album>/<file>")));
}

TEST(FolderPatternTest, EmptyFolderWhenNoPartIsLeft) {
    EXPECT_EQ(QString(),
            libraryFolderFor(makeRecord(QStringLiteral("Daft Punk")), QStringLiteral("<file>")));
    EXPECT_EQ(QString(),
            libraryFolderFor(makeRecord(QStringLiteral("Daft Punk")),
                    QStringLiteral("<album>/<file>")));
}

TEST(FolderPatternTest, TokensAreNotCaseSensitive) {
    EXPECT_EQ(QStringLiteral("Daft Punk"),
            libraryFolderFor(makeRecord(QStringLiteral("Daft Punk")),
                    QStringLiteral("<Artist>/<FILE>")));
}

} // namespace

// End of AI-generated text.
