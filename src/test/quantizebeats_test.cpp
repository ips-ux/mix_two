// AI-generated (Claude Code). Start of AI-generated text.
// Tests for [ChannelN],quantize_beats: the beat value (1/8, 1/4, 1/2 or 1
// beat) that quantized cue, hot cue and loop actions snap to.
// End of AI-generated text.

#include <gtest/gtest.h>

#include "control/controlproxy.h"
#include "engine/controls/cuecontrol.h"
#include "engine/controls/quantizecontrol.h"
#include "test/signalpathtest.h"

namespace {

mixxx::audio::FrameDiff_t beatLengthFrames(TrackPointer pTrack) {
    return (60.0 / pTrack->getBpm()) * pTrack->getSampleRate();
}

} // namespace

class QuantizeBeatsTest : public BaseSignalPathTest {
  protected:
    void SetUp() override {
        BaseSignalPathTest::SetUp();
        m_pQuantizeEnabled = std::make_unique<ControlProxy>(m_sGroup1, "quantize");
        // AllowMissingOrInvalid: a missing control fails the test, not the run.
        m_pQuantizeBeats = std::make_unique<ControlProxy>(m_sGroup1,
                "quantize_beats",
                nullptr,
                ControlFlag::AllowMissingOrInvalid);
        m_pQuantizeBeatsCycle = std::make_unique<ControlProxy>(m_sGroup1,
                "quantize_beats_cycle",
                nullptr,
                ControlFlag::AllowMissingOrInvalid);
        m_pClosestBeat = std::make_unique<ControlProxy>(m_sGroup1, "beat_closest");
        m_pPrevBeat = std::make_unique<ControlProxy>(m_sGroup1, "beat_prev");
        m_pNextBeat = std::make_unique<ControlProxy>(m_sGroup1, "beat_next");
        m_pHotcue1Set = std::make_unique<ControlProxy>(m_sGroup1, "hotcue_1_set");
        m_pHotcue1Position = std::make_unique<ControlProxy>(m_sGroup1, "hotcue_1_position");
        m_pLoopIn = std::make_unique<ControlProxy>(m_sGroup1, "loop_in");
        m_pLoopStartPosition = std::make_unique<ControlProxy>(
                m_sGroup1, "loop_start_position");
        m_pBeatloopSize = std::make_unique<ControlProxy>(m_sGroup1, "beatloop_size");
        m_pBeatloopActivate = std::make_unique<ControlProxy>(m_sGroup1, "beatloop_activate");
    }

    TrackPointer loadTestTrackWithBpm(double bpm) {
        const QString location = getTestDir().filePath(QStringLiteral("sine-30.wav"));
        TrackPointer pTrack = Track::newTemporary(
                mixxx::FileAccess(mixxx::FileInfo(location)));
        pTrack->setAudioProperties(
                mixxx::audio::ChannelCount(2),
                mixxx::audio::SampleRate(44100),
                mixxx::audio::Bitrate(),
                mixxx::Duration::fromSeconds(180));
        pTrack->trySetBpm(bpm);
        BaseSignalPathTest::loadTrack(m_pMixerDeck1.get(), pTrack);
        ProcessBuffer();
        return pTrack;
    }

    void setCurrentFramePosition(mixxx::audio::FramePos position) {
        m_pChannel1->getEngineBuffer()->queueNewPlaypos(position, EngineBuffer::SEEK_STANDARD);
        ProcessBuffer();
    }

    std::unique_ptr<ControlProxy> m_pQuantizeEnabled;
    std::unique_ptr<ControlProxy> m_pQuantizeBeats;
    std::unique_ptr<ControlProxy> m_pQuantizeBeatsCycle;
    std::unique_ptr<ControlProxy> m_pClosestBeat;
    std::unique_ptr<ControlProxy> m_pPrevBeat;
    std::unique_ptr<ControlProxy> m_pNextBeat;
    std::unique_ptr<ControlProxy> m_pHotcue1Set;
    std::unique_ptr<ControlProxy> m_pHotcue1Position;
    std::unique_ptr<ControlProxy> m_pLoopIn;
    std::unique_ptr<ControlProxy> m_pLoopStartPosition;
    std::unique_ptr<ControlProxy> m_pBeatloopSize;
    std::unique_ptr<ControlProxy> m_pBeatloopActivate;
};

TEST_F(QuantizeBeatsTest, DefaultIsWholeBeat) {
    ASSERT_TRUE(m_pQuantizeBeats->valid());
    EXPECT_DOUBLE_EQ(1.0, m_pQuantizeBeats->get());

    TrackPointer pTrack = loadTestTrackWithBpm(120.0);
    const auto beat = beatLengthFrames(pTrack);
    m_pQuantizeEnabled->set(1);
    setCurrentFramePosition(mixxx::audio::FramePos(8.3 * beat));

    EXPECT_FRAMEPOS_EQ_CONTROL(mixxx::audio::FramePos(8 * beat), m_pClosestBeat);
}

