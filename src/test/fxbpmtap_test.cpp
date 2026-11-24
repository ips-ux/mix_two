// AI-generated (Claude Code). Start of AI-generated text.
// Tests of the tapped tempo of the Mix Two BEAT FX (DDJ-800 manual p.64-66):
// the tap average and the override of the group feature state.
#include <gtest/gtest.h>

#include "effects/fxbpmtap.h"
#include "engine/effects/groupfeaturestate.h"

namespace {

// Taps at the given times (milliseconds). Returns whether the last one
// changed the BPM.
bool tapAll(FxBpmTap& tap, std::initializer_list<qint64> times) {
    bool changed = false;
    for (qint64 t : times) {
        changed = tap.tap(t);
    }
    return changed;
}

GroupFeatureState deckState() {
    GroupFeatureState state;
    state.beat_length = GroupFeatureBeatLength{0.4, 1.0};
    state.beat_fraction_buffer_end = 0.25;
    state.gain = 0.5;
    return state;
}

} // namespace

TEST(FxBpmTapTest, StartsInAutoWithNoBpm) {
    FxBpmTap tap;
    EXPECT_EQ(0.0, tap.bpm());
    EXPECT_FALSE(tap.isTapMode());
}

TEST(FxBpmTapTest, TwoTapsGiveNoBpm) {
    FxBpmTap tap;
    EXPECT_FALSE(tapAll(tap, {0, 500}));
    EXPECT_EQ(0.0, tap.bpm());
    EXPECT_FALSE(tap.isTapMode());
}

TEST(FxBpmTapTest, ThreeTapsGive120AndTapMode) {
    FxBpmTap tap;
    EXPECT_TRUE(tapAll(tap, {0, 500, 1000}));
    EXPECT_DOUBLE_EQ(120.0, tap.bpm());
    EXPECT_TRUE(tap.isTapMode());
}

TEST(FxBpmTapTest, FourthTapGivesAverageOfThreeIntervals) {
    FxBpmTap tap;
    tapAll(tap, {0, 500, 1000});
    EXPECT_TRUE(tap.tap(1600));
    // Intervals 500, 500 and 600 ms: average 533.33 ms.
    EXPECT_NEAR(60000.0 / (1600.0 / 3.0), tap.bpm(), 1e-9);
}

TEST(FxBpmTapTest, OnlyLastEightIntervalsCount) {
    FxBpmTap tap;
    // Two slow intervals (1000 ms), then eight of 500 ms.
    qint64 t = 0;
    tap.tap(t);
    t += 1000;
    tap.tap(t);
    t += 1000;
    tap.tap(t);
    for (int i = 0; i < 8; ++i) {
        t += 500;
        tap.tap(t);
    }
    EXPECT_DOUBLE_EQ(120.0, tap.bpm());
}

TEST(FxBpmTapTest, LongGapStartsNewRunAndKeepsOldBpm) {
    FxBpmTap tap;
    tapAll(tap, {0, 500, 1000});
    ASSERT_DOUBLE_EQ(120.0, tap.bpm());
    // The gap is 2500 ms: the first tap of a new run.
    EXPECT_FALSE(tap.tap(3500));
    EXPECT_DOUBLE_EQ(120.0, tap.bpm());
    EXPECT_FALSE(tap.tap(4000));
    EXPECT_DOUBLE_EQ(120.0, tap.bpm());
    // The third tap of the new run (400 ms intervals gives 150 BPM).
    tap = FxBpmTap();
    tapAll(tap, {0, 500, 1000});
    tapAll(tap, {3500, 3900});
    EXPECT_DOUBLE_EQ(120.0, tap.bpm());
    EXPECT_TRUE(tap.tap(4300));
    EXPECT_DOUBLE_EQ(150.0, tap.bpm());
}

TEST(FxBpmTapTest, GapOfExactlyTwoSecondsIsInTheSameRun) {
    FxBpmTap tap;
    tapAll(tap, {0, 1000, 2000});
    EXPECT_DOUBLE_EQ(60.0, tap.bpm());
    // A step of exactly 2000 ms is not a new run: intervals 1000, 1000, 2000.
    EXPECT_TRUE(tap.tap(4000));
    EXPECT_NEAR(60000.0 / (4000.0 / 3.0), tap.bpm(), 1e-9);
}

