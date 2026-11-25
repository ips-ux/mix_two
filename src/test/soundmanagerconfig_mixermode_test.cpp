// AI-generated (Claude Code). Start of AI-generated text.
#include <gtest/gtest.h>

#include <QDomDocument>
#include <QSignalSpy>
#include <gsl/pointers>

#include "soundio/soundmanagerconfig.h"
#include "effects/effectsmanager.h"
#include "engine/enginemixer.h"
#include "soundio/audiotargetmanager.h"
#include "soundio/soundmanager.h"
#include "util/cmdlineargs.h"
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
    EXPECT_EQ(1, result.droppedNames.size());
    EXPECT_FALSE(has(m_config.getOutputs(), AudioPathType::Headphones, 0, 2));
}

TEST_F(MixerModeTest, ExternalKeepsABusOutputOnFreeChannels) {
    m_config.addOutput(m_device, out(AudioPathType::Main, 0));
    m_config.addOutput(m_device, out(AudioPathType::Bus, 4));
    const auto result = m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 8);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok, result.status);
    EXPECT_TRUE(result.droppedNames.isEmpty());
    EXPECT_TRUE(has(m_config.getOutputs(), AudioPathType::Bus, 0, 4));
}

TEST_F(MixerModeTest, ExternalDropsHeadphonesAndBoothOnAnotherDeviceToo) {
    // The engine turns the main mix on for these two, so External cannot keep them.
    const SoundDeviceId other = makeDevice("Other", 1);
    m_config.addOutput(m_device, out(AudioPathType::Main, 0));
    m_config.addOutput(other, out(AudioPathType::Headphones, 0));
    m_config.addOutput(other, out(AudioPathType::Booth, 2));
    const auto result = m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok, result.status);
    EXPECT_EQ(2, result.droppedNames.size());
    EXPECT_FALSE(has(m_config.getOutputs(), AudioPathType::Headphones, 0, 0));
    EXPECT_FALSE(has(m_config.getOutputs(), AudioPathType::Booth, 0, 2));
    EXPECT_EQ(SoundManagerConfig::MixerSetup::External, m_config.mixerSetupState().setup);
    // Internal brings both back.
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::Internal, 4).status);
    EXPECT_TRUE(has(m_config.getOutputs(), AudioPathType::Headphones, 0, 0));
    EXPECT_TRUE(has(m_config.getOutputs(), AudioPathType::Booth, 0, 2));
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

// Stale or missing devices after a restart.

TEST_F(MixerModeTest, SavedOutputsFindTheirDeviceAgainWhenTheIndexChanged) {
    addMainAndHeadphones();
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);
    QDomDocument doc;
    QDomElement root = doc.createElement("SoundManagerConfig");
    doc.appendChild(root);
    m_config.writeMixerModeXml(&doc, &root);

    // After a restart the same device has index 7. The live decks use it.
    const SoundDeviceId now = makeDevice("DDJ-800", 7);
    SoundManagerConfig other(nullptr);
    other.setDeckCount(2);
    other.readMixerModeXml(root);
    other.addOutput(now, out(AudioPathType::Deck, 0, 0));
    other.addOutput(now, out(AudioPathType::Deck, 2, 1));
    EXPECT_EQ(0, other.repairSavedOutputs({now}));
    const auto result = other.setMixerMode(SoundManagerConfig::MixerMode::Internal, 4);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok, result.status);
    EXPECT_FALSE(result.mainFallback);
    const auto outputs = other.getOutputs();
    for (auto it = outputs.cbegin(); it != outputs.cend(); ++it) {
        EXPECT_EQ(now, it.key()) << "an output keeps a stale device id";
    }
    EXPECT_TRUE(has(outputs, AudioPathType::Main, 0, 0));
    EXPECT_TRUE(has(outputs, AudioPathType::Headphones, 0, 2));
}

TEST_F(MixerModeTest, SavedOutputsOfAGoneDeviceGiveMainOnTheDeviceInUse) {
    addMainAndHeadphones();
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);
    QDomDocument doc;
    QDomElement root = doc.createElement("SoundManagerConfig");
    doc.appendChild(root);
    m_config.writeMixerModeXml(&doc, &root);

    // The DDJ-800 is gone. The decks are now on "Speakers".
    const SoundDeviceId speakers = makeDevice("Speakers", 2);
    SoundManagerConfig other(nullptr);
    other.setDeckCount(2);
    other.readMixerModeXml(root);
    other.addOutput(speakers, out(AudioPathType::Deck, 0, 0));
    other.addOutput(speakers, out(AudioPathType::Deck, 2, 1));
    EXPECT_EQ(2, other.repairSavedOutputs({speakers}));
    const auto result = other.setMixerMode(SoundManagerConfig::MixerMode::Internal, 4);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok, result.status);
    EXPECT_TRUE(result.mainFallback);
    EXPECT_EQ("Speakers", result.fallbackDevice);
    const auto outputs = other.getOutputs();
    EXPECT_EQ(1, outputs.size());
    EXPECT_TRUE(has(outputs, AudioPathType::Main, 0, 0));
    EXPECT_EQ(speakers, outputs.constBegin().key());
}

