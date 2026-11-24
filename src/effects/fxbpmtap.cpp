// AI-generated (Claude Code). Start of AI-generated text.
#include "effects/fxbpmtap.h"

bool FxBpmTap::tap(std::int64_t timeMs) {
    if (m_hasLastTap) {
        const std::int64_t interval = timeMs - m_lastTapMs;
        if (interval <= 0) {
            // No time step (or a clock that went back): not a tap.
            return false;
        }
        if (interval > kMaxGapMs) {
            // A new run. The present BPM stays until the new run has enough taps.
            m_numIntervals = 0;
        } else {
            if (m_numIntervals == kMaxIntervals) {
                for (int i = 1; i < kMaxIntervals; ++i) {
                    m_intervals[i - 1] = m_intervals[i];
                }
                --m_numIntervals;
            }
            m_intervals[m_numIntervals++] = interval;
        }
    }
    m_hasLastTap = true;
    m_lastTapMs = timeMs;

    // A run of n taps has n - 1 intervals.
    if (m_numIntervals < kMinTaps - 1) {
        return false;
    }
    std::int64_t sum = 0;
    for (int i = 0; i < m_numIntervals; ++i) {
        sum += m_intervals[i];
    }
    const double averageMs = static_cast<double>(sum) / m_numIntervals;
    const double bpm = 60000.0 / averageMs;
    if (bpm < kMinBpm || bpm > kMaxBpm) {
        return false;
    }
    m_bpm = bpm;
    m_tapMode = true;
    return true;
}

void FxBpmTap::setTapMode(bool tapMode) {
    m_tapMode = tapMode && m_bpm > 0.0;
}

GroupFeatureState applyFxTapBpm(const GroupFeatureState& features, double tapBpm) {
    if (!(tapBpm > 0.0)) {
        return features;
    }
    GroupFeatureState tapped = features;
    tapped.beat_length = GroupFeatureBeatLength{60.0 / tapBpm, 1.0};
    tapped.beat_fraction_buffer_end.reset();
    return tapped;
}
// End of AI-generated text.
