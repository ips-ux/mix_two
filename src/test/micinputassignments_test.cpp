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
