// AI-generated (Claude Code). Start of AI-generated text.
// The RGB overview mixes the band colours of the skin, as the zoomed RGB
// waveform does.
// End of AI-generated text.
#include "waveform/renderers/waveformoverviewrenderer.h"

#include <gtest/gtest.h>

#include <QDomDocument>
#include <QImage>

#include "skin/legacy/skincontext.h"
#include "test/mixxxtest.h"
#include "waveform/renderers/waveformsignalcolors.h"

namespace {

class WaveformOverviewRendererTest : public MixxxTest {
  protected:
    WaveformOverviewRendererTest()
            : m_context(config(), QStringLiteral("test")) {
    }

    /// Signal colours from a skin node with these RGB band colours.
    WaveformSignalColors colors(const QString& low, const QString& mid, const QString& high) {
        QDomDocument doc;
        QDomElement node = doc.createElement(QStringLiteral("Overview"));
        const auto add = [&](const QString& name, const QString& value) {
            QDomElement child = doc.createElement(name);
            child.appendChild(doc.createTextNode(value));
            node.appendChild(child);
        };
        add(QStringLiteral("SignalColor"), QStringLiteral("#ffffff"));
        add(QStringLiteral("SignalRGBLowColor"), low);
        add(QStringLiteral("SignalRGBMidColor"), mid);
        add(QStringLiteral("SignalRGBHighColor"), high);
        WaveformSignalColors result;
        result.setup(node, m_context);
        return result;
    }

    /// One second of a waveform that has only the mid band.
    static ConstWaveformPointer midOnlyWaveform() {
        auto pWaveform = WaveformPointer::create(44100, 44100, 441, -1, 0);
        WaveformData* pData = pWaveform->data();
        for (int i = 0; i < pWaveform->getDataSize(); ++i) {
            pData[i].filtered.low = 0;
            pData[i].filtered.mid = 200;
            pData[i].filtered.high = 0;
            pData[i].filtered.all = 200;
        }
        return pWaveform;
    }

    SkinContext m_context;
};

TEST_F(WaveformOverviewRendererTest, RgbUsesTheSkinBandColors) {
    const QImage image = waveformOverviewRenderer::render(midOnlyWaveform(),
            mixxx::OverviewType::RGB,
            colors(QStringLiteral("#ff0000"),
                    QStringLiteral("#00e7d3"),
                    QStringLiteral("#0000dc")),
            QColor(Qt::blue));
    ASSERT_FALSE(image.isNull());

    // The mid colour, made as bright as possible: #00e7d3 * 255 / 0xe7.
    const QColor color = image.pixelColor(image.width() / 2, image.height() / 2 - 20);
    EXPECT_EQ(0, color.red());
    EXPECT_EQ(255, color.green());
    EXPECT_NEAR(233, color.blue(), 2);
}

} // namespace
