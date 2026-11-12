// AI-generated (Claude Code). Start of AI-generated text.
// The zoom value keeps its meaning in time for waveform data stored at
// the old and the new point rate, and the zoom steps are proportional.
// End of AI-generated text.
#include <gtest/gtest.h>

#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"

namespace {

TEST(WaveformZoomTest, SameSpanForOldAndNewData) {
    const double legacy = WaveformWidgetRenderer::zoomToVisualSamplesPerPixel(
            3.0, kLegacyMainWaveformSampleRate);
    const double current = WaveformWidgetRenderer::zoomToVisualSamplesPerPixel(
            3.0, kMainWaveformSampleRate);
    EXPECT_DOUBLE_EQ(3.0, legacy);
    EXPECT_DOUBLE_EQ(15.0, current);
    EXPECT_NEAR(legacy / kLegacyMainWaveformSampleRate,
            current / kMainWaveformSampleRate,
            1e-12);
}

TEST(WaveformZoomTest, SameSpanAt48k) {
    Waveform w48(48000, 480000, kMainWaveformSampleRate, -1, 0);
    Waveform w44(44100, 441000, kMainWaveformSampleRate, -1, 0);
    const double span48 = WaveformWidgetRenderer::zoomToVisualSamplesPerPixel(
                                  1.0, w48.getVisualSampleRate()) /
            w48.getVisualSampleRate();
    const double span44 = WaveformWidgetRenderer::zoomToVisualSamplesPerPixel(
                                  1.0, w44.getVisualSampleRate()) /
            w44.getVisualSampleRate();
    EXPECT_NEAR(span48, span44, 1e-9);
}

TEST(WaveformZoomTest, DeepestZoomIsOnePointPerPixel) {
    EXPECT_NEAR(1.0,
            WaveformWidgetRenderer::zoomToVisualSamplesPerPixel(
                    WaveformWidgetRenderer::s_waveformMinZoom,
                    kMainWaveformSampleRate),
            1e-9);
}

TEST(WaveformZoomTest, StepsAreMultiplicativeAndClamped) {
    EXPECT_DOUBLE_EQ(1.25, WaveformWidgetRenderer::steppedZoom(1.0, 1));
    EXPECT_DOUBLE_EQ(0.8, WaveformWidgetRenderer::steppedZoom(1.0, -1));
    EXPECT_DOUBLE_EQ(10.0, WaveformWidgetRenderer::steppedZoom(9.5, 1));
    EXPECT_DOUBLE_EQ(0.2, WaveformWidgetRenderer::steppedZoom(0.21, -1));
    EXPECT_NEAR(2.0,
            WaveformWidgetRenderer::steppedZoom(
                    WaveformWidgetRenderer::steppedZoom(2.0, 3), -3),
            1e-9);
}

TEST(WaveformZoomTest, NoWaveformKeepsZoom) {
    EXPECT_DOUBLE_EQ(3.0, WaveformWidgetRenderer::zoomToVisualSamplesPerPixel(3.0, 0));
}

} // namespace
