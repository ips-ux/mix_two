// AI-generated (Claude Code). Start of AI-generated text.
// Tests for the pixel of the slip lines on the overview.
// End of AI-generated text.
#include <gtest/gtest.h>

#include "widget/woverview.h"

namespace {

TEST(OverviewSlipLineTest, NoSlipPositionGivesNoLine) {
    EXPECT_EQ(-1, mixxx::overviewSlipLinePixel(-1.0, 399));
    EXPECT_EQ(-1, mixxx::overviewSlipLinePixel(-0.0001, 399));
}

TEST(OverviewSlipLineTest, MapsFractionToPixel) {
    EXPECT_EQ(0, mixxx::overviewSlipLinePixel(0.0, 399));
    EXPECT_EQ(399, mixxx::overviewSlipLinePixel(1.0, 399));
    EXPECT_EQ(200, mixxx::overviewSlipLinePixel(0.5, 400));
    EXPECT_EQ(100, mixxx::overviewSlipLinePixel(0.25, 400));
}

TEST(OverviewSlipLineTest, ClampsAboveOne) {
    EXPECT_EQ(399, mixxx::overviewSlipLinePixel(1.5, 399));
}

TEST(OverviewSlipLineTest, EmptyWidgetGivesPixelZero) {
    EXPECT_EQ(0, mixxx::overviewSlipLinePixel(0.7, 0));
}

} // namespace
