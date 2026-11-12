// AI-generated (Claude Code). Start of AI-generated text.
// Column sampling for the 3Band waveform: for each pixel column, the
// peak of the low, mid and high band over the stored points it covers.
// End of AI-generated text.
#pragma once

#include <QtGlobal>

struct WaveformData;

namespace mixxx::threeband {

/// Time over which the low band is held, in seconds.
inline constexpr double kLowHoldSeconds = 0.016;
/// Time over which the mid band is held, in seconds.
inline constexpr double kMidHoldSeconds = 0.004;

// AI-generated (Claude Code). Start of AI-generated text.
/// Height scale of each band, relative to the full half height.
// End of AI-generated text.
inline constexpr float kLowGain = 0.75f;
inline constexpr float kMidGain = 0.68f;
inline constexpr float kHighGain = 0.5f;

struct Peaks {
    uchar low = 0;
    uchar mid = 0;
    uchar high = 0;
};

/// Visual frames to add on each side of a column: round(rate * seconds / 2).
int holdFrames(double visualSampleRate, double seconds);

/// Largest low/mid/high over both channels of the visual frames
/// [frameStart, max(frameStop, frameStart + 1)), the low and mid range
/// made wider by their hold on each side. Frames outside
/// [0, dataSize / 2) count as zero.
Peaks peaksForColumn(const WaveformData* data,
        int dataSize,
        int frameStart,
        int frameStop,
        int lowHold,
        int midHold);

/// Brightness of the fill at rel (0 = centre line, 1 = band edge).
float coreShade(float rel);

} // namespace mixxx::threeband
