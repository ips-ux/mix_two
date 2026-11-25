// AI-generated (Claude Code). Start of AI-generated text.
#include <gtest/gtest.h>

#include <QDomDocument>

#include "soundio/soundmanagerconfig.h"
#include "soundio/soundmanagerutil.h"
#include "test/mixxxtest.h"

namespace {

SoundDeviceId makeDevice(const QString& name, int index) {
    SoundDeviceId id;
    id.name = name;
    id.deviceIndex = index;
    return id;
}

AudioOutput out(AudioPathType type, unsigned char base, unsigned char index = 0) {
    return AudioOutput(type, base, mixxx::audio::ChannelCount::stereo(), index);
}

class MixerModeTest : public MixxxTest {
  protected:
    MixerModeTest()
            : m_config(nullptr),
              m_device(makeDevice("DDJ-800", 3)) {
        m_config.setDeckCount(2);
    }

    // Main on channels 1-2 and headphones on 3-4 of the one device.
    void addMainAndHeadphones() {
        m_config.addOutput(m_device, out(AudioPathType::Main, 0));
        m_config.addOutput(m_device, out(AudioPathType::Headphones, 2));
    }

    static bool has(const QMultiHash<SoundDeviceId, AudioOutput>& outputs,
            AudioPathType type,
            unsigned char index,
            unsigned char base) {
        for (const AudioOutput& o : outputs) {
            if (o.getType() == type && o.getIndex() == index &&
                    o.getChannelGroup().getChannelBase() == base) {
                return true;
            }
        }
        return false;
    }

    SoundManagerConfig m_config;
    SoundDeviceId m_device;
};

TEST_F(MixerModeTest, DefaultModeIsInternal) {
    EXPECT_EQ(SoundManagerConfig::MixerMode::Internal, m_config.getMixerMode());
}

TEST_F(MixerModeTest, MainMixFollowsTheMode) {
    EXPECT_TRUE(SoundManagerConfig::mainMixEnabledFor(SoundManagerConfig::MixerMode::Internal));
    EXPECT_FALSE(SoundManagerConfig::mainMixEnabledFor(SoundManagerConfig::MixerMode::External));
}

TEST_F(MixerModeTest, ExternalGivesEveryDeckItsOwnStereoPair) {
    m_config.addOutput(m_device, out(AudioPathType::Main, 0));
    const auto result = m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok, result.status);
    EXPECT_EQ(SoundManagerConfig::MixerMode::External, m_config.getMixerMode());
    const auto outputs = m_config.getOutputs();
    EXPECT_EQ(2, outputs.size());
    EXPECT_TRUE(has(outputs, AudioPathType::Deck, 0, 0));
    EXPECT_TRUE(has(outputs, AudioPathType::Deck, 1, 2));
    EXPECT_FALSE(has(outputs, AudioPathType::Main, 0, 0));
}

TEST_F(MixerModeTest, ExternalUsesTheDeviceOfTheMainOutput) {
    const SoundDeviceId other = makeDevice("Other", 1);
    m_config.addOutput(m_device, out(AudioPathType::Main, 0));
    m_config.addOutput(other, out(AudioPathType::Booth, 0));
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);
    const auto outputs = m_config.getOutputs();
    int onMainDevice = 0;
    for (auto it = outputs.cbegin(); it != outputs.cend(); ++it) {
        if (it.value().getType() == AudioPathType::Deck) {
            EXPECT_EQ(m_device, it.key());
            ++onMainDevice;
        }
    }
    EXPECT_EQ(2, onMainDevice);
}

TEST_F(MixerModeTest, TooFewChannelsRefusesAndKeepsTheSetup) {
    addMainAndHeadphones();
    m_config.setDeckCount(4);
    const auto result = m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4);
    EXPECT_EQ(SoundManagerConfig::MixerModeStatus::TooFewChannels, result.status);
    EXPECT_EQ(SoundManagerConfig::MixerMode::Internal, m_config.getMixerMode());
    EXPECT_EQ(2, m_config.getOutputs().size());
    EXPECT_TRUE(has(m_config.getOutputs(), AudioPathType::Main, 0, 0));
    EXPECT_TRUE(has(m_config.getOutputs(), AudioPathType::Headphones, 0, 2));
}

TEST_F(MixerModeTest, StereoDeviceIsTooSmallForTwoDecks) {
    m_config.addOutput(m_device, out(AudioPathType::Main, 0));
    EXPECT_EQ(SoundManagerConfig::MixerModeStatus::TooFewChannels,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 2).status);
    EXPECT_TRUE(has(m_config.getOutputs(), AudioPathType::Main, 0, 0));
}

TEST_F(MixerModeTest, NoOutputDeviceRefuses) {
    EXPECT_EQ(SoundManagerConfig::MixerModeStatus::NoDevice,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 8).status);
    EXPECT_EQ(SoundManagerConfig::MixerMode::Internal, m_config.getMixerMode());
}

TEST_F(MixerModeTest, ExternalDropsAnOutputOnTheSameChannelsAndCountsIt) {
    addMainAndHeadphones();
    const auto result = m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok, result.status);
    EXPECT_EQ(1, result.droppedOutputs);
    EXPECT_FALSE(has(m_config.getOutputs(), AudioPathType::Headphones, 0, 2));
}

