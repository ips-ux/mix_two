#pragma once

#include <QString>
#include <QVector>

#include "audio/frame.h"
#include "audio/types.h"
#include "track/beats.h"

namespace mixxx {
namespace rekordbox {

/// One beat of a rekordbox beat grid (ANLZ PQTZ entry).
struct GridBeat {
    audio::FramePos position;
    /// The tempo rekordbox shows from this beat on, in BPM x 100.
    int tempo;
};

/// Mix Two: builds a beat grid from rekordbox's beats.
///
/// rekordbox stores each beat's time in whole milliseconds next to the tempo
/// it displays. Building the grid from the rounded times alone makes nearly
/// every beat its own tempo segment, and the BPM flickers by a few hundredths
/// (131.98 / 132.01) as the track plays. Like rekordbox, this keeps one
/// constant-tempo segment per run of beats with the same stored tempo, timed
/// by the run's first beat and the next run's first beat (or, for the last
/// run, its own last beat), so the millisecond rounding averages out.
///
/// Returns null for fewer than two usable beats.
BeatsPointer beatsFromGrid(audio::SampleRate sampleRate,
        const QVector<GridBeat>& grid,
        const QString& subVersion);

} // namespace rekordbox
} // namespace mixxx
