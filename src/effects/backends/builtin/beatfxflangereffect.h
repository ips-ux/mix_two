#pragma once

// Mix Two: BEAT FX FLANGER and PHASER (DDJ-800 manual p.71): one sweep
// cycle per BEAT fraction, in step with the beat grid. LEVEL/DEPTH: all the
// way counterclockwise is the original sound only; clockwise stresses the
// effect. The sweep phase is shared code; FLANGER sweeps a short delay,
// PHASER a chain of all-pass stages.

#include "effects/backends/builtin/beatfxcommon.h"
#include "util/class.h"

class BeatFxSweepState : public EffectState {
  public:
    static constexpr int kPhaserStages = 6;

    explicit BeatFxSweepState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        audioParametersChanged(engineParameters);
    }
    void audioParametersChanged(const mixxx::EngineParameters& engineParameters) {
        line.resize(engineParameters, 0.05);
        clear();
    }
    void clear() {
        line.clear();
        phase = 0;
        prevDepth = 0;
        feedbackL = feedbackR = 0;
        for (auto& s : apL) {
            s = 0;
        }
        for (auto& s : apR) {
            s = 0;
        }
    }

    beatfx::DelayLine line;
    double phase; // 0..1 within the sweep cycle
    CSAMPLE_GAIN prevDepth;
    CSAMPLE feedbackL, feedbackR;
    std::array<CSAMPLE, kPhaserStages> apL, apR;
};

template<bool Phaser>
class BeatFxSweepEffect : public EffectProcessorImpl<BeatFxSweepState> {
  public:
    BeatFxSweepEffect() = default;
    ~BeatFxSweepEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(BeatFxSweepState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(BeatFxSweepEffect);
};

using BeatFxFlanger = BeatFxSweepEffect<false>;
using BeatFxPhaser = BeatFxSweepEffect<true>;
