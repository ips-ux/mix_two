#include <gtest/gtest.h>

#include <cmath>

#include "control/controlobject.h"
#include "engine/channels/enginedeck.h"
#include "engine/enginebuffer.h"
#include "test/mockedenginebackendtest.h"
#include "util/time.h"

namespace {

/// Mix Two: the rekordbox-style platter ([ChannelN],platter_touch and
/// platter_turn), see docs/ddj800-program/SCRATCH-ENGINE.md.
class PlatterTest : public MockedEngineBackendTest {
  protected:
    static constexpr double kSecondsPerRevolution = 1.8; // 33 1/3 rpm

    // The jog input stage times the turns with the clock (rekordbox's 1 ms
    // jog ticks), so the tests run on a test clock, one buffer per buffer.
    PlatterTest() {
        mixxx::Time::setTestMode(true);
    }
    ~PlatterTest() override {
        mixxx::Time::setTestMode(false);
    }

    void ProcessBuffer() {
        const double rate = ControlObject::get(
                ConfigKey(QStringLiteral("[App]"), QStringLiteral("samplerate")));
        mixxx::Time::addTestTime(std::chrono::nanoseconds(
                std::llround(kProcessBufferSize / 2.0 / rate * 1e9)));
        MockedEngineBackendTest::ProcessBuffer();
    }

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
    turn(2.0); // two revolutions at once: far above 25x
    process(10);
    // Each buffer that would move faster than 25x is dropped. (The queue
    // spreads the jump over two buffers and a sliver; as in rekordbox, the
    // sliver is slow enough to pass.)
    EXPECT_LT(seconds() - start, 0.05 * 2.0 * kSecondsPerRevolution);
}

TEST_F(PlatterTest, MovementReachesTheTrackAboutTwoBuffersLate) {
    // rekordbox's jog input stage: the first entry of a movement lasts two
    // buffers, so the motion is spread over them.
    const double start = touchWhilePlaying();
    turn(0.02);
    ProcessBuffer();
    EXPECT_LT(seconds() - start, 0.6 * 0.02 * kSecondsPerRevolution);
    process(10);
    EXPECT_NEAR(start + 0.02 * kSecondsPerRevolution, seconds(), 2e-3);
}

TEST_F(PlatterTest, ReleaseResumesPlay) {
    const double held = touchWhilePlaying();
    touch(false);
    process(20);
    EXPECT_GT(seconds(), held + 0.1);
}

// Release, spin, disable and bend (SCRATCH-ENGINE.md section 2.1).
class PlatterSpinTest : public PlatterTest {
  protected:
    enum State { kIdle = 0, kScratch = 1, kSpin = 2, kDisable = 3, kBend = 4 };

    /// The engine's exact position in seconds of track (playposition only
    /// updates 15 times a second).
    double exact() const {
        return m_pChannel1->getEngineBuffer()->getPlayPos().value() /
                ControlObject::get(ConfigKey(m_sGroup1, "track_samplerate"));
    }

    int state() const {
        return static_cast<int>(ControlObject::get(ConfigKey(m_sGroup1, "platter_state")));
    }

    static double bufferSeconds() {
        return kProcessBufferSize / 2.0 /
                ControlObject::get(ConfigKey(QStringLiteral("[App]"),
                        QStringLiteral("samplerate")));
    }

    /// Revolutions per buffer for a platter at `speed` (1.0 = 33 1/3 rpm).
    static double turnAt(double speed) {
        return speed * bufferSeconds() / kSecondsPerRevolution;
    }

    void turnFor(double speed, int buffers) {
        for (int i = 0; i < buffers; ++i) {
            turn(turnAt(speed));
            ProcessBuffer();
        }
    }

