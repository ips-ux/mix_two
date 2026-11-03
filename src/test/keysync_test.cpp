#include <gtest/gtest.h>

#include <cmath>

#include "control/controlobject.h"
#include "engine/sync/enginesync.h"
#include "test/mockedenginebackendtest.h"
#include "track/beats.h"
#include "track/keyutils.h"

namespace {

using mixxx::track::io::key::ChromaticKey;

/// rekordbox-style KEY SYNC on/off mode ([ChannelN],keysync_enabled).
class KeySyncTest : public MockedEngineBackendTest {
  protected:
    void SetUp() override {
        MockedEngineBackendTest::SetUp();
        for (const auto& pTrack : {m_pTrack1, m_pTrack2}) {
            pTrack->trySetBeats(mixxx::Beats::fromConstTempo(
                    pTrack->getSampleRate(), mixxx::audio::kStartFramePos, mixxx::Bpm(128)));
        }
        setFileKey(m_sGroup1, ChromaticKey::C_MAJOR);
        setFileKey(m_sGroup2, ChromaticKey::D_MAJOR);
        // Deck 2 is the tempo master, deck 1 follows.
        ControlObject::set(ConfigKey(m_sGroup1, "sync_enabled"), 1.0);
        ControlObject::set(ConfigKey(m_sGroup2, "sync_leader"), 1.0);
        ProcessBuffer();
    }

    static void setFileKey(const QString& group, ChromaticKey key) {
        ControlObject::set(ConfigKey(group, "file_key"), static_cast<double>(key));
    }

    static ChromaticKey key(const QString& group) {
        return KeyUtils::keyFromNumericValue(ControlObject::get(ConfigKey(group, "key")));
    }

    static bool keySyncEnabled(const QString& group) {
        return ControlObject::get(ConfigKey(group, "keysync_enabled")) > 0;
    }

    static void setKeySync(const QString& group, bool enabled) {
        ControlObject::set(ConfigKey(group, "keysync_enabled"), enabled ? 1.0 : 0.0);
    }

    static bool keysCompatible(ChromaticKey a, ChromaticKey b) {
        return KeyUtils::getCompatibleKeys(b).contains(a);
    }
};

TEST_F(KeySyncTest, MatchesTheMastersKey) {
    ASSERT_FALSE(keysCompatible(key(m_sGroup1), ChromaticKey::F_SHARP_MAJOR));
    setKeySync(m_sGroup1, true);
    EXPECT_TRUE(keySyncEnabled(m_sGroup1));
    EXPECT_TRUE(keysCompatible(key(m_sGroup1), key(m_sGroup2)));
}

TEST_F(KeySyncTest, FollowsKeyChangesOfTheMaster) {
    setKeySync(m_sGroup1, true);
    // Shift the master by three semitones.
    ControlObject::set(ConfigKey(m_sGroup2, "pitch"), 3.0);
    ProcessBuffer();
    EXPECT_TRUE(keySyncEnabled(m_sGroup1));
    EXPECT_TRUE(keysCompatible(key(m_sGroup1), key(m_sGroup2)));
}

TEST_F(KeySyncTest, ManualKeyChangeEndsKeySync) {
    setKeySync(m_sGroup1, true);
    ControlObject::set(ConfigKey(m_sGroup1, "pitch"), 1.0);
    EXPECT_FALSE(keySyncEnabled(m_sGroup1));

    setKeySync(m_sGroup1, true);
    ControlObject::set(ConfigKey(m_sGroup1, "reset_key"), 1.0);
    EXPECT_FALSE(keySyncEnabled(m_sGroup1));
}

TEST_F(KeySyncTest, StaysOffWithoutAnAnalyzedKey) {
    setFileKey(m_sGroup2, ChromaticKey::INVALID);
    setKeySync(m_sGroup1, true);
    EXPECT_FALSE(keySyncEnabled(m_sGroup1));
}

TEST_F(KeySyncTest, TurningItOffKeepsTheKey) {
    setKeySync(m_sGroup1, true);
    const ChromaticKey synced = key(m_sGroup1);
    setKeySync(m_sGroup1, false);
    EXPECT_FALSE(keySyncEnabled(m_sGroup1));
    EXPECT_EQ(synced, key(m_sGroup1));
}

} // namespace

