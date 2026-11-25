// AI-generated (Claude Code). Start of AI-generated text.
#include <gtest/gtest.h>

#include <QDateTime>
#include <QMetaProperty>
#include <QVariant>

#include "test/mixxxtest.h"
#include "track/track.h"

// The INFO tab of a deck reads these Track properties by name.
class TrackDisplayPropsTest : public MixxxTest {};

TEST_F(TrackDisplayPropsTest, propertiesExist) {
    const auto& meta = Track::staticMetaObject;
    EXPECT_NE(-1, meta.indexOfProperty("fileType"));
    EXPECT_NE(-1, meta.indexOfProperty("bitrateText"));
    EXPECT_NE(-1, meta.indexOfProperty("dateAddedText"));
}

TEST_F(TrackDisplayPropsTest, propertiesReturnStoredValues) {
    auto pTrack = Track::newTemporary();
    pTrack->setType(QStringLiteral("flac"));
    pTrack->setBitrate(320);
    pTrack->setDateAdded(QDateTime(QDate(2024, 3, 5), QTime(10, 0)));

    EXPECT_EQ(QStringLiteral("flac"), pTrack->property("fileType").toString());
    EXPECT_EQ(QStringLiteral("320"), pTrack->property("bitrateText").toString());
    const QString dateText = pTrack->property("dateAddedText").toString();
    EXPECT_FALSE(dateText.isEmpty());
    EXPECT_TRUE(dateText.contains(QStringLiteral("2024")) ||
            dateText.contains(QStringLiteral("24")));
}

TEST_F(TrackDisplayPropsTest, emptyValuesGiveEmptyText) {
    auto pTrack = Track::newTemporary();
    EXPECT_TRUE(pTrack->property("fileType").toString().isEmpty());
    EXPECT_TRUE(pTrack->property("bitrateText").toString().isEmpty());
    EXPECT_TRUE(pTrack->property("dateAddedText").toString().isEmpty());
}
// End of AI-generated text.
