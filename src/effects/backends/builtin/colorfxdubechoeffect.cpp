#include "effects/backends/builtin/colorfxdubechoeffect.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"
#include "util/sample.h"

namespace {
// The echo repeats every 3/8 beat (a dub "slap" that lands off the beat).
constexpr double kDelayBeats = 0.375;
constexpr CSAMPLE_GAIN kFeedback = 0.6f;
// Left: band-pass on the mids. Right: high-pass on the highs.
constexpr double kMidHz = 900.0;
constexpr double kMidQ = 0.8;
constexpr double kHighHz = 2800.0;
} // namespace

// static
QString ColorFxDubEchoEffect::getId() {
    return QStringLiteral("org.mixtwo.colorfx.dubecho");
}

// static
EffectManifestPointer ColorFxDubEchoEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    colorfx::setCommonManifest(pManifest,
            getId(),
            QStringLiteral("DUB ECHO"),
            QObject::tr("An echo repeats and fades after the sound."),
            QObject::tr("Left: echo on the mid range only. Right: echo on "
                        "the high range only. Farther from the centre is more echo."));
    return pManifest;
}

void ColorFxDubEchoEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pColor = parameters.value("color");
}

void ColorFxDubEchoEffect::processChannel(ColorFxDubEchoState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const colorfx::Turn t = colorfx::turn(m_pColor->value());
    const double sampleRate = engineParameters.sampleRate();
    const int frames = engineParameters.framesPerBuffer();
    double delayFrames = kDelayBeats * beatfx::beatSeconds(groupFeatures) * sampleRate;
    delayFrames = std::min(delayFrames, pState->line.frames() - 2.0);

    if (t.right) {
        pState->band.set(colorfx::Biquad::Type::HighPass, kHighHz, 0.8, sampleRate);
    } else {
        pState->band.set(colorfx::Biquad::Type::BandPass, kMidHz, kMidQ, sampleRate);
    }
    const auto sendTarget = static_cast<CSAMPLE_GAIN>(t.amount);
    const RampingValue<CSAMPLE_GAIN> send(pState->prevSend, sendTarget, frames);

    for (int f = 0; f < frames; ++f) {
        CSAMPLE eL, eR;
        pState->line.read(delayFrames, &eL, &eR);
        const CSAMPLE_GAIN s = send.getNth(f);
        // Only the chosen band enters the echo, scaled by the knob; the
        // repeats feed back through the same band.
        CSAMPLE inL = pInput[f * 2] * s;
        CSAMPLE inR = pInput[f * 2 + 1] * s;
        pState->band.run(&inL, &inR);
        pState->line.write(colorfx::clampSample(inL + eL * kFeedback),
                colorfx::clampSample(inR + eR * kFeedback));
        pOutput[f * 2] = colorfx::clampSample(pInput[f * 2] + eL);
        pOutput[f * 2 + 1] = colorfx::clampSample(pInput[f * 2 + 1] + eR);
    }

    if (enableState == EffectEnableState::Disabling) {
        pState->clear();
    } else {
        pState->prevSend = sendTarget;
    }
}
