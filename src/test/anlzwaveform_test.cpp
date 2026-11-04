#include "library/rekordbox/anlzwaveform.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <functional>
#include <memory>

#include "audio/types.h"
#include "waveform/waveform.h"

namespace {

using mixxx::audio::SampleRate;
using mixxx::rekordbox::AnlzWaveforms;
using mixxx::rekordbox::anlzWaveformsFrom;

struct Bands {
    unsigned char low = 0;
    unsigned char mid = 0;
    unsigned char high = 0;
    unsigned char all = 0;
};

/// The band values of the visual sample that starts at an audio frame.
using BandsAt = std::function<Bands(SINT frame)>;

BandsAt flat(unsigned char value) {
    return [value](SINT) { return Bands{value, value, value, value}; };
}

/// Makes a waveform like the analyser does for the main waveform (441 visual
/// samples per second) and fills both channels from `bandsAt`.
std::unique_ptr<Waveform> makeWaveform(
        int sampleRate, SINT frames, const BandsAt& bandsAt) {
    auto pWaveform = std::make_unique<Waveform>(sampleRate, frames, 441, -1, 0);
    const double ratio = pWaveform->getAudioVisualRatio();
    WaveformData* pData = pWaveform->data();
    for (int i = 0; i < pWaveform->getDataSize(); ++i) {
        const Bands bands = bandsAt(static_cast<SINT>((i / 2) * ratio));
        pData[i].filtered.low = bands.low;
        pData[i].filtered.mid = bands.mid;
        pData[i].filtered.high = bands.high;
        pData[i].filtered.all = bands.all;
    }
    return pWaveform;
}

AnlzWaveforms convert(int sampleRate, SINT frames, const BandsAt& bandsAt) {
    const auto pWaveform = makeWaveform(sampleRate, frames, bandsAt);
    return anlzWaveformsFrom(*pWaveform, frames, SampleRate(sampleRate));
}

quint8 byteAt(const QByteArray& bytes, int index) {
    return static_cast<quint8>(bytes.at(index));
}

quint16 u16At(const QByteArray& bytes, int entry) {
    return static_cast<quint16>((byteAt(bytes, entry * 2) << 8) |
            byteAt(bytes, entry * 2 + 1));
}

TEST(AnlzWaveformTest, DetailLengthFollowsSampleRate) {
    // 10 s at 48 kHz: 480000 frames -> 1500 entries, not 1633.
    const AnlzWaveforms w = convert(48000, 480000, flat(100));
    EXPECT_EQ(1500, w.detailEntries);
    EXPECT_EQ(1500, w.detail.size());
    EXPECT_EQ(3000, w.colorDetail.size());
    EXPECT_EQ(4500, w.threeBandDetail.size());
    EXPECT_EQ(400, w.preview.size());
    EXPECT_EQ(100, w.tinyPreview.size());
    EXPECT_EQ(7200, w.colorPreview.size());
    EXPECT_EQ(3600, w.threeBandPreview.size());
}

TEST(AnlzWaveformTest, PartialLastSecondRoundsUp) {
    // 44100 + 294 frames -> 151 entries (150 per second, ceil).
    EXPECT_EQ(151, convert(44100, 44394, flat(100)).detailEntries);
    // One frame more than 150 whole entries -> 151.
    EXPECT_EQ(151, convert(44100, 44100 + 1, flat(100)).detailEntries);
    EXPECT_EQ(150, convert(44100, 44100, flat(100)).detailEntries);
}

TEST(AnlzWaveformTest, BitLayoutOfDetailEntriesLowOnly) {
    // low 255, mid 0, high 0 -> height 31, whiteness 0 (no mid).
    const AnlzWaveforms w = convert(44100, 44100, [](SINT) {
        return Bands{255, 0, 0, 255};
    });
    ASSERT_EQ(150, w.detailEntries);
    EXPECT_EQ(0x1F, byteAt(w.detail, 0));
    // R 0, G 0, B 7, height 31 -> 7 << 7 | 31 << 2.
    EXPECT_EQ(0x03FC, u16At(w.colorDetail, 0));
    EXPECT_EQ(0, byteAt(w.threeBandDetail, 0));   // mid
    EXPECT_EQ(0, byteAt(w.threeBandDetail, 1));   // high
    EXPECT_EQ(127, byteAt(w.threeBandDetail, 2)); // low
}

TEST(AnlzWaveformTest, BitLayoutOfDetailEntriesMidOnly) {
    // mid 255 only -> whiteness 7.
    const AnlzWaveforms w = convert(44100, 44100, [](SINT) {
        return Bands{0, 255, 0, 255};
    });
    ASSERT_EQ(150, w.detailEntries);
    EXPECT_EQ(0xFF, byteAt(w.detail, 0));
    EXPECT_EQ(0xE07C, u16At(w.colorDetail, 0));
    EXPECT_EQ(127, byteAt(w.threeBandDetail, 0));
    EXPECT_EQ(0, byteAt(w.threeBandDetail, 1));
    EXPECT_EQ(0, byteAt(w.threeBandDetail, 2));
}

TEST(AnlzWaveformTest, BitLayoutOfDetailEntriesHighOnly) {
    const AnlzWaveforms w = convert(44100, 44100, [](SINT) {
        return Bands{0, 0, 255, 255};
    });
    ASSERT_EQ(150, w.detailEntries);
    EXPECT_EQ(0x1F, byteAt(w.detail, 0));
    // G 7 << 10 | height 31 << 2.
    EXPECT_EQ(0x1C7C, u16At(w.colorDetail, 0));
    EXPECT_EQ(0, byteAt(w.threeBandDetail, 0));
    EXPECT_EQ(127, byteAt(w.threeBandDetail, 1));
    EXPECT_EQ(0, byteAt(w.threeBandDetail, 2));
}

TEST(AnlzWaveformTest, WhitenessIsRoundedFromMidShare) {
    // max 255, mid 128: 7 * 128 / 255 = 3.51 -> 4. Height 255 >> 3 = 31.
    const AnlzWaveforms w = convert(44100, 44100, [](SINT) {
        return Bands{255, 128, 0, 255};
    });
    EXPECT_EQ((4 << 5) | 31, byteAt(w.detail, 0));
    // max 255, mid 120: 7 * 120 / 255 = 3.29 -> 3.
    const AnlzWaveforms u = convert(44100, 44100, [](SINT) {
        return Bands{255, 120, 0, 255};
    });
    EXPECT_EQ((3 << 5) | 31, byteAt(u.detail, 0));
    // max 8, mid 4: 7 * 4 / 8 = 3.5 -> 4. Height 8 >> 3 = 1.
    const AnlzWaveforms v = convert(44100, 44100, [](SINT) {
        return Bands{8, 4, 0, 8};
    });
    EXPECT_EQ((4 << 5) | 1, byteAt(v.detail, 0));
}

TEST(AnlzWaveformTest, HeightBelowEightHasNoWhiteness) {
    // max 7 >> 3 = 0: height 0 and whiteness 0 although the bands differ.
    const AnlzWaveforms w = convert(44100, 44100, [](SINT) {
        return Bands{7, 0, 0, 7};
    });
    EXPECT_EQ(0, byteAt(w.detail, 0));
}

TEST(AnlzWaveformTest, EntryTakesMaximumOfOverlappingVisualSamples) {
    // At 44100 Hz a visual sample is 100 frames and a detail entry 294 frames.
    // Entry 0 is frames [0, 294): visual samples 0-2. Entry 1 is [294, 588):
    // visual samples 2-5. A peak in visual sample 2 (frames 200-299) is in
    // both entries; a peak in visual sample 3 (frames 300-399) is in entry 1
    // only.
    const AnlzWaveforms w = convert(44100, 44100, [](SINT frame) {
        if (frame == 200) {
            return Bands{0, 0, 80, 80};
        }
        if (frame == 300) {
            return Bands{0, 0, 160, 160};
        }
        return Bands{};
    });
    ASSERT_EQ(150, w.detailEntries);
    EXPECT_EQ(80 >> 1, byteAt(w.threeBandDetail, 0 * 3 + 1));
    EXPECT_EQ(160 >> 1, byteAt(w.threeBandDetail, 1 * 3 + 1));
    EXPECT_EQ(0, byteAt(w.threeBandDetail, 2 * 3 + 1));
}

TEST(AnlzWaveformTest, EntryTakesMaximumOverBothChannels) {
    auto pWaveform = makeWaveform(44100, 44100, [](SINT) { return Bands{}; });
    WaveformData* pData = pWaveform->data();
    // Visual sample 0: left 10, right 100 (mid).
    pData[0].filtered.mid = 10;
    pData[1].filtered.mid = 100;
    const AnlzWaveforms w = anlzWaveformsFrom(*pWaveform, 44100, SampleRate(44100));
    EXPECT_EQ(100 >> 1, byteAt(w.threeBandDetail, 0));
}

TEST(AnlzWaveformTest, ShortWaveformDoesNotReadPastItsData) {
    // The waveform has data for one second, the track is two seconds long. The
    // entries after the data are silent and nothing reads outside the data.
    const auto pWaveform = makeWaveform(44100, 44100, [](SINT frame) {
        return frame < 44100 ? Bands{255, 255, 255, 255} : Bands{};
    });
    const AnlzWaveforms w = anlzWaveformsFrom(*pWaveform, 88200, SampleRate(44100));
    ASSERT_EQ(300, w.detailEntries);
    ASSERT_EQ(300, w.detail.size());
    EXPECT_EQ(0xFF, byteAt(w.detail, 0));
    EXPECT_EQ(0xFF, byteAt(w.detail, 149));
    EXPECT_EQ(0, byteAt(w.detail, 150));
    EXPECT_EQ(0, byteAt(w.detail, 299));
}

TEST(AnlzWaveformTest, LastEntryOfOddRateGetsData) {
    // 48 kHz: a visual sample is 108.84 frames. The last entry (one frame
    // long) still reads a visual sample.
    const AnlzWaveforms w = convert(48000, 48001, flat(255));
    ASSERT_EQ(151, w.detailEntries);
    EXPECT_EQ(0xFF, byteAt(w.detail, 0));
    EXPECT_EQ(0xFF, byteAt(w.detail, 150));
}

TEST(AnlzWaveformTest, PreviewIsMeanOfDetail) {
    // The first half of the track is loud, the second half is silent. 10 s at
    // 44100 Hz: 1500 entries.
    const SINT frames = 441000;
    const AnlzWaveforms w = convert(44100, frames, [frames](SINT frame) {
        if (frame < frames / 2) {
            return Bands{255, 128, 64, 255};
        }
        return Bands{};
    });
    ASSERT_EQ(1500, w.detailEntries);

    // Height 31, whiteness 4 (see WhitenessIsRoundedFromMidShare).
    const quint8 loud = (4 << 5) | 31;
    EXPECT_EQ(loud, byteAt(w.preview, 0));
    EXPECT_EQ(loud, byteAt(w.preview, 199));
    EXPECT_EQ(0, byteAt(w.preview, 200));
    EXPECT_EQ(0, byteAt(w.preview, 399));

    EXPECT_EQ(15, byteAt(w.tinyPreview, 0));
    EXPECT_EQ(15, byteAt(w.tinyPreview, 49));
    EXPECT_EQ(0, byteAt(w.tinyPreview, 50));
    EXPECT_EQ(0, byteAt(w.tinyPreview, 99));

    // PWV4 column 0: b0 = 255 >> 1 = 127, b1 = 0x80, b2 = 127, then the
    // maxima of mid, high and low.
    ASSERT_EQ(7200, w.colorPreview.size());
    EXPECT_EQ(127, byteAt(w.colorPreview, 0));
    EXPECT_EQ(0x80, byteAt(w.colorPreview, 1));
    EXPECT_EQ(127, byteAt(w.colorPreview, 2));
    EXPECT_EQ(64, byteAt(w.colorPreview, 3));
    EXPECT_EQ(32, byteAt(w.colorPreview, 4));
    EXPECT_EQ(127, byteAt(w.colorPreview, 5));
    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(0, byteAt(w.colorPreview, 1199 * 6 + i)) << i;
    }

