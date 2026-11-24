// AI-generated (Claude Code). Start of AI-generated text.
// Tests of the BEAT ranges of the Mix Two BEAT FX (DDJ-800 manual p.70, 71,
// 73, 74): FLANGER and PHASER go from 1/16 to 64 beats; ENIGMA JET and
// MOBIUS go from -64 to 64 beats (a negative value makes the pitch fall); all
// other BEAT FX keep 1/16 to 16 beats.
#include <gtest/gtest.h>

#include <QMap>
#include <QSharedPointer>
#include <cmath>

#include "effects/backends/builtin/beatfxcommon.h"
#include "effects/backends/builtin/beatfxechoeffect.h"
#include "effects/backends/builtin/beatfxenigmajeteffect.h"
#include "effects/backends/builtin/beatfxflangereffect.h"
#include "effects/backends/builtin/beatfxmobiuseffect.h"
#include "engine/effects/engineeffectparameter.h"
#include "engine/effects/groupfeaturestate.h"
#include "util/samplebuffer.h"

namespace {

// The lists are written out here on purpose: they are an independent
// statement of the ranges in the manual, not a copy of the production lists.
constexpr double kWide[] = {1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 1, 2, 4, 8, 16, 32, 64};
constexpr double kSigned[] = {-64, -32, -16, -8, -4, -2, -1, -0.5, -0.25, -0.125, -0.0625,
        0.0625, 0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32, 64};

bool hasParameter(const EffectManifestPointer& pManifest, const QString& id) {
    for (const auto& pParameter : pManifest->parameters()) {
        if (pParameter->id() == id) {
            return true;
        }
    }
    return false;
}

// The manifest's first parameter must be `beat`, with the given number of
// steps, and its default must be one beat.
void expectBeatParameter(const EffectManifestPointer& pManifest, int steps, bool isSigned) {
    ASSERT_FALSE(pManifest->parameters().isEmpty());
    const auto pBeat = pManifest->parameters().first();
    EXPECT_EQ(QStringLiteral("beat"), pBeat->id());
    EXPECT_EQ(0.0, pBeat->getMinimum());
    EXPECT_EQ(static_cast<double>(steps - 1), pBeat->getMaximum());
    const double beats = isSigned ? beatfx::signedBeats(pBeat->getDefault())
                                  : beatfx::wideBeatFraction(pBeat->getDefault());
    EXPECT_EQ(1.0, beats);
}

} // namespace

TEST(BeatFxBeatTest, WideListEnds) {
    EXPECT_EQ(1.0 / 16, beatfx::wideBeatFraction(0));
    EXPECT_EQ(1.0, beatfx::wideBeatFraction(beatfx::kDefaultWideBeatIndex));
    EXPECT_EQ(64.0, beatfx::wideBeatFraction(10));
    for (int i = 0; i < 11; ++i) {
        EXPECT_EQ(kWide[i], beatfx::wideBeatFraction(i)) << "index " << i;
    }
}

TEST(BeatFxBeatTest, WideListIsClamped) {
    EXPECT_EQ(1.0 / 16, beatfx::wideBeatFraction(-1));
    EXPECT_EQ(1.0 / 16, beatfx::wideBeatFraction(-100));
    EXPECT_EQ(64.0, beatfx::wideBeatFraction(11));
    EXPECT_EQ(64.0, beatfx::wideBeatFraction(1000));
}

TEST(BeatFxBeatTest, SignedListValues) {
    EXPECT_EQ(-64.0, beatfx::signedBeats(0));
    EXPECT_EQ(-1.0 / 16, beatfx::signedBeats(10));
    EXPECT_EQ(1.0 / 16, beatfx::signedBeats(11));
    EXPECT_EQ(1.0, beatfx::signedBeats(beatfx::kDefaultSignedBeatIndex));
    EXPECT_EQ(64.0, beatfx::signedBeats(21));
    for (int i = 0; i < 22; ++i) {
        EXPECT_EQ(kSigned[i], beatfx::signedBeats(i)) << "index " << i;
    }
}

TEST(BeatFxBeatTest, SignedListIsClamped) {
    EXPECT_EQ(-64.0, beatfx::signedBeats(-1));
    EXPECT_EQ(-64.0, beatfx::signedBeats(-100));
    EXPECT_EQ(64.0, beatfx::signedBeats(22));
    EXPECT_EQ(64.0, beatfx::signedBeats(1000));
}