    /// Touches a paused deck and scratches forward at `speed`.
    void scratchPaused(double speed) {
        touch(true);
        process(10); // > 60 ms dead zone
        turnFor(speed, 10);
    }
};

TEST_F(PlatterSpinTest, ReleaseWhileTurningSpins) {
    process(1);
    const double before = exact();
    scratchPaused(1.5); // 10 buffers of turning under the hand
    touch(false);
    turnFor(1.5, 30); // the platter runs on by itself
    EXPECT_EQ(kSpin, state());
    // The paused track follows the platter, under the hand and free-running:
    // every step counts once, none is replayed by the spin.
    EXPECT_NEAR(before + 40 * turnAt(1.5) * kSecondsPerRevolution, exact(), 2e-3);
}

TEST_F(PlatterSpinTest, SpinStopsWithThePlatter) {
    process(1);
    const double before = exact();
    scratchPaused(1.5);
    touch(false);
    turnFor(1.5, 10);
    process(6); // the queued motion plays out, then two empty buffers
    EXPECT_EQ(kIdle, state()); // paused at the touch: no disable phase
    const double stopped = exact();
    // Every step counts once; the first empty buffer repeats the last speed
    // (rekordbox), so it stops one buffer of motion past the platter.
    EXPECT_NEAR(before + 21 * turnAt(1.5) * kSecondsPerRevolution, stopped, 2e-3);
    process(20);
    EXPECT_NEAR(stopped, exact(), 1e-4);
}

TEST_F(PlatterSpinTest, SpinEndsWhenThePlatterReverses) {
    scratchPaused(1.5);
    touch(false);
    turnFor(1.5, 5);
    ASSERT_EQ(kSpin, state());
    turnFor(-1.5, 1);
    EXPECT_NE(kSpin, state());
}

TEST_F(PlatterSpinTest, BackspinRunsOnUntilTwoEmptyBuffers) {
    touchWhilePlaying();
    turnFor(-3.0, 10);
    touch(false);
    turnFor(-3.0, 10); // backspin: against play, so never handed back as slow
    ASSERT_EQ(kSpin, state());
    // The platter has stopped. Whatever motion is still queued plays out,
    // then the first empty buffer repeats the speed and the second ends the
    // spin (rekordbox's ~37 ms run-on). Here the turns come once per buffer
    // with empty milliseconds between them, which rekordbox's direction
    // quirk (0.5 ms entries) shortens for a backwards platter, so little
    // stays queued; the real deck sends every millisecond.
    int runOn = 0;
    while (state() == kSpin && runOn < 10) {
        ProcessBuffer();
        ++runOn;
    }
    EXPECT_GE(runOn, 2);
    EXPECT_LE(runOn, 5);
    EXPECT_EQ(kDisable, state()); // the deck played at the touch
}

TEST_F(PlatterSpinTest, ReleasingAStillPlatterDoesNotSpin) {
    scratchPaused(1.5);
    process(8); // the hand holds the platter still
    touch(false);
    ProcessBuffer();
    EXPECT_EQ(kIdle, state());
}

TEST_F(PlatterSpinTest, ForwardSpinHandsBackToPlayThenIgnoresTheCoast) {
    touchWhilePlaying();
    turnFor(3.0, 10);
    touch(false);
    turnFor(3.0, 5);
    ASSERT_EQ(kSpin, state());
    // Slower than play: three movements in a row end the spin (the first
    // slow buffer still averages with the fast one before it).
    turnFor(0.6, 4);
    EXPECT_EQ(kDisable, state());
    // Disabled: the coasting platter neither scratches nor bends.
    const double before = exact();
    turnFor(0.6, 30);
    EXPECT_EQ(kDisable, state());
    EXPECT_NEAR(30 * bufferSeconds(), exact() - before, 0.005);
    // The jog goes quiet: idle again.
    process(8);
    EXPECT_EQ(kIdle, state());
}

TEST_F(PlatterSpinTest, PushingAgainstTheSpinEndsTheDisablePhase) {
    touchWhilePlaying();
    turnFor(3.0, 10);
    touch(false);
    turnFor(3.0, 5);
    turnFor(0.6, 4);
    ASSERT_EQ(kDisable, state());
    turn(-0.2);
    ProcessBuffer();
    EXPECT_EQ(kDisable, state()); // 0.2 rev: not yet
    turn(-0.2);
    ProcessBuffer();
    EXPECT_EQ(kIdle, state()); // more than 1215/3240 rev
}

TEST_F(PlatterSpinTest, UntouchedPlatterBendsTheTempo) {
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    process(20);
    const double before = exact();
    turnFor(2.0, 40); // 2x platter speed: +6 %
    EXPECT_EQ(kBend, state());
    EXPECT_NEAR(40 * bufferSeconds() * 1.06, exact() - before, 0.006);
    process(6); // > 56 ms without movement
    EXPECT_EQ(kIdle, state());
}

TEST_F(PlatterSpinTest, BendIsCappedAt42Percent) {
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    process(20);
    const double before = exact();
    turnFor(20.0, 40); // 3 % per 1x would be +60 %
    EXPECT_NEAR(40 * bufferSeconds() * 1.42, exact() - before, 0.02);
}

TEST_F(PlatterSpinTest, SlowUntouchedPlatterDoesNotBend) {
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    process(20);
    const double before = exact();
    turnFor(0.4, 40); // under 0.5x
    EXPECT_EQ(kIdle, state());
    EXPECT_NEAR(40 * bufferSeconds(), exact() - before, 0.005);
}

} // namespace
