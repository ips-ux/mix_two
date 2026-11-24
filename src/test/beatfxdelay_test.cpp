// AI-generated (Claude Code). Start of AI-generated text.
// Tests of the Mix Two BEAT FX DELAY and PING PONG (DJM-900NXS2 manual p.15
// as the written specification). The effects return the wet sound only: the
// engine adds the dry sound (setAddDryToWet), so the output at depth 0 is
// silent.
#include <gtest/gtest.h>

#include <QMap>
#include <QSharedPointer>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <vector>

#include "effects/backends/builtin/beatfxcommon.h"
#include "effects/backends/builtin/beatfxdelayeffect.h"
#include "effects/backends/effectsbackend.h"
#include "effects/backends/effectsbackendmanager.h"
#include "engine/effects/engineeffectparameter.h"
#include "engine/effects/groupfeaturestate.h"
#include "mixxxtest.h"
#include "util/samplebuffer.h"

namespace {

constexpr int kSampleRate = 48000;
constexpr int kBufferFrames = 256;
// A short beat keeps the test small: 480 frames.
constexpr double kBeatSeconds = 0.01;
constexpr int kBeatFrames = 480;

template<class Effect>
class Rig {
  public:
    Rig(double beatIndex, double depth)
            : m_engineParameters(mixxx::audio::SampleRate(kSampleRate), kBufferFrames),
              m_state(m_engineParameters),
              m_input(m_engineParameters.samplesPerBuffer()),
              m_output(m_engineParameters.samplesPerBuffer()) {
        const auto pManifest = Effect::getManifest();
        QMap<QString, EngineEffectParameterPointer> parameters;
        for (const auto& pParameter : pManifest->parameters()) {
            parameters.insert(pParameter->id(),
                    EngineEffectParameterPointer(new EngineEffectParameter(pParameter)));
        }
        parameters.value(QStringLiteral("beat"))->setValue(beatIndex);
        parameters.value(QStringLiteral("depth"))->setValue(depth);
        m_effect.loadEngineEffectParameters(parameters);
        m_groupFeatures.beat_length = GroupFeatureBeatLength{kBeatSeconds, kBeatSeconds};
    }

    // Runs one buffer. `input` holds interleaved samples; missing samples
    // are silent. The output is appended to m_out.
    void run(const std::vector<float>& input,
            EffectEnableState enableState = EffectEnableState::Enabled) {
        for (int i = 0; i < m_engineParameters.samplesPerBuffer(); ++i) {
            m_input[i] = static_cast<std::size_t>(i) < input.size() ? input[i] : 0.0f;
        }
        m_effect.processChannel(&m_state,
                m_input.data(),
                m_output.data(),
                m_engineParameters,
                enableState,
                m_groupFeatures);
        for (int i = 0; i < m_engineParameters.samplesPerBuffer(); ++i) {
            m_out.push_back(m_output[i]);
        }
    }

    // An impulse on both channels at frame 0 in the first buffer, then
    // silence, for `buffers` buffers.
    void runImpulse(int buffers) {
        for (int b = 0; b < buffers; ++b) {
            std::vector<float> in;
            if (b == 0) {
                in = {1.0f, 1.0f};
            }
            run(in);
        }
    }

    float left(int frame) const {
        return m_out[frame * 2];
    }
    float right(int frame) const {
        return m_out[frame * 2 + 1];
    }
    // The largest absolute value of a channel within +-1 frame of `frame`.
    float peak(int frame, int channel) const {
        float result = 0;
        for (int f = frame - 1; f <= frame + 1; ++f) {
            result = std::max(result, std::abs(m_out[f * 2 + channel]));
        }
        return result;
    }
    int frames() const {
        return static_cast<int>(m_out.size() / 2);
    }

    std::vector<float> m_out;