TEST(BeatFxBeatTest, SignedListRisesStrictly) {
    for (int i = 1; i < 22; ++i) {
        EXPECT_LT(beatfx::signedBeats(i - 1), beatfx::signedBeats(i)) << "index " << i;
    }
}

TEST(BeatFxBeatTest, StepValueForTheThreeRanges) {
    double beats = 0;
    ASSERT_TRUE(beatfx::beatStepValue(9, 0, &beats));
    EXPECT_EQ(1.0 / 16, beats);
    ASSERT_TRUE(beatfx::beatStepValue(9, 8, &beats));
    EXPECT_EQ(16.0, beats);
    ASSERT_TRUE(beatfx::beatStepValue(11, 10, &beats));
    EXPECT_EQ(64.0, beats);
    ASSERT_TRUE(beatfx::beatStepValue(22, 0, &beats));
    EXPECT_EQ(-64.0, beats);
    EXPECT_FALSE(beatfx::beatStepValue(10, 0, &beats));
    EXPECT_FALSE(beatfx::beatStepValue(11, 11, &beats));
    EXPECT_FALSE(beatfx::beatStepValue(22, -1, &beats));
}

TEST(BeatFxBeatTest, LabelsShowTheSignAndTheFractions) {
    EXPECT_EQ(QStringLiteral("1/16"), beatfx::beatLabel(1.0 / 16));
    EXPECT_EQ(QStringLiteral("-1/16"), beatfx::beatLabel(-1.0 / 16));
    EXPECT_EQ(QStringLiteral("1/2"), beatfx::beatLabel(0.5));
    EXPECT_EQ(QStringLiteral("1"), beatfx::beatLabel(1));
    EXPECT_EQ(QStringLiteral("-64"), beatfx::beatLabel(-64));
    EXPECT_EQ(QStringLiteral("32"), beatfx::beatLabel(32));
}

TEST(BeatFxBeatTest, FlangerAndPhaserHaveElevenBeatSteps) {
    expectBeatParameter(BeatFxFlanger::getManifest(), 11, false);
    expectBeatParameter(BeatFxPhaser::getManifest(), 11, false);
}

TEST(BeatFxBeatTest, EnigmaJetAndMobiusHaveTwentyTwoSignedBeatSteps) {
    expectBeatParameter(BeatFxEnigmaJetEffect::getManifest(), 22, true);
    expectBeatParameter(BeatFxMobiusSaw::getManifest(), 22, true);
    expectBeatParameter(BeatFxMobiusTri::getManifest(), 22, true);
}

TEST(BeatFxBeatTest, EnigmaJetAndMobiusHaveNoDescendParameter) {
    EXPECT_FALSE(hasParameter(BeatFxEnigmaJetEffect::getManifest(), QStringLiteral("descend")));
    EXPECT_FALSE(hasParameter(BeatFxMobiusSaw::getManifest(), QStringLiteral("descend")));
    EXPECT_FALSE(hasParameter(BeatFxMobiusTri::getManifest(), QStringLiteral("descend")));
}

TEST(BeatFxBeatTest, OtherBeatFxKeepNineSteps) {
    const auto pEcho = BeatFxEcho::getManifest();
    ASSERT_FALSE(pEcho->parameters().isEmpty());
    const auto pBeat = pEcho->parameters().first();
    EXPECT_EQ(QStringLiteral("beat"), pBeat->id());
    EXPECT_EQ(0.0, pBeat->getMinimum());
    EXPECT_EQ(8.0, pBeat->getMaximum());
    EXPECT_EQ(1.0, beatfx::beatFraction(pBeat->getDefault()));
    EXPECT_EQ(16.0, beatfx::beatFraction(8));
}

