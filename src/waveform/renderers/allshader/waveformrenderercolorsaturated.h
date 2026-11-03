#pragma once

#include <array>

#include "rendergraph/geometrynode.h"
#include "util/class.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"

namespace allshader {
class WaveformRendererColorSaturated;
} // namespace allshader

// <!-- AI-generated (Claude Code, 2026-11-02). Start of AI-generated text. -->
/// The "Color Saturated" scrolling waveform. Each column has the height of the
/// loudest band. Its colour goes from the base colour (from the preferences)
/// to white as the mid band gets smaller than the loudest band.
// <!-- End of AI-generated text. -->
class allshader::WaveformRendererColorSaturated final
        : public allshader::WaveformRendererSignalBase,
          public rendergraph::GeometryNode {
  public:
    explicit WaveformRendererColorSaturated(WaveformWidgetRenderer* waveformWidget,
            ::WaveformRendererAbstract::PositionSource type =
                    ::WaveformRendererAbstract::Play,
            ::WaveformRendererSignalBase::Options options =
                    ::WaveformRendererSignalBase::Option::None);

    void onSetup(const QDomNode& node) override;

    bool supportsSlip() const override {
        return true;
    }

    // Virtuals for rendergraph::Node
    void preprocess() override;

  private:
    struct ColorF {
        float r;
        float g;
        float b;
    };

    bool m_isSlipRenderer;
    ::WaveformRendererSignalBase::Options m_options;
    // One colour for each whiteness value (0-7).
    std::array<ColorF, 8> m_colors;

    void updateColors();
    bool preprocessInner();

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererColorSaturated);
};