namespace {

/// rekordbox MASTER behaviour ([ChannelN],sync_leader).
class MasterSyncTest : public MockedEngineBackendTest {
  protected:
    void SetUp() override {
        MockedEngineBackendTest::SetUp();
        for (const auto& pTrack : {m_pTrack1, m_pTrack2}) {
            pTrack->trySetBeats(mixxx::Beats::fromConstTempo(
                    pTrack->getSampleRate(), mixxx::audio::kStartFramePos, mixxx::Bpm(128)));
        }
        ControlObject::set(ConfigKey(m_sGroup1, "sync_enabled"), 1.0);
        ControlObject::set(ConfigKey(m_sGroup2, "sync_enabled"), 1.0);
        ProcessBuffer();
    }

    QString leaderGroup() const {
        EngineChannel* pLeader = m_pEngineSync->getLeaderChannel();
        return pLeader ? pLeader->getGroup() : QString();
    }

    void play(const QString& group, bool playing) {
        ControlObject::set(ConfigKey(group, "play"), playing ? 1.0 : 0.0);
        ProcessBuffer();
    }
};

TEST_F(MasterSyncTest, MasterOnPausedDeck) {
    ControlObject::set(ConfigKey(m_sGroup2, "sync_leader"), 1.0);
    ProcessBuffer();
    EXPECT_EQ(m_sGroup2, leaderGroup());
    EXPECT_EQ(2.0, ControlObject::get(ConfigKey(m_sGroup2, "sync_leader")));
}

TEST_F(MasterSyncTest, AutomaticMasterStaysWhenTheOldMasterResumes) {
    // Deck 2 is silent (fader down), which must not matter.
    ControlObject::set(ConfigKey(m_sGroup2, "volume"), 0.0);
    play(m_sGroup1, true);
    play(m_sGroup2, true);
    ASSERT_EQ(m_sGroup1, leaderGroup());

    // The master stops: the other playing deck takes over.
    play(m_sGroup1, false);
    EXPECT_EQ(m_sGroup2, leaderGroup());

    // It keeps MASTER when deck 1 plays again.
    play(m_sGroup1, true);
    EXPECT_EQ(m_sGroup2, leaderGroup());
}

} // namespace

namespace {

/// rekordbox tempo behaviour under SYNC: only the MASTER's slider sets the
/// tempo, the range widens to fit, and the slider BPM shows mismatches.
class SyncTempoTest : public MockedEngineBackendTest {
  protected:
    void SetUp() override {
        MockedEngineBackendTest::SetUp();
        setTrackBpm(m_pTrack1, 128);
        setTrackBpm(m_pTrack2, 128);
        for (const auto& group : {m_sGroup1, m_sGroup2}) {
            ControlObject::set(ConfigKey(group, "rateRange"), 0.06);
            ControlObject::set(ConfigKey(group, "rate_dir"), 1.0);
        }
        ProcessBuffer();
    }

    static void setTrackBpm(const TrackPointer& pTrack, double bpm) {
        pTrack->trySetBeats(mixxx::Beats::fromConstTempo(
                pTrack->getSampleRate(), mixxx::audio::kStartFramePos, mixxx::Bpm(bpm)));
    }

    // Deck 1 is MASTER and plays, deck 2 follows.
    void syncDeck2ToDeck1() {
        ControlObject::set(ConfigKey(m_sGroup1, "sync_leader"), 1.0);
        ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
        ProcessBuffer();
        ControlObject::set(ConfigKey(m_sGroup2, "sync_enabled"), 1.0);
        ProcessBuffer();
    }

