#pragma once

// Mix Two: BEAT FX MOBIUS (SAW) and MOBIUS (TRI) (DDJ-800 manual p.73): a
// sawtooth or triangle wave whose pitch seems to rise (or fall) without
// end, one octave per BEAT fraction, mixed into the sound. It sounds with
// no input too. LEVEL/DEPTH is the wave's volume. The endless rise is a
// Shepard tone: six voices an octave apart, each fading in at the bottom
// and out at the top of a six-octave span.

#include "effects/backends/builtin/beatfxcommon.h"
#include "util/class.h"

class BeatFxMobiusState : public EffectState {
  public:
    static constexpr int kVoices = 6;

    explicit BeatFxMobiusState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        clear();
    }
    void clear() {
        sweep = 0;
        prevDepth = 0;
        for (auto& p : osc) {
            p = 0;
        }
    }

    double sweep; // 0..1, position of voice 0 in the span
    CSAMPLE_GAIN prevDepth;
    std::array<double, kVoices> osc; // waveform phases, 0..1
};

template<bool Triangle>
class BeatFxMobiusEffect : public EffectProcessorImpl<BeatFxMobiusState> {
  public:
    BeatFxMobiusEffect() = default;
    ~BeatFxMobiusEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(BeatFxMobiusState* pState,
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
    EngineEffectParameterPointer m_pDescend;

    DISALLOW_COPY_AND_ASSIGN(BeatFxMobiusEffect);
};

using BeatFxMobiusSaw = BeatFxMobiusEffect<false>;
using BeatFxMobiusTri = BeatFxMobiusEffect<true>;
