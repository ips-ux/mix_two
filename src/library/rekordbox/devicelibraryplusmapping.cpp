#include "library/rekordbox/devicelibraryplusmapping.h"

#include <mp3guessenc.h>

#include <QByteArray>
#include <QCryptographicHash>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QtDebug>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

#include "track/keyutils.h"
#include "track/taglib/trackmetadata_file.h"
#include "util/mp3timingshift.h"

namespace mixxx {
namespace rekordbox {

namespace {

struct RekordboxColor {
    int id;
    QRgb rgb;
};

// The eight colours that rekordbox gives to memory cues.
constexpr std::array<RekordboxColor, 8> kRekordboxColors = {{
        {1, qRgb(0xF8, 0x70, 0xF8)}, // Pink
        {2, qRgb(0xF8, 0x09, 0x00)}, // Red
        {3, qRgb(0xF8, 0xA0, 0x30)}, // Orange
        {4, qRgb(0xF8, 0xE3, 0x31)}, // Yellow
        {5, qRgb(0x1E, 0xE0, 0x00)}, // Green
        {6, qRgb(0x16, 0xC0, 0xF8)}, // Aqua
        {7, qRgb(0x01, 0x50, 0xF8)}, // Blue
        {8, qRgb(0x98, 0x08, 0xF8)}, // Purple
}};

constexpr int kMaxNameUnits = 255;

bool isUnsafeNameChar(QChar c) {
    switch (c.unicode()) {
    case '\\':
    case '/':
    case ':':
    case '*':
    case '?':
    case '"':
    case '<':
    case '>':
    case '|':
        return true;
    default:
        return c.unicode() < 0x20;
    }
}

void removeTrailingDotsAndSpaces(QString* pName) {
    int size = pName->size();
    while (size > 0 &&
            (pName->at(size - 1) == QLatin1Char('.') ||
                    pName->at(size - 1) == QLatin1Char(' '))) {
        --size;
    }
    pName->truncate(size);
}

bool isWindowsDeviceName(const QString& name) {
    // The device name is the part before the first dot, without trailing spaces.
    const int dot = name.indexOf(QLatin1Char('.'));
    QString base = dot >= 0 ? name.left(dot) : name;
    while (base.endsWith(QLatin1Char(' '))) {
        base.chop(1);
    }
    base = base.toUpper();
    if (base == QLatin1String("CON") || base == QLatin1String("PRN") ||
            base == QLatin1String("AUX") || base == QLatin1String("NUL")) {
        return true;
    }
    if (base.size() == 4 &&
            (base.startsWith(QLatin1String("COM")) ||
                    base.startsWith(QLatin1String("LPT")))) {
        const QChar digit = base.at(3);
        return digit >= QLatin1Char('1') && digit <= QLatin1Char('9');
    }
    return false;
}

/// The first `units` UTF-16 units of the text, not cut inside a surrogate pair.
QString leftWithoutSplittingPair(const QString& text, int units) {
    if (units >= text.size()) {
        return text;
    }
    if (units > 0 && text.at(units - 1).isHighSurrogate() &&
            text.at(units).isLowSurrogate()) {
        --units;
    }
    return text.left(units);
}

// MPEG audio, Layer III. The version bits are 3 for MPEG-1, 2 for MPEG-2 and 0
// for MPEG-2.5 (1 is reserved).
constexpr int kMpeg1Layer3Bitrates[16] = {
        0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
constexpr int kMpeg2Layer3Bitrates[16] = {
        0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
constexpr int kMpeg1SampleRates[4] = {44100, 48000, 32000, 0};
constexpr int kMpeg2SampleRates[4] = {22050, 24000, 16000, 0};
constexpr int kMpeg25SampleRates[4] = {11025, 12000, 8000, 0};
// Where a VBRI header stands, from the start of the frame.
constexpr qsizetype kVbriOffset = 36;

struct MpegFrame {
    int length = 0;
    int samples = 0;
    // Where "Xing" or "Info" stands, from the start of the frame.
    int infoOffset = 0;
};

/// Reads the header at `pos`. False when it is not a valid Layer III header.
bool parseMpegFrame(const QByteArray& data, qsizetype pos, MpegFrame* pFrame) {
    if (pos < 0 || pos + 4 > data.size()) {
        return false;
    }
    const auto b = [&](qsizetype i) {
        return static_cast<quint8>(data.at(pos + i));
    };
    if (b(0) != 0xff || (b(1) & 0xe0) != 0xe0) {
        return false;
    }
    const int version = (b(1) >> 3) & 0x03;
    const int layer = (b(1) >> 1) & 0x03;
    if (version == 1 || layer != 1) {
        return false;
    }
    const bool mpeg1 = version == 3;
    const int bitrate = (mpeg1 ? kMpeg1Layer3Bitrates : kMpeg2Layer3Bitrates)[b(2) >> 4];
    const int sampleRateIndex = (b(2) >> 2) & 0x03;
    const int sampleRate = version == 3 ? kMpeg1SampleRates[sampleRateIndex]
            : version == 2              ? kMpeg2SampleRates[sampleRateIndex]
                                        : kMpeg25SampleRates[sampleRateIndex];
    if (bitrate == 0 || sampleRate == 0) {
        return false;
    }
    const int padding = (b(2) >> 1) & 0x01;
    pFrame->length = (mpeg1 ? 144 : 72) * bitrate * 1000 / sampleRate + padding;
    pFrame->samples = mpeg1 ? 1152 : 576;
    const bool mono = (b(3) >> 6) == 3;
    const bool hasCrc = (b(1) & 0x01) == 0;
    const int sideInfo = mpeg1 ? (mono ? 17 : 32) : (mono ? 9 : 17);
    pFrame->infoOffset = 4 + (hasCrc ? 2 : 0) + sideInfo;
    return pFrame->length >= 4;
}

bool hasTagAt(const QByteArray& data, qsizetype pos, const char* tag) {
    const qsizetype size = static_cast<qsizetype>(std::strlen(tag));
    return pos >= 0 && pos + size <= data.size() &&
            std::memcmp(data.constData() + pos, tag, static_cast<size_t>(size)) == 0;
}

/// The end of an ID3v2 tag that starts at `pos`, or `pos` when there is none.
qsizetype id3v2End(const QByteArray& data, qsizetype pos) {
    if (!hasTagAt(data, pos, "ID3") || pos + 10 > data.size()) {
        return pos;
    }
    const auto b = [&](qsizetype i) {
        return static_cast<quint8>(data.at(pos + i));
    };
    if ((b(6) | b(7) | b(8) | b(9)) & 0x80) {
        return pos; // not a sync-safe size
    }
    const qsizetype size = (static_cast<qsizetype>(b(6)) << 21) |
            (static_cast<qsizetype>(b(7)) << 14) |
            (static_cast<qsizetype>(b(8)) << 7) | static_cast<qsizetype>(b(9));
    const bool hasFooter = (b(5) & 0x10) != 0;
    return pos + 10 + size + (hasFooter ? 10 : 0);
}

} // namespace

int dlpFileType(const QString& fileExtension) {
    const QString extension = fileExtension.toLower();
    if (extension == QLatin1String("mp3")) {
        return 1;
    }
    if (extension == QLatin1String("mp4")) {
        return 3;
    }
    if (extension == QLatin1String("m4a") || extension == QLatin1String("aac")) {
        return 4;
    }
    if (extension == QLatin1String("flac")) {
        return 5;
    }
    if (extension == QLatin1String("wav")) {
        return 11;
    }
    if (extension == QLatin1String("aif") || extension == QLatin1String("aiff")) {
        return 12;
    }
    return 0;
}

int dlpColorId(const std::optional<QRgb>& color) {
    if (!color.has_value() || (*color & 0xffffff) == 0) {
        return 0;
    }
    int bestId = 0;
    int bestDistance = std::numeric_limits<int>::max();
    for (const RekordboxColor& candidate : kRekordboxColors) {
        const int dr = qRed(*color) - qRed(candidate.rgb);
        const int dg = qGreen(*color) - qGreen(candidate.rgb);
        const int db = qBlue(*color) - qBlue(candidate.rgb);
        const int distance = dr * dr + dg * dg + db * db;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestId = candidate.id;
        }
    }
    return bestId;
}

QString dlpKeyName(track::io::key::ChromaticKey key) {
    if (key == track::io::key::INVALID) {
        return QString();
    }
    return KeyUtils::keyToString(key, KeyUtils::KeyNotation::Lancelot);
}

int dlpBpmx100(double bpm) {
    if (!std::isfinite(bpm) || !(bpm > 0.0)) {
        return 0;
    }
    return static_cast<int>(std::lround(bpm * 100.0));
}

QString dlpDate(const QDateTime& dateTime) {
    const QDate date = dateTime.isValid() ? dateTime.toLocalTime().date()
                                          : QDate::currentDate();
    return date.toString(QStringLiteral("yyyy-MM-dd"));
}

QString fatSafeName(const QString& name) {
    QString safe = name;
    for (QChar& c : safe) {
        if (isUnsafeNameChar(c)) {
            c = QLatin1Char('_');
        }
    }
    removeTrailingDotsAndSpaces(&safe);
    if (safe.isEmpty()) {
        return QStringLiteral("_");
    }
    if (isWindowsDeviceName(safe)) {
        safe.prepend(QLatin1Char('_'));
    }
    if (safe.size() <= kMaxNameUnits) {
        return safe;
    }

    // Cut the name, but keep the extension.
    const int dot = safe.lastIndexOf(QLatin1Char('.'));
    if (dot > 0 && safe.size() - dot < kMaxNameUnits) {
        const QString extension = safe.mid(dot);
        return leftWithoutSplittingPair(safe.left(dot), kMaxNameUnits - extension.size()) +
                extension;
    }
    QString cut = leftWithoutSplittingPair(safe, kMaxNameUnits);
    removeTrailingDotsAndSpaces(&cut);
    return cut.isEmpty() ? QStringLiteral("_") : cut;
}

QString dlpAudioStickPath(
        const QString& artist, const QString& album, const QString& fileName) {
    return QStringLiteral("/Contents/") +
            fatSafeName(artist.isEmpty() ? QStringLiteral("UnknownArtist") : artist) +
            QLatin1Char('/') +
            fatSafeName(album.isEmpty() ? QStringLiteral("UnknownAlbum") : album) +
            QLatin1Char('/') + fatSafeName(fileName);
}

QString dlpPathKey(const QString& stickPath) {
    QString key = stickPath.toCaseFolded();
    key.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return key;
}

bool dlpIsSafeStickPath(const QString& stickPath) {
    if (stickPath.size() < 2 || stickPath.front() != QLatin1Char('/')) {
        return false;
    }
    for (const QChar c : stickPath) {
        if (c == QLatin1Char('\\') || c == QLatin1Char(':') || c.unicode() < 0x20) {
            return false;
        }
    }
    if (QDir::cleanPath(stickPath) != stickPath) {
        return false;
    }
    const QStringList parts = stickPath.mid(1).split(QLatin1Char('/'));
    for (const QString& part : parts) {
        if (part.isEmpty() || part == QLatin1String(".") || part == QLatin1String("..")) {
            return false;
        }
    }
    return true;
}

QString dlpAnlzFolder(const QString& stickPath,
        const QSet<QString>& usedFolderKeys,
        const QString& stickRoot) {
    const QByteArray digest = QCryptographicHash::hash(
            dlpPathKey(stickPath).toUtf8(), QCryptographicHash::Sha1);
    const auto b = [&](int i) {
        return static_cast<quint32>(static_cast<quint8>(digest.at(i)));
    };
    // The first 12 bits are the P part, the next 32 bits are the name.
    const quint32 prefix = (b(0) << 4) | (b(1) >> 4);
    quint32 name = ((b(1) & 0x0f) << 28) | (b(2) << 20) | (b(3) << 12) |
            (b(4) << 4) | (b(5) >> 4);
    for (;;) {
        const QString folder = QStringLiteral("/PIONEER/USBANLZ/P%1/%2")
                                       .arg(prefix, 3, 16, QLatin1Char('0'))
                                       .arg(name, 8, 16, QLatin1Char('0'))
                                       .toUpper();
        if (!usedFolderKeys.contains(dlpPathKey(folder))) {
            if (stickRoot.isEmpty()) {
                return folder;
            }
            const QFileInfo onStick(stickRoot + folder);
            if (!onStick.exists() && !onStick.isSymLink()) {
                return folder;
            }
        }
        ++name; // wraps at 32 bits
    }
}

std::optional<quint32> dlpMpegPcmFrames(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QByteArray data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return std::nullopt;
    }

