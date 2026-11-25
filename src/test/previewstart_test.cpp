// AI-generated (Claude Code). Start of AI-generated text.
// A click on the overview column starts the preview at the clicked position,
// and the click flow of the track list drives the preview deck only as the
// rules say. The deck is a fake.
// End of AI-generated text.
#include <gtest/gtest.h>

#include <vector>

#include "library/tabledelegates/previewclick.h"
#include "library/tabledelegates/previewclickcontroller.h"
#include "test/mixxxtest.h"
#include "track/track.h"

namespace {

using previewclick::previewPositionFromClick;
using Origin = PreviewClickController::Origin;

TEST(PreviewStartTest, ClickInTheMiddleIsHalfTheTrack) {
    EXPECT_DOUBLE_EQ(100.0, previewPositionFromClick(200, 400, 200.0));
}

TEST(PreviewStartTest, ClickAtTheEdgesIsStartAndEnd) {
    EXPECT_DOUBLE_EQ(0.0, previewPositionFromClick(0, 400, 200.0));
    EXPECT_DOUBLE_EQ(200.0, previewPositionFromClick(400, 400, 200.0));
}

TEST(PreviewStartTest, ClickOutsideTheCellIsClamped) {
    EXPECT_DOUBLE_EQ(0.0, previewPositionFromClick(-20, 400, 200.0));
    EXPECT_DOUBLE_EQ(200.0, previewPositionFromClick(900, 400, 200.0));
}

TEST(PreviewStartTest, UnknownSizeGivesZero) {
    EXPECT_DOUBLE_EQ(0.0, previewPositionFromClick(10, 0, 200.0));
    EXPECT_DOUBLE_EQ(0.0, previewPositionFromClick(10, 400, 0.0));
}

// A fake preview deck. A load finishes only when the test calls finishLoad().
class FakePort : public PreviewPlayerPort {
  public:
    bool isLoaded(const TrackPointer& pTrack) const override {
        return pTrack && pTrack == loaded;
    }
    bool isPlaying() const override {
        return playing;
    }
    double positionSeconds(const TrackPointer&) const override {
        return position;
    }
    void load(const TrackPointer& pTrack) override {
        loading = pTrack;
        ++loadCount;
    }
    void seekAndPlay(const TrackPointer& pTrack, double seconds) override {
        // The deck can only seek the track that it holds.
        ASSERT_EQ(loaded, pTrack);
        position = seconds;
        playing = true;
        seeks.push_back(seconds);
    }
    void stop() override {
        playing = false;
        ++stopCount;
    }
    void finishLoad() {
        loaded = loading;
        loading.reset();
    }

    TrackPointer loaded;
    TrackPointer loading;
    bool playing = false;
    double position = 0.0;
    int loadCount = 0;
    int stopCount = 0;
    std::vector<double> seeks;
};

class PreviewFlowTest : public MixxxTest {
  protected:
    PreviewFlowTest()
            : controller(&port),
              track(Track::newTemporary()),
              other(Track::newTemporary()) {
    }
    // A click and the end of the double click time, as the view does it.
    void clickAndWait(const TrackPointer& pTrack, Origin origin, double start) {
        controller.clickOrArm(pTrack, origin, start, 200.0, now);
        now += 500;
        controller.fireDelayed(now, 500);
    }

