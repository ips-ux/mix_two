#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QString>
#include <cmath>
#include <QtDebug>
#include <memory>
#include <tuple>
#include <vector>

#include "control/controlobject.h"
#include "effects/backends/effectsbackendmanager.h"
#include "effects/effectchain.h"
#include "effects/effectslot.h"
#include "effects/effectsmanager.h"
#include "engine/channels/enginechannel.h"
#include "engine/enginemixer.h"
#include "gtest/gtest.h"
#include "test/signalpathtest.h"
#include "util/sample.h"
#include "util/types.h"

using ::testing::Return;
using ::testing::_;

namespace {

class EngineChannelMock : public EngineChannel {
  public:
    EngineChannelMock(const QString& group,
            ChannelOrientation defaultOrientation,
            EngineMixer* pEngineMixer)
            : EngineChannel(pEngineMixer->registerChannelGroup(group),
                      defaultOrientation,
                      nullptr,
                      /*isTalkoverChannel*/ false,
                      /*isPrimarydeck*/ true) {
    }

    void applyVolume(CSAMPLE* pBuff, const std::size_t bufferSize) {
        Q_UNUSED(pBuff);
        Q_UNUSED(bufferSize);
    }

    MOCK_METHOD(ActiveState, updateActiveState, (), (override));
    MOCK_METHOD(bool, isActive, (), (override));
    MOCK_METHOD(bool, isMainMixEnabled, (), (override, const));
    MOCK_METHOD(bool, isPflEnabled, (), (override, const));
    MOCK_METHOD(void, process, (CSAMPLE * pInOut, const std::size_t bufferSize), (override));
    MOCK_METHOD(void, collectFeatures, (GroupFeatureState * pGroupFeatures), (override, const));
    MOCK_METHOD(void, postProcess, (const std::size_t bufferSize), (override));
};

class EngineMixerTest
        : public BaseSignalPathTest,
          public testing::WithParamInterface<std::tuple<int, bool>> {
  protected:
    void assertMainBufferMatchesGolden(const QString& testName) {
        assertBufferMatchesReference(m_pEngineMixer->getMainBuffer(),
                QStringLiteral("%1-main").arg(testName));
    };

    void assertHeadphoneBufferMatchesGolden(const QString& testName) {
        assertBufferMatchesReference(m_pEngineMixer->getHeadphoneBuffer(),
                QStringLiteral("%1-headphone").arg(testName));
    };
    void assertBuffers() {
        QString testname = QString(::testing::UnitTest::GetInstance()->current_test_info()->name());
        assertMainBufferMatchesGolden(testname);
        assertHeadphoneBufferMatchesGolden(testname);
    }

    // this contains all the ugliness...
    std::pair<EngineChannelMock*, std::span<const CSAMPLE>> makeChannel(
            const QString& group, float bufferInitValue) {
        auto pChannelOwner = std::make_unique<EngineChannelMock>(
                group, EngineChannel::CENTER, m_pEngineMixer.get());
        auto* pChannel = pChannelOwner.get();
        m_pEngineMixer->addChannel(std::move(pChannelOwner));

        // Pretend that the channel processed the buffer by filling it with values
        auto buffer = m_pEngineMixer->getChannelBuffer(group);
        SampleUtil::fill(const_cast<CSAMPLE*>(buffer.data()), bufferInitValue, buffer.size());
        return {pChannel, buffer};
    }

  public:
    static std::string nameGenerator(
            const testing::TestParamInfo<EngineMixerTest::ParamType>& info) {
        auto [channelCount, isPfl] = info.param;
        std::string name;
        switch (channelCount) {
        case 1:
            name = "Single";
            break;
        case 2:
            name = "Two";
            break;
        case 3:
            name = "Three";
            break;
        }
        name += "Channel";
        if (isPfl) {
            name += "PFL";
        }
        return name;
    };
};

INSTANTIATE_TEST_SUITE_P(EngineMixerTestSuite,
        EngineMixerTest,
        testing::Combine(testing::Values(1, 2, 3), testing::Bool()),
        EngineMixerTest::nameGenerator);

TEST_P(EngineMixerTest, OutputWorks) {
    const auto [channelCount, isPfl] = GetParam();

    std::vector<std::pair<EngineChannelMock*, std::span<const CSAMPLE>>> channels;
    channels.reserve(channelCount);
    for (int i = 1; i <= channelCount; ++i) {
        QString group = QStringLiteral("[Test%1]").arg(i);
        CSAMPLE bufferInitValue = i * 0.1f;
        channels.push_back(makeChannel(group, bufferInitValue));
    }

    for (auto [pChannel, buffer] : channels) {
        // Instruct the mock to claim it is active and main
        EXPECT_CALL(*pChannel, updateActiveState())
                .Times(1)
                .WillOnce(Return(EngineChannel::ActiveState::Active));
        // behavior needs to differ slightly depending on whether channel is PFL
        if (isPfl) {
            EXPECT_CALL(*pChannel, isActive())
                    .Times(2)
                    .WillRepeatedly(Return(true));
        } else {
            EXPECT_CALL(*pChannel, isActive())
                    .Times(1)
                    .WillOnce(Return(true));
        }
        EXPECT_CALL(*pChannel, isMainMixEnabled())
                .Times(1)
                .WillOnce(Return(true));
        EXPECT_CALL(*pChannel, isPflEnabled())
                .Times(1)
                .WillOnce(Return(isPfl));
        EXPECT_CALL(*pChannel, collectFeatures(_))
                .Times(1);
        EXPECT_CALL(*pChannel, postProcess(static_cast<int>(buffer.size())))
                .Times(1);

        // Instruct the mock to just return when process() gets called.
        EXPECT_CALL(*pChannel, process(_, static_cast<int>(buffer.size())))
                .Times(1)
                .WillOnce(Return());
    }

    m_pEngineMixer->process(static_cast<int>(channels.at(0).second.size()));

    assertBuffers();
}

// Mix Two: MASTER CUE ([Master],main_cue) puts the main mix in the
// headphones even with MIXING (headMix) turned fully to CUE.
class EngineMixerMainCueTest : public EngineMixerTest {
  protected:
    void processOneMainChannel() {
        auto [pChannel, buffer] = makeChannel(QStringLiteral("[Test1]"), 0.5f);
        EXPECT_CALL(*pChannel, updateActiveState())
                .WillRepeatedly(Return(EngineChannel::ActiveState::Active));
        EXPECT_CALL(*pChannel, isActive()).WillRepeatedly(Return(true));
        EXPECT_CALL(*pChannel, isMainMixEnabled()).WillRepeatedly(Return(true));
        EXPECT_CALL(*pChannel, isPflEnabled()).WillRepeatedly(Return(false));
        EXPECT_CALL(*pChannel, collectFeatures(_)).Times(testing::AnyNumber());
        EXPECT_CALL(*pChannel, postProcess(_)).Times(testing::AnyNumber());
        EXPECT_CALL(*pChannel, process(_, _)).WillRepeatedly(Return());
        // Twice, so the headphone gain ramps have settled.
        m_pEngineMixer->process(static_cast<int>(buffer.size()));
        m_pEngineMixer->process(static_cast<int>(buffer.size()));
        m_bufferSize = buffer.size();
    }

