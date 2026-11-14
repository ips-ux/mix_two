#pragma once

#include <QByteArray>

#include "audio/types.h"
#include "util/types.h"
#include "waveform/waveform.h"

namespace mixxx {
namespace rekordbox {

/// The seven waveform pictures of a rekordbox analysis, as the bytes of
/// their section bodies after the section header.
struct AnlzWaveforms {
    /// PWAV: 400 bytes, bits 0-4 height, bits 5-7 whiteness.
    QByteArray preview;
    /// PWV2: 100 bytes, bits 0-3 height.
    QByteArray tinyPreview;
    /// PWV3: one byte per detail entry, as PWAV.
    QByteArray detail;
    /// PWV4: 1200 columns of 6 bytes.
    QByteArray colorPreview;
    /// PWV5: one big-endian u16 per detail entry.
    QByteArray colorDetail;
    /// PWV6: 1200 columns of 3 bytes (low, mid, high).
    QByteArray threeBandPreview;
    /// PWV7: 3 bytes (low, mid, high) per detail entry.
    QByteArray threeBandDetail;
    /// The number of detail entries: ceil(frames * 150 / sampleRate).
    int detailEntries = 0;
};

inline constexpr int kDetailEntriesPerSecond = 150;
inline constexpr int kPreviewColumns = 400;
inline constexpr int kTinyPreviewColumns = 100;
inline constexpr int kColorPreviewColumns = 1200;

/// Mix Two: converts the analyser's waveform to rekordbox's pictures.
/// `frames` is the track length in audio frames. Returns empty arrays when
/// the waveform has no data.
///
/// The pictures do not match the ones rekordbox makes for the same track,
/// because the analyser is different.
AnlzWaveforms anlzWaveformsFrom(const Waveform& waveform,
        SINT frames,
        audio::SampleRate sampleRate);

} // namespace rekordbox
} // namespace mixxx
