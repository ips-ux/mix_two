#include "widget/wbeatcount.h"

#include <cmath>

#include "moc_wbeatcount.cpp"

namespace mixxx {

QString formatBarsBeats(double beats) {
    if (!(beats >= 0.0)) {
        return QStringLiteral("-");
    }
    constexpr int kBeatsPerBar = 4;
    const auto wholeBeats = static_cast<qint64>(std::floor(beats));
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