    static double get(const QString& group, const char* item) {
        return ControlObject::get(ConfigKey(group, item));
    }
};

TEST_F(SyncTempoTest, FollowerSliderDoesNothing) {
    syncDeck2ToDeck1();
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), 0.5);
    ProcessBuffer();
    EXPECT_DOUBLE_EQ(128.0, get(m_sGroup1, "bpm"));
    EXPECT_DOUBLE_EQ(128.0, get(m_sGroup2, "bpm"));
    EXPECT_DOUBLE_EQ(0.0, get(m_sGroup2, "rate"));
}

TEST_F(SyncTempoTest, MasterSliderMovesTheFollower) {
    syncDeck2ToDeck1();
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), 0.5);
    ProcessBuffer();
    EXPECT_NEAR(128.0 * 1.03, get(m_sGroup1, "bpm"), 1e-6);
    EXPECT_NEAR(128.0 * 1.03, get(m_sGroup2, "bpm"), 1e-6);
}

TEST_F(SyncTempoTest, SyncWidensTheRangeJustEnough) {
    setTrackBpm(m_pTrack2, 118); // needs +8.5%: ±10
    syncDeck2ToDeck1();
    EXPECT_NEAR(128.0, get(m_sGroup2, "bpm"), 1e-6);
    EXPECT_DOUBLE_EQ(0.10, get(m_sGroup2, "rateRange"));
    EXPECT_DOUBLE_EQ(0.06, get(m_sGroup1, "rateRange"));
}

TEST_F(SyncTempoTest, SyncWidensTheRangeToWide) {
    setTrackBpm(m_pTrack2, 100); // needs +28%: WIDE
    syncDeck2ToDeck1();
    EXPECT_NEAR(128.0, get(m_sGroup2, "bpm"), 1e-6);
    EXPECT_DOUBLE_EQ(1.0, get(m_sGroup2, "rateRange"));
}

TEST_F(SyncTempoTest, UnsyncedRangeIsLeftAlone) {
    ControlObject::set(ConfigKey(m_sGroup2, "bpm"), 128.0 * 1.2);
    ProcessBuffer();
    EXPECT_DOUBLE_EQ(0.06, get(m_sGroup2, "rateRange"));
}

TEST_F(SyncTempoTest, SyncUsesTheTracksOwnBpmNotTheSliders) {
    // Deck 2's 128 BPM track is slowed to below half the master's tempo.
    ControlObject::set(ConfigKey(m_sGroup2, "rateRange"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), -0.55);
    ProcessBuffer();
    ASSERT_NEAR(57.6, get(m_sGroup2, "bpm"), 1e-6);
    syncDeck2ToDeck1();
    EXPECT_NEAR(128.0, get(m_sGroup2, "bpm"), 1e-6);
}

TEST_F(SyncTempoTest, HalfTempoTrackSyncsAtDoubleTempo) {
    // A 64 BPM track against a 128 BPM master still plays at its own speed.
    setTrackBpm(m_pTrack2, 64);
    syncDeck2ToDeck1();
    EXPECT_NEAR(64.0, get(m_sGroup2, "bpm"), 1e-6);
}

TEST_F(SyncTempoTest, RangeButtonStepsTheRekordboxRanges) {
    ControlObject::set(ConfigKey(m_sGroup2, "bpm"), 128.0 * 1.03);
    for (double expected : {0.10, 0.16, 1.0, 0.06}) {
        ControlObject::set(ConfigKey(m_sGroup2, "rate_range_next"), 1.0);
        ControlObject::set(ConfigKey(m_sGroup2, "rate_range_next"), 0.0);
        EXPECT_DOUBLE_EQ(expected, get(m_sGroup2, "rateRange"));
        // The tempo stays, the slider moves.
        EXPECT_NEAR(128.0 * 1.03, get(m_sGroup2, "bpm"), 1e-6);
    }
}

