#include "effects/backends/builtin/beatfxechoeffect.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"
#include "util/sample.h"

namespace {
// The repeats fade by this much each time (DDJ-800: "gradually attenuated").
constexpr CSAMPLE_GAIN kFeedback = 0.62f;
// LOW CUT ECHO: one-pole high-pass on the repeats, cutoff in Hz.
constexpr double kLowCutHz = 350.0;
} // namespace

template<bool LowCut>
QString BeatFxEchoEffect<LowCut>::getId() {
    return LowCut ? QStringLiteral("org.mixtwo.beatfx.lowcutecho")
                  : QStringLiteral("org.mixtwo.beatfx.echo");
}

template<bool LowCut>
EffectManifestPointer BeatFxEchoEffect<LowCut>::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    beatfx::setCommonManifest(pManifest,
            getId(),
            LowCut ? QStringLiteral("LOW CUT ECHO") : QStringLiteral("ECHO"),
            LowCut ? QObject::tr("A delayed sound with the low range reduced "
                                 "repeats at the beat fraction and fades.")
                   : QObject::tr("A delayed sound repeats at the beat "
                                 "fraction and fades."));
    beatfx::addBeatParameter(pManifest);
    beatfx::addDepthParameter(pManifest,
            QObject::tr("Balance between the original sound and the echo"));
    return pManifest;
}

template<bool LowCut>
void BeatFxEchoEffect<LowCut>::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeat = parameters.value("beat");
    m_pDepth = parameters.value("depth");
}

template<bool LowCut>
void BeatFxEchoEffect<LowCut>::processChannel(BeatFxEchoState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const double seconds = beatfx::beatFraction(m_pBeat->value()) * beatfx::beatSeconds(groupFeatures);
    double targetFrames = seconds * engineParameters.sampleRate();
    const double maxFrames = pState->line.frames() - 2;
    if (targetFrames > maxFrames) {
        targetFrames = maxFrames;
    }
    if (pState->delayFrames <= 0) {
        pState->delayFrames = targetFrames;
    }
    const auto depth = static_cast<CSAMPLE_GAIN>(m_pDepth->value());
    const RampingValue<CSAMPLE_GAIN> wet(pState->prevDepth, depth, engineParameters.framesPerBuffer());
    // A changed beat fraction moves the read point over one buffer, so the
    // repeats do not click.
    const RampingValue<double> delay(pState->delayFrames, targetFrames, engineParameters.framesPerBuffer());

    const CSAMPLE hpCoef = static_cast<CSAMPLE>(
            std::exp(-2.0 * M_PI * kLowCutHz / engineParameters.sampleRate()));

    const int frames = engineParameters.framesPerBuffer();
    for (int f = 0; f < frames; ++f) {
        CSAMPLE echoL, echoR;
        pState->line.read(delay.getNth(f), &echoL, &echoR);
        if (LowCut) {
            // y[n] = a * (y[n-1] + x[n] - x[n-1])
            const CSAMPLE outL = hpCoef * (pState->hpLeft + echoL - pState->lastInLeft);
            const CSAMPLE outR = hpCoef * (pState->hpRight + echoR - pState->lastInRight);
            pState->lastInLeft = echoL;
            pState->lastInRight = echoR;
            pState->hpLeft = outL;
            pState->hpRight = outR;
            echoL = outL;
            echoR = outR;
        }
        const CSAMPLE inL = pInput[f * 2];
        const CSAMPLE inR = pInput[f * 2 + 1];
        pState->line.write(beatfx::clampSample(inL + echoL * kFeedback),
                beatfx::clampSample(inR + echoR * kFeedback));
        const CSAMPLE_GAIN w = wet.getNth(f);
        pOutput[f * 2] = echoL * w;
        pOutput[f * 2 + 1] = echoR * w;
    }

    if (enableState == EffectEnableState::Disabling) {
        SampleUtil::applyRampingGain(pOutput, 1.0, 0.0, engineParameters.samplesPerBuffer());
        pState->clear();
    } else {
        pState->prevDepth = depth;
        pState->delayFrames = targetFrames;
    }
}

template class BeatFxEchoEffect<false>;
template class BeatFxEchoEffect<true>;
