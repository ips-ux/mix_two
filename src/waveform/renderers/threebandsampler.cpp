#include "waveform/renderers/threebandsampler.h"

#include <algorithm>
#include <cmath>

#include "waveform/waveform.h"

namespace mixxx::threeband {

int holdFrames(double visualSampleRate, double seconds) {
    return static_cast<int>(std::lround(visualSampleRate * seconds / 2.0));
}

Peaks peaksForColumn(const WaveformData* data,
        int dataSize,
        int frameStart,
        int frameStop,
        int lowHold,
        int midHold) {
    Peaks peaks;
    const int frameCount = dataSize / 2;
    if (!data || frameCount <= 0) {
        return peaks;
    }
    frameStop = std::max(frameStop, frameStart + 1);

    // Scans the frames [from, to) of the track, both channels, and raises
    // the given peak. Frames outside the track count as zero.
    auto scan = [&](int from, int to, uchar WaveformFilteredData::*band, uchar& peak) {
        from = std::max(from, 0);
        to = std::min(to, frameCount);
        for (int frame = from; frame < to; ++frame) {
            peak = std::max(peak, data[2 * frame].filtered.*band);
            peak = std::max(peak, data[2 * frame + 1].filtered.*band);
        }
    };

    scan(frameStart - std::max(lowHold, 0),
            frameStop + std::max(lowHold, 0),
            &WaveformFilteredData::low,
            peaks.low);
    scan(frameStart - std::max(midHold, 0),
            frameStop + std::max(midHold, 0),
            &WaveformFilteredData::mid,
            peaks.mid);
    scan(frameStart, frameStop, &WaveformFilteredData::high, peaks.high);
    return peaks;
}

float coreShade(float rel) {
    rel = std::clamp(rel, 0.0f, 1.0f);
    return 1.0f - 0.55f * rel * rel;
}

} // namespace mixxx::threeband
