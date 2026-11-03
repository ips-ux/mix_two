#pragma once

#include <QString>

#include "audio/types.h"
#include "track/track_decl.h"

namespace mixxx {
namespace rekordbox {

/// Applies rekordbox's analysis (beat grid, memory and hot cues) to a track.
///
/// `anlzPath` is the legacy ANLZ0000.DAT. When an ANLZ0000.EXT sits next to
/// it, the grid comes from the .DAT and the cues from the .EXT (which adds
/// colours and comments); otherwise both come from the .DAT.
void readAnalysisFiles(TrackPointer track,
        audio::SampleRate sampleRate,
        int timingOffset,
        const QString& anlzPath);

} // namespace rekordbox
} // namespace mixxx
