#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <QList>
#include <algorithm>
#include <cstdlib>
#include <optional>

#include "track/cue.h"

namespace previewclick {

// A click on the preview skips ahead by this time (rekordbox: 30 seconds).
constexpr double kSkipAheadSeconds = 30.0;

// Choice: "near a mark" means this many pixels on each side of the mark.
constexpr int kCueToleranceDefaultPx = 6;

/// A cue mark on the overview column: the time of the cue and its colour (0xRRGGBB).
struct CueMark {
    double seconds;
    unsigned int rgb;
};

/// The position after a click during the preview: the old position plus the
/// step, but never past the end of the track and never below the start.
inline double skipAhead(double positionSeconds, double stepSeconds, double lengthSeconds) {
    return std::clamp(positionSeconds + stepSeconds, 0.0, std::max(lengthSeconds, 0.0));
}

/// The time in the track for a click at `xInCell` in a cell of `widthPx`
/// (the overview column shows the whole track over the cell width). The
/// result is between 0 and the length.
inline double previewPositionFromClick(int xInCell, int widthPx, double lengthSeconds) {
    if (widthPx <= 0 || lengthSeconds <= 0.0) {
        return 0.0;
    }
    const double fraction = std::clamp(static_cast<double>(xInCell) / widthPx, 0.0, 1.0);
    return fraction * lengthSeconds;
}

/// The x position in a cell of `widthPx` for a cue at `cueSeconds` in a track
/// of `lengthSeconds`. Gives 0 when the length is not known.
inline int cueX(double cueSeconds, double lengthSeconds, int widthPx) {
    if (lengthSeconds <= 0.0 || widthPx <= 0) {
        return 0;
    }
    const double fraction = std::clamp(cueSeconds / lengthSeconds, 0.0, 1.0);
    return std::min(static_cast<int>(fraction * widthPx), widthPx - 1);
}

/// The index of the cue that is nearest to `x` and not further away than
/// `tolerancePx`, or -1 when no cue is that near.
inline int nearestCue(int x, const QList<int>& cueXs, int tolerancePx) {
    int best = -1;
    int bestDistance = tolerancePx + 1;
    for (int i = 0; i < cueXs.size(); ++i) {
        const int distance = std::abs(cueXs[i] - x);
        if (distance < bestDistance) {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

/// The time of the cue mark that a click at `xInCell` hits, if any.
inline std::optional<double> cueNearClick(const QList<CueMark>& marks,
        int xInCell,
        int cellWidthPx,
        double lengthSeconds,
        int tolerancePx) {
    if (lengthSeconds <= 0.0 || cellWidthPx <= 0) {
        return std::nullopt;
    }
    QList<int> xs;
    xs.reserve(marks.size());
    for (const CueMark& mark : marks) {
        xs.append(cueX(mark.seconds, lengthSeconds, cellWidthPx));
    }
    const int index = nearestCue(xInCell, xs, tolerancePx);
    if (index < 0) {
        return std::nullopt;
    }
    return marks[index].seconds;
}

/// The marks of the memory cues and hot cues of a track. Other cue types (main
/// cue, intro, outro, loops and so on) have no mark. Memory cues are red, as in
/// rekordbox; a hot cue has its own colour.
inline QList<CueMark> cueMarksOf(const QList<CuePointer>& cues, double sampleRate) {
    QList<CueMark> marks;
    if (sampleRate <= 0.0) {
        return marks;
    }
    for (const CuePointer& pCue : cues) {
        if (!pCue) {
            continue;
        }
        const mixxx::CueType type = pCue->getType();
        if (type != mixxx::CueType::HotCue && type != mixxx::CueType::MemoryCue) {
            continue;
        }
        const mixxx::audio::FramePos position = pCue->getPosition();
        if (!position.isValid()) {
            continue;
        }
        const unsigned int rgb = type == mixxx::CueType::MemoryCue
                ? 0xE0202Au
                : static_cast<unsigned int>(pCue->getColor());
        marks.append({position.value() / sampleRate, rgb});
    }
    return marks;
}

} // namespace previewclick

// End of AI-generated text.
