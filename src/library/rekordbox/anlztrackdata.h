#pragma once

#include <QList>
#include <QRgb>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

#include "audio/types.h"
#include "library/rekordbox/anlzwaveform.h"
#include "library/rekordbox/rekordboxbeats.h"
#include "util/types.h"

class Track;

namespace mixxx {
namespace rekordbox {

/// Mix Two: one cue, loop or hot cue of an analysis file (ANLZ PCPT and PCP2
/// entry).
struct AnlzCue {
    /// 0 for a memory cue or loop, 1-8 for the hot cues A-H.
    int hotCue = 0;
    /// The start as audio time in ms. The writer adds `timingOffsetMs`.
    quint32 timeMs = 0;
    /// The end as audio time in ms; the writer adds `timingOffsetMs`. A cue
    /// with an end is a loop.
    std::optional<quint32> loopEndMs;
    /// May be empty.
    QString comment;
    /// Hot cue: the RGB colour in PCP2. Memory cue: the writer takes the
    /// nearest of the eight colour IDs of rekordbox.
    std::optional<QRgb> color;
};

/// Mix Two: everything the writer needs for the analysis files of one track.
struct AnlzTrackData {
    /// The track on the stick, e.g. "/Contents/Artist/Album/file.mp3", with
    /// forward slashes.
    QString stickPath;
    audio::SampleRate sampleRate;
    /// The track length in audio frames.
    SINT frames = 0;
    /// The MP3 decoder offset. The writer adds it to every time it writes
    /// (beats, cues and loop ends); the result is never below 0.
    int timingOffsetMs = 0;
    /// The position in frames and the tempo in BPM x 100.
    QVector<GridBeat> beats;
    /// The number of each beat in its bar (1-4), parallel to `beats`
    /// (GridBeat stays as the rekordbox reader uses it). The writer writes 1
    /// for a beat that has no entry.
    QVector<int> beatInBar;
    /// hotCue 1-8, ascending.
    QList<AnlzCue> hotCues;
    /// hotCue 0, ascending timeMs, at most 10.
    QList<AnlzCue> memoryCues;
    AnlzWaveforms waveforms;
    /// MPEG frames x 1152 for an MP3 file, else 0 (the PVBR index is empty).
    quint32 mp3PcmFrames = 0;
};

/// Mix Two: fills the value from a track and its waveform. `stickPath` and
/// `timingOffsetMs` come from the export job. Warnings (ASD-STE100, one per
/// line) go to *pWarnings.
///
/// Returns nullopt and sets *pErrorMessage when the waveform is missing or
/// not complete (it needs `getCompletion() == getDataSize()`), when the track
/// has no length, or when the sample rate is not valid. The messages are
/// "The track has no waveform. Analyse the track first." and "The track has
/// no valid sample rate. Analyse the track again.".
///
/// The function does not call `track.getWaveform()`: the caller gives the
/// waveform (the one of the analysis database for a track that is not
/// loaded). It can run on a worker thread: it reads the track only through
/// its getters, which lock the track, and it takes the cue list once with
/// `getCuePoints()`. The waveform must not change during the call.
///
/// Cue and loop times are audio time in ms; the writer adds the timing offset.
/// A memory cue or loop with the default cue or loop colour has no colour.
/// The beat at the first beat marker is beat 1 of its bar.
std::optional<AnlzTrackData> anlzTrackDataFrom(const Track& track,
        const ConstWaveformPointer& pWaveform,
        const QString& stickPath,
        int timingOffsetMs,
        QStringList* pWarnings,
        QString* pErrorMessage);

} // namespace rekordbox
} // namespace mixxx