TEST_F(SyncTempoTest, SliderBpmShowsWhereTheSliderIs) {
    // Without a controller the on-screen slider is the slider.
    EXPECT_DOUBLE_EQ(128.0, get(m_sGroup2, "tempo_slider_bpm"));
    EXPECT_EQ(1.0, get(m_sGroup2, "tempo_slider_matched"));

    syncDeck2ToDeck1();
    ControlObject::set(ConfigKey(m_sGroup2, "rate_physical"), -0.5);
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), -0.5);
    ProcessBuffer();
    EXPECT_NEAR(128.0 * 0.97, get(m_sGroup2, "tempo_slider_bpm"), 1e-6);
    EXPECT_EQ(0.0, get(m_sGroup2, "tempo_slider_matched"));

    // The slider reaches the synced tempo.
    ControlObject::set(ConfigKey(m_sGroup2, "rate_physical"), 0.0);
    EXPECT_EQ(1.0, get(m_sGroup2, "tempo_slider_matched"));
}

} // namespace

namespace {

/// Pioneer Sync mode (Preferences > Decks > Sync options): Grid Lock vs BPM
/// Sync, Grid Lock's beat grid, and the round SYNC button's light.
class SyncModeTest : public MockedEngineBackendTest {
  protected:
    void SetUp() override {
        MockedEngineBackendTest::SetUp();
        for (const auto& pTrack : {m_pTrack1, m_pTrack2}) {
            pTrack->trySetBeats(mixxx::Beats::fromConstTempo(
                    pTrack->getSampleRate(), mixxx::audio::kStartFramePos, mixxx::Bpm(128)));
        }
        ProcessBuffer();
    }

    static void setSyncType(EngineSync::SyncType type, double grid = 1.0) {
        ControlObject::set(kSyncTypeConfigKey, static_cast<double>(type));
        ControlObject::set(kSyncGridConfigKey, grid);
    }

    static double get(const QString& group, const char* item) {
        return ControlObject::get(ConfigKey(group, item));
    }

    /// Deck 1 is MASTER and plays a little; then deck 2 starts synced, a
    /// fraction of a beat behind. Returns deck 2's beat offset from deck 1.
    double startLateAndMeasure() {
        ControlObject::set(ConfigKey(m_sGroup1, "sync_leader"), 1.0);
        ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
        for (int i = 0; i < 13; ++i) {
            ProcessBuffer();
        }
        ControlObject::set(ConfigKey(m_sGroup2, "sync_enabled"), 1.0);
        ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);
        for (int i = 0; i < 40; ++i) {
            ProcessBuffer();
        }
        double offset = get(m_sGroup2, "beat_distance") - get(m_sGroup1, "beat_distance");
        return offset - std::round(offset);
    }
};

TEST_F(SyncModeTest, GridLockPutsTheBeatsTogether) {
    setSyncType(EngineSync::SyncType::GridLock);
    EXPECT_NEAR(0.0, startLateAndMeasure(), 0.02);
}

TEST_F(SyncModeTest, BpmSyncLeavesThePhaseToTheDj) {
    setSyncType(EngineSync::SyncType::BpmSync);
    const double offset = startLateAndMeasure();
    EXPECT_GT(std::fabs(offset), 0.05);
    EXPECT_NEAR(get(m_sGroup1, "bpm"), get(m_sGroup2, "bpm"), 1e-6);
}

TEST_F(SyncModeTest, GridLockSnapsToItsBeatGrid) {
    // BPM Sync shows where deck 2 starts relative to deck 1 ...
    setSyncType(EngineSync::SyncType::BpmSync);
    const double free = startLateAndMeasure();
    ASSERT_GT(std::fabs(free - 0.25 * std::round(free / 0.25)), 0.03);
}

