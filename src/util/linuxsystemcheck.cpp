// AI-generated (Claude Code). Start of AI-generated text.
#include "util/linuxsystemcheck.h"

#include <QFile>
#include <QRegularExpression>
#include <QSysInfo>

#include "controllers/controllermappinginfo.h"
#include "controllers/midi/midicontroller.h"

#ifdef Q_OS_LINUX
#include "controllers/midi/midiportwatcher.h"
#endif

namespace mixxx {

bool kernelOlderThan(const QString& release, int major, int minor) {
    static const QRegularExpression kVersion(QStringLiteral("^\\s*(\\d+)\\.(\\d+)"));
    const QRegularExpressionMatch match = kVersion.match(release);
    if (!match.hasMatch()) {
        return false;
    }
    bool majorOk = false;
    bool minorOk = false;
    const int releaseMajor = match.captured(1).toInt(&majorOk);
    const int releaseMinor = match.captured(2).toInt(&minorOk);
    if (!majorOk || !minorOk) {
        return false;
    }
    if (releaseMajor != major) {
        return releaseMajor < major;
    }
    return releaseMinor < minor;
}

bool soundCardsListDdj800(const QString& cardsText) {
    static const QRegularExpression kDdj800(QStringLiteral("\\bDDJ-800\\b"),
            QRegularExpression::CaseInsensitiveOption);
    return kDdj800.match(cardsText).hasMatch();
}

bool rtNoticeNeeded(unsigned int cur, unsigned int max) {
    Q_UNUSED(max);
    return cur < kRequiredRtPrio;
}

bool shouldShowNotice(bool conditionHolds, bool alreadyShown) {
    return conditionHolds && !alreadyShown;
}

QString currentKernelRelease() {
#ifdef Q_OS_LINUX
    // On Linux this is the text of `uname -r`.
    return QSysInfo::kernelVersion();
#else
    return QString();
#endif
}

bool isDdj800Connected(const QStringList& midiPortNames, const QString& cardsText) {
    // The ALSA snapshot contains both client and port rows. Match the quoted
    // port name, not the row metadata or a client with no matching port.
    static const QRegularExpression kAlsaPort(QStringLiteral("^Port\\s+\\d+\\s*:\\s*\"([^\"]+)\""));
    ProductInfo ddj800;
    ddj800.protocol = QStringLiteral("midi");
    ddj800.device_name = QStringLiteral("DDJ-800");
    for (const QString& row : midiPortNames) {
        const auto match = kAlsaPort.match(row);
        if (match.hasMatch() && MidiController::matchProductInfo(match.captured(1), ddj800)) {
            return true;
        }
    }
    return soundCardsListDdj800(cardsText);
}

bool isDdj800Connected() {
#ifdef Q_OS_LINUX
    const QStringList midiPortNames = midi::currentPortNames().value_or(QStringList{});
    QFile cards(QStringLiteral("/proc/asound/cards"));
    const QString cardsText = cards.open(QIODevice::ReadOnly | QIODevice::Text)
            ? QString::fromUtf8(cards.readAll())
            : QString();
    return isDdj800Connected(midiPortNames, cardsText);
#else
    return false;
#endif
}

} // namespace mixxx

// End of AI-generated text.
