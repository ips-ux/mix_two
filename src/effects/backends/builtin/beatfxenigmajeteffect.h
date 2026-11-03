#pragma once

// Mix Two: BEAT FX ENIGMA JET (DDJ-800 manual p.70): a flanger whose pitch
// seems to rise (or fall) without end, one sweep per BEAT fraction. Two
// delay voices sweep half a cycle apart, each silent at the ends of its
// sweep, so the ear hears one endless rise. LEVEL/DEPTH: counterclockwise
// is the original sound only; clockwise stresses the effect.

#include "effects/backends/builtin/beatfxcommon.h"
#include "util/class.h"

class BeatFxEnigmaJetState : public EffectState {
  public:
    explicit BeatFxEnigmaJetState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        audioParametersChanged(engineParameters);
    }
    void audioParametersChanged(const mixxx::EngineParameters& engineParameters) {
        line.resize(engineParameters, 0.05);
        clear();
    }
    void clear() {
        line.clear();
        sweep = 0;
        prevDepth = 0;
        feedbackL = feedbackR = 0;
    }

    beatfx::DelayLine line;
    double sweep; // 0..1 of voice 0
    CSAMPLE_GAIN prevDepth;
    CSAMPLE feedbackL, feedbackR;
};

class BeatFxEnigmaJetEffect : public EffectProcessorImpl<BeatFxEnigmaJetState> {
  public:
    BeatFxEnigmaJetEffect() = default;
    ~BeatFxEnigmaJetEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(BeatFxEnigmaJetState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(BeatFxEnigmaJetEffect);
};