TEST_F(SyncModeTest, GridLockQuarterBeatGrid) {
    // ... and a 1/4 grid snaps that start to the nearest quarter beat, which
    // need not be a whole beat.
    setSyncType(EngineSync::SyncType::GridLock, 0.25);
    const double offset = startLateAndMeasure();
    EXPECT_NEAR(0.0, offset - 0.25 * std::round(offset / 0.25), 0.02);
    EXPECT_GT(std::fabs(offset), 0.2);
}

TEST_F(SyncModeTest, IndicatorShowsSyncedAndMaster) {
    setSyncType(EngineSync::SyncType::GridLock);
    EXPECT_EQ(0.0, get(m_sGroup1, "sync_indicator"));
    ControlObject::set(ConfigKey(m_sGroup1, "sync_enabled"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "sync_enabled"), 1.0);
    ProcessBuffer();
    // Nothing plays: both blue.
    EXPECT_EQ(1.0, get(m_sGroup1, "sync_indicator"));
    EXPECT_EQ(1.0, get(m_sGroup2, "sync_indicator"));
    // The master plays: red-orange.
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ProcessBuffer();
    const bool deck1Master = get(m_sGroup1, "sync_indicator") == 2.0;
    const bool deck2Master = get(m_sGroup2, "sync_indicator") == 2.0;
    EXPECT_TRUE(deck1Master != deck2Master);
    EXPECT_TRUE(deck1Master);
    // BPM Sync shifts the value by 3 (the "BPM SYNC" label).
    setSyncType(EngineSync::SyncType::BpmSync);
    EXPECT_EQ(5.0, get(m_sGroup1, "sync_indicator"));
    EXPECT_EQ(4.0, get(m_sGroup2, "sync_indicator"));
}

TEST_F(SyncModeTest, ChosenMasterIsRedOrangeWhilePaused) {
    ControlObject::set(ConfigKey(m_sGroup1, "sync_enabled"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "sync_leader"), 1.0);
    ProcessBuffer();
    EXPECT_EQ(2.0, get(m_sGroup2, "sync_indicator"));
    EXPECT_EQ(1.0, get(m_sGroup1, "sync_indicator"));
}

} // namespace

namespace {

/// KEY SYNC with decks that are only loaded: no SYNC, nothing playing.
class KeySyncLoadedDecksTest : public MockedEngineBackendTest {};

TEST_F(KeySyncLoadedDecksTest, MatchesTheOtherLoadedDeck) {
    ControlObject::set(ConfigKey(m_sGroup1, "file_key"),
            static_cast<double>(mixxx::track::io::key::C_MAJOR));
    ControlObject::set(ConfigKey(m_sGroup2, "file_key"),
            static_cast<double>(mixxx::track::io::key::D_MAJOR));
    ProcessBuffer();
    ControlObject::set(ConfigKey(m_sGroup1, "keysync_enabled"), 1.0);
    EXPECT_EQ(1.0, ControlObject::get(ConfigKey(m_sGroup1, "keysync_enabled")));
    const auto key1 = KeyUtils::keyFromNumericValue(
            ControlObject::get(ConfigKey(m_sGroup1, "key")));
    EXPECT_TRUE(KeyUtils::getCompatibleKeys(mixxx::track::io::key::D_MAJOR).contains(key1));

    // A second press turns it off again.
    ControlObject::set(ConfigKey(m_sGroup1, "keysync_enabled"), 0.0);
    EXPECT_EQ(0.0, ControlObject::get(ConfigKey(m_sGroup1, "keysync_enabled")));
}

TEST_F(KeySyncLoadedDecksTest, StaysOffWithoutAnotherKey) {
    ControlObject::set(ConfigKey(m_sGroup1, "file_key"),
            static_cast<double>(mixxx::track::io::key::C_MAJOR));
    ProcessBuffer();
    ControlObject::set(ConfigKey(m_sGroup1, "keysync_enabled"), 1.0);
    EXPECT_EQ(0.0, ControlObject::get(ConfigKey(m_sGroup1, "keysync_enabled")));
}

} // namespace
