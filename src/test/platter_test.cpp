#include <gtest/gtest.h>

#include <cmath>

#include "control/controlobject.h"
#include "test/mockedenginebackendtest.h"

namespace {

/// Mix Two: the rekordbox-style platter ([ChannelN],platter_touch and
/// platter_turn), see docs/ddj800-program/SCRATCH-ENGINE.md.
class PlatterTest : public MockedEngineBackendTest {
  protected:
    static constexpr double kSecondsPerRevolution = 1.8; // 33 1/3 rpm

    /// The deck's position in seconds of track.
    double seconds() const {
        const double frames = ControlObject::get(ConfigKey(m_sGroup1, "playposition")) *
                ControlObject::get(ConfigKey(m_sGroup1, "track_samples")) / 2.0;
        return frames / ControlObject::get(ConfigKey(m_sGroup1, "track_samplerate"));
    }

    void touch(bool touched) {
        ControlObject::set(ConfigKey(m_sGroup1, "platter_touch"), touched ? 1.0 : 0.0);
    }

    void turn(double revolutions) {
        ControlObject::set(ConfigKey(m_sGroup1, "platter_turn"), revolutions);
    }

    void process(int buffers) {
        for (int i = 0; i < buffers; ++i) {
            ProcessBuffer();
        }
    }

    /// Plays a little, then touches the platter and waits out the dead zone.
    double touchWhilePlaying() {
        ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
        process(20);
        touch(true);
        process(10); // > 60 ms
        return seconds();
    }
};

TEST_F(PlatterTest, TouchHoldsTheTrack) {
    const double held = touchWhilePlaying();
    process(10);
    EXPECT_NEAR(held, seconds(), 1e-3);
}

TEST_F(PlatterTest, TheTrackFollowsThePlatter) {
    const double start = touchWhilePlaying();
    // Half a revolution forward in small steps, then a quarter back.
    for (int i = 0; i < 50; ++i) {
        turn(0.01);
        ProcessBuffer();
    }
    for (int i = 0; i < 25; ++i) {
        turn(-0.01);
        ProcessBuffer();
    }
    process(10);
    EXPECT_NEAR(start + 0.25 * kSecondsPerRevolution, seconds(), 2e-3);
}

TEST_F(PlatterTest, SpeedLimitDelaysButLosesNothing) {
    const double start = touchWhilePlaying();
    // A flick: about 12x in one buffer, more than the 8x limit.
    turn(0.08);
    ProcessBuffer();
    EXPECT_LT(seconds(), start + 0.08 * kSecondsPerRevolution - 1e-3);
    process(10);
    EXPECT_NEAR(start + 0.08 * kSecondsPerRevolution, seconds(), 2e-3);
}

TEST_F(PlatterTest, DeadZoneIgnoresTheLandingHand) {
    // Deck 1 gets a tiny turn right after the touch, deck 2 none; with the
    // same timing both must end up in the same place. (The playposition
    // display value updates only every few buffers, so compare settled
    // positions rather than reading one right after the touch.)
    for (const auto& group : {m_sGroup1, m_sGroup2}) {
        ControlObject::set(ConfigKey(group, "play"), 1.0);
    }
    process(20);
    for (const auto& group : {m_sGroup1, m_sGroup2}) {
        ControlObject::set(ConfigKey(group, "platter_touch"), 1.0);
    }
    ProcessBuffer();
    turn(0.001); // deck 1: under 9/3240 of a revolution, within 60 ms
    process(15);
    auto secondsOf = [](const QString& group) {
        return ControlObject::get(ConfigKey(group, "playposition")) *
                ControlObject::get(ConfigKey(group, "track_samples")) / 2.0 /
                ControlObject::get(ConfigKey(group, "track_samplerate"));
    };
    EXPECT_NEAR(secondsOf(m_sGroup2), secondsOf(m_sGroup1), 1e-4);
}

TEST_F(PlatterTest, GlitchesAreDropped) {
    const double start = touchWhilePlaying();
    turn(2.0); // two revolutions in one buffer: far above 25x
    process(10);
    EXPECT_NEAR(start, seconds(), 1e-3);
}

TEST_F(PlatterTest, ReleaseResumesPlay) {
    const double held = touchWhilePlaying();
    touch(false);
    process(20);
    EXPECT_GT(seconds(), held + 0.1);
}

} // namespace