    FakePort port;
    PreviewClickController controller;
    TrackPointer track;
    TrackPointer other;
    qint64 now = 1000;
};

TEST_F(PreviewFlowTest, ArtworkClickLoadsFirstAndPlaysFromTheStartAfterTheLoad) {
    port.loaded = other; // an old track is in the deck
    clickAndWait(track, Origin::Artwork, 0.0);
    EXPECT_EQ(1, port.loadCount);
    EXPECT_TRUE(port.seeks.empty());
    // The deck reports the old track first: nothing may be applied to it.
    controller.onTrackLoaded();
    EXPECT_TRUE(port.seeks.empty());
    port.finishLoad();
    controller.onDeckTrackChanged(track);
    controller.onTrackLoaded();
    ASSERT_EQ(1u, port.seeks.size());
    EXPECT_DOUBLE_EQ(0.0, port.seeks[0]);
    EXPECT_TRUE(port.playing);
    EXPECT_EQ(track, controller.owner());
}

TEST_F(PreviewFlowTest, ClickOnTheTrackThatTheDeckHoldsPlaysAtOnceAfterTheWait) {
    port.loaded = track;
    clickAndWait(track, Origin::Overview, 80.0);
    EXPECT_EQ(0, port.loadCount);
    ASSERT_EQ(1u, port.seeks.size());
    EXPECT_DOUBLE_EQ(80.0, port.seeks[0]);
}

TEST_F(PreviewFlowTest, NothingStartsBeforeTheDoubleClickTimeHasPassed) {
    port.loaded = track;
    controller.clickOrArm(track, Origin::Artwork, 0.0, 200.0, now);
    EXPECT_FALSE(controller.fireDelayed(now + 499, 500));
    EXPECT_TRUE(port.seeks.empty());
    EXPECT_TRUE(controller.fireDelayed(now + 500, 500));
    EXPECT_EQ(1u, port.seeks.size());
}

TEST_F(PreviewFlowTest, ADoubleClickCancelsTheStart) {
    port.loaded = other;
    controller.clickOrArm(track, Origin::Artwork, 0.0, 200.0, now);
    controller.cancelDelayed();
    EXPECT_FALSE(controller.fireDelayed(now + 600, 500));
    EXPECT_EQ(0, port.loadCount);
    EXPECT_TRUE(port.seeks.empty());
}

TEST_F(PreviewFlowTest, ArtworkClickDuringTheOwnPreviewSkipsAheadAtOnce) {
    port.loaded = track;
    clickAndWait(track, Origin::Artwork, 0.0);
    port.position = 10.0;
    EXPECT_TRUE(controller.isImmediate(track));
    controller.clickOrArm(track, Origin::Artwork, 0.0, 200.0, now);
    EXPECT_FALSE(controller.isArmed());
    EXPECT_DOUBLE_EQ(40.0, port.seeks.back());
    // The next fast click skips again, and clamps at the end.
    port.position = 190.0;
    controller.clickOrArm(track, Origin::Artwork, 0.0, 200.0, now);
    EXPECT_DOUBLE_EQ(200.0, port.seeks.back());
    EXPECT_EQ(0, port.loadCount);
}

TEST_F(PreviewFlowTest, OverviewClickDuringTheOwnPreviewSeeksAtOnce) {
    port.loaded = track;
    clickAndWait(track, Origin::Overview, 20.0);
    controller.clickOrArm(track, Origin::Overview, 150.0, 200.0, now);
    EXPECT_DOUBLE_EQ(150.0, port.seeks.back());
}

TEST_F(PreviewFlowTest, APreviewOfAnotherSourceIsNotTakenOver) {
    // The preview button plays this track. A click does not skip it, seek it or own it.
    port.loaded = track;
    port.playing = true;
    port.position = 12.0;
    clickAndWait(track, Origin::Artwork, 0.0);
    EXPECT_TRUE(port.seeks.empty());
    EXPECT_FALSE(controller.owner());
    controller.leftCell();
    controller.stop();
    EXPECT_EQ(0, port.stopCount);
    EXPECT_TRUE(port.playing);
}

TEST_F(PreviewFlowTest, LeavingTheCellStopsAnArtworkPreview) {
    port.loaded = track;
    clickAndWait(track, Origin::Artwork, 0.0);
    controller.leftCell();
    EXPECT_FALSE(port.playing);
    EXPECT_FALSE(controller.owner());
}

TEST_F(PreviewFlowTest, LeavingTheCellKeepsAnOverviewPreview) {
    port.loaded = track;
    clickAndWait(track, Origin::Overview, 50.0);
    controller.leftCell();
    EXPECT_TRUE(port.playing);
    EXPECT_EQ(track, controller.owner());
}

TEST_F(PreviewFlowTest, LeavingTheCellDropsAClickThatWaits) {
    port.loaded = track;
    controller.clickOrArm(track, Origin::Artwork, 0.0, 200.0, now);
    controller.leftCell();
    EXPECT_FALSE(controller.isArmed());
    EXPECT_FALSE(controller.fireDelayed(now + 600, 500));
}

TEST_F(PreviewFlowTest, OwnerEndsWhenThePreviewStopsSoANewPreviewIsNotStopped) {
    port.loaded = track;
    clickAndWait(track, Origin::Artwork, 0.0);
    // The preview ends (or the user stops it with the stop control).
    port.playing = false;
    controller.onPlayChanged(false);
    EXPECT_FALSE(controller.owner());
    // The preview button now plays the same track. The mouse leaves the cell.
    port.playing = true;
    controller.leftCell();
    EXPECT_TRUE(port.playing);
    EXPECT_EQ(0, port.stopCount);
}

TEST_F(PreviewFlowTest, OwnerEndsWhenAnotherTrackReachesTheDeck) {
    port.loaded = track;
    clickAndWait(track, Origin::Artwork, 0.0);
    port.loaded = other;
    controller.onDeckTrackChanged(other);
    EXPECT_FALSE(controller.owner());
    controller.leftCell();
    EXPECT_EQ(0, port.stopCount);
}

TEST_F(PreviewFlowTest, AStartThatWaitsForTheLoadIsDroppedWhenAnotherTrackArrives) {
    port.loaded = other;
    clickAndWait(track, Origin::Artwork, 0.0);
    // The button loads a third track while ours loads.
    auto third = Track::newTemporary();
    port.loaded = third;
    controller.onDeckTrackChanged(third);
    controller.onTrackLoaded();
    EXPECT_TRUE(port.seeks.empty());
}

TEST_F(PreviewFlowTest, StopStopsOnlyWhatTheControllerStarted) {
    port.loaded = track;
    clickAndWait(track, Origin::Overview, 10.0);
    controller.stop();
    EXPECT_EQ(1, port.stopCount);
    controller.stop();
    EXPECT_EQ(1, port.stopCount);
}

} // namespace
