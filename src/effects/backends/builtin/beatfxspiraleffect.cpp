#include "effects/backends/builtin/beatfxspiraleffect.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"
#include "util/sample.h"

namespace {
// The read point moves towards a new delay time at this fraction of the
// distance per frame: slow enough that the pitch glide is audible, as on
// the DJM, where SPIRAL's pitch changes with the delay time.
constexpr double kGlide = 0.00025;
constexpr CSAMPLE_GAIN kMinFeedback = 0.35f;
constexpr CSAMPLE_GAIN kMaxFeedback = 0.92f;
} // namespace

// static
QString BeatFxSpiralEffect::getId() {
    return QStringLiteral("org.mixtwo.beatfx.spiral");
}

// static
EffectManifestPointer BeatFxSpiralEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    beatfx::setCommonManifest(pManifest,
            getId(),
            QStringLiteral("SPIRAL"),
            QObject::tr("A reverberating delay. The pitch changes when the "
                        "delay time changes."));
    beatfx::addBeatParameter(pManifest);
    beatfx::addDepthParameter(pManifest,
            QObject::tr("Feedback, and the balance between the original "
                        "sound and the effect sound"));
    return pManifest;
}

void BeatFxSpiralEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeat = parameters.value("beat");
    m_pDepth = parameters.value("depth");
}

void BeatFxSpiralEffect::processChannel(BeatFxSpiralState* pState,
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
    const CSAMPLE_GAIN feedback = kMinFeedback + (kMaxFeedback - kMinFeedback) * depth;
    const RampingValue<CSAMPLE_GAIN> wet(pState->prevDepth, depth, engineParameters.framesPerBuffer());

    double delayFrames = pState->delayFrames;
    const int frames = engineParameters.framesPerBuffer();
    for (int f = 0; f < frames; ++f) {
        delayFrames += (targetFrames - delayFrames) * kGlide;
        CSAMPLE l, r;
        pState->line.read(delayFrames, &l, &r);
        pState->line.write(beatfx::clampSample(pInput[f * 2] + l * feedback),
                beatfx::clampSample(pInput[f * 2 + 1] + r * feedback));
        const CSAMPLE_GAIN w = wet.getNth(f);
        pOutput[f * 2] = l * w;
        pOutput[f * 2 + 1] = r * w;
    }

    if (enableState == EffectEnableState::Disabling) {
        SampleUtil::applyRampingGain(pOutput, 1.0, 0.0, engineParameters.samplesPerBuffer());
        pState->clear();
    } else {
        pState->prevDepth = depth;
        pState->delayFrames = delayFrames;
    }
}
