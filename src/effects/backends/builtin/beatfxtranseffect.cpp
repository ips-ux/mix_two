#include "effects/backends/builtin/beatfxtranseffect.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"
#include "util/sample.h"

namespace {
// At LEVEL/DEPTH MAX the sound is cut for this fraction of each period.
constexpr double kMaxCutDuty = 0.75;
// Gate edge: time constant in seconds, so the cuts do not click.
constexpr double kEdgeSeconds = 0.0015;
} // namespace

// static
QString BeatFxTransEffect::getId() {
    return QStringLiteral("org.mixtwo.beatfx.trans");
}

// static
EffectManifestPointer BeatFxTransEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    beatfx::setCommonManifest(pManifest,
            getId(),
            QStringLiteral("TRANS"),
            QObject::tr("The sound is cut at the beat fraction."));
    pManifest->setAddDryToWet(false);
    beatfx::addBeatParameter(pManifest);
    beatfx::addDepthParameter(pManifest,
            QObject::tr("Duty of the cuts, and the balance between the "
                        "original sound and the effect sound"));
    return pManifest;
}

void BeatFxTransEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeat = parameters.value("beat");
    m_pDepth = parameters.value("depth");
}

void BeatFxTransEffect::processChannel(BeatFxTransState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const double fraction = beatfx::beatFraction(m_pBeat->value());
    const double periodFrames = fraction * beatfx::beatSeconds(groupFeatures) *
            engineParameters.sampleRate();
    const int frames = engineParameters.framesPerBuffer();

    // In step with the beat grid: the phase at the buffer start follows
    // from the beat fraction at its end.
    if (groupFeatures.beat_fraction_buffer_end.has_value() &&
            groupFeatures.beat_length.has_value()) {
        const double beatFrames = groupFeatures.beat_length->seconds * engineParameters.sampleRate();
        double beatPhase = *groupFeatures.beat_fraction_buffer_end - frames / beatFrames;
        beatPhase -= std::floor(beatPhase);
        double periodPhase = (beatPhase / fraction);
        pState->phase = periodPhase - std::floor(periodPhase);
    }

    const auto depth = static_cast<CSAMPLE_GAIN>(m_pDepth->value());
    const RampingValue<CSAMPLE_GAIN> mix(pState->prevDepth, depth, frames);
    const double cutDuty = kMaxCutDuty * depth;
    const CSAMPLE edge = static_cast<CSAMPLE>(
            1.0 - std::exp(-1.0 / (kEdgeSeconds * engineParameters.sampleRate())));

    for (int f = 0; f < frames; ++f) {
        const CSAMPLE target = pState->phase < (1.0 - cutDuty) ? 1.0f : 0.0f;
        pState->gate += (target - pState->gate) * edge;
        pState->phase += 1.0 / periodFrames;
        if (pState->phase >= 1.0) {
            pState->phase -= 1.0;
        }
        const CSAMPLE_GAIN m = mix.getNth(f);
        const CSAMPLE_GAIN g = (1 - m) + m * pState->gate;
        pOutput[f * 2] = pInput[f * 2] * g;
        pOutput[f * 2 + 1] = pInput[f * 2 + 1] * g;
    }

    if (enableState == EffectEnableState::Disabling) {
        pState->clear();
    } else {
        pState->prevDepth = depth;
    }
}