TEST(FxBpmTapTest, SlowRunBelow40BpmIsIgnored) {
    FxBpmTap tap;
    // Intervals of 2000 ms give 30 BPM.
    EXPECT_FALSE(tapAll(tap, {0, 2000, 4000}));
    EXPECT_EQ(0.0, tap.bpm());
    EXPECT_FALSE(tap.isTapMode());
}

TEST(FxBpmTapTest, FastRunAbove300BpmIsIgnored) {
    FxBpmTap tap;
    // Intervals of 150 ms give 400 BPM.
    EXPECT_FALSE(tapAll(tap, {0, 150, 300, 450}));
    EXPECT_EQ(0.0, tap.bpm());
    EXPECT_FALSE(tap.isTapMode());
}

TEST(FxBpmTapTest, IgnoredResultKeepsEarlierBpm) {
    FxBpmTap tap;
    tapAll(tap, {0, 500, 1000});
    ASSERT_DOUBLE_EQ(120.0, tap.bpm());
    // A new run of 150 ms intervals gives 400 BPM: ignored.
    EXPECT_FALSE(tapAll(tap, {4000, 4150, 4300}));
    EXPECT_DOUBLE_EQ(120.0, tap.bpm());
}

TEST(FxBpmTapTest, TapWithNoTimeStepIsIgnored) {
    FxBpmTap tap;
    tap.tap(100);
    EXPECT_FALSE(tap.tap(100));
    EXPECT_FALSE(tap.tap(50));
    EXPECT_EQ(0.0, tap.bpm());
}

TEST(FxBpmTapTest, AutoModeKeepsTheTappedBpm) {
    FxBpmTap tap;
    tapAll(tap, {0, 500, 1000});
    ASSERT_TRUE(tap.isTapMode());
    tap.setTapMode(false);
    EXPECT_FALSE(tap.isTapMode());
    EXPECT_DOUBLE_EQ(120.0, tap.bpm());
    // Back to TAP by hand with the kept BPM.
    tap.setTapMode(true);
    EXPECT_TRUE(tap.isTapMode());
}

TEST(FxBpmTapTest, TapModeByHandWithNoBpmGoesBackToAuto) {
    FxBpmTap tap;
    tap.setTapMode(true);
    EXPECT_FALSE(tap.isTapMode());
}

TEST(FxBpmTapTest, TapInAutoModeSwitchesToTapMode) {
    FxBpmTap tap;
    tapAll(tap, {0, 500, 1000});
    tap.setTapMode(false);
    EXPECT_TRUE(tap.tap(1500));
    EXPECT_TRUE(tap.isTapMode());
}

TEST(FxBpmTapOverrideTest, AutoReturnsStateUnchanged) {
    const GroupFeatureState in = deckState();
    const GroupFeatureState out = applyFxTapBpm(in, 0.0);
    ASSERT_TRUE(out.beat_length.has_value());
    EXPECT_DOUBLE_EQ(0.4, out.beat_length->seconds);
    EXPECT_DOUBLE_EQ(1.0, out.beat_length->scratch_rate);
    ASSERT_TRUE(out.beat_fraction_buffer_end.has_value());
    EXPECT_DOUBLE_EQ(0.25, *out.beat_fraction_buffer_end);
    ASSERT_TRUE(out.gain.has_value());
    EXPECT_DOUBLE_EQ(0.5, *out.gain);
}

TEST(FxBpmTapOverrideTest, TapReplacesBeatLengthAndDropsGridPosition) {
    const GroupFeatureState out = applyFxTapBpm(deckState(), 120.0);
    ASSERT_TRUE(out.beat_length.has_value());
    EXPECT_DOUBLE_EQ(0.5, out.beat_length->seconds);
    EXPECT_DOUBLE_EQ(1.0, out.beat_length->scratch_rate);
    EXPECT_FALSE(out.beat_fraction_buffer_end.has_value());
    // Other fields stay.
    ASSERT_TRUE(out.gain.has_value());
    EXPECT_DOUBLE_EQ(0.5, *out.gain);
}

TEST(FxBpmTapOverrideTest, TapWorksOnAStateWithNoBeatData) {
    const GroupFeatureState out = applyFxTapBpm(GroupFeatureState(), 120.0);
    ASSERT_TRUE(out.beat_length.has_value());
    EXPECT_DOUBLE_EQ(0.5, out.beat_length->seconds);
    EXPECT_FALSE(out.beat_fraction_buffer_end.has_value());
}
// End of AI-generated text.
