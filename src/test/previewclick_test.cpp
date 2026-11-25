// AI-generated (Claude Code). Start of AI-generated text.
// Preview from the track list: a click on the artwork starts the preview and a
// click during the preview skips ahead; a click near a cue mark on the overview
// column starts the preview at that cue.
// End of AI-generated text.
#include <gtest/gtest.h>

#include <QList>

#include "library/tabledelegates/previewclick.h"
#include "test/mixxxtest.h"
#include "track/track.h"

namespace {

using namespace previewclick;

TEST(PreviewClickTest, SkipAheadAddsThirtySeconds) {
    EXPECT_DOUBLE_EQ(40.0, skipAhead(10.0, kSkipAheadSeconds, 200.0));
}

TEST(PreviewClickTest, SkipAheadClampsAtTheEnd) {
    EXPECT_DOUBLE_EQ(200.0, skipAhead(180.0, kSkipAheadSeconds, 200.0));
    EXPECT_DOUBLE_EQ(200.0, skipAhead(200.0, kSkipAheadSeconds, 200.0));
}

TEST(PreviewClickTest, SkipAheadNeverGoesBelowZero) {
    EXPECT_DOUBLE_EQ(0.0, skipAhead(-5.0, 0.0, 200.0));
}

TEST(PreviewClickTest, CueXScalesToTheCellWidth) {
    EXPECT_EQ(0, cueX(0.0, 200.0, 400));
    EXPECT_EQ(200, cueX(100.0, 200.0, 400));
    EXPECT_EQ(399, cueX(200.0, 200.0, 400));
    EXPECT_EQ(0, cueX(10.0, 0.0, 400));
}

TEST(PreviewClickTest, NearestCuePicksTheClosestInsideTheTolerance) {
    const QList<int> xs = {10, 100, 104, 300};
    EXPECT_EQ(1, nearestCue(99, xs, 6));
    EXPECT_EQ(2, nearestCue(103, xs, 6));
    EXPECT_EQ(3, nearestCue(306, xs, 6));
}

TEST(PreviewClickTest, NearestCueIsMinusOneOutsideTheTolerance) {
    const QList<int> xs = {10, 100};
    EXPECT_EQ(-1, nearestCue(50, xs, 6));
    EXPECT_EQ(-1, nearestCue(107, xs, 6));
    EXPECT_EQ(-1, nearestCue(5, QList<int>(), 6));
}

TEST(PreviewClickTest, CueNearClickReturnsTheSecondsOfTheCue) {
    const QList<CueMark> marks = {{30.0, 0xff0000}, {120.0, 0x00ff00}};
    // Cell of 400 px for 200 s: the cue at 120 s is at x = 240.
    const auto hit = cueNearClick(marks, 243, 400, 200.0, kCueToleranceDefaultPx);
    ASSERT_TRUE(hit.has_value());
    EXPECT_DOUBLE_EQ(120.0, *hit);
}

TEST(PreviewClickTest, CueNearClickIsEmptyAwayFromAMark) {
    const QList<CueMark> marks = {{30.0, 0xff0000}};
    EXPECT_FALSE(cueNearClick(marks, 200, 400, 200.0, kCueToleranceDefaultPx).has_value());
    EXPECT_FALSE(cueNearClick({}, 60, 400, 200.0, kCueToleranceDefaultPx).has_value());
    EXPECT_FALSE(cueNearClick(marks, 60, 400, 0.0, kCueToleranceDefaultPx).has_value());
}

TEST(PreviewClickTest, CueMarksShowHotAndMemoryCuesOnly) {
    auto pTrack = Track::newTemporary();
    pTrack->setAudioProperties(mixxx::audio::ChannelCount(2),
            mixxx::audio::SampleRate(44100),
            mixxx::audio::Bitrate(),
            mixxx::Duration::fromSeconds(180));
    pTrack->createAndAddCue(mixxx::CueType::HotCue,
            0,
            mixxx::audio::FramePos(10 * 44100),
            mixxx::audio::kInvalidFramePos);
    pTrack->createAndAddCue(mixxx::CueType::MemoryCue,
            Cue::kNoHotCue,
            mixxx::audio::FramePos(60 * 44100),
            mixxx::audio::kInvalidFramePos);
    pTrack->createAndAddCue(mixxx::CueType::Intro,
            Cue::kNoHotCue,
            mixxx::audio::FramePos(5 * 44100),
            mixxx::audio::FramePos(8 * 44100));
    const QList<CueMark> marks = cueMarksOf(pTrack->getCuePoints(), 44100.0);
    ASSERT_EQ(2, marks.size());
    EXPECT_DOUBLE_EQ(10.0, marks[0].seconds);
    EXPECT_DOUBLE_EQ(60.0, marks[1].seconds);
    // The hot cue keeps its own colour and the memory cue is red.
    EXPECT_EQ(static_cast<unsigned int>(
                      mixxx::PredefinedColorPalettes::kDefaultCueColor),
            marks[0].rgb);
    EXPECT_EQ(0xE0202Au, marks[1].rgb);
}

TEST(PreviewClickTest, CueMarksOfNoCuesAreEmpty) {
    EXPECT_TRUE(cueMarksOf({}, 44100.0).isEmpty());
    EXPECT_TRUE(cueMarksOf({}, 0.0).isEmpty());
}

} // namespace
