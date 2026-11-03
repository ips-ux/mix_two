#include "waveform/renderers/allshader/waveformrenderercolorsaturated.h"

#include "rendergraph/material/rgbmaterial.h"
#include "rendergraph/vertexupdaters/rgbvertexupdater.h"
#include "track/track.h"
#include "util/math.h"
#include "waveform/bandcolor.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"
#include "waveform/waveformwidgetfactory.h"

// <!-- AI-generated (Claude Code, 2026-11-02). Start of AI-generated text. -->
using namespace rendergraph;

namespace allshader {

namespace {
quint8 toBand(float value) {
    return static_cast<quint8>(math_clamp(value, 0.f, 255.f));
}
} // namespace

WaveformRendererColorSaturated::WaveformRendererColorSaturated(
        WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererAbstract::PositionSource type,
        ::WaveformRendererSignalBase::Options options)
        : WaveformRendererSignalBase(waveformWidget, options),
          m_isSlipRenderer(type == ::WaveformRendererAbstract::Slip),
          m_options(options),
          m_colors{} {
    initForRectangles<RGBMaterial>(0);
    setUsePreprocess(true);
    updateColors();
}

void WaveformRendererColorSaturated::onSetup(const QDomNode&) {
    // The factory creates the waveform widgets again when the base colour
    // changes, so the colour table is made here and not for each frame.
    updateColors();
}

void WaveformRendererColorSaturated::updateColors() {
    const QColor base = WaveformWidgetFactory::instance()->getColorSaturatedBaseColor();
    for (std::size_t w = 0; w < m_colors.size(); ++w) {
        const QColor color = mixxx::bandcolor::colorSaturated(base, static_cast<quint8>(w));
        m_colors[w] = ColorF{static_cast<float>(color.redF()),
                static_cast<float>(color.greenF()),
                static_cast<float>(color.blueF())};
    }
}

void WaveformRendererColorSaturated::preprocess() {
    if (!preprocessInner()) {
        if (geometry().vertexCount() != 0) {
            geometry().allocate(0);
            markDirtyGeometry();
        }
    }
}

bool WaveformRendererColorSaturated::preprocessInner() {
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

    // Prevents a division by zero on waveform initialization
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

    const float heightFactorAbs = allGain * halfBreadth / m_maxValue;
    const float heightFactor[2] = {-heightFactorAbs, heightFactorAbs};
    const bool splitLeftRight = m_options & ::WaveformRendererSignalBase::Option::SplitStereoSignal;

    // Effective visual frame for x
    double xVisualFrame = qRound(firstVisualFrame / visualIncrementPerPixel) *
            visualIncrementPerPixel;

    const int numVerticesPerLine = 6; // 2 triangles

    const int reserved = numVerticesPerLine *
            // Slip renderer only render a single channel, so the vertices count doesn't change
            ((splitLeftRight && !m_isSlipRenderer ? pixelLength * 2 : pixelLength) + 1);

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

        const int visualIndexStart = std::max(visualFrameStart * 2, 0);
        const int visualIndexStop =
                std::min(std::max(visualFrameStop, visualFrameStart + 1) * 2, dataSize - 1);

        const float fpos = static_cast<float>(pos) * invDevicePixelRatio;

        // The largest low, mid and high values over the column.
        // Without split stereo, only the first field is used.
        uchar u8maxLow[2]{};
        uchar u8maxMid[2]{};
        uchar u8maxHigh[2]{};
        for (int chn = 0; chn < 2; chn++) {
            const int signalChn = splitLeftRight ? chn : 0;
            // data is interleaved left / right
            for (int i = visualIndexStart + chn; i < visualIndexStop + chn; i += 2) {
                const WaveformData& waveformData = data[i];
                u8maxLow[signalChn] = math_max(u8maxLow[signalChn], waveformData.filtered.low);
                u8maxMid[signalChn] = math_max(u8maxMid[signalChn], waveformData.filtered.mid);
                u8maxHigh[signalChn] = math_max(u8maxHigh[signalChn], waveformData.filtered.high);
            }
        }

        for (int chn = 0;
                chn < (splitLeftRight && !m_isSlipRenderer ? 2 : 1);
                chn++) {
            const float maxLow = static_cast<float>(u8maxLow[chn]) * lowGain;
            const float maxMid = static_cast<float>(u8maxMid[chn]) * midGain;
            const float maxHigh = static_cast<float>(u8maxHigh[chn]) * highGain;

            // The height is the loudest band, the colour comes from the
            // whiteness of the same bands (see bandcolor.h).
            const float height = math_max3(maxLow, maxMid, maxHigh);
            const mixxx::bandcolor::Bands bands{
                    toBand(maxLow), toBand(maxMid), toBand(maxHigh)};
            const ColorF& color = m_colors[mixxx::bandcolor::whiteness3(bands)];

            if (!splitLeftRight) {
                vertexUpdater.addRectangle(
                        {fpos - halfPixelSize,
                                halfBreadth - heightFactorAbs * height},
                        {fpos + halfPixelSize,
                                m_isSlipRenderer
                                        ? halfBreadth
                                        : halfBreadth + heightFactorAbs * height},
                        {color.r, color.g, color.b});
            } else {
                // heightFactor is negative for left (chn 0) and positive for
                // right (chn 1).
                vertexUpdater.addRectangle({fpos - halfPixelSize, halfBreadth},
                        {fpos + halfPixelSize,
                                halfBreadth + heightFactor[chn] * height},
                        {color.r, color.g, color.b});
            }
        }

        xVisualFrame += visualIncrementPerPixel;
    }

    DEBUG_ASSERT(reserved == vertexUpdater.index());

    markDirtyMaterial();

    return true;
}

} // namespace allshader
// <!-- End of AI-generated text. -->
