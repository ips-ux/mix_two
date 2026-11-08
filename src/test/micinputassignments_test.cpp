// AI-generated (Claude Code). Start of AI-generated text.
#include <gtest/gtest.h>

#include "soundio/micinputassignments.h"
#include "test/mixxxtest.h"

namespace {

using micinputs::Assignment;
using micinputs::Assignments;

Assignment make(const QString& name, int base = 0, int count = 2) {
    Assignment a;
    a.deviceName = name;
    a.channelBase = base;
    a.channelCount = count;
    return a;
}

Assignment makePending(const QString& name, int base = 0, int count = 2) {
    Assignment a = make(name, base, count);
    a.pending = true;
    return a;
}

class MicInputAssignmentsTest : public MixxxTest {
  protected:
    void setRaw(int n, const QString& device, const QString& base, const QString& count) {
        config()->set(ConfigKey("[Soundcard]", QString("mic_input_%1_device").arg(n)), device);
        config()->set(ConfigKey("[Soundcard]", QString("mic_input_%1_channel").arg(n)), base);
        config()->set(ConfigKey("[Soundcard]", QString("mic_input_%1_channels").arg(n)), count);
    }
};

TEST_F(MicInputAssignmentsTest, LiveInputReplacesSaved) {
    Assignments saved{{0, make("A")}};
    Assignments live{{0, make("B", 2, 1)}};
    const Assignments result = micinputs::merge(saved, live, {"A", "B"});
    ASSERT_TRUE(result.contains(0));
    EXPECT_EQ(make("B", 2, 1), result.value(0));
}

TEST_F(MicInputAssignmentsTest, ClearedWhenDevicePresent) {
    Assignments saved{{0, make("A")}};
    const Assignments result = micinputs::merge(saved, {}, {"A"});
    EXPECT_FALSE(result.contains(0));
}

TEST_F(MicInputAssignmentsTest, KeptWhenDeviceMissing) {
    Assignments saved{{0, make("A", 2, 1)}};
    const Assignments result = micinputs::merge(saved, {}, {});
    ASSERT_TRUE(result.contains(0));
    EXPECT_EQ("A", result.value(0).deviceName);
    EXPECT_EQ(2, result.value(0).channelBase);
    EXPECT_EQ(1, result.value(0).channelCount);
    EXPECT_TRUE(result.value(0).pending);
}

TEST_F(MicInputAssignmentsTest, PendingKeptWhenDeviceReturns) {
    Assignments saved{{0, makePending("A", 2, 1)}};
    const Assignments result = micinputs::merge(saved, {}, {"A"});
    ASSERT_TRUE(result.contains(0));
    EXPECT_EQ(makePending("A", 2, 1), result.value(0));
}

TEST_F(MicInputAssignmentsTest, LiveClearsPending) {
    Assignments saved{{0, makePending("A")}};
    Assignments live{{0, make("A")}};
    const Assignments result = micinputs::merge(saved, live, {"A"});
    ASSERT_TRUE(result.contains(0));
    EXPECT_FALSE(result.value(0).pending);
}

TEST_F(MicInputAssignmentsTest, OthersUntouched) {
    Assignments saved{{0, make("A")}, {2, make("C", 4, 2)}};
    Assignments live{{0, make("B")}};
    const Assignments result = micinputs::merge(saved, live, {"A", "B"});
    ASSERT_TRUE(result.contains(0));
    ASSERT_TRUE(result.contains(2));
    EXPECT_EQ("B", result.value(0).deviceName);
    EXPECT_EQ(makePending("C", 4, 2), result.value(2));
}

TEST_F(MicInputAssignmentsTest, ApiRoundTripWithoutRestoreKeepsMics) {
    // WASAPI > ASIO > WASAPI through the sound API box: the config that comes
    // back has no mic inputs, and every saved device is present.
    const Assignments saved{{0, make("A")}, {1, make("B", 0, 1)}};
    const QStringList available{"A", "B"};
    // A plain snapshot would take this for "cleared" and drop both.
    EXPECT_TRUE(micinputs::merge(saved, {}, available).isEmpty());

    const Assignments kept = micinputs::merge(
            micinputs::keepOnReturn(saved, {}), {}, available);
    EXPECT_EQ(makePending("A"), kept.value(0));
    EXPECT_EQ(makePending("B", 0, 1), kept.value(1));
    EXPECT_TRUE(micinputs::needsRestore(kept, available));

    // After the restore the inputs are live again and pending is cleared.
    const Assignments restored = micinputs::merge(kept, saved, available);
    EXPECT_EQ(saved, restored);
    EXPECT_FALSE(micinputs::needsRestore(restored, available));
}

TEST_F(MicInputAssignmentsTest, KeepOnReturnLeavesLiveMics) {
    const Assignments saved{{0, make("A")}, {2, make("C")}};
    const Assignments live{{0, make("A")}};
    const Assignments kept = micinputs::keepOnReturn(saved, live);
    EXPECT_FALSE(kept.value(0).pending);
    EXPECT_TRUE(kept.value(2).pending);
}

TEST_F(MicInputAssignmentsTest, NeedsRestoreOnlyForPresentDevice) {
    const Assignments pending{{0, makePending("A")}};
    EXPECT_FALSE(micinputs::needsRestore(pending, {}));
    EXPECT_TRUE(micinputs::needsRestore(pending, {"A"}));
    EXPECT_FALSE(micinputs::needsRestore({{0, make("A")}}, {"A"}));
}

TEST_F(MicInputAssignmentsTest, ExcludedDevices) {
    EXPECT_TRUE(micinputs::isExcludedDevice("Line (PIONEER DJ DDJ-800) [Loopback]"));
    EXPECT_TRUE(micinputs::isExcludedDevice("Line In (PIONEER DJ DDJ-800)"));
    EXPECT_TRUE(micinputs::isExcludedDevice("Main Output 1/2 (Audient EVO4) [Loopback]"));
    EXPECT_FALSE(micinputs::isExcludedDevice("Microphone (HD Pro Webcam C920)"));
    EXPECT_FALSE(micinputs::isExcludedDevice("Mic | Line 1/2 (Audient EVO4)"));
}

TEST_F(MicInputAssignmentsTest, MergeDropsExcludedDevices) {
    const QString loopback = "Line (PIONEER DJ DDJ-800) [Loopback]";
    const QString ddj = "Line In (PIONEER DJ DDJ-800)";
    // A live input on an excluded device is not kept.
    const Assignments fromLive = micinputs::merge({}, {{0, make(loopback)}}, {loopback});
    EXPECT_TRUE(fromLive.isEmpty());
    // A saved one is dropped, also when it is pending or its device is gone.
    const Assignments saved{{0, make(loopback)}, {1, makePending(ddj)}, {2, make("Gone")}};
    const Assignments fromSaved = micinputs::merge(saved, {}, {loopback, ddj});
    EXPECT_EQ(1, fromSaved.size());
    EXPECT_TRUE(fromSaved.contains(2));
    EXPECT_FALSE(micinputs::needsRestore(saved, {loopback, ddj}));
}

TEST_F(MicInputAssignmentsTest, ChannelCount) {
    EXPECT_EQ(2, micinputs::channelCountFor(0));
    EXPECT_EQ(1, micinputs::channelCountFor(1));
    EXPECT_EQ(2, micinputs::channelCountFor(2));
    EXPECT_EQ(2, micinputs::channelCountFor(8));
}

TEST_F(MicInputAssignmentsTest, RoundTrip) {
    Assignments x{{0, make("Mic (USB Audio [2])", 0, 1)},
            {2, makePending("Gone", 0, 2)},
            {3, make(QStringLiteral("Mikrofon (Zoé)"), 2, 2)}};
    micinputs::save(config(), x);
    EXPECT_EQ(x, micinputs::load(config()));
    saveAndReloadConfig();
    EXPECT_EQ(x, micinputs::load(config()));
}

TEST_F(MicInputAssignmentsTest, DamagedKeysIgnored) {
    setRaw(1, "", "0", "2");       // empty device
    setRaw(2, "Dev", "-1", "2");   // negative channel
    setRaw(3, "Dev", "0", "0");    // zero channels
    setRaw(4, "Dev", "0", "abc");  // text channels
    EXPECT_TRUE(micinputs::load(config()).isEmpty());

    // A good mic beside a damaged one still loads.
    setRaw(1, "Good", "2", "1");
    const Assignments loaded = micinputs::load(config());
    EXPECT_EQ(1, loaded.size());
    EXPECT_EQ(make("Good", 2, 1), loaded.value(0));
}

TEST_F(MicInputAssignmentsTest, SaveRemovesCleared) {
    micinputs::save(config(), {{0, make("A")}});
    EXPECT_EQ(1, micinputs::load(config()).size());
    micinputs::save(config(), {});
    EXPECT_TRUE(micinputs::load(config()).isEmpty());
}

} // namespace
// End of AI-generated text.
