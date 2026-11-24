// AI-generated (Claude Code). Start of AI-generated text.
#include <gtest/gtest.h>

#include "library/trackset/playtimegate.h"
#include "track/trackid.h"

namespace {

const QString kDeck1 = QStringLiteral("[Channel1]");
const QString kDeck2 = QStringLiteral("[Channel2]");
const TrackId kTrackA(QVariant(1));
const TrackId kTrackB(QVariant(2));

TEST(PlayTimeGateTest, FiftyNineSecondsDoNotCount) {
    PlayTimeGate gate;
    EXPECT_FALSE(gate.advance(kDeck1, kTrackA, true, 59.0));
}

TEST(PlayTimeGateTest, SixtySecondsCount) {
    PlayTimeGate gate;
    EXPECT_FALSE(gate.advance(kDeck1, kTrackA, true, 59.0));
    EXPECT_TRUE(gate.advance(kDeck1, kTrackA, true, 1.0));
}

TEST(PlayTimeGateTest, ItFiresOnlyOncePerLoad) {
    PlayTimeGate gate;
    EXPECT_TRUE(gate.advance(kDeck1, kTrackA, true, 61.0));
    EXPECT_FALSE(gate.advance(kDeck1, kTrackA, true, 30.0));
    EXPECT_FALSE(gate.advance(kDeck1, kTrackA, true, 90.0));
}

TEST(PlayTimeGateTest, PauseKeepsTheCount) {
    PlayTimeGate gate;
    EXPECT_FALSE(gate.advance(kDeck1, kTrackA, true, 40.0));
    // A paused deck does not add time, however long the pause is.
    EXPECT_FALSE(gate.advance(kDeck1, kTrackA, false, 500.0));
    EXPECT_FALSE(gate.advance(kDeck1, kTrackA, true, 19.0));
    EXPECT_TRUE(gate.advance(kDeck1, kTrackA, true, 1.0));
}

TEST(PlayTimeGateTest, EachDeckCountsAlone) {
    PlayTimeGate gate;
    EXPECT_FALSE(gate.advance(kDeck1, kTrackA, true, 40.0));
    EXPECT_FALSE(gate.advance(kDeck2, kTrackB, true, 40.0));
    EXPECT_TRUE(gate.advance(kDeck1, kTrackA, true, 20.0));
    EXPECT_FALSE(gate.advance(kDeck2, kTrackB, true, 10.0));
    EXPECT_TRUE(gate.advance(kDeck2, kTrackB, true, 10.0));
}

TEST(PlayTimeGateTest, ANewTrackOnTheDeckStartsAtZero) {
    PlayTimeGate gate;
    EXPECT_FALSE(gate.advance(kDeck1, kTrackA, true, 50.0));
    EXPECT_FALSE(gate.advance(kDeck1, kTrackB, true, 50.0));
    EXPECT_TRUE(gate.advance(kDeck1, kTrackB, true, 10.0));
}

TEST(PlayTimeGateTest, ResetStartsTheSameTrackAgain) {
    PlayTimeGate gate;
    EXPECT_TRUE(gate.advance(kDeck1, kTrackA, true, 60.0));
    gate.reset(kDeck1);
    EXPECT_FALSE(gate.advance(kDeck1, kTrackA, true, 59.0));
    EXPECT_TRUE(gate.advance(kDeck1, kTrackA, true, 1.0));
}

TEST(PlayTimeGateTest, AnInvalidTrackNeverFires) {
    PlayTimeGate gate;
    EXPECT_FALSE(gate.advance(kDeck1, TrackId(), true, 120.0));
}

} // namespace
// End of AI-generated text.