  private:
    mixxx::EngineParameters m_engineParameters;
    Effect m_effect;
    typename Effect::StateType m_state;
    GroupFeatureState m_groupFeatures;
    mixxx::SampleBuffer m_input;
    mixxx::SampleBuffer m_output;
};

using DelayRig = Rig<BeatFxDelayEffect>;
using PingPongRig = Rig<BeatFxPingPongEffect>;

// Beat index 4 is one beat.
constexpr double kOneBeat = 4;

// Buffers to run so that more than 4 beat times are in the output.
constexpr int kBuffers = (kBeatFrames * 4 + 600) / kBufferFrames + 1;

void expectBeatAndDepth(const EffectManifestPointer& pManifest,
        const QString& id,
        const QString& name) {
    EXPECT_EQ(id, pManifest->id());
    EXPECT_EQ(name, pManifest->name());
    ASSERT_EQ(2, pManifest->parameters().size());
    const auto pBeat = pManifest->parameters().at(0);
    EXPECT_EQ(QStringLiteral("beat"), pBeat->id());
    EXPECT_EQ(0.0, pBeat->getMinimum());
    EXPECT_EQ(8.0, pBeat->getMaximum()); // nine steps
    EXPECT_EQ(1.0, beatfx::beatFraction(pBeat->getDefault()));
    EXPECT_EQ(QStringLiteral("depth"), pManifest->parameters().at(1)->id());
    EXPECT_FALSE(pManifest->parameters().at(1)->description().isEmpty());
}

} // namespace

TEST(BeatFxDelayTest, DelayManifest) {
    expectBeatAndDepth(BeatFxDelayEffect::getManifest(),
            QStringLiteral("org.mixtwo.beatfx.delay"),
            QStringLiteral("DELAY"));
}

TEST(BeatFxDelayTest, PingPongManifest) {
    expectBeatAndDepth(BeatFxPingPongEffect::getManifest(),
            QStringLiteral("org.mixtwo.beatfx.pingpong"),
            QStringLiteral("PING PONG"));
}

TEST(BeatFxDelayTest, BothEffectsAddTheDryToTheWet) {
    EXPECT_TRUE(BeatFxDelayEffect::getManifest()->addDryToWet());
    EXPECT_TRUE(BeatFxPingPongEffect::getManifest()->addDryToWet());
}

class BeatFxDelayRegistryTest : public MixxxTest {};

TEST_F(BeatFxDelayRegistryTest, BothEffectsAreRegisteredInTheBuiltInBackend) {
    EffectsBackendManager manager;
    EXPECT_TRUE(manager.getManifest(
            QStringLiteral("org.mixtwo.beatfx.delay"), EffectBackendType::BuiltIn));
    EXPECT_TRUE(manager.getManifest(
            QStringLiteral("org.mixtwo.beatfx.pingpong"), EffectBackendType::BuiltIn));
}

TEST(BeatFxDelayTest, DelayGivesOneRepeatAtTheBeatTime) {
    DelayRig rig(kOneBeat, 1.0);
    rig.runImpulse(kBuffers);
    ASSERT_GT(rig.frames(), kBeatFrames * 4);
    EXPECT_GT(rig.peak(kBeatFrames, 0), 0.9f);
    EXPECT_GT(rig.peak(kBeatFrames, 1), 0.9f);
    EXPECT_LT(rig.peak(kBeatFrames * 2, 0), 1e-4f);
    EXPECT_LT(rig.peak(kBeatFrames * 2, 1), 1e-4f);
    EXPECT_LT(rig.peak(kBeatFrames * 3, 0), 1e-4f);
    EXPECT_LT(rig.peak(kBeatFrames * 3, 1), 1e-4f);
    // Nothing before the repeat, and nothing after it either.
    for (int f = 0; f < kBeatFrames - 2; ++f) {
        ASSERT_NEAR(0.0f, rig.left(f), 1e-6f) << "frame " << f;
    }
    for (int f = kBeatFrames + 2; f < rig.frames(); ++f) {
        ASSERT_NEAR(0.0f, rig.left(f), 1e-6f) << "frame " << f;
        ASSERT_NEAR(0.0f, rig.right(f), 1e-6f) << "frame " << f;
    }
}

