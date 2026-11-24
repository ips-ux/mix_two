// AI-generated (Claude Code). Start of AI-generated text.
// Tests of the Mix Two BEAT FX VINYL BRAKE (DJM-900NXS2 manual p.17 as the
// written specification). The effect returns the whole sound of the channel:
// the engine does not add the dry sound.
#include <gtest/gtest.h>

#include <QMap>
#include <QSharedPointer>
#include <algorithm>
#include <cmath>
#include <vector>

#include "effects/backends/builtin/beatfxcommon.h"
#include "effects/backends/builtin/beatfxvinylbrakeeffect.h"
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
constexpr double kSineHz = 2000.0;
constexpr double kPi = 3.14159265358979323846;
// Beat index 6 is 4 beats: the brake time is 0.04 s, 1920 frames.
constexpr double kFourBeats = 6;
constexpr double kEightBeats = 7;
constexpr int kFourBeatFrames = 1920;
// The crossfade of the effect is 10 ms.
constexpr int kCrossfadeFrames = 480;

class BrakeRig {
  public:
    BrakeRig(double beatIndex, double depth, double bufferSeconds = BeatFxVinylBrakeState::kMaxSeconds)
            : m_engineParameters(mixxx::audio::SampleRate(kSampleRate), kBufferFrames),
              m_state(m_engineParameters, bufferSeconds),
              m_input(m_engineParameters.samplesPerBuffer()),
              m_output(m_engineParameters.samplesPerBuffer()) {
        const auto pManifest = BeatFxVinylBrakeEffect::getManifest();
        for (const auto& pParameter : pManifest->parameters()) {
            m_parameters.insert(pParameter->id(),
                    EngineEffectParameterPointer(new EngineEffectParameter(pParameter)));
        }
        m_parameters.value(QStringLiteral("beat"))->setValue(beatIndex);
        m_parameters.value(QStringLiteral("depth"))->setValue(depth);
        m_effect.loadEngineEffectParameters(m_parameters);
        m_groupFeatures.beat_length = GroupFeatureBeatLength{kBeatSeconds, kBeatSeconds};
    }

    void setDepth(double depth) {
        m_parameters.value(QStringLiteral("depth"))->setValue(depth);
    }

    // Runs one buffer of a sine input (both channels). The input and the
    // output go to m_in and m_out.
    void run(double amplitude = 0.5,
            EffectEnableState enableState = EffectEnableState::Enabled) {
        for (int f = 0; f < kBufferFrames; ++f) {
            const auto sample = static_cast<float>(
                    amplitude * std::sin(2.0 * kPi * kSineHz * m_frame / kSampleRate));
            ++m_frame;
            m_input[f * 2] = sample;
            m_input[f * 2 + 1] = sample;
        }
        m_effect.processChannel(&m_state,
                m_input.data(),
                m_output.data(),
                m_engineParameters,
                enableState,
                m_groupFeatures);
        for (int i = 0; i < m_engineParameters.samplesPerBuffer(); ++i) {
            m_in.push_back(m_input[i]);
            m_out.push_back(m_output[i]);
        }
    }

    void runBuffers(int buffers) {
        for (int b = 0; b < buffers; ++b) {
            run();
        }
    }
    // Runs buffers until at least `frames` frames are in the output.
    void runFrames(int frames) {
        while (this->frames() < frames) {
            run();
        }
    }

    int frames() const {
        return static_cast<int>(m_out.size() / 2);
    }
    float left(int frame) const {
        return m_out[frame * 2];
    }
    float right(int frame) const {
        return m_out[frame * 2 + 1];
    }
    float inLeft(int frame) const {
        return m_in[frame * 2];
    }
    // The largest absolute output value in [from, to).
    float peak(int from, int to) const {
        float result = 0;
        for (int f = from; f < to; ++f) {
            result = std::max(result, std::max(std::abs(left(f)), std::abs(right(f))));
        }
        return result;
    }
    // The sign changes of the left output in [from, to), without the
    // silent samples.
    int crossings(int from, int to) const {
        int count = 0;
        float previous = 0;
        for (int f = from; f < to; ++f) {
            const float x = left(f);
            if (std::abs(x) < 1e-7f) {
                continue;
            }
            if (previous != 0 && ((x > 0) != (previous > 0))) {
                ++count;
            }
            previous = x;
        }
        return count;
    }
    // The sign changes of the input in [from, to).
    int inputCrossings(int from, int to) const {
        int count = 0;
        float previous = 0;
        for (int f = from; f < to; ++f) {
            const float x = inLeft(f);
            if (std::abs(x) < 1e-7f) {
                continue;
            }
            if (previous != 0 && ((x > 0) != (previous > 0))) {
                ++count;
            }
            previous = x;
        }
        return count;
    }
    // The first frame from which the output is silent to the end.
    int silentFrom() const {
        int f = frames();
        while (f > 0 && std::abs(left(f - 1)) < 1e-7f && std::abs(right(f - 1)) < 1e-7f) {
            --f;
        }
        return f;
    }

