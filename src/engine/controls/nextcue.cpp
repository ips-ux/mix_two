#include "engine/controls/nextcue.h"

namespace mixxx {

namespace {

// AI-generated (Claude Code). Start of AI-generated text.
/// The fraction of a beat from the beat at `it` back to `position`: 0 when
/// `position` is on that beat, else in (-1, 0). `it` is
/// Beats::iteratorFrom(position), the first beat at or after `position`.
// End of AI-generated text.
double fractionBeforeBeat(Beats::ConstIterator it, audio::FramePos position) {
    const audio::FramePos beatPosition = *it;
    if (beatPosition <= position) {
        return 0.0;
    }
    const audio::FramePos prevBeatPosition = *(it - 1);
    const audio::FrameDiff_t beatLength = beatPosition - prevBeatPosition;
    if (beatLength <= 0) {
        return 0.0;
    }
    return -(beatPosition - position) / beatLength;
}

} // namespace

double beatsToNextCue(const Beats& beats,
        audio::FramePos position,
        std::span<const double> cueSamplePositions) {
    if (!position.isValid()) {
        return -1.0;
    }
    audio::FramePos nextCuePosition = audio::kInvalidFramePos;
    for (const double samplePosition : cueSamplePositions) {
        const auto cuePosition =
                audio::FramePos::fromEngineSamplePosMaybeInvalid(samplePosition);
        if (!cuePosition.isValid() || cuePosition <= position) {
            continue;
        }
        if (!nextCuePosition.isValid() || cuePosition < nextCuePosition) {
            nextCuePosition = cuePosition;
        }
    }
    if (!nextCuePosition.isValid()) {
        return -1.0;
    }
    const auto positionIt = beats.iteratorFrom(position);
    const auto cueIt = beats.iteratorFrom(nextCuePosition);
    const double wholeBeats = static_cast<double>(cueIt - positionIt);
    return wholeBeats + fractionBeforeBeat(cueIt, nextCuePosition) -
            fractionBeforeBeat(positionIt, position);
}

} // namespace mixxx
