#pragma once

// Mix Two: SOUND COLOR FX SWEEP (DJM-900NXS2 manual p.15). COLOR left: a
// gate makes the sound tighter with less volume (farther left is tighter);
// right: a band-pass filter whose bandwidth narrows as the knob turns.

#include "effects/backends/builtin/colorfxcommon.h"
#include "util/class.h"

class ColorFxSweepState : public EffectState {
  public:
    explicit ColorFxSweepState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        clear();
    }
    void clear() {
        band.clear();
        envelope = 0;
        gateGain = 1;
    }

    colorfx::Biquad band;
    CSAMPLE envelope; // the gate's level follower
    CSAMPLE gateGain;
};

class ColorFxSweepEffect : public EffectProcessorImpl<ColorFxSweepState> {
  public:
    ColorFxSweepEffect() = default;
    ~ColorFxSweepEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(ColorFxSweepState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(ColorFxSweepEffect);
};