TEST_F(QuantizeBeatsTest, QuarterSnapsToSubdivision) {
    TrackPointer pTrack = loadTestTrackWithBpm(120.0);
    const auto beat = beatLengthFrames(pTrack);
    m_pQuantizeEnabled->set(1);
    m_pQuantizeBeats->set(0.25);
    EXPECT_DOUBLE_EQ(0.25, m_pQuantizeBeats->get());
    setCurrentFramePosition(mixxx::audio::FramePos(8.3 * beat));

    EXPECT_FRAMEPOS_EQ_CONTROL(mixxx::audio::FramePos(8.25 * beat), m_pClosestBeat);
    // beat_prev and beat_next stay whole beats.
    EXPECT_FRAMEPOS_EQ_CONTROL(mixxx::audio::FramePos(8 * beat), m_pPrevBeat);
    EXPECT_FRAMEPOS_EQ_CONTROL(mixxx::audio::FramePos(9 * beat), m_pNextBeat);

    // A quantized hot cue snaps to the subdivision too.
    m_pHotcue1Set->set(1);
    m_pHotcue1Set->set(0);
    EXPECT_FRAMEPOS_EQ_CONTROL(mixxx::audio::FramePos(8.25 * beat), m_pHotcue1Position);
}

TEST_F(QuantizeBeatsTest, NoEffectWhenQuantizeOff) {
    TrackPointer pTrack = loadTestTrackWithBpm(120.0);
    const auto beat = beatLengthFrames(pTrack);
    m_pQuantizeEnabled->set(0);
    m_pQuantizeBeats->set(0.25);
    const auto position = mixxx::audio::FramePos(8.3 * beat);
    setCurrentFramePosition(position);

    m_pHotcue1Set->set(1);
    m_pHotcue1Set->set(0);
    EXPECT_FRAMEPOS_EQ_CONTROL(position, m_pHotcue1Position);

    m_pLoopIn->set(1);
    m_pLoopIn->set(0);
    ProcessBuffer();
    EXPECT_FRAMEPOS_EQ_CONTROL(position, m_pLoopStartPosition);
}

TEST_F(QuantizeBeatsTest, InvalidValueIgnored) {
    m_pQuantizeBeats->set(0.5);
    EXPECT_DOUBLE_EQ(0.5, m_pQuantizeBeats->get());
    m_pQuantizeBeats->set(0.3);
    EXPECT_DOUBLE_EQ(0.5, m_pQuantizeBeats->get());
    m_pQuantizeBeats->set(0.0);
    EXPECT_DOUBLE_EQ(0.5, m_pQuantizeBeats->get());
    m_pQuantizeBeats->set(2.0);
    EXPECT_DOUBLE_EQ(0.5, m_pQuantizeBeats->get());
}

TEST_F(QuantizeBeatsTest, LoopInSnapsToSubdivision) {
    TrackPointer pTrack = loadTestTrackWithBpm(120.0);
    const auto beat = beatLengthFrames(pTrack);
    m_pQuantizeEnabled->set(1);
    m_pQuantizeBeats->set(0.5);
    setCurrentFramePosition(mixxx::audio::FramePos(8.3 * beat));

    m_pLoopIn->set(1);
    m_pLoopIn->set(0);
    ProcessBuffer();
    EXPECT_FRAMEPOS_EQ_CONTROL(mixxx::audio::FramePos(8.5 * beat), m_pLoopStartPosition);
}

TEST_F(QuantizeBeatsTest, BeatloopStartsOnSubdivision) {
    TrackPointer pTrack = loadTestTrackWithBpm(120.0);
    const auto beat = beatLengthFrames(pTrack);
    m_pQuantizeEnabled->set(1);
    m_pQuantizeBeats->set(0.125);
    setCurrentFramePosition(mixxx::audio::FramePos(8.3 * beat));

    m_pBeatloopSize->set(4);
    m_pBeatloopActivate->set(1);
    m_pBeatloopActivate->set(0);
    ProcessBuffer();
    // 8.3 beats: the closest 1/8 point is 8.25 beats.
    EXPECT_FRAMEPOS_EQ_CONTROL(mixxx::audio::FramePos(8.25 * beat), m_pLoopStartPosition);
}

TEST_F(QuantizeBeatsTest, CycleGoesThroughTheValues) {
    EXPECT_DOUBLE_EQ(1.0, m_pQuantizeBeats->get());
    const double expected[] = {0.125, 0.25, 0.5, 1.0, 0.125};
    for (double value : expected) {
        m_pQuantizeBeatsCycle->set(1);
        m_pQuantizeBeatsCycle->set(0);
        EXPECT_DOUBLE_EQ(value, m_pQuantizeBeats->get());
    }
}

TEST(QuantizeSubdivisionTest, HelperRoundsToStep) {
    using mixxx::audio::FramePos;
    const FramePos prev(1000);
    const FramePos next(2000);
    EXPECT_DOUBLE_EQ(1000.0, quantizeSubdivided(prev, next, FramePos(1400), 1.0).value());
    EXPECT_DOUBLE_EQ(2000.0, quantizeSubdivided(prev, next, FramePos(1600), 1.0).value());
    EXPECT_DOUBLE_EQ(1250.0, quantizeSubdivided(prev, next, FramePos(1300), 0.25).value());
    EXPECT_DOUBLE_EQ(1375.0, quantizeSubdivided(prev, next, FramePos(1400), 0.125).value());
    EXPECT_DOUBLE_EQ(2000.0, quantizeSubdivided(prev, next, FramePos(1900), 0.5).value());
}