    double headphoneLevel() const {
        double sum = 0;
        for (CSAMPLE v : m_pEngineMixer->getHeadphoneBuffer().subspan(0, m_bufferSize)) {
            sum += std::fabs(v);
        }
        return sum / m_bufferSize;
    }

    double mainLevel() const {
        double sum = 0;
        for (CSAMPLE v : m_pEngineMixer->getMainBuffer().subspan(0, m_bufferSize)) {
            sum += std::fabs(v);
        }
        return sum / m_bufferSize;
    }

    std::size_t m_bufferSize = 0;
};

TEST_F(EngineMixerMainCueTest, MainCueSendsTheMainMixToTheHeadphones) {
    ControlObject::set(ConfigKey(QStringLiteral("[Master]"), QStringLiteral("headMix")), -1.0);
    ControlObject::set(ConfigKey(QStringLiteral("[Master]"), QStringLiteral("main_cue")), 1.0);
    processOneMainChannel();
    ASSERT_GT(mainLevel(), 0.0);
    EXPECT_NEAR(mainLevel(), headphoneLevel(), 1e-4);
}

TEST_F(EngineMixerMainCueTest, WithoutMainCueTheCueSideIsSilent) {
    ControlObject::set(ConfigKey(QStringLiteral("[Master]"), QStringLiteral("headMix")), -1.0);
    ControlObject::set(ConfigKey(QStringLiteral("[Master]"), QStringLiteral("main_cue")), 0.0);
    processOneMainChannel();
    ASSERT_GT(mainLevel(), 0.0);
    EXPECT_NEAR(0.0, headphoneLevel(), 1e-6);
}

// AI-generated (Claude Code). Start of AI-generated text.
// Mix Two: an effect unit that is on for a channel is not heard on the CUE
// (headphone) signal of that channel, as on a Pioneer mixer. It is heard on
// the main mix.
class EngineMixerCueEffectTest : public EngineMixerMainCueTest {
  protected:
    const QString m_group = QStringLiteral("[Test1]");
    const QString m_unit = QStringLiteral("[EffectRack1_EffectUnit1]");
    const QString m_slot = QStringLiteral("[EffectRack1_EffectUnit1_Effect1]");

