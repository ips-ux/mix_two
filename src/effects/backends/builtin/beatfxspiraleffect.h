#pragma once

// Mix Two: BEAT FX SPIRAL (DDJ-800 manual p.69, DJM-900NXS2 p.15): a
// feedback delay that reverberates; a change of the delay time glides the
// read point, so the pitch changes with it. LEVEL/DEPTH sets feedback and
// the balance.

#include "effects/backends/builtin/beatfxcommon.h"
#include "util/class.h"

class BeatFxSpiralState : public EffectState {
  public:
    static constexpr double kMaxSeconds = 8.0;

    explicit BeatFxSpiralState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        audioParametersChanged(engineParameters);
    }
    void audioParametersChanged(const mixxx::EngineParameters& engineParameters) {
        line.resize(engineParameters, kMaxSeconds);
        clear();
    }
    void clear() {
        line.clear();
        delayFrames = 0;
        prevDepth = 0;
    }

    beatfx::DelayLine line;
    double delayFrames;
    CSAMPLE_GAIN prevDepth;
};

class BeatFxSpiralEffect : public EffectProcessorImpl<BeatFxSpiralState> {
  public:
    BeatFxSpiralEffect() = default;
    ~BeatFxSpiralEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(BeatFxSpiralState* pState,
            const CSAMPLE* pInput,
            CSAMPLE* pOutput,
            const mixxx::EngineParameters& engineParameters,
            const EffectEnableState enableState,
            const GroupFeatureState& groupFeatures) override;

  private:
    QString debugString() const {
        return getId();
    }

    EngineEffectParameterPointer m_pBeat;
    EngineEffectParameterPointer m_pDepth;

    DISALLOW_COPY_AND_ASSIGN(BeatFxSpiralEffect);
};
