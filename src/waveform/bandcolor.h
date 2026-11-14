#pragma once

#include <QColor>
#include <QtGlobal>

// <!-- AI-generated (Claude Code, 2026-11-02). Start of AI-generated text. -->
// Band values to picture values for the ANLZ waveform writer and the
// on-screen waveforms. The writer has its own height, whiteness and colour
// constants, fitted to rekordbox's own files; the screen keeps its rules.
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
/// The exported height: the loudest band times 34 / 256 (0-31). Used by the
/// USB export only.
quint8 height5(Bands b);

/// The on-screen whiteness: the mid band as a share of the loudest band,
/// 7 * mid / loudest rounded (0-7). Bass-led columns stay near 0, mid-led
/// columns go to 7. 0 when the loudest band is below 8. Used by the Color
/// Saturated waveform and overview.
quint8 whiteness3(Bands b);

/// The exported whiteness, as rekordbox's blue waveform does: 4.5 * mid /
/// loudest rounded (0-5), fitted to rekordbox's own files. 0 when height5 is
/// 0. Used by the USB export only.
quint8 exportWhiteness3(Bands b);

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
