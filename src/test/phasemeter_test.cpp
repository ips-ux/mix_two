// AI-generated (Claude Code). Start of AI-generated text.
// Tests for the phase offset of the deck phase meter (mixxx::phaseOffset).
// End of AI-generated text.
#include <gtest/gtest.h>

#include "widget/wphasemeter.h"

namespace {

constexpr double kEps = 1e-9;

TEST(PhaseMeterMath, InPhaseIsZero) {
    EXPECT_NEAR(0.0, mixxx::phaseOffset(0.3, 0.3), kEps);
}

TEST(PhaseMeterMath, Ahead) {
    EXPECT_NEAR(0.1, mixxx::phaseOffset(0.4, 0.3), kEps);
}

TEST(PhaseMeterMath, Behind) {
    EXPECT_NEAR(-0.1, mixxx::phaseOffset(0.2, 0.3), kEps);
}

TEST(PhaseMeterMath, WrapsAcrossBeatBoundary) {
    EXPECT_NEAR(0.1, mixxx::phaseOffset(0.05, 0.95), kEps);
    EXPECT_NEAR(-0.1, mixxx::phaseOffset(0.95, 0.05), kEps);
}

TEST(PhaseMeterMath, HalfBeatIsNegativeHalf) {
    EXPECT_NEAR(-0.5, mixxx::phaseOffset(0.5, 0.0), kEps);
}

} // namespace
