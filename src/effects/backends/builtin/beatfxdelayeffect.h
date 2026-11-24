#pragma once

// AI-generated (Claude Code). Start of AI-generated text.
// Mix Two: BEAT FX DELAY and PING PONG (DJM-900NXS2 manual p.15 as the
// written specification). The DDJ-800 BEAT FX SELECT control does not have
// them; they are on the screen only.
//   DELAY: one repeat of the input, delayed by the BEAT fraction. No
//     feedback.
//   PING PONG: repeats that fade and go from left to right, one BEAT
//     fraction apart.
// With the fader down the repeats still sound.

#include "effects/backends/builtin/beatfxcommon.h"
#include "util/class.h"

class BeatFxDelayState : public EffectState {
  public:
    static constexpr double kMaxSeconds = 8.0; // 16 beats at 120 BPM

    explicit BeatFxDelayState(const mixxx::EngineParameters& engineParameters)
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

class BeatFxPingPongState : public EffectState {
  public:
    static constexpr double kMaxSeconds = 8.0; // 16 beats at 120 BPM

    explicit BeatFxPingPongState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        audioParametersChanged(engineParameters);
    }
    void audioParametersChanged(const mixxx::EngineParameters& engineParameters) {
        leftLine.resize(engineParameters, kMaxSeconds);
        rightLine.resize(engineParameters, kMaxSeconds);
        clear();
    }
    void clear() {
        leftLine.clear();
        rightLine.clear();
        delayFrames = 0;
        prevDepth = 0;
    }

    beatfx::DelayLine leftLine;
    beatfx::DelayLine rightLine;
    double delayFrames;
    CSAMPLE_GAIN prevDepth;
};

class BeatFxDelayEffect : public EffectProcessorImpl<BeatFxDelayState> {
  public:
    using StateType = BeatFxDelayState;

    BeatFxDelayEffect() = default;
    ~BeatFxDelayEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(BeatFxDelayState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(BeatFxDelayEffect);
};

class BeatFxPingPongEffect : public EffectProcessorImpl<BeatFxPingPongState> {
  public:
    using StateType = BeatFxPingPongState;

    BeatFxPingPongEffect() = default;
    ~BeatFxPingPongEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(BeatFxPingPongState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(BeatFxPingPongEffect);
};
// End of AI-generated text.