    std::vector<float> m_in;
    std::vector<float> m_out;

  private:
    mixxx::EngineParameters m_engineParameters;
    BeatFxVinylBrakeEffect m_effect;
    BeatFxVinylBrakeState m_state;
    GroupFeatureState m_groupFeatures;
    QMap<QString, EngineEffectParameterPointer> m_parameters;
    mixxx::SampleBuffer m_input;
    mixxx::SampleBuffer m_output;
    long long m_frame = 0;
};

} // namespace

class BeatFxVinylBrakeRegistryTest : public MixxxTest {};

TEST_F(BeatFxVinylBrakeRegistryTest, IsRegisteredInTheBuiltInBackend) {
    EffectsBackendManager manager;
    EXPECT_TRUE(manager.getManifest(
            QStringLiteral("org.mixtwo.beatfx.vinylbrake"), EffectBackendType::BuiltIn));
}

TEST(BeatFxVinylBrakeTest, ManifestHasBeatThenDepthAndReplacesTheSound) {
    const auto pManifest = BeatFxVinylBrakeEffect::getManifest();
    EXPECT_EQ(QStringLiteral("org.mixtwo.beatfx.vinylbrake"), pManifest->id());
    EXPECT_EQ(QStringLiteral("VINYL BRAKE"), pManifest->name());
    EXPECT_FALSE(pManifest->addDryToWet());
    ASSERT_EQ(2, pManifest->parameters().size());
    const auto& beat = pManifest->parameters().at(0);
    EXPECT_EQ(QStringLiteral("beat"), beat->id());
    EXPECT_DOUBLE_EQ(0.0, beat->getMinimum());
    EXPECT_DOUBLE_EQ(8.0, beat->getMaximum()); // nine steps
    EXPECT_DOUBLE_EQ(4.0, beat->getDefault()); // 1 beat
    EXPECT_EQ(QStringLiteral("depth"), pManifest->parameters().at(1)->id());
}

TEST(BeatFxVinylBrakeTest, DepthZeroPassesTheInputThroughExactly) {
    BrakeRig rig(kFourBeats, 0.0);
    rig.runBuffers(20);
    for (int f = 0; f < rig.frames(); ++f) {
        ASSERT_EQ(rig.inLeft(f), rig.left(f)) << "frame " << f;
        ASSERT_EQ(rig.inLeft(f), rig.right(f)) << "frame " << f;
    }
}

TEST(BeatFxVinylBrakeTest, DepthOneBrakesToSilenceInTheBrakeTime) {
    BrakeRig rig(kFourBeats, 1.0);
    rig.runFrames(kFourBeatFrames * 3);
    // Before T the sound is there.
    EXPECT_GT(rig.peak(kFourBeatFrames / 4, kFourBeatFrames / 2), 0.05f);
    EXPECT_GT(rig.peak(kFourBeatFrames / 2, kFourBeatFrames * 3 / 4), 0.01f);
    // After T the output is silent, and it stays silent.
    EXPECT_LE(rig.silentFrom(), kFourBeatFrames + 2);
    EXPECT_EQ(0.0f, rig.peak(kFourBeatFrames + 2, rig.frames()));
}

TEST(BeatFxVinylBrakeTest, ThePitchFallsDuringTheBrake) {
    BrakeRig rig(kFourBeats, 1.0);
    rig.runFrames(kFourBeatFrames);
    const int half = kFourBeatFrames / 2;
    const int first = rig.crossings(0, half);
    const int second = rig.crossings(half, kFourBeatFrames);
    // The speed goes from 1 to 0.5, then from 0.5 to 0: about 3 to 1.
    EXPECT_GT(first, 0);
    EXPECT_GT(second, 0);
    EXPECT_GT(first, 2 * second) << first << " and " << second;
    // The first half has the pitch of a sine that slows to 0.75 on average.
    EXPECT_NEAR(rig.inputCrossings(0, half) * 0.75, first, first * 0.15);
}

