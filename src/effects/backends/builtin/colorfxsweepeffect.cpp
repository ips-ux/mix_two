#include "effects/backends/builtin/colorfxsweepeffect.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/sample.h"

namespace {
// Gate (left): the threshold rises with the knob; below it the gain drops
// steeply, so only the hits come through. Fast attack, short release.
constexpr CSAMPLE kGateMaxThreshold = 0.35f;
constexpr double kAttackSeconds = 0.002;
constexpr double kReleaseSeconds = 0.06;
// Band-pass (right): fixed centre, Q rising as the band narrows.
constexpr double kBandHz = 1100.0;
constexpr double kMinQ = 0.7;
constexpr double kMaxQ = 12.0;
} // namespace

// static
QString ColorFxSweepEffect::getId() {
    return QStringLiteral("org.mixtwo.colorfx.sweep");
}

// static
EffectManifestPointer ColorFxSweepEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    colorfx::setCommonManifest(pManifest,
            getId(),
            QStringLiteral("SWEEP"),
            QObject::tr("Left: a gate. Right: a band-pass filter."),
            QObject::tr("Left: a gate makes the sound tighter with less "
                        "volume. Right: the band-pass filter narrows."));
    return pManifest;
}

void ColorFxSweepEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pColor = parameters.value("color");
}

void ColorFxSweepEffect::processChannel(ColorFxSweepState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    Q_UNUSED(groupFeatures);
    const colorfx::Turn t = colorfx::turn(m_pColor->value());
    const double sampleRate = engineParameters.sampleRate();
    const int frames = engineParameters.framesPerBuffer();

    if (t.amount < 0.01) {
        SampleUtil::copy(pOutput, pInput, engineParameters.samplesPerBuffer());
        pState->clear();
    } else if (t.right) {
        const double q = kMinQ * std::pow(kMaxQ / kMinQ, t.amount);
        pState->band.set(colorfx::Biquad::Type::BandPass, kBandHz, q, sampleRate);
        // A band-pass loses level as it narrows; make up some of it.
        const CSAMPLE makeup = static_cast<CSAMPLE>(1.0 + 0.5 * t.amount);
        for (int f = 0; f < frames; ++f) {
            CSAMPLE l = pInput[f * 2];
            CSAMPLE r = pInput[f * 2 + 1];
            pState->band.run(&l, &r);
            pOutput[f * 2] = colorfx::clampSample(l * makeup);
            pOutput[f * 2 + 1] = colorfx::clampSample(r * makeup);
        }
    } else {
        const CSAMPLE threshold = kGateMaxThreshold * static_cast<CSAMPLE>(t.amount);
        const CSAMPLE attack = static_cast<CSAMPLE>(1.0 - std::exp(-1.0 / (kAttackSeconds * sampleRate)));
        const CSAMPLE release = static_cast<CSAMPLE>(1.0 - std::exp(-1.0 / (kReleaseSeconds * sampleRate)));
        for (int f = 0; f < frames; ++f) {
            const CSAMPLE l = pInput[f * 2];
            const CSAMPLE r = pInput[f * 2 + 1];
            const CSAMPLE peak = std::max(std::abs(l), std::abs(r));
            pState->envelope += (peak - pState->envelope) * (peak > pState->envelope ? attack : release);
            // Below the threshold the gain falls off as the square of the
            // shortfall: tighter the farther left the knob is.
            CSAMPLE target = 1.0f;
            if (pState->envelope < threshold) {
                const CSAMPLE ratio = pState->envelope / threshold;
                target = ratio * ratio;
            }
            pState->gateGain += (target - pState->gateGain) * (target > pState->gateGain ? attack : release);
            pOutput[f * 2] = l * pState->gateGain;
            pOutput[f * 2 + 1] = r * pState->gateGain;
        }
    }

    if (enableState == EffectEnableState::Disabling) {
        pState->clear();
    }
}
