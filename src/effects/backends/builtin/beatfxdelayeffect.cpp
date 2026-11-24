// AI-generated (Claude Code). Start of AI-generated text.
#include "effects/backends/builtin/beatfxdelayeffect.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"
#include "util/sample.h"

namespace {
// PING PONG: the repeats fade by this much at each change of side. This is
// the same number as the feedback of ECHO (beatfxechoeffect.cpp keeps its
// constant private).
constexpr CSAMPLE_GAIN kFeedback = 0.62f;

// The delay in frames for the beat value and the tempo, inside the buffer.
double targetDelayFrames(double beatParameter,
        const GroupFeatureState& groupFeatures,
        const mixxx::EngineParameters& engineParameters,
        int lineFrames) {
    const double seconds = beatfx::beatFraction(beatParameter) *
            beatfx::beatSeconds(groupFeatures);
    double frames = seconds * engineParameters.sampleRate();
    const double maxFrames = lineFrames - 2;
    if (frames > maxFrames) {
        frames = maxFrames;
    }
    return frames;
}
} // namespace

// ---- DELAY ----

QString BeatFxDelayEffect::getId() {
    return QStringLiteral("org.mixtwo.beatfx.delay");
}

EffectManifestPointer BeatFxDelayEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    beatfx::setCommonManifest(pManifest,
            getId(),
            QStringLiteral("DELAY"),
            QObject::tr("A delay sound plays one time, after the beat "
                        "fraction."));
    beatfx::addBeatParameter(pManifest);
    beatfx::addDepthParameter(pManifest,
            QObject::tr("Balance between the original sound and the delay sound"));
    return pManifest;
}

void BeatFxDelayEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeat = parameters.value("beat");
    m_pDepth = parameters.value("depth");
}

void BeatFxDelayEffect::processChannel(BeatFxDelayState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const double targetFrames = targetDelayFrames(
            m_pBeat->value(), groupFeatures, engineParameters, pState->line.frames());
    if (pState->delayFrames <= 0) {
        pState->delayFrames = targetFrames;
    }
    const auto depth = static_cast<CSAMPLE_GAIN>(m_pDepth->value());
    // The same gain law as ECHO: the output is the repeat times DEPTH. The
    // engine adds the dry sound.
    const RampingValue<CSAMPLE_GAIN> wet(pState->prevDepth, depth, engineParameters.framesPerBuffer());
    // A changed beat fraction moves the read point over one buffer, so the
    // repeat does not click.
    const RampingValue<double> delay(pState->delayFrames, targetFrames, engineParameters.framesPerBuffer());

    const int frames = engineParameters.framesPerBuffer();
    for (int f = 0; f < frames; ++f) {
        CSAMPLE repeatL, repeatR;
        pState->line.read(delay.getNth(f), &repeatL, &repeatR);
        // No feedback: the line holds the input only, so there is one repeat.
        pState->line.write(pInput[f * 2], pInput[f * 2 + 1]);
        const CSAMPLE_GAIN w = wet.getNth(f);
        pOutput[f * 2] = repeatL * w;
        pOutput[f * 2 + 1] = repeatR * w;
    }

    if (enableState == EffectEnableState::Disabling) {
        SampleUtil::applyRampingGain(pOutput, 1.0, 0.0, engineParameters.samplesPerBuffer());
        pState->clear();
    } else {
        pState->prevDepth = depth;
        pState->delayFrames = targetFrames;
    }
}

// ---- PING PONG ----

QString BeatFxPingPongEffect::getId() {
    return QStringLiteral("org.mixtwo.beatfx.pingpong");
}

EffectManifestPointer BeatFxPingPongEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    beatfx::setCommonManifest(pManifest,
            getId(),
            QStringLiteral("PING PONG"),
            QObject::tr("A delay sound repeats and fades. It goes from left "
                        "to right at the beat fraction."));
    beatfx::addBeatParameter(pManifest);
    beatfx::addDepthParameter(pManifest,
            QObject::tr("Balance between the original sound and the echo"));
    return pManifest;
}

void BeatFxPingPongEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeat = parameters.value("beat");
    m_pDepth = parameters.value("depth");
}

void BeatFxPingPongEffect::processChannel(BeatFxPingPongState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const double targetFrames = targetDelayFrames(
            m_pBeat->value(), groupFeatures, engineParameters, pState->leftLine.frames());
    if (pState->delayFrames <= 0) {
        pState->delayFrames = targetFrames;
    }
    const auto depth = static_cast<CSAMPLE_GAIN>(m_pDepth->value());
    const RampingValue<CSAMPLE_GAIN> wet(pState->prevDepth, depth, engineParameters.framesPerBuffer());
    const RampingValue<double> delay(pState->delayFrames, targetFrames, engineParameters.framesPerBuffer());

    const int frames = engineParameters.framesPerBuffer();
    for (int f = 0; f < frames; ++f) {
        // Both lines are one beat long. The echoes come out left at 1, right
        // at 2, left at 3 beat times, each one quieter.
        CSAMPLE leftOut, unusedL, rightOut, unusedR;
        pState->leftLine.read(delay.getNth(f), &leftOut, &unusedL);
        pState->rightLine.read(delay.getNth(f), &rightOut, &unusedR);
        // The mono sum of the input goes into the left line, with the
        // right line's output fed back.
        const CSAMPLE mono = 0.5f * (pInput[f * 2] + pInput[f * 2 + 1]);
        const CSAMPLE intoLeft = beatfx::clampSample(mono + rightOut * kFeedback);
        const CSAMPLE intoRight = beatfx::clampSample(leftOut * kFeedback);
        pState->leftLine.write(intoLeft, intoLeft);
        pState->rightLine.write(intoRight, intoRight);
        const CSAMPLE_GAIN w = wet.getNth(f);
        pOutput[f * 2] = leftOut * w;
        pOutput[f * 2 + 1] = rightOut * w;
    }

    if (enableState == EffectEnableState::Disabling) {
        SampleUtil::applyRampingGain(pOutput, 1.0, 0.0, engineParameters.samplesPerBuffer());
        pState->clear();
    } else {
        pState->prevDepth = depth;
        pState->delayFrames = targetFrames;
    }
}
// End of AI-generated text.
