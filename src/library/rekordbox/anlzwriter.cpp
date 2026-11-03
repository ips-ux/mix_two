#include "library/rekordbox/anlzwriter.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>

#include "library/rekordbox/devicelibraryplusmapping.h"

namespace mixxx {
namespace rekordbox {

namespace {

constexpr quint32 kNoLoopEnd = 0xffffffff;
constexpr quint32 kPcptEntrySize = 56;
constexpr int kPvbrIndexEntries = 400;
// The legacy list in the .DAT file holds the hot cues A-C, the one in the
// .EXT file holds D-H.
constexpr int kHotCuesInDat = 3;

/// Appends big-endian values to a byte array.
class ByteWriter {
  public:
    ByteWriter& u8(quint8 value) {
        m_bytes.append(static_cast<char>(value));
        return *this;
    }
    ByteWriter& u16(quint16 value) {
        u8(static_cast<quint8>(value >> 8));
        return u8(static_cast<quint8>(value & 0xff));
    }
    ByteWriter& u32(quint32 value) {
        u16(static_cast<quint16>(value >> 16));
        return u16(static_cast<quint16>(value & 0xffff));
    }
    ByteWriter& raw(const QByteArray& bytes) {
        m_bytes.append(bytes);
        return *this;
    }
    ByteWriter& raw(const char* pBytes, int size) {
        m_bytes.append(pBytes, size);
        return *this;
    }
    ByteWriter& zeros(int count) {
        m_bytes.append(QByteArray(count, '\0'));
        return *this;
    }
    const QByteArray& bytes() const {
        return m_bytes;
    }

