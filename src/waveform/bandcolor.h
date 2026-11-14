#pragma once

#include <QColor>
#include <QtGlobal>

// <!-- AI-generated (Claude Code, 2026-11-02). Start of AI-generated text. -->
// Band values to picture values. The ANLZ waveform writer and the on-screen
// waveforms use the same rules, so they show the same picture.
// <!-- End of AI-generated text. -->

namespace mixxx {
namespace bandcolor {

/// The largest values of the three bands over one picture column (0-255 each).
struct Bands {
    quint8 low;
    quint8 mid;
    quint8 high;
};

// <!-- AI-generated (Claude Code, 2026-11-13). Start of AI-generated text. -->
/// The loudest band times 34 / 256 (0-31).
quint8 height5(Bands b);

/// The mid band as a share of the loudest band, 4.5 * mid / loudest rounded
/// (0-5), as rekordbox's blue waveform does: bass-led columns stay near 0,
/// mid-led columns go to 5. 0 when the height is 0.
quint8 whiteness3(Bands b);

/// The 16-bit colour detail word: red, green and blue in 3 bits each, then the
/// height in 5 bits. The colour is a blend of the bands (red from low, green
/// from mid, blue from mid and high, with weights fitted to rekordbox's own
/// files), scaled so that the largest channel is 7, as in rekordbox's own
/// files.
quint16 colorDetailEntry(Bands b);
// <!-- End of AI-generated text. -->

/// The blend from `base` (whiteness 0) to white (whiteness 7). A whiteness
/// above 7 counts as 7. Alpha is 255.
QColor colorSaturated(QColor base, quint8 whiteness);

} // namespace bandcolor
} // namespace mixxx
