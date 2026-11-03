#pragma once

// Mix Two: SOUND COLOR FX FILTER and NOISE (DJM-900NXS2 manual p.15).
// FILTER: left lowers a low-pass cutoff, right raises a high-pass cutoff.
// NOISE: white noise through a filter is mixed in; left lowers the
// filter's cutoff, right raises it; farther from the centre is louder.

#include <random>

#include "effects/backends/builtin/colorfxcommon.h"
#include "util/class.h"

class ColorFxFilterState : public EffectState {
  public:
    explicit ColorFxFilterState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters),
              rng(0x5eed) {
        clear();
    }
    void clear() {
        filter.clear();
        prevGain = 0;
    }

    colorfx::Biquad filter;
    CSAMPLE_GAIN prevGain;
    std::minstd_rand rng;
};

template<bool Noise>
class ColorFxFilterEffect : public EffectProcessorImpl<ColorFxFilterState> {
  public:
    ColorFxFilterEffect() = default;
    ~ColorFxFilterEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(ColorFxFilterState* pState,
            const CSAMPLE* pInput,
            CSAMPLE* pOutput,
            const mixxx::EngineParameters& engineParameters,
            const EffectEnableState enableState,
            const GroupFeatureState& groupFeatures) override;

  private:
    QString debugString() const {
        return getId();
    }

    EngineEffectParameterPointer m_pColor;

    DISALLOW_COPY_AND_ASSIGN(ColorFxFilterEffect);
};

using ColorFxFilter = ColorFxFilterEffect<false>;
using ColorFxNoise = ColorFxFilterEffect<true>;
