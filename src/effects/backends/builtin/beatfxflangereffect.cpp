#include "effects/backends/builtin/beatfxflangereffect.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"
#include "util/sample.h"

namespace {
constexpr double kFlangerMinMs = 0.4;
constexpr double kFlangerMaxMs = 7.0;
constexpr CSAMPLE_GAIN kFlangerFeedback = 0.55f;
constexpr double kPhaserMinHz = 180.0;
constexpr double kPhaserMaxHz = 3200.0;
constexpr CSAMPLE_GAIN kPhaserFeedback = 0.4f;

// Sweep phase at the buffer start, in step with the beat grid when there
// is one (as TRANS); otherwise the free-running phase.
double sweepPhase(const BeatFxSweepState* pState,
        const GroupFeatureState& groupFeatures,
        double fraction,
        double sampleRate,
        int frames) {
    if (groupFeatures.beat_fraction_buffer_end.has_value() &&
            groupFeatures.beat_length.has_value()) {
        const double beatFrames = groupFeatures.beat_length->seconds * sampleRate;
        double beatPhase = *groupFeatures.beat_fraction_buffer_end - frames / beatFrames;
        beatPhase -= std::floor(beatPhase);
        const double p = beatPhase / fraction;
        return p - std::floor(p);
    }
    return pState->phase;
}
} // namespace

template<bool Phaser>
QString BeatFxSweepEffect<Phaser>::getId() {
    return Phaser ? QStringLiteral("org.mixtwo.beatfx.phaser")
                  : QStringLiteral("org.mixtwo.beatfx.flanger");
}

template<bool Phaser>
EffectManifestPointer BeatFxSweepEffect<Phaser>::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    beatfx::setCommonManifest(pManifest,
            getId(),
            Phaser ? QStringLiteral("PHASER") : QStringLiteral("FLANGER"),
            Phaser ? QObject::tr("A phaser sweeps once per beat fraction.")
                   : QObject::tr("A flanger sweeps once per beat fraction."));
    pManifest->setAddDryToWet(false);
    beatfx::addBeatParameter(pManifest);
    beatfx::addDepthParameter(pManifest,
            QObject::tr("Counterclockwise: the original sound only. "
                        "Clockwise: more effect."));
    return pManifest;
}

template<bool Phaser>
void BeatFxSweepEffect<Phaser>::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeat = parameters.value("beat");
    m_pDepth = parameters.value("depth");
}

template<bool Phaser>
void BeatFxSweepEffect<Phaser>::processChannel(BeatFxSweepState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const double fraction = beatfx::beatFraction(m_pBeat->value());
    const double sampleRate = engineParameters.sampleRate();
    const double cycleFrames = fraction * beatfx::beatSeconds(groupFeatures) * sampleRate;
    const int frames = engineParameters.framesPerBuffer();
    double phase = sweepPhase(pState, groupFeatures, fraction, sampleRate, frames);

    const auto depth = static_cast<CSAMPLE_GAIN>(m_pDepth->value());
    const RampingValue<CSAMPLE_GAIN> mix(pState->prevDepth, depth, frames);

    for (int f = 0; f < frames; ++f) {
        // One triangle cycle per beat fraction, 0..1..0.
        const double tri = 1.0 - std::abs(2.0 * phase - 1.0);
        phase += 1.0 / cycleFrames;
        if (phase >= 1.0) {
            phase -= 1.0;
        }
        const CSAMPLE inL = pInput[f * 2];
        const CSAMPLE inR = pInput[f * 2 + 1];
        CSAMPLE wetL, wetR;
        if (Phaser) {
            // Six first-order all-pass stages, corner swept on a log scale.
            const double hz = kPhaserMinHz * std::pow(kPhaserMaxHz / kPhaserMinHz, tri);
            const double t = std::tan(M_PI * hz / sampleRate);
            const CSAMPLE a = static_cast<CSAMPLE>((t - 1.0) / (t + 1.0));
            CSAMPLE xL = inL + pState->feedbackL * kPhaserFeedback;
            CSAMPLE xR = inR + pState->feedbackR * kPhaserFeedback;
            for (int s = 0; s < BeatFxSweepState::kPhaserStages; ++s) {
                const CSAMPLE yL = a * xL + pState->apL[s];
                pState->apL[s] = xL - a * yL;
                xL = yL;
                const CSAMPLE yR = a * xR + pState->apR[s];
                pState->apR[s] = xR - a * yR;
                xR = yR;
            }
            pState->feedbackL = xL;
            pState->feedbackR = xR;
            wetL = 0.5f * (inL + xL);
            wetR = 0.5f * (inR + xR);
        } else {
            const double ms = kFlangerMinMs + (kFlangerMaxMs - kFlangerMinMs) * tri;
            CSAMPLE dL, dR;
            pState->line.read(ms * 0.001 * sampleRate, &dL, &dR);
            pState->line.write(beatfx::clampSample(inL + dL * kFlangerFeedback),
                    beatfx::clampSample(inR + dR * kFlangerFeedback));
            wetL = 0.5f * (inL + dL);
            wetR = 0.5f * (inR + dR);
        }
        const CSAMPLE_GAIN m = mix.getNth(f);
        pOutput[f * 2] = inL * (1 - m) + wetL * m;
        pOutput[f * 2 + 1] = inR * (1 - m) + wetR * m;
    }
    pState->phase = phase;

    if (enableState == EffectEnableState::Disabling) {
        pState->clear();
    } else {
        pState->prevDepth = depth;
    }
}

template class BeatFxSweepEffect<false>;
template class BeatFxSweepEffect<true>;
