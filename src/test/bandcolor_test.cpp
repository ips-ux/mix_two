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
    EXPECT_EQ(0, mixxx::bandcolor::whiteness3(Bands{200, 200, 10}));
    EXPECT_EQ(7, mixxx::bandcolor::whiteness3(Bands{200, 0, 10}));
}

TEST(BandColorTest, ColorDetailEntryPacksBandsAndHeight) {
    // Low only: R 0, G 0, B 7, height 31 -> 7 << 7 | 31 << 2.
    EXPECT_EQ(0x03FC, mixxx::bandcolor::colorDetailEntry(Bands{255, 0, 0}));
    // Mid only: R 7, G 0, B 0, height 31.
    EXPECT_EQ(0xE07C, mixxx::bandcolor::colorDetailEntry(Bands{0, 255, 0}));
}

TEST(BandColorTest, ColorSaturatedBlendsToWhite) {
    const QColor base(0, 104, 255);
    EXPECT_EQ(base, mixxx::bandcolor::colorSaturated(base, 0));
    EXPECT_EQ(QColor(255, 255, 255), mixxx::bandcolor::colorSaturated(base, 7));
    EXPECT_EQ(QColor(255, 255, 255), mixxx::bandcolor::colorSaturated(base, 9));
}

} // namespace
// <!-- End of AI-generated text. -->
