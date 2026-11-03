#include "library/rekordbox/rekordboxbeats.h"

#include <vector>

namespace mixxx {
namespace rekordbox {

BeatsPointer beatsFromGrid(audio::SampleRate sampleRate,
        const QVector<GridBeat>& grid,
        const QString& subVersion) {
    if (!sampleRate.isValid()) {
        return nullptr;
    }

    // Beats must be strictly ascending; the import clamps early times to 1 ms.
    QVector<GridBeat> beats;
    beats.reserve(grid.size());
    for (const auto& beat : grid) {
        if (!beat.position.isValid() ||
                (!beats.isEmpty() && beat.position <= beats.back().position)) {
            continue;
        }
        beats.append(GridBeat{beat.position.toLowerFrameBoundary(), beat.tempo});
    }
    if (beats.size() < 2) {
        return nullptr;
    }

    // One marker per run of beats with the same stored tempo.
    std::vector<BeatMarker> markers;
    int runStart = 0;
    for (int i = 1; i < beats.size(); ++i) {
        if (beats[i].tempo != beats[runStart].tempo) {
            markers.emplace_back(beats[runStart].position, i - runStart);
            runStart = i;
        }
    }

    const int lastIndex = static_cast<int>(beats.size()) - 1;
    const auto lastMarkerPosition = beats[runStart].position;
    Bpm lastMarkerBpm;
    if (lastIndex > runStart) {
        const double beatLengthFrames =
                (beats[lastIndex].position - lastMarkerPosition) / (lastIndex - runStart);
        lastMarkerBpm = Bpm(60.0 * sampleRate / beatLengthFrames);
    } else if (beats[runStart].tempo > 0) {
        lastMarkerBpm = Bpm(beats[runStart].tempo / 100.0);
    } else {
        const double beatLengthFrames = beats[lastIndex].position - beats[lastIndex - 1].position;
        lastMarkerBpm = Bpm(60.0 * sampleRate / beatLengthFrames);
    }
    if (!lastMarkerBpm.isValid()) {
        return nullptr;
    }

    return Beats::fromBeatMarkers(
            sampleRate, markers, lastMarkerPosition, lastMarkerBpm, subVersion);
}

} // namespace rekordbox
} // namespace mixxx