    // One PFL channel with a constant level, and a bitcrusher in effect unit 1.
    void setUpPflChannelAndEffect() {
        m_pEffectsManager->registerInputChannel(
                m_pEngineMixer->registerChannelGroup(m_group));
        m_pEffectsManager->setup();

        auto pManifest = m_pEffectsManager->getBackendManager()->getManifest(
                QStringLiteral("org.mixxx.effects.bitcrusher"),
                EffectBackendType::BuiltIn);
        ASSERT_TRUE(pManifest);
        m_pEffectsManager->getStandardEffectChain(0)->getEffectSlot(0)->loadEffectWithDefaults(
                pManifest);
        ControlObject::set(ConfigKey(m_unit, QStringLiteral("mix")), 1.0);
        ControlObject::set(ConfigKey(m_unit, QStringLiteral("enabled")), 1.0);
        ControlObject::set(ConfigKey(m_slot, QStringLiteral("enabled")), 1.0);
        // The lowest bit depth: a constant level comes out as another level.
        ControlObject::set(ConfigKey(m_slot, QStringLiteral("meta")), 1.0);
        ControlObject::set(ConfigKey(QStringLiteral("[Master]"), QStringLiteral("headMix")), -1.0);
        ControlObject::set(ConfigKey(QStringLiteral("[Master]"), QStringLiteral("main_cue")), 0.0);

        auto [pChannel, buffer] = makeChannel(m_group, 0.3f);
        EXPECT_CALL(*pChannel, updateActiveState())
                .WillRepeatedly(Return(EngineChannel::ActiveState::Active));
        EXPECT_CALL(*pChannel, isActive()).WillRepeatedly(Return(true));
        EXPECT_CALL(*pChannel, isMainMixEnabled()).WillRepeatedly(Return(true));
        EXPECT_CALL(*pChannel, isPflEnabled()).WillRepeatedly(Return(true));
        EXPECT_CALL(*pChannel, collectFeatures(_)).Times(testing::AnyNumber());
        EXPECT_CALL(*pChannel, postProcess(_)).Times(testing::AnyNumber());
        // The channel gives the same level in each callback. The main mix
        // changes the channel buffer in place, so fill it again each time.
        EXPECT_CALL(*pChannel, process(_, _))
                .WillRepeatedly([](CSAMPLE* pInOut, const std::size_t bufferSize) {
                    SampleUtil::fill(pInOut, 0.3f, bufferSize);
                });
        m_bufferSize = buffer.size();
    }

    // Several buffers, so the gain ramps and the effect fades have settled.
    void processSomeBuffers() {
        for (int i = 0; i < 6; ++i) {
            m_pEngineMixer->process(static_cast<int>(m_bufferSize));
        }
    }

    std::vector<CSAMPLE> copyOf(std::span<const CSAMPLE> buffer) const {
        const auto used = buffer.subspan(0, m_bufferSize);
        return std::vector<CSAMPLE>(used.begin(), used.end());
    }

    // Mean distance, sample by sample, of a buffer from an earlier copy. The
    // mean level alone does not show the bitcrusher.
    double distance(std::span<const CSAMPLE> buffer, const std::vector<CSAMPLE>& reference) const {
        double sum = 0;
        for (std::size_t i = 0; i < reference.size(); ++i) {
            sum += std::fabs(buffer[i] - reference[i]);
        }
        return sum / reference.size();
    }

    void setEffectForChannel(bool on) {
        ControlObject::set(ConfigKey(m_unit, QStringLiteral("group_[Test1]_enable")),
                on ? 1.0 : 0.0);
    }
};

TEST_F(EngineMixerCueEffectTest, ChannelEffectIsNotHeardOnCue) {
    setUpPflChannelAndEffect();
    setEffectForChannel(false);
    processSomeBuffers();
    ASSERT_GT(mainLevel(), 0.01);
    ASSERT_GT(headphoneLevel(), 0.01);
    const std::vector<CSAMPLE> dryMain = copyOf(m_pEngineMixer->getMainBuffer());
    const std::vector<CSAMPLE> dryCue = copyOf(m_pEngineMixer->getHeadphoneBuffer());

    setEffectForChannel(true);
    processSomeBuffers();
    // The effect is heard on the main mix.
    EXPECT_GT(distance(m_pEngineMixer->getMainBuffer(), dryMain), 0.01);
    // The CUE signal stays dry.
    EXPECT_LT(distance(m_pEngineMixer->getHeadphoneBuffer(), dryCue), 1e-4);
}

TEST_F(EngineMixerCueEffectTest, EffectUnitOnTheHeadphonesIsHeardOnCue) {
    setUpPflChannelAndEffect();
    setEffectForChannel(false);
    processSomeBuffers();
    ASSERT_GT(headphoneLevel(), 0.01);
    const std::vector<CSAMPLE> dryMain = copyOf(m_pEngineMixer->getMainBuffer());
    const std::vector<CSAMPLE> dryCue = copyOf(m_pEngineMixer->getHeadphoneBuffer());

    // The unit is on for the headphone output, not for the channel.
    ControlObject::set(ConfigKey(m_unit, QStringLiteral("group_[Headphone]_enable")), 1.0);
    processSomeBuffers();
    EXPECT_GT(distance(m_pEngineMixer->getHeadphoneBuffer(), dryCue), 0.01);
    EXPECT_LT(distance(m_pEngineMixer->getMainBuffer(), dryMain), 1e-4);
}
// End of AI-generated text.

} // namespace