    // PWV6: mean of the PWV7 bytes (mid, high, low).
    ASSERT_EQ(3600, w.threeBandPreview.size());
    EXPECT_EQ(64, byteAt(w.threeBandPreview, 0));
    EXPECT_EQ(32, byteAt(w.threeBandPreview, 1));
    EXPECT_EQ(127, byteAt(w.threeBandPreview, 2));
    EXPECT_EQ(0, byteAt(w.threeBandPreview, 1199 * 3 + 0));
    EXPECT_EQ(0, byteAt(w.threeBandPreview, 1199 * 3 + 1));
    EXPECT_EQ(0, byteAt(w.threeBandPreview, 1199 * 3 + 2));
}

TEST(AnlzWaveformTest, PreviewColumnsRepeatEntriesOfShortTracks) {
    // 1 s = 150 entries for 400 preview columns: column c covers the entry
    // c * 150 / 400, so the columns 0-2 show entry 0 and column 3 shows
    // entry 1. Only entry 0 is loud: it holds the visual samples 0-2, and the
    // loud ones are 0 and 1 (frames 0-199).
    const AnlzWaveforms w = convert(44100, 44100, [](SINT frame) {
        return frame < 200 ? Bands{255, 0, 0, 255} : Bands{};
    });
    ASSERT_EQ(150, w.detailEntries);
    EXPECT_EQ(0x1F, byteAt(w.preview, 0));
    EXPECT_EQ(0x1F, byteAt(w.preview, 1));
    EXPECT_EQ(0x1F, byteAt(w.preview, 2));
    EXPECT_EQ(0, byteAt(w.preview, 3));
}

