#include "waveform/renderers/allshader/waveformrenderer3band.h"

#include <algorithm>
#include <cmath>

#include "rendergraph/material/rgbmaterial.h"
#include "rendergraph/vertexupdaters/rgbvertexupdater.h"
#include "track/track.h"
#include "waveform/renderers/threebandsampler.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"

using namespace rendergraph;

namespace {

// AI-generated (Claude Code). Start of AI-generated text.
constexpr int kBandCount = 3;
// Each band has two halves (up and down). Each half has two shaded
// segments and one rim.
constexpr int kRectanglesPerColumn = kBandCount * 2 * 3;
constexpr int kVerticesPerRectangle = 6; // 2 triangles

// A band is drawn with a rim only if it is at least this part of the half
// breadth.
constexpr float kMinRimFraction = 0.02f;

// The colours of one band, in 0..1.
struct BandColors {
    QVector3D centre; // at the centre line
    QVector3D middle; // at half the band height
    QVector3D edge;   // at the band edge
    QVector3D rim;
};

BandColors makeBandColors(int r, int g, int b) {
    const QVector3D base(r / 255.f, g / 255.f, b / 255.f);
    auto rimComponent = [](int c) {
        return std::min(c * 1.15f + 25.f, 255.f) / 255.f;
    };
    return BandColors{
            base * mixxx::threeband::coreShade(0.f),
            base * mixxx::threeband::coreShade(0.5f),
            base * mixxx::threeband::coreShade(1.f),
            QVector3D(rimComponent(r), rimComponent(g), rimComponent(b))};
}

// Fixed band colours: low blue, mid orange, high white.
const BandColors kBandColors[kBandCount] = {
        makeBandColors(28, 92, 235),
        makeBandColors(240, 150, 40),
        makeBandColors(255, 255, 255),
};

// Adds one half of a band, from the centre line `centreY` to the edge at
// `centreY + direction * height` (direction is -1 for up, +1 for down).
// Always adds 3 rectangles; a part that is not drawn has zero area.
void addBandHalf(RGBVertexUpdater& vertexUpdater,
        float x1,
        float x2,
        float centreY,
        float direction,
        float height,
        bool drawRim,
        float rimHeight,
        const BandColors& colors) {
    const float middleY = centreY + direction * height * 0.5f;
    const float edgeY = centreY + direction * height;
    vertexUpdater.addRectangleVGradient(
            {x1, centreY}, {x2, middleY}, colors.centre, colors.middle);
    vertexUpdater.addRectangleVGradient(
            {x1, middleY}, {x2, edgeY}, colors.middle, colors.edge);
    const float rimInnerY = drawRim ? edgeY - direction * std::min(rimHeight, height) : edgeY;
    vertexUpdater.addRectangle({x1, rimInnerY}, {x2, edgeY}, colors.rim);
}
// End of AI-generated text.

} // namespace

