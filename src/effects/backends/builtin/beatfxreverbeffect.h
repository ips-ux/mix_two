#pragma once

// Mix Two: BEAT FX REVERB (DDJ-800 manual p.69, DJM-900NXS2 p.16): the
// BEAT buttons set the extent of the reverberation (1 to 100 %),
// LEVEL/DEPTH the balance. The reverberator is Mixxx's plate (CAPS, GPL).

#include <Reverb.h>

#include "effects/backends/builtin/beatfxcommon.h"
#include "util/class.h"

class BeatFxReverbState : public EffectState {
  public:
    explicit BeatFxReverbState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters),
              sampleRate(engineParameters.sampleRate()),
              sendPrevious(0) {
        reverb.init(sampleRate);
    }

    float sampleRate;
    float sendPrevious;
    MixxxPlateX2 reverb;
};

class BeatFxReverbEffect : public EffectProcessorImpl<BeatFxReverbState> {
  public:
    BeatFxReverbEffect() = default;
    ~BeatFxReverbEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(BeatFxReverbState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(BeatFxReverbEffect);
};
