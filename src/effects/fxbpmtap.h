#pragma once
// AI-generated (Claude Code). Start of AI-generated text.
// The tapped tempo of a Mix Two effect unit (DDJ-800 manual p.64-66): the
// BPM is the average of the intervals between the taps of the present run.
// The class has no engine or Qt event dependency. A test drives it with
// times in milliseconds.

#include <array>
#include <cstdint>

#include "engine/effects/groupfeaturestate.h"

class FxBpmTap {
  public:
    // The manual says "more than two times", so a run needs 3 taps.
    static constexpr int kMinTaps = 3;
    // The values below are the choice of the controller, not of the manual.
    // A longer gap between two taps starts a new run (milliseconds).
    static constexpr std::int64_t kMaxGapMs = 2000;
    // Only the last intervals count in the average.
    static constexpr int kMaxIntervals = 8;
    // A BPM outside this range is ignored.
    static constexpr double kMinBpm = 40.0;
    static constexpr double kMaxBpm = 300.0;

    // Adds a tap at the given time (milliseconds, any steady clock).
    // Returns true when the BPM was updated. A successful tap sets TAP mode.
    bool tap(std::int64_t timeMs);

    // The tapped BPM, or 0 when there is none.
    double bpm() const {
        return m_bpm;
    }

    bool isTapMode() const {
        return m_tapMode;
    }

    // Setting TAP mode with no tapped BPM has no effect (the mode stays
    // AUTO). Setting AUTO keeps the tapped BPM.
    void setTapMode(bool tapMode);

  private:
    std::array<std::int64_t, kMaxIntervals> m_intervals{};
    int m_numIntervals = 0;
    bool m_hasLastTap = false;
    std::int64_t m_lastTapMs = 0;
    double m_bpm = 0.0;
    bool m_tapMode = false;
};

// Returns the group features that the effects of a unit in TAP mode get: the
// beat length is the tapped beat time and there is no grid position, so the
// effects run free. A tapped BPM of 0 or less (AUTO) returns the state
// unchanged. No allocation, so the audio thread can call it.
GroupFeatureState applyFxTapBpm(const GroupFeatureState& features, double tapBpm);
// End of AI-generated text.
