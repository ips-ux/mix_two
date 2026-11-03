#include "effects/backends/builtin/beatfxrolleffect.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"
#include "util/sample.h"

namespace {
// Short fade at the loop seam so the repeats do not click.
constexpr int kSeamFrames = 64;
} // namespace

template<bool Slip>
QString BeatFxRollEffect<Slip>::getId() {
    return Slip ? QStringLiteral("org.mixtwo.beatfx.sliproll")
                : QStringLiteral("org.mixtwo.beatfx.roll");
}

template<bool Slip>
EffectManifestPointer BeatFxRollEffect<Slip>::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    beatfx::setCommonManifest(pManifest,
            getId(),
            Slip ? QStringLiteral("SLIP ROLL") : QStringLiteral("ROLL"),
            Slip ? QObject::tr("Records the sound when the effect turns on and "
                               "repeats it at the beat fraction. A new beat "
                               "fraction records again.")
                 : QObject::tr("Records the sound when the effect turns on and "
                               "repeats it at the beat fraction."));
    pManifest->setAddDryToWet(false);
    beatfx::addBeatParameter(pManifest);
    beatfx::addDepthParameter(pManifest,
            QObject::tr("Balance between the original sound and the roll"));
    return pManifest;
}

template<bool Slip>
void BeatFxRollEffect<Slip>::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeat = parameters.value("beat");
    m_pDepth = parameters.value("depth");
}

template<bool Slip>
void BeatFxRollEffect<Slip>::processChannel(BeatFxRollState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const double seconds = beatfx::beatFraction(m_pBeat->value()) * beatfx::beatSeconds(groupFeatures);
    int wantFrames = static_cast<int>(seconds * engineParameters.sampleRate());
    if (wantFrames > pState->maxFrames) {
        wantFrames = pState->maxFrames;
    }
    if (wantFrames < kSeamFrames * 2) {
        wantFrames = kSeamFrames * 2;
    }

    if (enableState == EffectEnableState::Enabling) {
        pState->clear();
    }
    if (pState->loopFrames == 0) {
        pState->loopFrames = wantFrames;
    } else if (wantFrames != pState->loopFrames) {
        if (Slip) {
            // SLIP ROLL: a new beat fraction records again.
            pState->recorded = 0;
            pState->position = 0;
        } else if (pState->position >= wantFrames) {
            pState->position = 0;
        }
        pState->loopFrames = wantFrames;
    }

    const auto depth = static_cast<CSAMPLE_GAIN>(m_pDepth->value());
    const RampingValue<CSAMPLE_GAIN> mix(pState->prevDepth, depth, engineParameters.framesPerBuffer());
    const int frames = engineParameters.framesPerBuffer();
    for (int f = 0; f < frames; ++f) {
        const CSAMPLE inL = pInput[f * 2];
        const CSAMPLE inR = pInput[f * 2 + 1];
        CSAMPLE rollL, rollR;
        if (pState->recorded < pState->maxFrames) {
            // Still recording (ROLL keeps recording past the loop length so
            // a longer beat fraction has sound to repeat).
            pState->buffer[pState->recorded * 2] = inL;
            pState->buffer[pState->recorded * 2 + 1] = inR;
            ++pState->recorded;
        }
        const int loop = std::min(pState->loopFrames, std::max(pState->recorded, 1));
        if (pState->recorded < pState->loopFrames) {
            // The first pass through plays the input while it is recorded.
            rollL = inL;
            rollR = inR;
        } else {
            if (pState->position >= loop) {
                pState->position = 0;
            }
            const int p = pState->position;
            rollL = pState->buffer[p * 2];
            rollR = pState->buffer[p * 2 + 1];
            // Fade the seam: the end of the loop crossfades into its start.
            if (p >= loop - kSeamFrames) {
                const CSAMPLE t = static_cast<CSAMPLE>(loop - p) / kSeamFrames;
                const int q = p - (loop - kSeamFrames);
                rollL = rollL * t + pState->buffer[q * 2] * (1 - t);
                rollR = rollR * t + pState->buffer[q * 2 + 1] * (1 - t);
            }
            ++pState->position;
        }
        const CSAMPLE_GAIN m = mix.getNth(f);
        pOutput[f * 2] = inL * (1 - m) + rollL * m;
        pOutput[f * 2 + 1] = inR * (1 - m) + rollR * m;
    }

    if (enableState == EffectEnableState::Disabling) {
        pState->clear();
    } else {
        pState->prevDepth = depth;
    }
}

template class BeatFxRollEffect<false>;
template class BeatFxRollEffect<true>;
