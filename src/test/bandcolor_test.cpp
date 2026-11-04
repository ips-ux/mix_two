// <!-- AI-generated (Claude Code, 2026-11-02). Start of AI-generated text. -->
#include "waveform/bandcolor.h"

#include <gtest/gtest.h>

#include <QColor>

namespace {

using mixxx::bandcolor::Bands;

TEST(BandColorTest, Height5IsLoudestBandOver8) {
    EXPECT_EQ(0, mixxx::bandcolor::height5(Bands{0, 0, 0}));
    EXPECT_EQ(31, mixxx::bandcolor::height5(Bands{255, 10, 10}));
    EXPECT_EQ(31, mixxx::bandcolor::height5(Bands{10, 10, 255}));
}

TEST(BandColorTest, Whiteness3FollowsMidShare) {
    EXPECT_EQ(0, mixxx::bandcolor::whiteness3(Bands{0, 0, 0}));
    EXPECT_EQ(7, mixxx::bandcolor::whiteness3(Bands{200, 200, 10}));
    EXPECT_EQ(0, mixxx::bandcolor::whiteness3(Bands{200, 0, 10}));
    // 7 * 100 / 200 = 3.5 -> 4.
    EXPECT_EQ(4, mixxx::bandcolor::whiteness3(Bands{200, 100, 10}));
}

TEST(BandColorTest, ColorDetailEntryPacksBandsAndHeight) {
    // Low only: red. R 7, G 0, B 0, height 31 -> 7 << 13 | 31 << 2.
    EXPECT_EQ(0xE07C, mixxx::bandcolor::colorDetailEntry(Bands{255, 0, 0}));
    // Mid only: cyan #00e7d3. G 7, B 7 * 0xd3 / 0xe7 = 6.39 -> 6.
    EXPECT_EQ(0x1F7C, mixxx::bandcolor::colorDetailEntry(Bands{0, 255, 0}));
    // High only: blue. B 7.
    EXPECT_EQ(0x03FC, mixxx::bandcolor::colorDetailEntry(Bands{0, 0, 255}));
    // Low and high: R 7, B 7 * 255 * 0xdc / (255 * 0xff) = 6.04 -> 6.
    EXPECT_EQ(0xE37C, mixxx::bandcolor::colorDetailEntry(Bands{255, 0, 255}));
    // Silence: height 0 and no colour.
    EXPECT_EQ(0x0000, mixxx::bandcolor::colorDetailEntry(Bands{0, 0, 0}));
}

TEST(BandColorTest, ColorSaturatedBlendsToWhite) {
    const QColor base(0, 104, 255);
    EXPECT_EQ(base, mixxx::bandcolor::colorSaturated(base, 0));
    EXPECT_EQ(QColor(255, 255, 255), mixxx::bandcolor::colorSaturated(base, 7));
    EXPECT_EQ(QColor(255, 255, 255), mixxx::bandcolor::colorSaturated(base, 9));
}

TEST(BandColorTest, ColorSaturatedBassIsBase) {
    // The Color Saturated renderers use this mapping: a column with no mid
    // band (a kick) shows the base colour.
    const QColor base(0, 104, 255);
    const Bands b{200, 0, 10};
    EXPECT_EQ(base,
            mixxx::bandcolor::colorSaturated(
                    base, mixxx::bandcolor::whiteness3(b)));
}

} // namespace
// <!-- End of AI-generated text. -->