namespace {

// Runs the effect for some buffers at the given beat index and a 60 BPM beat
// length; every output sample must be finite.
template<class Effect, class State>
void expectFiniteOutput(double beatIndex) {
    const mixxx::EngineParameters engineParameters(
            mixxx::audio::SampleRate(48000), 512);
    const auto pManifest = Effect::getManifest();
    QMap<QString, EngineEffectParameterPointer> parameters;
    for (const auto& pParameter : pManifest->parameters()) {
        parameters.insert(pParameter->id(),
                EngineEffectParameterPointer(new EngineEffectParameter(pParameter)));
    }
    parameters.value(QStringLiteral("beat"))->setValue(beatIndex);
    parameters.value(QStringLiteral("depth"))->setValue(1.0);

    Effect effect;
    effect.loadEngineEffectParameters(parameters);
    State state(engineParameters);

    GroupFeatureState groupFeatures;
    groupFeatures.beat_length = GroupFeatureBeatLength{1.0, 1.0}; // 60 BPM

    mixxx::SampleBuffer input(engineParameters.samplesPerBuffer());
    mixxx::SampleBuffer output(engineParameters.samplesPerBuffer());
    for (int i = 0; i < engineParameters.samplesPerBuffer(); ++i) {
        input[i] = 0.5f * static_cast<CSAMPLE>(std::sin(i * 0.05));
    }
    for (int run = 0; run < 40; ++run) {
        effect.processChannel(&state,
                input.data(),
                output.data(),
                engineParameters,
                EffectEnableState::Enabled,
                groupFeatures);
        for (int i = 0; i < engineParameters.samplesPerBuffer(); ++i) {
            ASSERT_TRUE(std::isfinite(output[i])) << "run " << run << " sample " << i;
        }
    }
}

} // namespace

TEST(BeatFxBeatTest, EnigmaJetRunsAtThe64BeatSettingsAt60Bpm) {
    expectFiniteOutput<BeatFxEnigmaJetEffect, BeatFxEnigmaJetState>(21); // +64 beats
    expectFiniteOutput<BeatFxEnigmaJetEffect, BeatFxEnigmaJetState>(0);  // -64 beats
}

TEST(BeatFxBeatTest, MobiusRunsAtThe64BeatSettingsAt60Bpm) {
    expectFiniteOutput<BeatFxMobiusSaw, BeatFxMobiusState>(21);
    expectFiniteOutput<BeatFxMobiusSaw, BeatFxMobiusState>(0);
    expectFiniteOutput<BeatFxMobiusTri, BeatFxMobiusState>(21);
    expectFiniteOutput<BeatFxMobiusTri, BeatFxMobiusState>(0);
}

TEST(BeatFxBeatTest, FlangerAndPhaserRunAt64BeatsAt60Bpm) {
    expectFiniteOutput<BeatFxFlanger, BeatFxSweepState>(10);
    expectFiniteOutput<BeatFxPhaser, BeatFxSweepState>(10);
}

// AI-generated (Claude Code). Start of AI-generated text.
// The phase at the buffer start: a cycle of one beat or less follows the beat
// grid; a longer cycle does not restart at each beat (it keeps the free phase).
TEST(BeatFxBeatTest, SweepPhaseFollowsTheGridOnlyForCyclesUpToOneBeat) {
    GroupFeatureState grid;
    grid.beat_length = GroupFeatureBeatLength{1.0, 1.0};
    const double sampleRate = 48000;
    const int frames = 480; // 0.01 beat
    const double free = 0.37;

    // Buffer end at 0.5 beat: the buffer start is at 0.49 beat.
    grid.beat_fraction_buffer_end = 0.5;
    EXPECT_NEAR(0.49, beatFxSweepPhase(free, grid, 1.0, sampleRate, frames), 1e-9);
    EXPECT_NEAR(0.98, beatFxSweepPhase(free, grid, 0.5, sampleRate, frames), 1e-9);
    // No grid: the free phase.
    EXPECT_EQ(free, beatFxSweepPhase(free, GroupFeatureState(), 1.0, sampleRate, frames));

    // A cycle of 4 beats: the phase is the free phase at every beat position,
    // so it does not restart when the grid position wraps to the next beat.
    for (double position : {0.005, 0.25, 0.5, 0.999}) {
        grid.beat_fraction_buffer_end = position;
        EXPECT_EQ(free, beatFxSweepPhase(free, grid, 4.0, sampleRate, frames))
                << "position " << position;
        EXPECT_EQ(free, beatFxSweepPhase(free, grid, 64.0, sampleRate, frames));
    }
}
// End of AI-generated text.
// End of AI-generated text.