TEST(BeatFxVinylBrakeTest, DepthOneHalfKeepsHalfSpeed) {
    BrakeRig rig(kFourBeats, 0.5);
    rig.runFrames(kFourBeatFrames * 4);
    // The speed is at 0.5 after T / 2. Look at a later window.
    const int from = kFourBeatFrames;
    const int to = kFourBeatFrames * 3;
    EXPECT_GT(rig.peak(from, to), 0.05f);
    const double ratio = static_cast<double>(rig.crossings(from, to)) /
            rig.inputCrossings(from, to);
    EXPECT_NEAR(0.5, ratio, 0.06);
}

TEST(BeatFxVinylBrakeTest, BackToDepthZeroAfterAStopReturnsToTheInput) {
    BrakeRig rig(kFourBeats, 1.0);
    rig.runFrames(kFourBeatFrames * 2);
    const int stoppedAt = rig.frames();
    EXPECT_EQ(0.0f, rig.peak(stoppedAt - kBufferFrames, stoppedAt));
    rig.setDepth(0.0);
    // The speed leaves 0 at once and the sound starts again: silent
    // at first, then sound.
    const int releasedAt = stoppedAt;
    rig.runFrames(releasedAt + kFourBeatFrames + kCrossfadeFrames + kBufferFrames * 3);
    EXPECT_LT(rig.peak(releasedAt, releasedAt + 4), 0.05f);
    EXPECT_GT(rig.peak(releasedAt + kFourBeatFrames / 4, releasedAt + kFourBeatFrames / 2), 0.05f);
    // After T plus the crossfade the output is the input again.
    const int exactFrom = releasedAt + kFourBeatFrames + kCrossfadeFrames + 2 * kBufferFrames;
    ASSERT_GT(rig.frames(), exactFrom);
    for (int f = exactFrom; f < rig.frames(); ++f) {
        ASSERT_EQ(rig.inLeft(f), rig.left(f)) << "frame " << f;
        ASSERT_EQ(rig.inLeft(f), rig.right(f)) << "frame " << f;
    }
}

TEST(BeatFxVinylBrakeTest, ALongRunWithAShortBufferStaysInRange) {
    // A buffer of 0.1 s overflows many times in this run: speed 0.3 gives
    // a lag of 0.7 frames for each frame.
    BrakeRig rig(kFourBeats, 1.0, 0.1);
    rig.runFrames(kSampleRate / 2);
    rig.setDepth(0.3);
    rig.runFrames(kSampleRate * 4);
    rig.setDepth(1.0);
    rig.runFrames(kSampleRate * 5);
    rig.setDepth(0.0);
    rig.runFrames(kSampleRate * 6);
    for (int i = 0; i < static_cast<int>(rig.m_out.size()); ++i) {
        ASSERT_TRUE(std::isfinite(rig.m_out[i])) << "sample " << i;
        ASSERT_LE(std::abs(rig.m_out[i]), 1.0f) << "sample " << i;
    }
}

TEST(BeatFxVinylBrakeTest, TheBeatValueSetsTheBrakeTime) {
    BrakeRig four(kFourBeats, 1.0);
    four.runFrames(kFourBeatFrames * 4);
    BrakeRig eight(kEightBeats, 1.0);
    eight.runFrames(kFourBeatFrames * 4);
    const int fourStop = four.silentFrom();
    const int eightStop = eight.silentFrom();
    EXPECT_NEAR(kFourBeatFrames, fourStop, kBufferFrames);
    EXPECT_NEAR(kFourBeatFrames * 2, eightStop, kBufferFrames);
    EXPECT_GT(eightStop, fourStop + kFourBeatFrames / 2);
}

TEST(BeatFxVinylBrakeTest, SwitchingOffClearsTheState) {
    BrakeRig rig(kFourBeats, 1.0);
    rig.runFrames(kFourBeatFrames * 2);
    rig.run(0.5, EffectEnableState::Disabling);
    rig.setDepth(0.0);
    const int from = rig.frames();
    rig.runBuffers(4);
    // The next switch-on starts at normal speed, so there is no lag.
    for (int f = from; f < rig.frames(); ++f) {
        ASSERT_EQ(rig.inLeft(f), rig.left(f)) << "frame " << f;
    }
}
// End of AI-generated text.
