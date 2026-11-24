#pragma once

// AI-generated (Claude Code). Start of AI-generated text.
// Mix Two: BEAT FX VINYL BRAKE (DJM-900NXS2 manual p.17 as the written
// specification). The playback speed of the input sound falls steadily to
// zero in the time that the BEAT fraction sets, as a record does when you
// stop the turntable. LEVEL/DEPTH sets the speed to reach: all the way
// counterclockwise is normal speed, all the way clockwise is a full stop.
// The effect replaces the sound of the channel. The DDJ-800 BEAT FX SELECT
// control does not have it; it is on the screen only.

#include <QtGlobal>

#include "effects/backends/builtin/beatfxcommon.h"
#include "util/class.h"
#include "util/samplebuffer.h"

class BeatFxVinylBrakeState : public EffectState {
  public:
    // Choice: the record buffer holds 20 seconds for each channel.
    static constexpr double kMaxSeconds = 20.0;
    // Choice: the crossfade from the lagged sound to the live input when the
    // speed is back at 1.
    static constexpr double kCrossfadeSeconds = 0.010;
    // Choice: below this speed the level falls linearly to silence at speed
    // 0, so the stop does not click.
    static constexpr double kStopFadeSpeed = 0.05;

    explicit BeatFxVinylBrakeState(const mixxx::EngineParameters& engineParameters,
            double maxSeconds = kMaxSeconds)
            : EffectState(engineParameters),
              m_maxSeconds(maxSeconds) {
        audioParametersChanged(engineParameters);
    }
    void audioParametersChanged(const mixxx::EngineParameters& engineParameters) {
        // The buffer is sized here, never in processChannel.
        frames = qMax(8, static_cast<int>(m_maxSeconds * engineParameters.sampleRate()));
        buffer = mixxx::SampleBuffer(frames * 2);
        crossfadeFrames = qMax(1.0, kCrossfadeSeconds * engineParameters.sampleRate());
        clear();
    }
    void clear() {
        buffer.clear();
        writeIndex = 0;
        speed = 1.0;
        lag = 0.0;
        crossfade = 0.0;
    }

    mixxx::SampleBuffer buffer;
    int frames = 0;
    double crossfadeFrames = 1;
    // The frame that is written next.
    int writeIndex = 0;
    // The playback speed, 0 to 1.
    double speed = 1.0;
    // How far the read position is behind the live input, in frames.
    double lag = 0.0;
    // The share of the live input in the output while the lag goes back to 0.
    double crossfade = 0.0;

  private:
    double m_maxSeconds;
};

class BeatFxVinylBrakeEffect : public EffectProcessorImpl<BeatFxVinylBrakeState> {
  public:
    using StateType = BeatFxVinylBrakeState;

    BeatFxVinylBrakeEffect() = default;
    ~BeatFxVinylBrakeEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(BeatFxVinylBrakeState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(BeatFxVinylBrakeEffect);
};
// End of AI-generated text.
