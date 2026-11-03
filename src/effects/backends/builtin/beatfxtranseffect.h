#pragma once

// Mix Two: BEAT FX TRANS (DDJ-800 manual p.70, DJM-900NXS2 p.16): the sound
// is cut at the BEAT fraction, in step with the beat grid when there is
// one. LEVEL/DEPTH sets the duty (how long each cut lasts) and the balance.

#include "effects/backends/builtin/beatfxcommon.h"
#include "util/class.h"

class BeatFxTransState : public EffectState {
  public:
    explicit BeatFxTransState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        clear();
    }
    void clear() {
        phase = 0;
        gate = 1;
        prevDepth = 0;
    }

    double phase;   // 0..1 within the cut period
    CSAMPLE gate;   // smoothed gate, 0..1
    CSAMPLE_GAIN prevDepth;
};

class BeatFxTransEffect : public EffectProcessorImpl<BeatFxTransState> {
  public:
    BeatFxTransEffect() = default;
    ~BeatFxTransEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(BeatFxTransState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(BeatFxTransEffect);
};
