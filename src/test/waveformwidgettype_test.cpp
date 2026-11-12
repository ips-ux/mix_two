// AI-generated (Claude Code). Start of AI-generated text.
// The 3Band waveform type is registered, and the one-time switch makes it
// the waveform type of a profile that did not have the switch yet.
// End of AI-generated text.
#include <gtest/gtest.h>

#include <algorithm>

#include "waveform/waveformwidgetfactory.h"
#include "waveform/widgets/waveformwidgettype.h"

namespace {

TEST(WaveformWidgetTypeTest, ThreeBandIsRegistered) {
    const auto& values = WaveformWidgetType::kValues;
    EXPECT_NE(values.end(),
            std::find(values.begin(), values.end(), WaveformWidgetType::ThreeBand));
    EXPECT_EQ(18, static_cast<int>(WaveformWidgetType::ThreeBand));
}

TEST(WaveformWidgetFactoryDefaultTest, SwitchesToThreeBandOnce) {
    EXPECT_EQ(WaveformWidgetType::ThreeBand,
            WaveformWidgetFactory::mixtwoInitialType(WaveformWidgetType::RGB, false));
    EXPECT_EQ(WaveformWidgetType::ThreeBand,
            WaveformWidgetFactory::mixtwoInitialType(WaveformWidgetType::Empty, false));
    EXPECT_EQ(WaveformWidgetType::RGB,
            WaveformWidgetFactory::mixtwoInitialType(WaveformWidgetType::RGB, true));
    EXPECT_EQ(WaveformWidgetType::Stacked,
            WaveformWidgetFactory::mixtwoInitialType(WaveformWidgetType::Stacked, true));
}

TEST(WaveformWidgetFactoryDefaultTest, DefaultIsThreeBand) {
    EXPECT_EQ(WaveformWidgetType::ThreeBand, WaveformWidgetFactory::defaultType());
}

TEST(WaveformWidgetFactoryDefaultTest, DefaultFallsBackToRgb) {
    EXPECT_EQ(WaveformWidgetType::ThreeBand,
            WaveformWidgetFactory::defaultTypeIfAvailable(true));
    EXPECT_EQ(WaveformWidgetType::RGB,
            WaveformWidgetFactory::defaultTypeIfAvailable(false));
}

} // namespace
