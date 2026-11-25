// AI-generated (Claude Code). Start of AI-generated text.
// Tests for the next-cue countdown (mixxx::beatsToNextCue) and its
// bars.beats text (mixxx::formatBarsBeats).
// End of AI-generated text.
#include "engine/controls/nextcue.h"

#include <gtest/gtest.h>

#include <array>

#include "audio/types.h"
#include "track/beats.h"
#include "track/bpm.h"
#include "widget/wbeatcount.h"

namespace {

constexpr auto kSampleRate = mixxx::audio::SampleRate(44100);
constexpr auto kBpm = mixxx::Bpm(120.0);
// 120 BPM at 44100 Hz: 22050 frames per beat.
constexpr double kBeatFrames = 22050.0;
constexpr double kNoCue = -1.0;

/// The engine sample position (stereo samples) of a beat.
double beatToSamplePos(double beat) {
    return mixxx::audio::FramePos(beat * kBeatFrames).toEngineSamplePos();
}

class NextCueTest : public testing::Test {
  protected:
    mixxx::BeatsPointer m_pBeats = mixxx::Beats::fromConstTempo(
            kSampleRate, mixxx::audio::kStartFramePos, kBpm);
};

TEST_F(NextCueTest, NoCuesIsMinusOne) {
    const std::array<double, 3> cues = {kNoCue, kNoCue, kNoCue};
    EXPECT_EQ(-1.0,
            mixxx::beatsToNextCue(*m_pBeats, mixxx::audio::kStartFramePos, cues));
    EXPECT_EQ(-1.0,
            mixxx::beatsToNextCue(*m_pBeats,
                    mixxx::audio::kStartFramePos,
                    std::span<const double>()));
}

TEST_F(NextCueTest, CueBehindIgnored) {
    const auto position = mixxx::audio::FramePos(8 * kBeatFrames);
    const std::array<double, 2> cues = {beatToSamplePos(2), beatToSamplePos(8)};
    // The cue at 2 is behind; the cue at 8 is at the position, not after it.
    EXPECT_EQ(-1.0, mixxx::beatsToNextCue(*m_pBeats, position, cues));
}

TEST_F(NextCueTest, CountsBeats) {
    const std::array<double, 1> cues = {beatToSamplePos(8)};
    EXPECT_DOUBLE_EQ(8.0,
            mixxx::beatsToNextCue(*m_pBeats, mixxx::audio::kStartFramePos, cues));
    // Between two beats the count has a fraction.
    EXPECT_DOUBLE_EQ(5.5,
            mixxx::beatsToNextCue(*m_pBeats,
                    mixxx::audio::FramePos(2.5 * kBeatFrames),
                    cues));
}

TEST_F(NextCueTest, PicksNearestOfHotAndMemory) {
    // Hot cue slots first, then memory cue slots, as CueControl fills them.
    const std::array<double, 4> cues = {
            beatToSamplePos(16), kNoCue, beatToSamplePos(1), beatToSamplePos(4)};
    const auto position = mixxx::audio::FramePos(2 * kBeatFrames);
    EXPECT_DOUBLE_EQ(2.0, mixxx::beatsToNextCue(*m_pBeats, position, cues));
}

TEST_F(NextCueTest, CountdownRoundingShowsWholeBeat) {
    // One frame after the grid start the cue is 7.99995 beats away. The
    // text is 2.0 (8 beats), not 1.3.
    const std::array<double, 1> cues = {beatToSamplePos(8)};
    const double beats = mixxx::beatsToNextCue(*m_pBeats, mixxx::audio::FramePos(1.0), cues);
    EXPECT_LT(beats, 8.0);
    EXPECT_EQ(QStringLiteral("2.0"), mixxx::formatBarsBeats(beats));
}

TEST(WBeatCountTest, WholeBeatCountTolerance) {
    EXPECT_EQ(8, mixxx::wholeBeatCount(8.0));
    EXPECT_EQ(8, mixxx::wholeBeatCount(7.9999));
    EXPECT_EQ(8, mixxx::wholeBeatCount(7.995));
    EXPECT_EQ(7, mixxx::wholeBeatCount(7.98));
    EXPECT_EQ(7, mixxx::wholeBeatCount(7.5));
    EXPECT_EQ(0, mixxx::wholeBeatCount(0.0));
    EXPECT_EQ(-1, mixxx::wholeBeatCount(-1.0));
}

TEST(WBeatCountTest, FormatsRounding) {
    EXPECT_EQ(QStringLiteral("2.0"), mixxx::formatBarsBeats(7.9999));
    EXPECT_EQ(QStringLiteral("1.0"), mixxx::formatBarsBeats(3.999));
    EXPECT_EQ(QStringLiteral("1.3"), mixxx::formatBarsBeats(7.98));
    EXPECT_EQ(QStringLiteral("0.0"), mixxx::formatBarsBeats(0.0));
}

TEST_F(NextCueTest, CurrentCountFromGridStart) {
    EXPECT_DOUBLE_EQ(0.0,
            mixxx::beatsFromGridStart(*m_pBeats, mixxx::audio::kStartFramePos));
    EXPECT_DOUBLE_EQ(8.0,
            mixxx::beatsFromGridStart(*m_pBeats, mixxx::audio::FramePos(8 * kBeatFrames)));
    EXPECT_DOUBLE_EQ(5.5,
            mixxx::beatsFromGridStart(*m_pBeats, mixxx::audio::FramePos(5.5 * kBeatFrames)));
    // 51 beats are 12 bars and 3 beats.
    EXPECT_EQ(QStringLiteral("12.3"),
            mixxx::formatBarsBeats(mixxx::beatsFromGridStart(
                    *m_pBeats, mixxx::audio::FramePos(51 * kBeatFrames))));
}

TEST_F(NextCueTest, CurrentCountInvalidAndBeforeFirstBeat) {
    EXPECT_EQ(-1.0, mixxx::beatsFromGridStart(*m_pBeats, mixxx::audio::kInvalidFramePos));
    // The first beat is half a beat into the track.
    const auto pLate = mixxx::Beats::fromConstTempo(
            kSampleRate, mixxx::audio::FramePos(0.5 * kBeatFrames), kBpm);
    EXPECT_EQ(-1.0,
            mixxx::beatsFromGridStart(*pLate, mixxx::audio::FramePos(0.25 * kBeatFrames)));
    EXPECT_DOUBLE_EQ(0.0,
            mixxx::beatsFromGridStart(*pLate, mixxx::audio::FramePos(0.5 * kBeatFrames)));
    EXPECT_DOUBLE_EQ(1.5,
            mixxx::beatsFromGridStart(*pLate, mixxx::audio::FramePos(2.0 * kBeatFrames)));
}

TEST(WBeatCountTest, Formats) {
    EXPECT_EQ(QStringLiteral("12.3"), mixxx::formatBarsBeats(51.0));
    EXPECT_EQ(QStringLiteral("1.0"), mixxx::formatBarsBeats(4.0));
    EXPECT_EQ(QStringLiteral("-"), mixxx::formatBarsBeats(-1.0));
}

} // namespace
