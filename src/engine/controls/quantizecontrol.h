#pragma once

#include "engine/controls/enginecontrol.h"
#include "preferences/usersettings.h"
#include "track/beats.h"
#include "track/track_decl.h"

class ControlObject;
class ControlPushButton;

// AI-generated (Claude Code). Start of AI-generated text.
// Mix Two: the point closest to `position` on a grid of `stepBeats` beats
// between the whole beats `prevBeat` and `nextBeat`. A step of 1 (or more)
// gives the closest whole beat. Pure arithmetic: safe in the engine thread.
mixxx::audio::FramePos quantizeSubdivided(mixxx::audio::FramePos prevBeat,
        mixxx::audio::FramePos nextBeat,
        mixxx::audio::FramePos position,
        double stepBeats);
// End of AI-generated text.

class QuantizeControl : public EngineControl {
    Q_OBJECT
  public:
    QuantizeControl(const QString& group, UserSettingsPointer pConfig);
    ~QuantizeControl() override;

    void setFrameInfo(mixxx::audio::FramePos currentPosition,
            mixxx::audio::FramePos trackEndPosition,
            mixxx::audio::SampleRate sampleRate) override;
    void trackLoaded(TrackPointer pNewTrack) override;
    void trackBeatsUpdated(mixxx::BeatsPointer pBeats) override;

  private slots:
    void slotQuantizeBeatsChangeRequest(double value);
    void slotQuantizeBeatsCycle(double value);

  private:
    // Update positions of previous and next beats from beatgrid.
    void lookupBeatPositions(mixxx::audio::FramePos position);
    // Update position of the closest beat based on existing previous and
    // next beat values.  Usually callers will call lookupBeatPositions first.
    void updateClosestBeat(mixxx::audio::FramePos position);
    void playPosChanged(mixxx::audio::FramePos position);

    ControlPushButton* m_pCOQuantizeEnabled;
    ControlObject* m_pCONextBeat;
    ControlObject* m_pCOPrevBeat;
    ControlObject* m_pCOClosestBeat;
    // AI-generated (Claude Code). Start of AI-generated text.
    // Mix Two: [ChannelN],quantize_beats (0.125, 0.25, 0.5 or 1) and the
    // push control that steps it 1 > 1/8 > 1/4 > 1/2 > 1.
    ControlObject* m_pCOQuantizeBeats;
    ControlPushButton* m_pCOQuantizeBeatsCycle;
    // The beat value that beat_closest was computed with (engine thread).
    double m_appliedQuantizeBeats = 1.0;
    // End of AI-generated text.

    // m_pBeats is written from an engine worker thread
    mixxx::BeatsPointer m_pBeats;
};
