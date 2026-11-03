#pragma once

#include <QString>

namespace mixxx {

/// Mix Two: the one place that calls mp3guessenc_timing_shift_case() of the
/// mp3guessenc library. The function gives its result (the EXIT_CODE_CASE_*
/// values of <mp3guessenc.h>) for the MP3 file at `filePath`.
///
/// mp3guessenc keeps the state of a call in global variables of the library
/// (for example `musicmatch_tag` in mp3guessenc.c), so two calls at the same
/// time give wrong results. A lock in this function lets only one call at a
/// time run. The callers are rekordboxTimingOffsetMs() (the rekordbox reader on
/// the main thread, the Device Library Plus export on its worker thread) and
/// the Serato tag import (track/serato/tags.cpp, CoreAudio builds). Any thread
/// can call it. Never call mp3guessenc_timing_shift_case() directly.
int mp3guessencTimingShiftCase(const QString& filePath);

} // namespace mixxx