  private:
    QByteArray m_bytes;
};

/// The text as rekordbox stores it: UTF-16BE and a final u16 0. An empty text
/// has no bytes.
QByteArray textBytes(const QString& text) {
    if (text.isEmpty()) {
        return QByteArray();
    }
    ByteWriter writer;
    for (const QChar character : text) {
        writer.u16(character.unicode());
    }
    writer.u16(0);
    return writer.bytes();
}

/// A time in ms plus the timing offset, not below 0.
quint32 withOffset(quint32 ms, int offsetMs) {
    const qint64 shifted = static_cast<qint64>(ms) + offsetMs;
    return static_cast<quint32>(std::clamp<qint64>(
            shifted, 0, std::numeric_limits<quint32>::max()));
}

/// The end of a loop in ms plus the timing offset, or 0xffffffff for a cue.
quint32 loopEndWithOffset(const AnlzCue& cue, int offsetMs) {
    return cue.loopEndMs.has_value() ? withOffset(*cue.loopEndMs, offsetMs)
                                     : kNoLoopEnd;
}

/// 1 for a cue, 2 for a loop.
quint8 entryType(const AnlzCue& cue) {
    return cue.loopEndMs.has_value() ? 2 : 1;
}

/// The three bytes in front of the cue time: 0x0003e8 (= 1000).
ByteWriter& cueTimeBase(ByteWriter& writer) {
    return writer.u8(0).u8(0x03).u8(0xe8);
}

QByteArray pathSection(const AnlzTrackData& data) {
    const QByteArray path = textBytes(data.stickPath);
    ByteWriter body;
    body.u32(static_cast<quint32>(path.size())).raw(path);
    return anlz::section("PPTH", 16, body.bytes());
}

QByteArray vbrSection(const AnlzTrackData& data) {
    ByteWriter body;
    body.u32(0).zeros(kPvbrIndexEntries * 4).u32(data.mp3PcmFrames);
    return anlz::section("PVBR", 16, body.bytes());
}

QByteArray beatGridSection(const AnlzTrackData& data) {
    ByteWriter body;
    body.u32(0).u32(0x80000).u32(static_cast<quint32>(data.beats.size()));
    for (int i = 0; i < data.beats.size(); ++i) {
        const GridBeat& beat = data.beats[i];
        body.u16(static_cast<quint16>(data.beatInBar.value(i, 1)))
                .u16(static_cast<quint16>(std::clamp(beat.tempo, 0, 0xffff)))
                .u32(anlz::msFromFrames(beat.position, data.sampleRate, data.timingOffsetMs));
    }
    return anlz::section("PQTZ", 24, body.bytes());
}

/// PWAV and PWV2: the length, a constant and the bytes.
QByteArray previewSection(const char* fourcc, const QByteArray& bytes) {
    ByteWriter body;
    body.u32(static_cast<quint32>(bytes.size())).u32(0x10000).raw(bytes);
    return anlz::section(fourcc, 20, body.bytes());
}

/// PWV3, PWV5, PWV7 and PWV4: the size of an entry, the number of entries, a
/// constant and the bytes.
QByteArray entriesSection(const char* fourcc,
        quint32 entryBytes,
        quint32 constant,
        const QByteArray& bytes) {
    ByteWriter body;
    body.u32(entryBytes)
            .u32(static_cast<quint32>(bytes.size()) / entryBytes)
            .u32(constant)
            .raw(bytes);
    return anlz::section(fourcc, 24, body.bytes());
}

/// PWV6 is like PWV7 without the constant.
QByteArray threeBandPreviewSection(const QByteArray& bytes) {
    ByteWriter body;
    body.u32(3).u32(static_cast<quint32>(bytes.size()) / 3).raw(bytes);
    return anlz::section("PWV6", 20, body.bytes());
}

QByteArray bandPercentSection() {
    // Inferred: the percentages of the three bands. One track of the reference
    // stick has 100 100 100, others have 80-131.
    ByteWriter body;
    body.u16(0).u16(100).u16(100).u16(100);
    return anlz::section("PWVC", 14, body.bytes());
}

QByteArray cueEntry(const AnlzCue& cue, int offsetMs) {
    ByteWriter entry;
    entry.raw("PCPT", 4)
            .u32(28)
            .u32(kPcptEntrySize)
            .u32(static_cast<quint32>(cue.hotCue))
            .u32(0)
            .u32(0x10000)
            .u16(0xffff)
            .u16(0xffff)
            .u8(entryType(cue));
    cueTimeBase(entry)
            .u32(withOffset(cue.timeMs, offsetMs))
            .u32(loopEndWithOffset(cue, offsetMs))
            .zeros(16);
    return entry.bytes();
}

QByteArray extendedCueEntry(const AnlzCue& cue, int offsetMs) {
    const QByteArray comment = textBytes(cue.comment);
    const bool isHotCue = cue.hotCue != 0;
    const QRgb color = cue.color.value_or(qRgb(0, 0, 0));

    ByteWriter entry;
    entry.raw("PCP2", 4)
            .u32(16)
            .u32(88 + static_cast<quint32>(comment.size()))
            .u32(static_cast<quint32>(cue.hotCue))
            .u8(entryType(cue));
    cueTimeBase(entry)
            .u32(withOffset(cue.timeMs, offsetMs))
            .u32(loopEndWithOffset(cue, offsetMs))
            .u8(isHotCue ? 0 : static_cast<quint8>(dlpColorId(cue.color)))
            .u8(1)
            .zeros(6)
            .u16(0)
            .u16(0)
            .u32(static_cast<quint32>(comment.size()))
            .raw(comment)
            .u8(0)
            .u8(isHotCue ? qRed(color) : 0)
            .u8(isHotCue ? qGreen(color) : 0)
            .u8(isHotCue ? qBlue(color) : 0)
            .zeros(40);
    return entry.bytes();
}

enum class CueListType : quint32 {
    Memory = 0,
    Hot = 1,
};

/// The memory_count field of a PCOB list: 0xffffffff for a hot cue list and
/// for an empty memory cue list, 0 for a memory cue list with cues (Inferred
/// from lists of 0 and 1 cues).
quint32 memoryCountOf(CueListType type, int cueCount) {
    return (type == CueListType::Memory && cueCount > 0) ? 0 : 0xffffffff;
}

QByteArray cueListSection(CueListType type, const QList<AnlzCue>& cues, int offsetMs) {
    ByteWriter body;
    body.u32(static_cast<quint32>(type))
            .u16(0)
            .u16(static_cast<quint16>(cues.size()))
            .u32(memoryCountOf(type, static_cast<int>(cues.size())));
    for (const AnlzCue& cue : cues) {
        body.raw(cueEntry(cue, offsetMs));
    }
    return anlz::section("PCOB", 24, body.bytes());
}

QByteArray extendedCueListSection(
        CueListType type, const QList<AnlzCue>& cues, int offsetMs) {
    ByteWriter body;
    body.u32(static_cast<quint32>(type))
            .u16(static_cast<quint16>(cues.size()))
            .u16(0);
    for (const AnlzCue& cue : cues) {
        body.raw(extendedCueEntry(cue, offsetMs));
    }
    return anlz::section("PCO2", 20, body.bytes());
}

/// The hot cues of the list in the .DAT file (A-C) or of the one in the .EXT
/// file (D-H).
QList<AnlzCue> hotCuesOf(const AnlzTrackData& data, bool inDat) {
    QList<AnlzCue> result;
    for (const AnlzCue& cue : data.hotCues) {
        if ((cue.hotCue <= kHotCuesInDat) == inDat) {
            result.append(cue);
        }
    }
    return result;
}

QString writeErrorMessage(const QString& what) {
    return QStringLiteral("Cannot write the analysis files: %1").arg(what);
}

} // namespace

namespace anlz {

QByteArray fileBytes(const QList<QByteArray>& sections) {
    constexpr quint32 kHeaderSize = 28;
    quint32 total = kHeaderSize;
    for (const QByteArray& section : sections) {
        total += static_cast<quint32>(section.size());
    }
    ByteWriter file;
    file.raw("PMAI", 4)
            .u32(kHeaderSize)
            .u32(total)
            .u32(1)
            .u32(0x10000)
            .u32(0x10000)
            .u32(0);
    for (const QByteArray& section : sections) {
        file.raw(section);
    }
    return file.bytes();
}

QByteArray section(const char* fourcc, quint32 lenHeader, const QByteArray& body) {
    ByteWriter section;
    section.raw(fourcc, 4)
            .u32(lenHeader)
            .u32(12 + static_cast<quint32>(body.size()))
            .raw(body);
    return section.bytes();
}

QByteArray datBytes(const AnlzTrackData& data) {
    const AnlzWaveforms& w = data.waveforms;
    return fileBytes({
            pathSection(data),
            vbrSection(data),
            beatGridSection(data),
            previewSection("PWAV", w.preview),
            previewSection("PWV2", w.tinyPreview),
            cueListSection(CueListType::Hot, hotCuesOf(data, true), data.timingOffsetMs),
            cueListSection(CueListType::Memory, data.memoryCues, data.timingOffsetMs),
    });
}

QByteArray extBytes(const AnlzTrackData& data) {
    const AnlzWaveforms& w = data.waveforms;
    return fileBytes({
            pathSection(data),
            entriesSection("PWV3", 1, 0x960000, w.detail),
            cueListSection(CueListType::Hot, hotCuesOf(data, false), data.timingOffsetMs),
            // The memory cue list of the .EXT file is always empty (spec F20).
            cueListSection(CueListType::Memory, QList<AnlzCue>(), data.timingOffsetMs),
            extendedCueListSection(CueListType::Hot, data.hotCues, data.timingOffsetMs),
            extendedCueListSection(CueListType::Memory, data.memoryCues, data.timingOffsetMs),
            entriesSection("PWV5", 2, 0x960305, w.colorDetail),
            entriesSection("PWV4", 6, 0, w.colorPreview),
    });
}

QByteArray twoExBytes(const AnlzTrackData& data) {
    const AnlzWaveforms& w = data.waveforms;
    return fileBytes({
            pathSection(data),
            entriesSection("PWV7", 3, 0x960000, w.threeBandDetail),
            threeBandPreviewSection(w.threeBandPreview),
            bandPercentSection(),
    });
}

quint32 msFromFrames(audio::FramePos position,
        audio::SampleRate sampleRate,
        int timingOffsetMs) {
    if (sampleRate.value() == 0 || !position.isValid() ||
            !std::isfinite(position.value())) {
        return 0;
    }
    const double ms = std::round(position.value() * 1000.0 / sampleRate.value());
    const double shifted = std::clamp(ms + timingOffsetMs,
            0.0,
            static_cast<double>(std::numeric_limits<quint32>::max()));
    return static_cast<quint32>(shifted);
}

} // namespace anlz

bool writeAnlzFiles(const AnlzTrackData& data,
        const QString& folder,
        QString* pErrorMessage) {
    const auto fail = [&](const QString& what) {
        if (pErrorMessage) {
            *pErrorMessage = writeErrorMessage(what);
        }
        return false;
    };

    if (!QDir().mkpath(folder)) {
        return fail(folder);
    }
    const QDir dir(folder);

    struct Output {
        QString name;
        QByteArray bytes;
        std::unique_ptr<QSaveFile> pFile;
    };
    std::array<Output, 3> outputs = {{
            {QStringLiteral("ANLZ0000.DAT"), anlz::datBytes(data), nullptr},
            {QStringLiteral("ANLZ0000.EXT"), anlz::extBytes(data), nullptr},
            {QStringLiteral("ANLZ0000.2EX"), anlz::twoExBytes(data), nullptr},
    }};
    const auto cancelAll = [&outputs]() {
        for (Output& output : outputs) {
            if (output.pFile) {
                output.pFile->cancelWriting();
            }
        }
    };

    // Write all three files to their temporary files first. Nothing is in
    // place until every write has succeeded.
    for (Output& output : outputs) {
        output.pFile = std::make_unique<QSaveFile>(dir.filePath(output.name));
        output.pFile->setDirectWriteFallback(false);
        // flush() makes the buffered bytes reach the file: a full stick can
        // fail here and not in write() (spec 15).
        if (!output.pFile->open(QIODevice::WriteOnly) ||
                output.pFile->write(output.bytes) != output.bytes.size() ||
                !output.pFile->flush()) {
            const QString what =
                    output.name + QStringLiteral(": ") + output.pFile->errorString();
            cancelAll();
            return fail(what);
        }
    }

    for (size_t i = 0; i < outputs.size(); ++i) {
        if (outputs[i].pFile->commit()) {
            continue;
        }
        const QString what = outputs[i].name + QStringLiteral(": ") +
                outputs[i].pFile->errorString();
        cancelAll();
        if (i > 0) {
            // A file of the new set is in place already: leave no mixed set.
            for (const Output& output : outputs) {
                QFile::remove(dir.filePath(output.name));
            }
        }
        return fail(what);
    }
    return true;
}

} // namespace rekordbox
} // namespace mixxx
