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

} // namespace mixxx