    qsizetype pos = 0;
    for (qsizetype next = id3v2End(data, pos); next != pos; next = id3v2End(data, pos)) {
        pos = next;
    }
    const qsizetype end = data.size();

    const auto atTagStop = [&](qsizetype at) {
        return hasTagAt(data, at, "TAG") || hasTagAt(data, at, "APETAGEX");
    };

    quint64 pcmFrames = 0;
    bool synced = false;
    bool firstFrame = true;
    while (pos + 4 <= end) {
        if (atTagStop(pos)) {
            break;
        }
        MpegFrame frame;
        if (parseMpegFrame(data, pos, &frame) && pos + frame.length <= end) {
            const qsizetype nextPos = pos + frame.length;
            MpegFrame nextFrame;
            // After a frame, the next header must be valid, unless the data
            // ends there. This also keeps a false sync out.
            if (synced || nextPos == end || atTagStop(nextPos) ||
                    parseMpegFrame(data, nextPos, &nextFrame)) {
                bool isInfoFrame = false;
                if (firstFrame) {
                    firstFrame = false;
                    isInfoFrame = hasTagAt(data, pos + frame.infoOffset, "Xing") ||
                            hasTagAt(data, pos + frame.infoOffset, "Info") ||
                            hasTagAt(data, pos + kVbriOffset, "VBRI");
                }
                if (!isInfoFrame) {
                    pcmFrames += static_cast<quint64>(frame.samples);
                }
                pos = nextPos;
                synced = true;
                continue;
            }
        }
        synced = false;
        ++pos;
    }
    if (pcmFrames == 0 || pcmFrames > std::numeric_limits<quint32>::max()) {
        return std::nullopt;
    }
    return static_cast<quint32>(pcmFrames);
}

int rekordboxTimingOffsetMs(const QString& location) {
    // The following code accounts for timing offsets required to
    // correctly align timing information (cue points, loops, beatgrids)
    // exported from Rekordbox. This is caused by different MP3
    // decoders treating MP3s encoded in a variety of different cases
    // differently. The mp3guessenc library is used to determine which
    // case the MP3 is classified in. See the following PR for more
    // detailed information:
    // https://github.com/mixxxdj/mixxx/pull/2119

    int timingOffset = 0;

    if (location.endsWith(".mp3", Qt::CaseInsensitive)) {
        // The wrapper serialises the calls (mp3guessenc has global state).
        const int timingShiftCase = mp3guessencTimingShiftCase(location);

        qDebug() << "Timing shift case:" << timingShiftCase << "for MP3 file:" << location;

        switch (timingShiftCase) {
#ifdef __COREAUDIO__
        case EXIT_CODE_CASE_A:
            timingOffset = 12;
            break;
        case EXIT_CODE_CASE_B:
            timingOffset = 13;
            break;
        case EXIT_CODE_CASE_C:
            timingOffset = 26;
            break;
        case EXIT_CODE_CASE_D:
            timingOffset = 50;
            break;
#elif defined(__MAD__)
        case EXIT_CODE_CASE_A:
        case EXIT_CODE_CASE_D:
            timingOffset = 26;
            break;
#elif defined(__FFMPEG__)
        case EXIT_CODE_CASE_D:
            timingOffset = 26;
            break;
#endif
        }
    }

#ifdef __COREAUDIO__
    if (location.toLower().endsWith(".m4a")) {
        timingOffset = 48;
    }
#endif

    return timingOffset;
}

int dlpBitDepth(const QString& filePath, int fileType) {
    constexpr int kDefaultBitDepth = 16;
    switch (fileType) {
    case 5: {
        TagLib::FLAC::File file(TAGLIB_FILENAME_FROM_QSTRING(filePath));
        const auto* pProperties = file.isValid() ? file.audioProperties() : nullptr;
        if (pProperties && pProperties->bitsPerSample() > 0) {
            return pProperties->bitsPerSample();
        }
        break;
    }
    case 11: {
        TagLib::RIFF::WAV::File file(TAGLIB_FILENAME_FROM_QSTRING(filePath));
        const auto* pProperties = file.isValid() ? file.audioProperties() : nullptr;
        if (pProperties && pProperties->bitsPerSample() > 0) {
            return pProperties->bitsPerSample();
        }
        break;
    }
    case 12: {
        TagLib::RIFF::AIFF::File file(TAGLIB_FILENAME_FROM_QSTRING(filePath));
        const auto* pProperties = file.isValid() ? file.audioProperties() : nullptr;
        if (pProperties && pProperties->bitsPerSample() > 0) {
            return pProperties->bitsPerSample();
        }
        break;
    }
    default:
        break;
    }
    return kDefaultBitDepth;
}

} // namespace rekordbox
} // namespace mixxx
