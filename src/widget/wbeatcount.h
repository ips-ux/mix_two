#pragma once

#include <QString>

#include "widget/wlabel.h"

namespace mixxx {

// AI-generated (Claude Code). Start of AI-generated text.
/// Formats a beat count as bars.beats with 4 beats per bar: 51 beats are
/// "12.3" (12 bars and 3 beats). A negative value (no count) is "-".
// End of AI-generated text.
QString formatBarsBeats(double beats);

} // namespace mixxx

// AI-generated (Claude Code). Start of AI-generated text.
/// A label that shows its connected control (a beat count, for example
/// [ChannelN],beats_to_next_cue) as bars.beats.
// End of AI-generated text.
class WBeatCount : public WLabel {
    Q_OBJECT
  public:
    explicit WBeatCount(QWidget* pParent = nullptr);

    void setup(const QDomNode& node, const SkinContext& context) override;

    void onConnectedControlChanged(double dParameter, double dValue) override;
};