// What the setup really is.

TEST_F(MixerModeTest, SetupIsInternalWithMainAndNoDeckOutput) {
    addMainAndHeadphones();
    EXPECT_EQ(SoundManagerConfig::MixerSetup::Internal, m_config.mixerSetupState().setup);
}

TEST_F(MixerModeTest, SetupIsCustomWhenMainIsAddedInExternal) {
    addMainAndHeadphones();
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);
    m_config.addOutput(m_device, out(AudioPathType::Main, 4));
    EXPECT_EQ(SoundManagerConfig::MixerSetup::Custom, m_config.mixerSetupState().setup);
    EXPECT_TRUE(m_config.mixerSetupState().decksWithoutOutput.isEmpty());
}

TEST_F(MixerModeTest, SetupIsNotExternalWhenABoothOutputStays) {
    m_config.addOutput(m_device, out(AudioPathType::Deck, 0, 0));
    m_config.addOutput(m_device, out(AudioPathType::Deck, 2, 1));
    m_config.addOutput(makeDevice("Other", 1), out(AudioPathType::Booth, 0));
    EXPECT_EQ(SoundManagerConfig::MixerSetup::Custom, m_config.mixerSetupState().setup);
}

TEST_F(MixerModeTest, SyncSetsTheStoredModeFromTheOutputs) {
    addMainAndHeadphones();
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);
    // The user puts Main back by hand and removes the deck rows.
    m_config.clearOutputs();
    m_config.addOutput(m_device, out(AudioPathType::Main, 0));
    m_config.syncMixerModeToOutputs();
    EXPECT_EQ(SoundManagerConfig::MixerMode::Internal, m_config.getMixerMode());
}

TEST_F(MixerModeTest, TwoDecksToFourDecksInExternalNamesTheDecksWithoutOutput) {
    m_config.addOutput(m_device, out(AudioPathType::Main, 0));
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);
    EXPECT_TRUE(m_config.mixerSetupState().decksWithoutOutput.isEmpty());

    m_config.setCorrectDeckCount(4);
    const auto state = m_config.mixerSetupState();
    EXPECT_EQ(SoundManagerConfig::MixerSetup::Custom, state.setup);
    EXPECT_EQ((QList<int>{2, 3}), state.decksWithoutOutput);
}

TEST_F(MixerModeTest, SelectingExternalAgainGivesTheNewDecksAnOutput) {
    addMainAndHeadphones();
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 8).status);
    m_config.setCorrectDeckCount(4);
    ASSERT_FALSE(m_config.mixerSetupState().decksWithoutOutput.isEmpty());

    const auto result = m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 8);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok, result.status);
    EXPECT_EQ(SoundManagerConfig::MixerSetup::External, m_config.mixerSetupState().setup);
    EXPECT_TRUE(has(m_config.getOutputs(), AudioPathType::Deck, 3, 6));
    // The setup from before the first External is still the one that returns.
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::Internal, 8).status);
    EXPECT_TRUE(has(m_config.getOutputs(), AudioPathType::Main, 0, 0));
    EXPECT_TRUE(has(m_config.getOutputs(), AudioPathType::Headphones, 0, 2));
}

TEST_F(MixerModeTest, InternalOnACustomSetupRemovesTheDeckOutputs) {
    m_config.addOutput(m_device, out(AudioPathType::Main, 0));
    m_config.addOutput(m_device, out(AudioPathType::Deck, 4, 0));
    ASSERT_EQ(SoundManagerConfig::MixerSetup::Custom, m_config.mixerSetupState().setup);
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::Internal, 8).status);
    EXPECT_EQ(SoundManagerConfig::MixerSetup::Internal, m_config.mixerSetupState().setup);
}

TEST_F(MixerModeTest, InternalWithNoDeviceAtAllRefuses) {
    EXPECT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::Internal, 0).status);
    // Stored External with no outputs and nothing saved: no device to use.
    QDomDocument doc;
    QDomElement root = doc.createElement("SoundManagerConfig");
    root.setAttribute("mixer_mode", "external");
    doc.appendChild(root);
    m_config.readMixerModeXml(root);
    EXPECT_EQ(SoundManagerConfig::MixerModeStatus::NoDevice,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::Internal, 0).status);
}

// The target manager must accept an External setup.

TEST_F(MixerModeTest, TargetManagerAcceptsAnExternalSetupOnItsDevice) {
    m_config.addOutput(m_device, out(AudioPathType::Main, 0));
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);
    EXPECT_TRUE(AudioTargetManager::acceptsSavedSetup(m_config, true));
    EXPECT_FALSE(AudioTargetManager::acceptsSavedSetup(m_config, false));
}

