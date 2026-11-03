#pragma once

// Mix Two: BEAT FX PITCH (DDJ-800 manual p.72): the BEAT buttons set the
// pitch of the effect sound between -50 and +100 %, and LEVEL/DEPTH scales
// it: all the way counterclockwise is the original pitch. Shifting is done
// by Rubber Band, as Mixxx's Pitch Shift.

#include <memory>

#include "effects/backends/builtin/beatfxcommon.h"
#include "util/class.h"

namespace RubberBand {
class RubberBandStretcher;
}

class BeatFxPitchState : public EffectState {
  public:
    explicit BeatFxPitchState(const mixxx::EngineParameters& engineParameters);
    ~BeatFxPitchState() override;
    void audioParametersChanged(const mixxx::EngineParameters& engineParameters);

    std::unique_ptr<RubberBand::RubberBandStretcher> pStretcher;
    mixxx::SampleBuffer planar[2];
};

class BeatFxPitchEffect : public EffectProcessorImpl<BeatFxPitchState> {
  public:
    BeatFxPitchEffect() = default;
    ~BeatFxPitchEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(BeatFxPitchState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(BeatFxPitchEffect);
};
