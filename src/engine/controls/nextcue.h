#pragma once

#include <span>

#include "audio/frame.h"
#include "track/beats.h"

namespace mixxx {

// AI-generated (Claude Code). Start of AI-generated text.
/// Returns the number of beats (with the fraction) from `position` to the
/// first valid cue in `cueSamplePositions` that is strictly after `position`.
/// The cue positions are engine sample positions (stereo samples), as in
/// the hotcue_N_position and memory_cue_N_position controls; -1 marks an
/// empty slot. Returns -1 when no cue is after `position` or when
/// `position` is invalid.
/// Does not allocate; safe to call from the engine thread.
// End of AI-generated text.
double beatsToNextCue(const Beats& beats,
        audio::FramePos position,
        std::span<const double> cueSamplePositions);

// AI-generated (Claude Code). Start of AI-generated text.
/// Positions come from floating point arithmetic: a count of 7.9999 beats is
/// meant to be 8. A count within this many beats below a whole beat shows as
/// that whole beat. Choice: 0.01 beats (about 5 ms at 120 BPM).
constexpr double kBeatCountTolerance = 0.01;

/// The whole number of beats in `beats`, with the tolerance above. A
/// negative count (no count) gives -1.
int wholeBeatCount(double beats);

/// Returns the number of beats (with the fraction) from the first beat of
/// the grid to `position`: 0 on the first beat. Returns -1 when `position`
/// is invalid or before the first beat, or when the grid has no beat.
/// Does not allocate; safe to call from the engine thread.
double beatsFromGridStart(const Beats& beats, audio::FramePos position);
// End of AI-generated text.

} // namespace mixxx