namespace allshader {

WaveformRenderer3Band::WaveformRenderer3Band(WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererAbstract::PositionSource type,
        ::WaveformRendererSignalBase::Options options)
        : WaveformRendererSignalBase(waveformWidget, options),
          m_isSlipRenderer(type == ::WaveformRendererAbstract::Slip) {
    initForRectangles<RGBMaterial>(0);
    setUsePreprocess(true);
}

void WaveformRenderer3Band::onSetup(const QDomNode&) {
}

void WaveformRenderer3Band::preprocess() {
    if (!preprocessInner()) {
        if (geometry().vertexCount() != 0) {
            geometry().allocate(0);
            markDirtyGeometry();
        }
    }
}

bool WaveformRenderer3Band::preprocessInner() {
    TrackPointer pTrack = m_waveformRenderer->getTrackInfo();

    if (!pTrack || (m_isSlipRenderer && !m_waveformRenderer->isSlipActive())) {
        return false;
    }

    auto positionType = m_isSlipRenderer ? ::WaveformRendererAbstract::Slip
                                         : ::WaveformRendererAbstract::Play;

    ConstWaveformPointer waveform = pTrack->getWaveform();
    if (waveform.isNull()) {
        return false;
    }

    const int dataSize = waveform->getDataSize();
    if (dataSize <= 1) {
        return false;
    }

    const WaveformData* data = waveform->data();
    if (data == nullptr) {
        return false;
    }
#ifdef __STEM__
    auto stemInfo = pTrack->getStemInfo();
    // If this track is a stem track, skip the rendering
    if (!stemInfo.isEmpty() && waveform->hasStem() && !m_ignoreStem) {
        return false;
    }
#endif

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();
    const int length = static_cast<int>(m_waveformRenderer->getLength());
    const int pixelLength = static_cast<int>(m_waveformRenderer->getLength() * devicePixelRatio);
    const float invDevicePixelRatio = 1.f / devicePixelRatio;
    const float halfPixelSize = 0.5f / devicePixelRatio;

    // See waveformrenderersimple.cpp for a detailed explanation of the frame and index calculation
    const int visualFramesSize = dataSize / 2;
    const double firstVisualFrame =
            m_waveformRenderer->getFirstDisplayedPosition(positionType) * visualFramesSize;
    const double lastVisualFrame =
            m_waveformRenderer->getLastDisplayedPosition(positionType) * visualFramesSize;

    // Represents the # of visual frames per horizontal pixel.
    const double visualIncrementPerPixel =
            (lastVisualFrame - firstVisualFrame) / static_cast<double>(pixelLength);

    // Fixes a sporadic crash caused by a division by zero on waveform initialization
    if (visualIncrementPerPixel == 0.0) {
        return false;
    }

    // Per-band gain from the EQ knobs.
    float allGain = 1.0f;
    float lowGain = 1.0f;
    float midGain = 1.0f;
    float highGain = 1.0f;
    getGains(&allGain, &lowGain, &midGain, &highGain);

    const float breadth = static_cast<float>(m_waveformRenderer->getBreadth());
    const float halfBreadth = breadth / 2.0f;

    // AI-generated (Claude Code). Start of AI-generated text.
    // Height per peak unit of each band: band scale, EQ knob and visual gain.
    const float unitHeight = allGain * halfBreadth / m_maxValue;
    const float bandHeightFactor[kBandCount] = {
            mixxx::threeband::kLowGain * lowGain * unitHeight,
            mixxx::threeband::kMidGain * midGain * unitHeight,
            mixxx::threeband::kHighGain * highGain * unitHeight,
    };
    const float minRimHeight = kMinRimFraction * halfBreadth;

    const double visualSampleRate = waveform->getVisualSampleRate();
    const int lowHold = mixxx::threeband::holdFrames(
            visualSampleRate, mixxx::threeband::kLowHoldSeconds);
    const int midHold = mixxx::threeband::holdFrames(
            visualSampleRate, mixxx::threeband::kMidHoldSeconds);
    // End of AI-generated text.

    // Effective visual frame for x
    double xVisualFrame = qRound(firstVisualFrame / visualIncrementPerPixel) *
            visualIncrementPerPixel;

    // AI-generated (Claude Code). Start of AI-generated text.
    // The slip renderer writes the lower half with zero area, so the
    // vertex count is the same.
    // End of AI-generated text.
    const int reserved = kVerticesPerRectangle * (kRectanglesPerColumn * pixelLength + 1);

    geometry().setDrawingMode(Geometry::DrawingMode::Triangles);
    geometry().allocate(reserved);
    markDirtyGeometry();

    RGBVertexUpdater vertexUpdater{geometry().vertexDataAs<Geometry::RGBColoredPoint2D>()};
    vertexUpdater.addRectangle({0.f,
                                       halfBreadth - 0.5f},
            {static_cast<float>(length),
                    m_isSlipRenderer ? halfBreadth : halfBreadth + 0.5f},
            {static_cast<float>(m_axesColor_r),
                    static_cast<float>(m_axesColor_g),
                    static_cast<float>(m_axesColor_b)});

    const double maxSamplingRange = visualIncrementPerPixel / 2.0;

    for (int pos = 0; pos < pixelLength; ++pos) {
        const int visualFrameStart = std::lround(xVisualFrame - maxSamplingRange);
        const int visualFrameStop = std::lround(xVisualFrame + maxSamplingRange);

        const float fpos = static_cast<float>(pos) * invDevicePixelRatio;
        const float x1 = fpos - halfPixelSize;
        const float x2 = fpos + halfPixelSize;

        // AI-generated (Claude Code). Start of AI-generated text.
        const mixxx::threeband::Peaks peaks = mixxx::threeband::peaksForColumn(
                data, dataSize, visualFrameStart, visualFrameStop, lowHold, midHold);
        const uchar bandPeak[kBandCount] = {peaks.low, peaks.mid, peaks.high};

        // Low, mid, high: a later band covers an earlier band.
        for (int band = 0; band < kBandCount; ++band) {
            const float height = std::min(
                    bandPeak[band] * bandHeightFactor[band], halfBreadth);
            const bool drawRim = height >= minRimHeight;
            addBandHalf(vertexUpdater,
                    x1,
                    x2,
                    halfBreadth,
                    -1.f,
                    height,
                    drawRim,
                    invDevicePixelRatio,
                    kBandColors[band]);
            // The slip renderer draws the upper half only.
            addBandHalf(vertexUpdater,
                    x1,
                    x2,
                    halfBreadth,
                    1.f,
                    m_isSlipRenderer ? 0.f : height,
                    drawRim && !m_isSlipRenderer,
                    invDevicePixelRatio,
                    kBandColors[band]);
        }
        // End of AI-generated text.

        xVisualFrame += visualIncrementPerPixel;
    }

    DEBUG_ASSERT(reserved == vertexUpdater.index());

    markDirtyMaterial();

    return true;
}

} // namespace allshader
