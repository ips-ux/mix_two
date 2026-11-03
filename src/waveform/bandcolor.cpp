#include "waveform/bandcolor.h"

#include <algorithm>

// <!-- AI-generated (Claude Code, 2026-11-02). Start of AI-generated text. -->
namespace mixxx {
namespace bandcolor {

namespace {
int loudestOf(Bands b) {
    return std::max({b.low, b.mid, b.high});
}
} // namespace

quint8 height5(Bands b) {
    return static_cast<quint8>(loudestOf(b) >> 3);
}

quint8 whiteness3(Bands b) {
    const int loudest = loudestOf(b);
    if (height5(b) == 0) {
        return 0;
    }
    // 7 * (1 - mid / loudest), rounded to nearest.
    return static_cast<quint8>((7 * (loudest - b.mid) + loudest / 2) / loudest);
}

quint16 colorDetailEntry(Bands b) {
    return static_cast<quint16>(((b.mid >> 5) << 13) | ((b.high >> 5) << 10) |
            ((b.low >> 5) << 7) | (height5(b) << 2));
}

QColor colorSaturated(QColor base, quint8 whiteness) {
    const int w = std::min<int>(whiteness, 7);
    const auto blend = [w](int channel) {
        return (channel * (7 - w) + 255 * w + 3) / 7;
    };
    return QColor(blend(base.red()), blend(base.green()), blend(base.blue()), 255);
}

} // namespace bandcolor
} // namespace mixxx
// <!-- End of AI-generated text. -->