TEST(AnlzWaveformTest, PreviewColumnMeanRoundsToNearest) {
    // 4 s = 600 entries, 400 preview columns: column 1 covers the entries
    // [1, 3) with 600 / 400 = 1.5. Entry 1 has height 31 and whiteness 7,
    // entry 2 has 0 and 0: means 15.5 -> 16 and 3.5 -> 4.
    const AnlzWaveforms w = convert(44100, 4 * 44100, [](SINT frame) {
        // Entry 1 is frames [294, 588); the visual samples 3 and 4 (frames
        // 300-499) belong to it only (entry 2 starts at frame 588).
        return (frame == 300 || frame == 400) ? Bands{255, 255, 255, 255} : Bands{};
    });
    ASSERT_EQ(600, w.detailEntries);
    EXPECT_EQ(0xFF, byteAt(w.detail, 1));
    EXPECT_EQ(0, byteAt(w.detail, 2));
    EXPECT_EQ((4 << 5) | 16, byteAt(w.preview, 1));
}

TEST(AnlzWaveformTest, SilenceGivesZeros) {
    const AnlzWaveforms w = convert(44100, 441000, flat(0));
    ASSERT_EQ(1500, w.detailEntries);
    for (const QByteArray* pBytes : {&w.preview,
                 &w.tinyPreview,
                 &w.detail,
                 &w.colorPreview,
                 &w.colorDetail,
                 &w.threeBandPreview,
                 &w.threeBandDetail}) {
        EXPECT_FALSE(pBytes->isEmpty());
        EXPECT_EQ(QByteArray(pBytes->size(), '\0'), *pBytes);
    }
}

TEST(AnlzWaveformTest, EmptyWaveformGivesEmptyArrays) {
    const Waveform waveform;
    const AnlzWaveforms w = anlzWaveformsFrom(waveform, 441000, SampleRate(44100));
    EXPECT_EQ(0, w.detailEntries);
    EXPECT_TRUE(w.preview.isEmpty());
    EXPECT_TRUE(w.tinyPreview.isEmpty());
    EXPECT_TRUE(w.detail.isEmpty());
    EXPECT_TRUE(w.colorPreview.isEmpty());
    EXPECT_TRUE(w.colorDetail.isEmpty());
    EXPECT_TRUE(w.threeBandPreview.isEmpty());
    EXPECT_TRUE(w.threeBandDetail.isEmpty());
}

TEST(AnlzWaveformTest, ZeroFramesGiveEmptyArrays) {
    const auto pWaveform = makeWaveform(44100, 44100, flat(100));
    const AnlzWaveforms w = anlzWaveformsFrom(*pWaveform, 0, SampleRate(44100));
    EXPECT_EQ(0, w.detailEntries);
    EXPECT_TRUE(w.preview.isEmpty());
    EXPECT_TRUE(w.detail.isEmpty());
}

} // namespace