TEST_F(MixerModeTest, TargetManagerAcceptsAnExternalSetupOnAPcDevice) {
    const SoundDeviceId pc = makeDevice("Audient EVO8", 4);
    SoundManagerConfig config(nullptr);
    config.setDeckCount(2);
    config.addOutput(pc, out(AudioPathType::Main, 0));
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            config.setMixerMode(SoundManagerConfig::MixerMode::External, 8).status);
    EXPECT_TRUE(AudioTargetManager::acceptsSavedSetup(config, false));
    EXPECT_FALSE(AudioTargetManager::acceptsSavedSetup(config, true));
}

TEST_F(MixerModeTest, TargetManagerKeepsAnExternalSetupWhenDecksGrow) {
    m_config.addOutput(m_device, out(AudioPathType::Main, 0));
    ASSERT_EQ(SoundManagerConfig::MixerModeStatus::Ok,
            m_config.setMixerMode(SoundManagerConfig::MixerMode::External, 4).status);
    m_config.setCorrectDeckCount(4);
    EXPECT_TRUE(AudioTargetManager::acceptsSavedSetup(m_config, true));
}

TEST_F(MixerModeTest, TargetManagerStillRejectsASetupWithNoOutput) {
    EXPECT_FALSE(AudioTargetManager::acceptsSavedSetup(m_config, true));
    EXPECT_FALSE(AudioTargetManager::acceptsSavedSetup(m_config, false));
}

TEST_F(MixerModeTest, TheMessageNamesTheDecksWithoutOutput) {
    const QString text = AudioTargetManager::decksWithoutOutputText({2, 3});
    EXPECT_TRUE(text.contains("3, 4"));
    EXPECT_TRUE(text.contains("Preferences"));
    EXPECT_NE(AudioTargetManager::decksWithoutOutputText({2}),
            AudioTargetManager::decksWithoutOutputText({2, 3}));
}

TEST(DecksWithoutOutputNoticeTest, TellsOnceForEachSetOfDecks) {
    AudioTargetManager::DecksWithoutOutputNotice notice;
    EXPECT_FALSE(notice.shouldTell({}));
    EXPECT_TRUE(notice.shouldTell({2, 3}));
    EXPECT_FALSE(notice.shouldTell({2, 3}));
    EXPECT_TRUE(notice.shouldTell({3}));
    EXPECT_FALSE(notice.shouldTell({}));
    // After the decks have outputs again, a new gap is told again.
    EXPECT_TRUE(notice.shouldTell({3}));
}

// The deck count changes in the sound manager, without a device setup.
class DeckCountSignalTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_oldSettingsPath = CmdlineArgs::Instance().getSettingsPath();
        CmdlineArgs::Instance().setSettingsPath(getTestDataDir().path());
        // A saved setup that is External and has no output yet.
        QDomDocument doc;
        QDomElement root = doc.createElement("SoundManagerConfig");
        root.setAttribute("mixer_mode", "external");
        doc.appendChild(root);
        SoundManagerConfig saved(nullptr);
        saved.setAPI(SoundManagerConfig::kAPINone);
        saved.readMixerModeXml(root);
        ASSERT_TRUE(saved.writeToDisk());

        auto pChannelHandleFactory = std::make_shared<ChannelHandleFactory>();
        m_pEffectsManager = std::make_shared<EffectsManager>(config(), pChannelHandleFactory);
        m_pEngine = std::make_shared<EngineMixer>(
                config(), "[Master]", m_pEffectsManager.get(), pChannelHandleFactory, true);
        m_pSoundManager = std::make_shared<SoundManager>(config(), m_pEngine.get());
    }

    void TearDown() override {
        m_pSoundManager.reset();
        m_pEngine.reset();
        m_pEffectsManager.reset();
        CmdlineArgs::Instance().setSettingsPath(m_oldSettingsPath);
    }

    QString m_oldSettingsPath;
    std::shared_ptr<EffectsManager> m_pEffectsManager;
    std::shared_ptr<EngineMixer> m_pEngine;
    std::shared_ptr<SoundManager> m_pSoundManager;
};

TEST_F(DeckCountSignalTest, RisingDeckCountNamesTheDecksWithoutOutput) {
    ASSERT_EQ(SoundManagerConfig::MixerMode::External, m_pSoundManager->getConfig().getMixerMode());
    QSignalSpy spy(m_pSoundManager.get(), &SoundManager::decksWithoutOutput);
    m_pSoundManager->setConfiguredDeckCount(4);
    ASSERT_EQ(1, spy.count());
    EXPECT_EQ((QList<int>{0, 1, 2, 3}), spy.at(0).at(0).value<QList<int>>());
}

TEST_F(DeckCountSignalTest, SameDeckCountGivesNoSignal) {
    m_pSoundManager->setConfiguredDeckCount(4);
    QSignalSpy spy(m_pSoundManager.get(), &SoundManager::decksWithoutOutput);
    m_pSoundManager->setConfiguredDeckCount(4);
    EXPECT_EQ(0, spy.count());
}

} // namespace
// End of AI-generated text.
