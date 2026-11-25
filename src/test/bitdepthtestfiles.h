#pragma once

// AI-generated (Claude Code). Start of AI-generated text.
// Makes small PCM files with a chosen bit depth, for the tests of the bit
// depth column. The files in the test data folder are all 16-bit.

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QString>

namespace bitdepthtest {

inline QByteArray bytesOf(std::initializer_list<int> values) {
    QByteArray bytes;
    for (const int value : values) {
        bytes.append(static_cast<char>(value));
    }
    return bytes;
}
inline QByteArray u16le(quint16 v) {
    return bytesOf({v & 0xff, v >> 8});
}
inline QByteArray u32le(quint32 v) {
    return u16le(v & 0xffff) + u16le(v >> 16);
}
inline QByteArray u16be(quint16 v) {
    return bytesOf({v >> 8, v & 0xff});
}
inline QByteArray u32be(quint32 v) {
    return u16be(v >> 16) + u16be(v & 0xffff);
}

/// A stereo WAV file of silence, 44100 Hz, `bits` per sample (a multiple of 8).
inline QByteArray wavBytes(int bits, int frames = 44100) {
    const int frameBytes = 2 * bits / 8;
    const QByteArray pcm(frames * frameBytes, '\0');
    const QByteArray fmt = u16le(1) + u16le(2) + u32le(44100) +
            u32le(44100 * frameBytes) + u16le(frameBytes) + u16le(bits);
    const QByteArray body = QByteArray("WAVE") + "fmt " + u32le(fmt.size()) +
            fmt + "data" + u32le(pcm.size()) + pcm;
    return QByteArray("RIFF") + u32le(body.size()) + body;
}

/// A stereo AIFF file of silence, 44100 Hz, `bits` per sample (a multiple of 8).
inline QByteArray aiffBytes(int bits, int frames = 44100) {
    const int frameBytes = 2 * bits / 8;
    const QByteArray pcm(frames * frameBytes, '\0');
    // 80-bit extended 44100: exponent 0x400E, mantissa 0xAC44 << 48.
    const QByteArray comm = u16be(2) + u32be(frames) + u16be(bits) +
            u16be(0x400e) + u32be(0xac440000) + u32be(0);
    const QByteArray ssnd = u32be(0) + u32be(0) + pcm;
    const QByteArray body = QByteArray("AIFF") + "COMM" + u32be(comm.size()) +
            comm + "SSND" + u32be(ssnd.size()) + ssnd;
    return QByteArray("FORM") + u32be(body.size()) + body;
}

inline QString writeFile(const QDir& dir, const QString& name, const QByteArray& bytes) {
    const QString path = dir.filePath(name);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(bytes);
    }
    return path;
}

} // namespace bitdepthtest
// End of AI-generated text.
