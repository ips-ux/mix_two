#include "controllers/midi/midiportwatcher.h"

#include <QFile>
#include <QRegularExpression>

#if defined(Q_OS_WIN)
#include <windows.h>
// windows.h must be included first
#include <mmsystem.h>
#elif defined(Q_OS_MACOS)
#include <CoreMIDI/CoreMIDI.h>
#endif

namespace mixxx {

namespace midi {

namespace {

#if defined(Q_OS_MACOS)
QString endpointName(MIDIEndpointRef endpoint) {
    CFStringRef name = nullptr;
    if (MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name) != noErr ||
            !name) {
        return QString();
    }
    const QString result = QString::fromCFString(name);
    CFRelease(name);
    return result;
}
#endif

} // namespace

QStringList parseAlsaSeqClients(const QString& procContents) {
    // Keep lines such as
    //   Client  24 : "DDJ-800" [Kernel Legacy]
    //     Port   0 : "DDJ-800 MIDI 1" (RWeX)
    // and drop the pool statistics in between.
    // The "Client info" header is skipped: only numbered entries are kept.
    static const QRegularExpression kEntry(QStringLiteral("^(Client|Port)\\s+\\d+\\s*:"));
    QStringList result;
    const QStringList lines = procContents.split(QChar('\n'));
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (kEntry.match(trimmed).hasMatch()) {
            result.append(trimmed);
        }
    }
    return result;
}

std::optional<QStringList> currentPortNames() {
    QStringList names;
#if defined(Q_OS_WIN)
    const UINT numInputs = midiInGetNumDevs();
    for (UINT i = 0; i < numInputs; ++i) {
        MIDIINCAPSW caps;
        if (midiInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            names.append(QStringLiteral("in:") +
                    QString::fromWCharArray(caps.szPname));
        }
    }
    const UINT numOutputs = midiOutGetNumDevs();
    for (UINT i = 0; i < numOutputs; ++i) {
        MIDIOUTCAPSW caps;
        if (midiOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            names.append(QStringLiteral("out:") +
                    QString::fromWCharArray(caps.szPname));
        }
    }
#elif defined(Q_OS_MACOS)
    const ItemCount numSources = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < numSources; ++i) {
        names.append(QStringLiteral("in:") + endpointName(MIDIGetSource(i)));
    }
    const ItemCount numDestinations = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < numDestinations; ++i) {
        names.append(QStringLiteral("out:") + endpointName(MIDIGetDestination(i)));
    }
#elif defined(Q_OS_LINUX)
    QFile clients(QStringLiteral("/proc/asound/seq/clients"));
    if (!clients.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    names = parseAlsaSeqClients(QString::fromUtf8(clients.readAll()));
#else
    return std::nullopt;
#endif
    names.sort();
    return names;
}

} // namespace midi

} // namespace mixxx
