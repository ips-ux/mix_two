// AI-generated (Claude Code). Start of AI-generated text.
#include "track/bitdepth.h"

#include <QFileInfo>

#include "track/taglib/trackmetadata_file.h"

namespace mixxx {

namespace {

// A bit depth outside of this range is not a value that a PCM file has.
constexpr int kMinBitDepth = 1;
constexpr int kMaxBitDepth = 64;

template<class TFile>
int bitsPerSampleOf(const QString& filePath) {
    TFile file(TAGLIB_FILENAME_FROM_QSTRING(filePath));
    const auto* pProperties = file.isValid() ? file.audioProperties() : nullptr;
    if (pProperties && pProperties->bitsPerSample() > 0) {
        return pProperties->bitsPerSample();
    }
    return 0;
}

} // namespace

int bitDepthFromHeader(const QString& filePath, BitDepthFileKind kind) {
    if (filePath.isEmpty()) {
        return 0;
    }
    switch (kind) {
    case BitDepthFileKind::Flac:
        return bitsPerSampleOf<TagLib::FLAC::File>(filePath);
    case BitDepthFileKind::Wav:
        return bitsPerSampleOf<TagLib::RIFF::WAV::File>(filePath);
    case BitDepthFileKind::Aiff:
        return bitsPerSampleOf<TagLib::RIFF::AIFF::File>(filePath);
    }
    return 0;
}

int bitDepthFromFile(const QString& filePath) {
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QLatin1String("flac")) {
        return bitDepthFromHeader(filePath, BitDepthFileKind::Flac);
    }
    if (suffix == QLatin1String("wav") || suffix == QLatin1String("wave")) {
        return bitDepthFromHeader(filePath, BitDepthFileKind::Wav);
    }
    if (suffix == QLatin1String("aiff") || suffix == QLatin1String("aif") ||
            suffix == QLatin1String("aifc")) {
        return bitDepthFromHeader(filePath, BitDepthFileKind::Aiff);
    }
    // Lossy formats (MP3, AAC, Ogg Vorbis, Opus) have no bit depth. Other
    // lossless formats are not read: their value stays unknown.
    return 0;
}

QVariant bitDepthDisplayValue(const QVariant& stored) {
    if (stored.isNull()) {
        return QVariant();
    }
    bool ok = false;
    const int bits = stored.toInt(&ok);
    if (!ok || bits < kMinBitDepth || bits > kMaxBitDepth) {
        return QVariant();
    }
    return bits;
}

} // namespace mixxx
// End of AI-generated text.
