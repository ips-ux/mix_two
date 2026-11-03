#pragma once

// Mix Two: BEAT FX ROLL and SLIP ROLL (DDJ-800 manual p.72-73): when the
// effect turns on, the input is recorded for the BEAT fraction and then
// repeated. ROLL keeps that recording when the beat fraction changes and
// repeats a different length of it; SLIP ROLL records again. LEVEL/DEPTH
// is the balance between the original sound and the roll.

#include "effects/backends/builtin/beatfxcommon.h"
#include "util/class.h"

class BeatFxRollState : public EffectState {
  public:
    static constexpr double kMaxSeconds = 8.0;

    explicit BeatFxRollState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        audioParametersChanged(engineParameters);
    }
    void audioParametersChanged(const mixxx::EngineParameters& engineParameters) {
        maxFrames = static_cast<int>(kMaxSeconds * engineParameters.sampleRate());
        buffer = mixxx::SampleBuffer(maxFrames * 2);
        clear();
    }
    void clear() {
        buffer.clear();
        recorded = 0;
        position = 0;
        loopFrames = 0;
        prevDepth = 0;
    }

    mixxx::SampleBuffer buffer;
    int maxFrames;
    int recorded;   // frames recorded so far
    int position;   // playback position in the loop
    int loopFrames; // the loop length in use
    CSAMPLE_GAIN prevDepth;
};

template<bool Slip>
class BeatFxRollEffect : public EffectProcessorImpl<BeatFxRollState> {
  public:
    BeatFxRollEffect() = default;
    ~BeatFxRollEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(BeatFxRollState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(BeatFxRollEffect);
};

using BeatFxRoll = BeatFxRollEffect<false>;
using BeatFxSlipRoll = BeatFxRollEffect<true>;
