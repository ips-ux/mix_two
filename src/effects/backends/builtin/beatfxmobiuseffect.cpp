#include "effects/backends/builtin/beatfxmobiuseffect.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"
#include "util/sample.h"

namespace {
constexpr double kLowestHz = 55.0; // bottom of the six-octave span
constexpr CSAMPLE_GAIN kLevel = 0.25f;
} // namespace

template<bool Triangle>
QString BeatFxMobiusEffect<Triangle>::getId() {
    return Triangle ? QStringLiteral("org.mixtwo.beatfx.mobiustri")
                    : QStringLiteral("org.mixtwo.beatfx.mobiussaw");
}

template<bool Triangle>
EffectManifestPointer BeatFxMobiusEffect<Triangle>::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    beatfx::setCommonManifest(pManifest,
            getId(),
            Triangle ? QStringLiteral("MOBIUS (TRI)") : QStringLiteral("MOBIUS (SAW)"),
            QObject::tr("A wave whose pitch seems to rise or fall without "
                        "end, one octave per beat fraction, mixed into the sound."));
    pManifest->setAddDryToWet(false);
    beatfx::addSignedBeatParameter(pManifest);
    beatfx::addDepthParameter(pManifest, QObject::tr("Volume of the wave"));
    return pManifest;
}

template<bool Triangle>
void BeatFxMobiusEffect<Triangle>::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeat = parameters.value("beat");
    m_pDepth = parameters.value("depth");
}

template<bool Triangle>
void BeatFxMobiusEffect<Triangle>::processChannel(BeatFxMobiusState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const double sampleRate = engineParameters.sampleRate();
    // AI-generated (Claude Code). Start of AI-generated text.
    // The magnitude of the beat value is the length of one octave; a
    // negative value makes the pitch fall. There is no buffer whose size
    // depends on it: a 64-beat octave at 60 BPM only makes the step per
    // frame very small.
    // End of AI-generated text.
    const double beats = beatfx::signedBeats(m_pBeat->value());
    const double octaveFrames = std::abs(beats) * beatfx::beatSeconds(groupFeatures) * sampleRate;
    // One octave per beat fraction = 1/kVoices of the span.
    const double sweepStep = (beats < 0 ? -1.0 : 1.0) /
            (octaveFrames * BeatFxMobiusState::kVoices);
    const auto depth = static_cast<CSAMPLE_GAIN>(m_pDepth->value());
    const RampingValue<CSAMPLE_GAIN> level(pState->prevDepth, depth, engineParameters.framesPerBuffer());

    const int frames = engineParameters.framesPerBuffer();
    for (int f = 0; f < frames; ++f) {
        CSAMPLE wave = 0;
        for (int v = 0; v < BeatFxMobiusState::kVoices; ++v) {
            double pos = pState->sweep + static_cast<double>(v) / BeatFxMobiusState::kVoices;
            pos -= std::floor(pos);
            const double hz = kLowestHz * std::pow(2.0, pos * BeatFxMobiusState::kVoices);
            double& ph = pState->osc[v];
            ph += hz / sampleRate;
            if (ph >= 1.0) {
                ph -= 1.0;
            }
            const CSAMPLE shape = Triangle
                    ? static_cast<CSAMPLE>(4.0 * std::abs(ph - 0.5) - 1.0)
                    : static_cast<CSAMPLE>(2.0 * ph - 1.0);
            // Raised-cosine window: silent at the span's ends.
            const CSAMPLE window = static_cast<CSAMPLE>(0.5 - 0.5 * std::cos(2.0 * M_PI * pos));
            wave += shape * window;
        }
        pState->sweep += sweepStep;
        pState->sweep -= std::floor(pState->sweep);
        const CSAMPLE mixed = wave * kLevel * level.getNth(f);
        pOutput[f * 2] = beatfx::clampSample(pInput[f * 2] + mixed);
        pOutput[f * 2 + 1] = beatfx::clampSample(pInput[f * 2 + 1] + mixed);
    }

    if (enableState == EffectEnableState::Disabling) {
        pState->clear();
    } else {
        pState->prevDepth = depth;
    }
}

template class BeatFxMobiusEffect<false>;
template class BeatFxMobiusEffect<true>;
