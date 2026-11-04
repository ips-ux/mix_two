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

/// The loudest band divided by 8 (0-31).
quint8 height5(Bands b);

/// The mid band as a share of the loudest band (0-7), as rekordbox's blue
/// waveform does: bass-led columns stay near 0, mid-led columns go to 7.
/// 0 when the height is 0.
quint8 whiteness3(Bands b);

/// The 16-bit colour detail word: mid, high and low in 3 bits each, then the
/// height in 5 bits.
quint16 colorDetailEntry(Bands b);

/// The blend from `base` (whiteness 0) to white (whiteness 7). A whiteness
/// above 7 counts as 7. Alpha is 255.
QColor colorSaturated(QColor base, quint8 whiteness);

} // namespace bandcolor
} // namespace mixxx
