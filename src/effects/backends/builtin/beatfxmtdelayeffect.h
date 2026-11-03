#pragma once

// Mix Two: BEAT FX MULTI TAP DELAY (DDJ-800 manual p.68): up to 7 delayed
// copies at 1/8 of the BEAT fraction apart. LEVEL/DEPTH from MIN to centre
// raises the odd taps, from centre to MAX the even taps.

#include "effects/backends/builtin/beatfxcommon.h"
#include "util/class.h"

class BeatFxMtDelayState : public EffectState {
  public:
    static constexpr double kMaxSeconds = 8.0;

    explicit BeatFxMtDelayState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        audioParametersChanged(engineParameters);
    }
    void audioParametersChanged(const mixxx::EngineParameters& engineParameters) {
        line.resize(engineParameters, kMaxSeconds);
        clear();
    }
    void clear() {
        line.clear();
        prevOdd = prevEven = 0;
    }

    beatfx::DelayLine line;
    CSAMPLE_GAIN prevOdd, prevEven;
};

class BeatFxMtDelayEffect : public EffectProcessorImpl<BeatFxMtDelayState> {
  public:
    BeatFxMtDelayEffect() = default;
    ~BeatFxMtDelayEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(BeatFxMtDelayState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(BeatFxMtDelayEffect);
};
