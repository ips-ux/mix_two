#include "waveform/bandcolor.h"

#include <algorithm>

// <!-- AI-generated (Claude Code, 2026-11-02). Start of AI-generated text. -->
namespace mixxx {
namespace bandcolor {

namespace {
int loudestOf(Bands b) {
    return std::max({b.low, b.mid, b.high});
}

// <!-- AI-generated (Claude Code, 2026-11-13). Start of AI-generated text. -->
// The constants are fitted to rekordbox's own waveforms for the band split
// points of the analyser (150 Hz and 2000 Hz).
constexpr int kHeightScale = 34;     // height = loudest * 34 / 256
constexpr int kWhitenessTwice = 9;   // whiteness = 4.5 * mid / loudest
constexpr int kRedFromLow = 232;
constexpr int kGreenFromMid = 119;
constexpr int kBlueFromMid = 16;
constexpr int kBlueFromHigh = 204;
// <!-- End of AI-generated text. -->
} // namespace

quint8 height5(Bands b) {
    return static_cast<quint8>(std::min(31, loudestOf(b) * kHeightScale / 256));
}

quint8 whiteness3(Bands b) {
    const int loudest = loudestOf(b);
    if (height5(b) == 0) {
        return 0;
    }
    // 4.5 * mid / loudest, rounded to nearest.
    return static_cast<quint8>((kWhitenessTwice * b.mid + loudest) / (2 * loudest));
}

quint16 colorDetailEntry(Bands b) {
    const int red = b.low * kRedFromLow;
    const int green = b.mid * kGreenFromMid;
    const int blue = b.mid * kBlueFromMid + b.high * kBlueFromHigh;
    const int largest = std::max({red, green, blue});
    if (largest == 0) {
        return static_cast<quint16>(height5(b) << 2);
    }
    // Each channel 7 * channel / largest, rounded to nearest.
    const auto scaled = [largest](int channel) {
        return (7 * channel + largest / 2) / largest;
    };
    return static_cast<quint16>((scaled(red) << 13) | (scaled(green) << 10) |
            (scaled(blue) << 7) | (height5(b) << 2));
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