TEST(BeatFxDelayTest, PingPongAlternatesAndFades) {
    PingPongRig rig(kOneBeat, 1.0);
    rig.runImpulse(kBuffers);
    ASSERT_GT(rig.frames(), kBeatFrames * 4);
    const float first = rig.peak(kBeatFrames, 0);
    const float second = rig.peak(kBeatFrames * 2, 1);
    const float third = rig.peak(kBeatFrames * 3, 0);
    EXPECT_GT(first, 0.5f);
    EXPECT_GT(second, 0.1f);
    EXPECT_GT(third, 0.05f);
    EXPECT_GT(first, second);
    EXPECT_GT(second, third);
    // First echo left only, second right only, third left only.
    EXPECT_LT(rig.peak(kBeatFrames, 1), 1e-4f);
    EXPECT_LT(rig.peak(kBeatFrames * 2, 0), 1e-4f);
    EXPECT_LT(rig.peak(kBeatFrames * 3, 1), 1e-4f);
}

TEST(BeatFxDelayTest, DepthZeroGivesNoEcho) {
    DelayRig delay(kOneBeat, 0.0);
    delay.runImpulse(kBuffers);
    PingPongRig pingPong(kOneBeat, 0.0);
    pingPong.runImpulse(kBuffers);
    for (float sample : delay.m_out) {
        ASSERT_EQ(0.0f, sample);
    }
    for (float sample : pingPong.m_out) {
        ASSERT_EQ(0.0f, sample);
    }
}

TEST(BeatFxDelayTest, PingPongTailContinuesWithSilentInputAndStaysInRange) {
    PingPongRig rig(kOneBeat, 1.0);
    // Loud input for some buffers, then silence for many buffers.
    for (int b = 0; b < 12; ++b) {
        std::vector<float> in(kBufferFrames * 2);
        for (std::size_t i = 0; i < in.size(); ++i) {
            in[i] = 0.9f * static_cast<float>(std::sin(static_cast<double>(i) * 0.1));
        }
        rig.run(in);
    }
    const int silenceStart = rig.frames();
    for (int b = 0; b < 200; ++b) {
        rig.run({});
    }
    // The tail is still there right after the input stops.
    float tail = 0;
    for (int f = silenceStart; f < silenceStart + kBeatFrames; ++f) {
        tail = std::max(tail, std::abs(rig.left(f)));
        tail = std::max(tail, std::abs(rig.right(f)));
    }
    EXPECT_GT(tail, 0.05f);
    for (float sample : rig.m_out) {
        ASSERT_TRUE(std::isfinite(sample));
        ASSERT_LE(std::abs(sample), 1.0f);
    }
    // It fades out in the end.
    float last = 0;
    for (int f = rig.frames() - 100; f < rig.frames(); ++f) {
        last = std::max(last, std::abs(rig.left(f)));
        last = std::max(last, std::abs(rig.right(f)));
    }
    EXPECT_LT(last, 1e-3f);
}

TEST(BeatFxDelayTest, BothStayFiniteAndInRangeWithLoudInput) {
    DelayRig delay(kOneBeat, 1.0);
    PingPongRig pingPong(kOneBeat, 1.0);
    for (int b = 0; b < 40; ++b) {
        std::vector<float> in(kBufferFrames * 2);
        for (std::size_t i = 0; i < in.size(); ++i) {
            in[i] = 0.9f * static_cast<float>(std::sin(static_cast<double>(i) * 0.07));
        }
        delay.run(in);
        pingPong.run(in);
    }
    for (float sample : delay.m_out) {
        ASSERT_TRUE(std::isfinite(sample));
        ASSERT_LE(std::abs(sample), 1.0f);
    }
    for (float sample : pingPong.m_out) {
        ASSERT_TRUE(std::isfinite(sample));
        ASSERT_LE(std::abs(sample), 1.0f);
    }
}

TEST(BeatFxDelayTest, DisablingClearsTheTail) {
    PingPongRig rig(kOneBeat, 1.0);
    rig.run({1.0f, 1.0f});
    rig.run({}, EffectEnableState::Disabling);
    // After a disable, the next enabled run holds no old echo.
    const int start = rig.frames();
    for (int b = 0; b < kBuffers; ++b) {
        rig.run({});
    }
    for (int f = start; f < rig.frames(); ++f) {
        ASSERT_NEAR(0.0f, rig.left(f), 1e-6f);
        ASSERT_NEAR(0.0f, rig.right(f), 1e-6f);
    }
}
// End of AI-generated text.
