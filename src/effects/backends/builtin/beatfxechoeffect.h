#pragma once

// Mix Two: BEAT FX ECHO and LOW CUT ECHO (DDJ-800 manual p.66-67): a
// delayed sound repeats at the BEAT fraction and fades; LOW CUT ECHO
// reduces the low range of the repeats. With the fader down the tail
// keeps fading out.

#include "effects/backends/builtin/beatfxcommon.h"
#include "util/class.h"

class BeatFxEchoState : public EffectState {
  public:
    static constexpr double kMaxSeconds = 8.0; // 16 beats at 120 BPM

    explicit BeatFxEchoState(const mixxx::EngineParameters& engineParameters)
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
        hpLeft = hpRight = 0;
        lastInLeft = lastInRight = 0;
    }

    beatfx::DelayLine line;
    double delayFrames;
    CSAMPLE_GAIN prevDepth;
    CSAMPLE hpLeft, hpRight, lastInLeft, lastInRight;
};

template<bool LowCut>
class BeatFxEchoEffect : public EffectProcessorImpl<BeatFxEchoState> {
  public:
    BeatFxEchoEffect() = default;
    ~BeatFxEchoEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(BeatFxEchoState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(BeatFxEchoEffect);
};

using BeatFxEcho = BeatFxEchoEffect<false>;
using BeatFxLowCutEcho = BeatFxEchoEffect<true>;
