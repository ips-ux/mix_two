// AI-generated (Claude Code). Start of AI-generated text.
#include "util/linuxsystemcheck.h"

#include <QFile>
#include <QRegularExpression>
#include <QSysInfo>

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
    // "DDJ-8000" is another controller: no digit may follow.
    static const QRegularExpression kDdj800(QStringLiteral("DDJ-800(?!\\d)"),
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

bool isDdj800Connected() {
#ifdef Q_OS_LINUX
    QFile cards(QStringLiteral("/proc/asound/cards"));
    if (!cards.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    return soundCardsListDdj800(QString::fromUtf8(cards.readAll()));
#else
    return false;
#endif
}

} // namespace mixxx

// End of AI-generated text.
