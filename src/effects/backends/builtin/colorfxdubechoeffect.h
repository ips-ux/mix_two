#pragma once

// Mix Two: SOUND COLOR FX DUB ECHO (DJM-900NXS2 manual p.15): an echo,
// slightly delayed and fading over several repeats. COLOR left: the echo
// takes the mid range only; right: the high range only. Farther from the
// centre is more echo.

#include "effects/backends/builtin/beatfxcommon.h"
#include "effects/backends/builtin/colorfxcommon.h"
#include "util/class.h"

class ColorFxDubEchoState : public EffectState {
  public:
    explicit ColorFxDubEchoState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        audioParametersChanged(engineParameters);
    }
    void audioParametersChanged(const mixxx::EngineParameters& engineParameters) {
        line.resize(engineParameters, 2.0);
        clear();
    }
    void clear() {
        line.clear();
        band.clear();
        prevSend = 0;
    }

    beatfx::DelayLine line;
    colorfx::Biquad band;
    CSAMPLE_GAIN prevSend;
};

class ColorFxDubEchoEffect : public EffectProcessorImpl<ColorFxDubEchoState> {
  public:
    ColorFxDubEchoEffect() = default;
    ~ColorFxDubEchoEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(ColorFxDubEchoState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(ColorFxDubEchoEffect);
};
