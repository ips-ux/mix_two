// AI-generated (Claude Code). Start of AI-generated text.
// Tests for the column sampling of the 3Band waveform.
// End of AI-generated text.
#include <gtest/gtest.h>

#include <vector>

#include "waveform/renderers/threebandsampler.h"
#include "waveform/waveform.h"

namespace {

using namespace mixxx::threeband;

// Stereo-interleaved data with `frames` visual frames, all zero.
std::vector<WaveformData> makeData(int frames) {
    std::vector<WaveformData> d(static_cast<size_t>(frames) * 2);
    for (auto& v : d) {
        v.filtered = {0, 0, 0, 0};
    }
    return d;
}

int sizeOf(const std::vector<WaveformData>& d) {
    return static_cast<int>(d.size());
}

TEST(ThreeBandSamplerTest, HoldFrames) {
    EXPECT_EQ(18, holdFrames(2205, 0.016));
    EXPECT_EQ(4, holdFrames(441, 0.016));
    EXPECT_EQ(0, holdFrames(2205, 0));
}

TEST(ThreeBandSamplerTest, PeakOverColumn) {
    auto d = makeData(100);
    for (auto& v : d) {
        v.filtered.high = 5;
    }
    d[7 * 2 + 1].filtered.high = 90;
    EXPECT_EQ(90, peaksForColumn(d.data(), sizeOf(d), 4, 8, 0, 0).high);
    EXPECT_EQ(5, peaksForColumn(d.data(), sizeOf(d), 8, 12, 0, 0).high);
}

TEST(ThreeBandSamplerTest, OnePointColumn) {
    auto d = makeData(100);
    d[10 * 2].filtered.mid = 33;
    d[11 * 2].filtered.mid = 77;
    EXPECT_EQ(33, peaksForColumn(d.data(), sizeOf(d), 10, 10, 0, 0).mid);
    EXPECT_EQ(77, peaksForColumn(d.data(), sizeOf(d), 11, 11, 0, 0).mid);
}

TEST(ThreeBandSamplerTest, LowHoldWidensOnlyLow) {
    auto d = makeData(200);
    d[50 * 2].filtered.low = 80;
    d[50 * 2].filtered.high = 80;
    const Peaks wide = peaksForColumn(d.data(), sizeOf(d), 60, 61, 18, 0);
    EXPECT_EQ(80, wide.low);
    EXPECT_EQ(0, wide.high);
    const Peaks narrow = peaksForColumn(d.data(), sizeOf(d), 60, 61, 5, 0);
    EXPECT_EQ(0, narrow.low);
}

TEST(ThreeBandSamplerTest, OutsideTrackIsZero) {
    auto d = makeData(100);
    for (auto& v : d) {
        v.filtered = {9, 9, 9, 9};
    }
    const int n = sizeOf(d);
    auto expectZero = [](const Peaks& p) {
        EXPECT_EQ(0, p.low);
        EXPECT_EQ(0, p.mid);
        EXPECT_EQ(0, p.high);
    };
    expectZero(peaksForColumn(d.data(), n, -40, -30, 0, 0));
    expectZero(peaksForColumn(d.data(), n, n / 2 + 5, n / 2 + 9, 0, 0));
    expectZero(peaksForColumn(d.data(), 0, 0, 4, 2, 2));
    expectZero(peaksForColumn(d.data(), 1, 0, 4, 2, 2));
    // A range that crosses frame 0 reads only the valid frames.
    const Peaks p = peaksForColumn(d.data(), n, -3, 2, 0, 0);
    EXPECT_EQ(9, p.low);
    EXPECT_EQ(9, p.high);
}

TEST(ThreeBandSamplerTest, CoreShade) {
    EXPECT_FLOAT_EQ(1.0f, coreShade(0.0f));
    EXPECT_NEAR(0.45f, coreShade(1.0f), 1e-6);
    EXPECT_FLOAT_EQ(coreShade(1.0f), coreShade(2.0f));
}

} // namespace
