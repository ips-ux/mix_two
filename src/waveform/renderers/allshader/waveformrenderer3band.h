#pragma once

#include "rendergraph/geometrynode.h"
#include "util/class.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"

namespace allshader {
class WaveformRenderer3Band;
} // namespace allshader

// AI-generated (Claude Code). Start of AI-generated text.
/// The "3Band" scrolling waveform, as rekordbox does. The low (blue), mid
/// (orange) and high (white) band are drawn in this order, mirrored about
/// the centre line, so a later band covers an earlier band. Each band is
/// brightest at the centre line and darker at its edge, and has a lit rim
/// of 1 pixel on its edge. The split stereo option is not used.
// End of AI-generated text.
class allshader::WaveformRenderer3Band final
        : public allshader::WaveformRendererSignalBase,
          public rendergraph::GeometryNode {
  public:
    explicit WaveformRenderer3Band(WaveformWidgetRenderer* waveformWidget,
            ::WaveformRendererAbstract::PositionSource type =
                    ::WaveformRendererAbstract::Play,
            ::WaveformRendererSignalBase::Options options =
                    ::WaveformRendererSignalBase::Option::None);

    // Pure virtual from WaveformRendererSignalBase, not used
    void onSetup(const QDomNode& node) override;

    bool supportsSlip() const override {
        return true;
    }

    // Virtuals for rendergraph::Node
    void preprocess() override;

  private:
    bool m_isSlipRenderer;

    bool preprocessInner();

    DISALLOW_COPY_AND_ASSIGN(WaveformRenderer3Band);
};