TEST_F(MixerModeTest, ExternalKeepsAnOutputOnFreeChannels) {
    m_config.addOutput(m_device, out(AudioPathType::Main, 0));
    m_config.addOutput(m_device, out(AudioPathType::Headphones, 4));
    const auto result = m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 8);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok, result.status);
    EXPECT_EQ(0, result.droppedOutputs);
    EXPECT_TRUE(has(m_config.getOutputs(), AudioPathType::Headphones, 0, 4));
}

TEST_F(MixerModeTest, ExternalKeepsAnOutputOnAnotherDevice) {
    const SoundDeviceId other = makeDevice("Other", 1);
    m_config.addOutput(m_device, out(AudioPathType::Main, 0));
    m_config.addOutput(other, out(AudioPathType::Headphones, 0));
    const auto result = m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok, result.status);
    EXPECT_EQ(0, result.droppedOutputs);
    EXPECT_TRUE(has(m_config.getOutputs(), AudioPathType::Headphones, 0, 0));
}

TEST_F(MixerModeTest, InternalRestoresTheOutputsFromBeforeExternal) {
    addMainAndHeadphones();
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);
    const auto result = m_config.setMixerMode(SoundManagerConfig::MixerMode::Internal, 4);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok, result.status);
    EXPECT_EQ(SoundManagerConfig::MixerMode::Internal, m_config.getMixerMode());
    const auto outputs = m_config.getOutputs();
    EXPECT_EQ(2, outputs.size());
    EXPECT_TRUE(has(outputs, AudioPathType::Main, 0, 0));
    EXPECT_TRUE(has(outputs, AudioPathType::Headphones, 0, 2));
    EXPECT_FALSE(has(outputs, AudioPathType::Deck, 0, 0));
}

TEST_F(MixerModeTest, InternalWhenAlreadyInternalChangesNothing) {
    addMainAndHeadphones();
    const auto result = m_config.setMixerMode(SoundManagerConfig::MixerMode::Internal, 4);
    EXPECT_EQ(SoundManagerConfig::MixerModeStatus::Ok, result.status);
    EXPECT_EQ(2, m_config.getOutputs().size());
}

TEST_F(MixerModeTest, ExternalTwiceKeepsTheFirstSavedSetup) {
    addMainAndHeadphones();
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::Internal, 4).status);
    EXPECT_TRUE(has(m_config.getOutputs(), AudioPathType::Main, 0, 0));
}

TEST_F(MixerModeTest, InternalWithNothingSavedPutsMainBackOnTheFirstChannels) {
    // A setup that was saved as External without a saved Internal setup.
    m_config.addOutput(m_device, out(AudioPathType::Deck, 0, 0));
    m_config.addOutput(m_device, out(AudioPathType::Deck, 2, 1));
    QDomDocument doc;
    QDomElement root = doc.createElement("SoundManagerConfig");
    root.setAttribute("mixer_mode", "external");
    doc.appendChild(root);
    m_config.readMixerModeXml(root);
    ASSERT_EQ(SoundManagerConfig::MixerMode::External, m_config.getMixerMode());

    const auto result = m_config.setMixerMode(SoundManagerConfig::MixerMode::Internal, 4);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok, result.status);
    EXPECT_TRUE(has(m_config.getOutputs(), AudioPathType::Main, 0, 0));
    EXPECT_FALSE(has(m_config.getOutputs(), AudioPathType::Deck, 0, 0));
}

TEST_F(MixerModeTest, ModeAndSavedSetupSurviveTheXmlRoundTrip) {
    addMainAndHeadphones();
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);

    QDomDocument doc;
    QDomElement root = doc.createElement("SoundManagerConfig");
    doc.appendChild(root);
    m_config.writeMixerModeXml(&doc, &root);

    SoundManagerConfig other(nullptr);
    other.readMixerModeXml(root);
    EXPECT_EQ(SoundManagerConfig::MixerMode::External, other.getMixerMode());
    other.addOutput(m_device, out(AudioPathType::Deck, 0, 0));
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            other.setMixerMode(SoundManagerConfig::MixerMode::Internal, 4).status);
    EXPECT_TRUE(has(other.getOutputs(), AudioPathType::Main, 0, 0));
    EXPECT_TRUE(has(other.getOutputs(), AudioPathType::Headphones, 0, 2));
}

TEST_F(MixerModeTest, SavedSetupIsNotReadAsActiveSoundDevice) {
    addMainAndHeadphones();
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);
    QDomDocument doc;
    QDomElement root = doc.createElement("SoundManagerConfig");
    doc.appendChild(root);
    m_config.writeMixerModeXml(&doc, &root);
    EXPECT_EQ(0, root.elementsByTagName("SoundDevice").count());
}

TEST_F(MixerModeTest, ResetGoesBackToInternalAndForgetsTheSavedSetup) {
    addMainAndHeadphones();
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);
    m_config.resetMixerMode();
    EXPECT_EQ(SoundManagerConfig::MixerMode::Internal, m_config.getMixerMode());
    QDomDocument doc;
    QDomElement root = doc.createElement("SoundManagerConfig");
    doc.appendChild(root);
    m_config.writeMixerModeXml(&doc, &root);
    EXPECT_EQ(0, root.elementsByTagName("output").count());
}

} // namespace
// End of AI-generated text.
