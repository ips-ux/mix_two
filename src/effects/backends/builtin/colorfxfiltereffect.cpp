#include "effects/backends/builtin/colorfxfiltereffect.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"
#include "util/sample.h"

namespace {
// FILTER: the low-pass sweeps down to, and the high-pass up to, these.
constexpr double kLpfTopHz = 18000.0;
constexpr double kLpfBottomHz = 90.0;
constexpr double kHpfBottomHz = 30.0;
constexpr double kHpfTopHz = 9000.0;
constexpr double kFilterQ = 1.1; // the DJM's resonance at its default
// High-pass side only (owner, 2026-11-02): the resonance rises as the knob
// nears the top, peaks at kHpfPeakAt of the travel, and is back to flat
// at the end, so the sweep hisses on the way up without a shriek at the top.
constexpr double kHpfPeakQ = 3.6;
constexpr double kHpfPeakAt = 0.85;
constexpr double kHpfPeakWidth = 0.22; // half-width of the hump

double highPassQ(double amount) {
    const double d = std::abs(amount - kHpfPeakAt) / kHpfPeakWidth;
    if (d >= 1.0) {
        return kFilterQ;
    }
    const double hump = 0.5 + 0.5 * std::cos(M_PI * d);
    return kFilterQ + (kHpfPeakQ - kFilterQ) * hump;
}
// NOISE: the noise's own filter sweeps between these; level at the ends.
constexpr double kNoiseLowHz = 150.0;
constexpr double kNoiseHighHz = 12000.0;
constexpr CSAMPLE_GAIN kNoiseLevel = 0.35f;
} // namespace

template<bool Noise>
QString ColorFxFilterEffect<Noise>::getId() {
    return Noise ? QStringLiteral("org.mixtwo.colorfx.noise")
                 : QStringLiteral("org.mixtwo.colorfx.filter");
}

template<bool Noise>
EffectManifestPointer ColorFxFilterEffect<Noise>::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    if (Noise) {
        colorfx::setCommonManifest(pManifest,
                getId(),
                QStringLiteral("NOISE"),
                QObject::tr("White noise through a filter is mixed into the sound."),
                QObject::tr("Left: the noise filter's cutoff goes down. "
                            "Right: it goes up. Farther from the centre is louder."));
    } else {
        colorfx::setCommonManifest(pManifest,
                getId(),
                QStringLiteral("FILTER"),
                QObject::tr("The sound passes through a filter."),
                QObject::tr("Left: the low-pass cutoff goes down. "
                            "Right: the high-pass cutoff goes up."));
    }
    return pManifest;
}

template<bool Noise>
void ColorFxFilterEffect<Noise>::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pColor = parameters.value("color");
}

template<bool Noise>
void ColorFxFilterEffect<Noise>::processChannel(ColorFxFilterState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    Q_UNUSED(groupFeatures);
    const colorfx::Turn t = colorfx::turn(m_pColor->value());
    const double sampleRate = engineParameters.sampleRate();
    const int frames = engineParameters.framesPerBuffer();

    if (Noise) {
        // Left: low-pass coming down from the top. Right: high-pass going up.
        if (t.right) {
            pState->filter.set(colorfx::Biquad::Type::HighPass,
                    colorfx::sweepHz(kNoiseLowHz, kNoiseHighHz, t.amount), 0.9, sampleRate);
        } else {
            pState->filter.set(colorfx::Biquad::Type::LowPass,
                    colorfx::sweepHz(kNoiseHighHz, kNoiseLowHz, t.amount), 0.9, sampleRate);
        }
        const CSAMPLE_GAIN gain = kNoiseLevel * static_cast<CSAMPLE_GAIN>(t.amount);
        const RampingValue<CSAMPLE_GAIN> level(pState->prevGain, gain, frames);
        std::uniform_real_distribution<float> white(-1.0f, 1.0f);
        for (int f = 0; f < frames; ++f) {
            CSAMPLE nL = white(pState->rng);
            CSAMPLE nR = white(pState->rng);
            pState->filter.run(&nL, &nR);
            const CSAMPLE_GAIN g = level.getNth(f);
            pOutput[f * 2] = colorfx::clampSample(pInput[f * 2] + nL * g);
            pOutput[f * 2 + 1] = colorfx::clampSample(pInput[f * 2 + 1] + nR * g);
        }
        pState->prevGain = gain;
    } else {
        if (t.amount < 0.01) {
            SampleUtil::copy(pOutput, pInput, engineParameters.samplesPerBuffer());
            pState->filter.clear();
        } else {
            if (t.right) {
                pState->filter.set(colorfx::Biquad::Type::HighPass,
                        colorfx::sweepHz(kHpfBottomHz, kHpfTopHz, t.amount),
                        highPassQ(t.amount),
                        sampleRate);
            } else {
                pState->filter.set(colorfx::Biquad::Type::LowPass,
                        colorfx::sweepHz(kLpfTopHz, kLpfBottomHz, t.amount), kFilterQ, sampleRate);
            }
            for (int f = 0; f < frames; ++f) {
                CSAMPLE l = pInput[f * 2];
                CSAMPLE r = pInput[f * 2 + 1];
                pState->filter.run(&l, &r);
                pOutput[f * 2] = l;
                pOutput[f * 2 + 1] = r;
            }
        }
    }

    if (enableState == EffectEnableState::Disabling) {
        pState->clear();
    }
}

template class ColorFxFilterEffect<false>;
template class ColorFxFilterEffect<true>;
