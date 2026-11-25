#include "widget/wbeatcount.h"

#include <cmath>

#include "engine/controls/nextcue.h"
#include "moc_wbeatcount.cpp"

namespace mixxx {

QString formatBarsBeats(double beats) {
    // AI-generated (Claude Code). Start of AI-generated text.
    // A count just below a whole beat (7.9999) shows as that beat (2.0), not as 1.3.
    const int wholeBeats = wholeBeatCount(beats);
    // End of AI-generated text.
    if (wholeBeats < 0) {
        return QStringLiteral("-");
    }
    constexpr int kBeatsPerBar = 4;
    return QString::number(wholeBeats / kBeatsPerBar) + QChar('.') +
            QString::number(wholeBeats % kBeatsPerBar);
}

} // namespace mixxx

WBeatCount::WBeatCount(QWidget* pParent)
        : WLabel(pParent) {
}

void WBeatCount::setup(const QDomNode& node, const SkinContext& context) {
    WLabel::setup(node, context);
    setText(mixxx::formatBarsBeats(-1.0));
}

void WBeatCount::onConnectedControlChanged(double dParameter, double dValue) {
    Q_UNUSED(dParameter);
    // Show the control value, not its parameter.
    setText(mixxx::formatBarsBeats(dValue));
}
