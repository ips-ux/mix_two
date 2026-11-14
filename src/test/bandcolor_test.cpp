// <!-- AI-generated (Claude Code, 2026-11-02). Start of AI-generated text. -->
#include "waveform/bandcolor.h"

#include <gtest/gtest.h>

#include <QColor>

namespace {

using mixxx::bandcolor::Bands;

TEST(BandColorTest, Height5IsScaledLoudestBand) {
    EXPECT_EQ(0, mixxx::bandcolor::height5(Bands{0, 0, 0}));
    // 100 * 34 / 256 = 13.3 -> 13.
    EXPECT_EQ(13, mixxx::bandcolor::height5(Bands{100, 10, 10}));
    EXPECT_EQ(31, mixxx::bandcolor::height5(Bands{255, 10, 10}));
    EXPECT_EQ(31, mixxx::bandcolor::height5(Bands{10, 10, 255}));
}

TEST(BandColorTest, Whiteness3FollowsMidShare) {
    EXPECT_EQ(0, mixxx::bandcolor::whiteness3(Bands{0, 0, 0}));
    // Mid as loud as the loudest band: 4.5 -> 5.
    EXPECT_EQ(5, mixxx::bandcolor::whiteness3(Bands{200, 200, 10}));
    EXPECT_EQ(0, mixxx::bandcolor::whiteness3(Bands{200, 0, 10}));
    // 4.5 * 100 / 200 = 2.25 -> 2.
    EXPECT_EQ(2, mixxx::bandcolor::whiteness3(Bands{200, 100, 10}));
}

TEST(BandColorTest, ColorDetailEntryPacksBandsAndHeight) {
    // Low only: red. R 7, G 0, B 0, height 31 -> 7 << 13 | 31 << 2.
    EXPECT_EQ(0xE07C, mixxx::bandcolor::colorDetailEntry(Bands{255, 0, 0}));
    // Mid only: G 7, B 7 * 16 / 119 = 0.94 -> 1.
    EXPECT_EQ(0x1CFC, mixxx::bandcolor::colorDetailEntry(Bands{0, 255, 0}));
    // High only: blue. B 7.
    EXPECT_EQ(0x03FC, mixxx::bandcolor::colorDetailEntry(Bands{0, 0, 255}));
    // Low and high: R 7, B 7 * 204 / 232 = 6.16 -> 6.
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
