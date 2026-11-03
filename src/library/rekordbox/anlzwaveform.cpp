#include "library/rekordbox/anlzwaveform.h"

#include <QVector>
#include <algorithm>
#include <cmath>

namespace mixxx {
namespace rekordbox {

namespace {

/// The largest values of the four bands over the visual samples of one detail
/// entry (0-255 each).
struct EntryBands {
    quint8 low = 0;
    quint8 mid = 0;
    quint8 high = 0;
    quint8 all = 0;
};

/// The values of a detail entry that the pictures use.
struct EntryValues {
    EntryBands bands;
    quint8 height; // 0-31
    quint8 whiteness; // 0-7
};

/// Returns the mean of `sum` over `count` values, rounded to nearest.
int meanOf(qint64 sum, int count) {
    return static_cast<int>((sum + count / 2) / count);
}

EntryValues entryValuesOf(const EntryBands& bands) {
    EntryValues values;
    values.bands = bands;
    const int loudest = std::max({bands.low, bands.mid, bands.high});
    values.height = static_cast<quint8>(loudest >> 3);
    if (values.height == 0) {
        values.whiteness = 0;
    } else {
        // 7 * (1 - mid / loudest), rounded to nearest.
        values.whiteness = static_cast<quint8>(
                (7 * (loudest - bands.mid) + loudest / 2) / loudest);
    }
    return values;
}

/// Returns the largest band values over the visual samples (both channels)
/// that overlap the audio frames [startFrame, endFrame).
EntryBands maxBandsOverFrames(const Waveform& waveform,
        qint64 startFrame,
        qint64 endFrame) {
    // A visual sample is stereo-interleaved: index 2v is left, 2v + 1 right.
    const qint64 visualCount = waveform.getDataSize() / 2;
    const double ratio = waveform.getAudioVisualRatio();
    // Visual sample v covers the frames [v * ratio, (v + 1) * ratio).
    const qint64 first = static_cast<qint64>(std::floor(startFrame / ratio));
    if (first >= visualCount) {
        return EntryBands();
    }
    const qint64 last = std::min(visualCount,
            std::max(first + 1,
                    static_cast<qint64>(std::ceil(endFrame / ratio))));
    EntryBands bands;
    for (qint64 i = first * 2; i < last * 2; ++i) {
        const int index = static_cast<int>(i);
        bands.low = std::max(bands.low, waveform.getLow(index));
        bands.mid = std::max(bands.mid, waveform.getMid(index));
        bands.high = std::max(bands.high, waveform.getHigh(index));
        bands.all = std::max(bands.all, waveform.getAll(index));
    }
    return bands;
}

void appendU16BigEndian(QByteArray* pBytes, quint16 value) {
    pBytes->append(static_cast<char>(value >> 8));
    pBytes->append(static_cast<char>(value & 0xFF));
}

/// The first detail entry of a preview column and the entry after its last.
/// Every column has at least one entry.
struct ColumnRange {
    int begin;
    int end;
};

ColumnRange columnRange(int column, int columns, int entries) {
    const int begin = static_cast<int>(static_cast<qint64>(column) * entries / columns);
    const int end = static_cast<int>(static_cast<qint64>(column + 1) * entries / columns);
    return {begin, std::max(begin + 1, end)};
}

} // namespace

AnlzWaveforms anlzWaveformsFrom(const Waveform& waveform,
        SINT frames,
        audio::SampleRate sampleRate) {
    AnlzWaveforms result;
    const qint64 rate = sampleRate.value();
    if (frames <= 0 || rate <= 0 || waveform.getDataSize() <= 0 ||
            waveform.getAudioVisualRatio() <= 0) {
        return result;
    }

    const qint64 entries =
            (static_cast<qint64>(frames) * kDetailEntriesPerSecond + rate - 1) / rate;
    const int n = static_cast<int>(entries);
    result.detailEntries = n;

    // The detail pictures.
    QVector<EntryValues> values;
    values.reserve(n);
    result.detail.reserve(n);
    result.colorDetail.reserve(n * 2);
    result.threeBandDetail.reserve(n * 3);
    for (int k = 0; k < n; ++k) {
        const qint64 startFrame = k * rate / kDetailEntriesPerSecond;
        const qint64 endFrame = std::min<qint64>(
                (k + 1) * rate / kDetailEntriesPerSecond, frames);
        const EntryValues entry =
                entryValuesOf(maxBandsOverFrames(waveform, startFrame, endFrame));
        values.append(entry);

        result.detail.append(static_cast<char>((entry.whiteness << 5) | entry.height));
        appendU16BigEndian(&result.colorDetail,
                static_cast<quint16>(((entry.bands.mid >> 5) << 13) |
                        ((entry.bands.high >> 5) << 10) |
                        ((entry.bands.low >> 5) << 7) | (entry.height << 2)));
        result.threeBandDetail.append(static_cast<char>(entry.bands.mid >> 1));
        result.threeBandDetail.append(static_cast<char>(entry.bands.high >> 1));
        result.threeBandDetail.append(static_cast<char>(entry.bands.low >> 1));
    }

    // PWAV: the mean height and whiteness of the entries of each column.
    result.preview.reserve(kPreviewColumns);
    for (int c = 0; c < kPreviewColumns; ++c) {
        const ColumnRange range = columnRange(c, kPreviewColumns, n);
        qint64 heightSum = 0;
        qint64 whitenessSum = 0;
        for (int k = range.begin; k < range.end; ++k) {
            heightSum += values[k].height;
            whitenessSum += values[k].whiteness;
        }
        const int count = range.end - range.begin;
        result.preview.append(static_cast<char>(
                (meanOf(whitenessSum, count) << 5) | meanOf(heightSum, count)));
    }

    // PWV2: the mean height of the entries of each column, in 4 bits.
    result.tinyPreview.reserve(kTinyPreviewColumns);
    for (int c = 0; c < kTinyPreviewColumns; ++c) {
        const ColumnRange range = columnRange(c, kTinyPreviewColumns, n);
        qint64 heightSum = 0;
        for (int k = range.begin; k < range.end; ++k) {
            heightSum += values[k].height;
        }
        result.tinyPreview.append(static_cast<char>(
                meanOf(heightSum, range.end - range.begin) >> 1));
    }

    // PWV4 and PWV6 share the column ranges.
    result.colorPreview.reserve(kColorPreviewColumns * 6);
    result.threeBandPreview.reserve(kColorPreviewColumns * 3);
    for (int c = 0; c < kColorPreviewColumns; ++c) {
        const ColumnRange range = columnRange(c, kColorPreviewColumns, n);
        const int count = range.end - range.begin;
        int maxAll = 0;
        int maxMid = 0;
        int maxHigh = 0;
        int maxLow = 0;
        qint64 sumAll = 0;
        qint64 sumMid = 0;
        qint64 sumHigh = 0;
        qint64 sumLow = 0;
        for (int k = range.begin; k < range.end; ++k) {
            const EntryBands& bands = values[k].bands;
            maxAll = std::max<int>(maxAll, bands.all);
            maxMid = std::max<int>(maxMid, bands.mid);
            maxHigh = std::max<int>(maxHigh, bands.high);
            maxLow = std::max<int>(maxLow, bands.low);
            sumAll += bands.all;
            sumMid += bands.mid >> 1;
            sumHigh += bands.high >> 1;
            sumLow += bands.low >> 1;
        }

        const int b0 = maxAll >> 1;
        const int b1 = b0 == 0 ? 0 : 0x80 | ((127 - b0) >> 1);
        result.colorPreview.append(static_cast<char>(b0));
        result.colorPreview.append(static_cast<char>(b1));
        result.colorPreview.append(static_cast<char>(meanOf(sumAll, count) >> 1));
        result.colorPreview.append(static_cast<char>(maxMid >> 1));
        result.colorPreview.append(static_cast<char>(maxHigh >> 1));
        result.colorPreview.append(static_cast<char>(maxLow >> 1));

        result.threeBandPreview.append(static_cast<char>(meanOf(sumMid, count)));
        result.threeBandPreview.append(static_cast<char>(meanOf(sumHigh, count)));
        result.threeBandPreview.append(static_cast<char>(meanOf(sumLow, count)));
    }

    return result;
}

} // namespace rekordbox
} // namespace mixxx
