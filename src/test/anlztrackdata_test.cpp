#include "library/rekordbox/anlztrackdata.h"

#include <gtest/gtest.h>
#include <rekordbox_anlz.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QList>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "audio/frame.h"
#include "audio/types.h"
#include "library/rekordbox/anlzwriter.h"
#include "library/rekordbox/rekordboxanlz.h"
#include "track/beats.h"
#include "track/bpm.h"
#include "track/cue.h"
#include "track/track.h"
#include "util/color/predefinedcolorpalettes.h"
#include "util/color/rgbcolor.h"
#include "util/duration.h"
#include "waveform/waveform.h"

namespace {

using mixxx::CueType;
using mixxx::PredefinedColorPalettes;
using mixxx::RgbColor;
using mixxx::audio::FramePos;
using mixxx::audio::SampleRate;
using mixxx::rekordbox::AnlzCue;
using mixxx::rekordbox::AnlzTrackData;
using mixxx::rekordbox::anlzTrackDataFrom;
using mixxx::rekordbox::writeAnlzFiles;
namespace anlz = mixxx::rekordbox::anlz;

const QString kStickPath = QStringLiteral("/Contents/Artist/Album/file.mp3");
const QString kNoWaveformMessage =
        QStringLiteral("The track has no waveform. Analyse the track first.");

/// A parsed analysis file. The Kaitai objects read from the stream, so the
/// stream must live as long as the parsed file.
class ParsedAnlz {
  public:
    explicit ParsedAnlz(const QByteArray& bytes)
            : m_stream(std::string(bytes.constData(), static_cast<size_t>(bytes.size()))),
              m_kaitaiStream(&m_stream),
              m_anlz(&m_kaitaiStream) {
    }

    rekordbox_anlz_t& file() {
        return m_anlz;
    }

    /// The extended cue list (PCO2) of a type.
    rekordbox_anlz_t::cue_extended_tag_t* extendedCueList(
            rekordbox_anlz_t::cue_list_type_t type) {
        for (const auto& pSection : *m_anlz.sections()) {
            if (pSection->fourcc() != rekordbox_anlz_t::SECTION_TAGS_CUES_2) {
                continue;
            }
            auto* pTag = static_cast<rekordbox_anlz_t::cue_extended_tag_t*>(pSection->body());
            if (pTag->type() == type) {
                return pTag;
            }
        }
        return nullptr;
    }

  private:
    std::istringstream m_stream;
    kaitai::kstream m_kaitaiStream;
    rekordbox_anlz_t m_anlz;
};

/// The PCO2 entry of a hot cue (1-8) in the bytes of an .EXT file.
struct WrittenCue {
    quint32 timeMs = 0;
    quint32 loopTimeMs = 0;
};

std::optional<WrittenCue> writtenHotCue(const QByteArray& extFile, int hotCue) {
    ParsedAnlz parsed(extFile);
    auto* pList = parsed.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
    if (!pList) {
        return std::nullopt;
    }
    for (const auto& pEntry : *pList->cues()) {
        if (static_cast<int>(pEntry->hot_cue()) == hotCue) {
            return WrittenCue{pEntry->time(), pEntry->loop_time()};
        }
    }
    return std::nullopt;
}

/// The PCO2 entries of the memory cue list in the bytes of an .EXT file.
std::vector<WrittenCue> writtenMemoryCues(const QByteArray& extFile) {
    std::vector<WrittenCue> result;
    ParsedAnlz parsed(extFile);
    auto* pList = parsed.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES);
    if (!pList) {
        return result;
    }
    for (const auto& pEntry : *pList->cues()) {
        result.push_back(WrittenCue{pEntry->time(), pEntry->loop_time()});
    }
    return result;
}

QRgb rgbOf(RgbColor color) {
    const RgbColor::code_t code = color;
    return qRgb(qRed(code), qGreen(code), qBlue(code));
}

/// The frame of a time in ms.
FramePos framesAtMs(int sampleRate, double ms) {
    return FramePos(ms * sampleRate / 1000.0);
}

/// The time in ms of a frame position, rounded.
long msOf(int sampleRate, FramePos position) {
    return std::lround(position.value() * 1000.0 / sampleRate);
}

class AnlzTrackDataTest : public testing::Test {
  protected:
    static QSharedPointer<Waveform> makeWaveform(int sampleRate, double seconds) {
        const auto frames = static_cast<SINT>(seconds * sampleRate);
        QSharedPointer<Waveform> pWaveform(new Waveform(sampleRate, frames, 441, -1, 0));
        WaveformData* pData = pWaveform->data();
        for (int i = 0; i < pWaveform->getDataSize(); ++i) {
            pData[i].filtered.low = 100;
            pData[i].filtered.mid = 100;
            pData[i].filtered.high = 100;
            pData[i].filtered.all = 100;
        }
        pWaveform->setCompletion(pWaveform->getDataSize());
        return pWaveform;
    }

    /// A 60 s track with a constant 128 BPM grid that starts at 0.5 s.
    static TrackPointer makeTrack(int sampleRate) {
        TrackPointer pTrack(Track::newTemporary());
        pTrack->setAudioProperties(mixxx::audio::ChannelCount(2),
                SampleRate(sampleRate),
                mixxx::audio::Bitrate(),
                mixxx::Duration::fromSeconds(60));
        pTrack->trySetBeats(mixxx::Beats::fromConstTempo(
                SampleRate(sampleRate),
                FramePos(sampleRate / 2),
                mixxx::Bpm(128)));
        return pTrack;
    }

    static void addCue(const TrackPointer& pTrack,
            CueType type,
            int hotCue,
            double startMs,
            double endMs = -1,
            const QString& label = QString(),
            std::optional<RgbColor> color = std::nullopt) {
        const int rate = static_cast<int>(pTrack->getSampleRate().value());
        const CuePointer pCue = pTrack->createAndAddCue(type,
                hotCue,
                framesAtMs(rate, startMs),
                endMs < 0 ? mixxx::audio::kInvalidFramePos : framesAtMs(rate, endMs));
        pCue->setLabel(label);
        if (color) {
            pCue->setColor(*color);
        }
    }

    /// Builds the data with a complete 60 s waveform.
    static std::optional<AnlzTrackData> build(const TrackPointer& pTrack,
            int timingOffsetMs = 0,
            QStringList* pWarnings = nullptr,
            QString* pError = nullptr) {
        return buildWith(pTrack,
                makeWaveform(static_cast<int>(pTrack->getSampleRate().value()), 60.0),
                timingOffsetMs,
                pWarnings,
                pError);
    }

    static std::optional<AnlzTrackData> buildWith(const TrackPointer& pTrack,
            const ConstWaveformPointer& pWaveform,
            int timingOffsetMs = 0,
            QStringList* pWarnings = nullptr,
            QString* pError = nullptr) {
        QStringList warnings;
        QString error;
        return anlzTrackDataFrom(*pTrack,
                pWaveform,
                kStickPath,
                timingOffsetMs,
                pWarnings ? pWarnings : &warnings,
                pError ? pError : &error);
    }

    /// Builds the data of a track with cues, writes the files, reads them
    /// with the rekordbox reader and compares.
    void checkRoundTrip(int rate) {
        const auto pTrack = makeTrack(rate);
        const QString dropLabel = QString::fromUtf8("Drop \xF0\x9F\x8E\xA7");
        addCue(pTrack, CueType::HotCue, 0, 1000, -1, dropLabel, RgbColor(0x336699));
        addCue(pTrack,
                CueType::Loop,
                3,
                2000,
                4000,
                QStringLiteral("Hot loop"),
                RgbColor(0xCC2200));
        addCue(pTrack,
                CueType::MemoryCue,
                Cue::kNoHotCue,
                5000,
                -1,
                QStringLiteral("Break"),
                RgbColor(0x0150F8));
        addCue(pTrack,
                CueType::MemoryCue,
                Cue::kNoHotCue,
                9000,
                -1,
                QStringLiteral("Build"),
                RgbColor(0x1EE000));
        addCue(pTrack,
                CueType::Loop,
                Cue::kNoHotCue,
                12000,
                16000,
                QStringLiteral("Memory loop"),
                RgbColor(0x9808F8));
        pTrack->setMainCuePosition(framesAtMs(rate, 3000));

        QStringList warnings;
        QString error;
        const auto data = build(pTrack, 0, &warnings, &error);
        ASSERT_TRUE(data.has_value()) << error.toStdString();
        EXPECT_TRUE(warnings.isEmpty());

        // The loop end of a hot cue is in the data and in the written file.
        ASSERT_EQ(2, data->hotCues.size());
        EXPECT_EQ(4, data->hotCues.at(1).hotCue);
        ASSERT_TRUE(data->hotCues.at(1).loopEndMs.has_value());
        EXPECT_EQ(4000u, *data->hotCues.at(1).loopEndMs);

        QTemporaryDir dir;
        ASSERT_TRUE(dir.isValid());
        const QString folder = dir.filePath(QStringLiteral("PIONEER/USBANLZ/P000/00000000"));
        ASSERT_TRUE(writeAnlzFiles(*data, folder, &error)) << error.toStdString();

        const auto hotLoop = writtenHotCue(anlz::extBytes(*data), 4);
        ASSERT_TRUE(hotLoop.has_value());
        EXPECT_EQ(2000u, hotLoop->timeMs);
        EXPECT_EQ(4000u, hotLoop->loopTimeMs);

        const TrackPointer pRead(Track::newTemporary());
        pRead->setAudioProperties(mixxx::audio::ChannelCount(2),
                SampleRate(rate),
                mixxx::audio::Bitrate(),
                mixxx::Duration::fromSeconds(60));
        mixxx::rekordbox::readAnalysisFiles(pRead,
                SampleRate(rate),
                0,
                QDir(folder).filePath(QStringLiteral("ANLZ0000.DAT")));

        // Grid.
        ASSERT_TRUE(pRead->getBeats());
        EXPECT_NEAR(128.0, pRead->getBpm(), 0.005);
        // The grid starts at the first beat of the track (before the first
        // marker at 0.5 s); the times are whole ms.
        EXPECT_NEAR(pTrack->getBeats()->firstBeat().value(),
                pRead->getBeats()->firstBeat().value(),
                rate / 1000.0);

        // Hot cue A: position, label and colour.
        const CuePointer pHotA = pRead->findHotcueByIndex(0);
        ASSERT_TRUE(pHotA);
        EXPECT_NEAR(rate * 1.0, pHotA->getPosition().value(), rate / 1000.0 + 0.5);
        EXPECT_EQ(dropLabel, pHotA->getLabel());
        EXPECT_EQ(0x336699u, static_cast<unsigned>(pHotA->getColor()));

        // Hot cue D: the position (the reader does not keep hot cue ends).
        const CuePointer pHotD = pRead->findHotcueByIndex(3);
        ASSERT_TRUE(pHotD);
        EXPECT_NEAR(rate * 2.0, pHotD->getPosition().value(), rate / 1000.0 + 0.5);
        EXPECT_EQ(QStringLiteral("Hot loop"), pHotD->getLabel());
        EXPECT_EQ(0xCC2200u, static_cast<unsigned>(pHotD->getColor()));

        // Memory cues: the main cue (3 s), two cues and a loop.
        QList<CuePointer> memory;
        for (const CuePointer& pCue : pRead->getCuePoints()) {
            if (pCue->getType() == CueType::MemoryCue) {
                memory.append(pCue);
            }
        }
        std::sort(memory.begin(), memory.end(), [](const CuePointer& a, const CuePointer& b) {
            return a->getPosition() < b->getPosition();
        });
        ASSERT_EQ(4, memory.size());
        const long expectedStartMs[] = {3000, 5000, 9000, 12000};
        const QString expectedLabel[] = {
                QString(),
                QStringLiteral("Break"),
                QStringLiteral("Build"),
                QStringLiteral("Memory loop")};
        for (int i = 0; i < 4; ++i) {
            EXPECT_NEAR(expectedStartMs[i], msOf(rate, memory[i]->getPosition()), 1.0) << i;
            EXPECT_EQ(expectedLabel[i], memory[i]->getLabel()) << i;
            EXPECT_EQ(i == 3, memory[i]->getEndPosition().isValid()) << i;
        }
        EXPECT_NEAR(16000, msOf(rate, memory[3]->getEndPosition()), 1.0);
        // Memory cue colours are quantised to the eight rekordbox colours.
        EXPECT_EQ(0x0150F8u, static_cast<unsigned>(memory[1]->getColor()));
        EXPECT_EQ(0x1EE000u, static_cast<unsigned>(memory[2]->getColor()));
        EXPECT_EQ(0x9808F8u, static_cast<unsigned>(memory[3]->getColor()));

        // The first memory cue is the main cue.
        EXPECT_NEAR(3000, msOf(rate, pRead->getMainCuePosition()), 1.0);
    }
};

TEST_F(AnlzTrackDataTest, RoundTripKeepsGridCuesAndLabels) {
    checkRoundTrip(44100);
}

TEST_F(AnlzTrackDataTest, RoundTripAt48kHz) {
    checkRoundTrip(48000);
}

TEST_F(AnlzTrackDataTest, FillsTheBasicFields) {
    const auto pTrack = makeTrack(48000);
    const auto data = build(pTrack, 25);
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(kStickPath, data->stickPath);
    EXPECT_EQ(48000, data->sampleRate.value());
    EXPECT_EQ(60 * 48000, data->frames);
    EXPECT_EQ(25, data->timingOffsetMs);
    EXPECT_EQ(0u, data->mp3PcmFrames);
    EXPECT_EQ(60 * 150, data->waveforms.detailEntries);
    EXPECT_EQ(400, data->waveforms.preview.size());

    // 128 BPM from 0.5 s: one beat per 22500 frames (0.46875 s) at 48 kHz.
    ASSERT_GT(data->beats.size(), 100);
    EXPECT_EQ(pTrack->getBeats()->firstBeat().value(), data->beats.first().position.value());
    EXPECT_LT(data->beats.first().position.value(), 22500.0);
    for (const auto& beat : data->beats) {
        EXPECT_EQ(12800, beat.tempo);
        EXPECT_LT(beat.position.value(), 60.0 * 48000);
    }
    EXPECT_NEAR(60.0 * 48000, data->beats.last().position.value(), 22500.5);
}

TEST_F(AnlzTrackDataTest, SkipsCueTypesThatAreNotWritten) {
    const auto pTrack = makeTrack(44100);
    addCue(pTrack, CueType::Intro, Cue::kNoHotCue, 1000, 2000);
    addCue(pTrack, CueType::Outro, Cue::kNoHotCue, 3000, 4000);
    addCue(pTrack, CueType::N60dBSound, Cue::kNoHotCue, 5000, 6000);
    addCue(pTrack, CueType::Jump, Cue::kNoHotCue, 7000, 8000);
    addCue(pTrack, CueType::Beat, Cue::kNoHotCue, 9000);
    QStringList warnings;
    const auto data = build(pTrack, 0, &warnings);
    ASSERT_TRUE(data.has_value());
    EXPECT_TRUE(data->hotCues.isEmpty());
    EXPECT_TRUE(data->memoryCues.isEmpty());
    EXPECT_TRUE(warnings.isEmpty());
}

TEST_F(AnlzTrackDataTest, SortsHotCuesAndMemoryCues) {
    const auto pTrack = makeTrack(44100);
    addCue(pTrack, CueType::HotCue, 5, 30000);
    addCue(pTrack, CueType::HotCue, 1, 20000);
    addCue(pTrack, CueType::HotCue, 7, 10000);
    addCue(pTrack, CueType::MemoryCue, Cue::kNoHotCue, 8000);
    addCue(pTrack, CueType::MemoryCue, Cue::kNoHotCue, 2000);
    addCue(pTrack, CueType::MemoryCue, Cue::kNoHotCue, 5000);
    const auto data = build(pTrack);
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(3, data->hotCues.size());
    EXPECT_EQ(2, data->hotCues[0].hotCue);
    EXPECT_EQ(6, data->hotCues[1].hotCue);
    EXPECT_EQ(8, data->hotCues[2].hotCue);
    ASSERT_EQ(3, data->memoryCues.size());
    EXPECT_EQ(2000u, data->memoryCues[0].timeMs);
    EXPECT_EQ(5000u, data->memoryCues[1].timeMs);
    EXPECT_EQ(8000u, data->memoryCues[2].timeMs);
    for (const AnlzCue& cue : data->memoryCues) {
        EXPECT_EQ(0, cue.hotCue);
    }
}

TEST_F(AnlzTrackDataTest, HotCuesAboveHGiveOneWarning) {
    const auto pTrack = makeTrack(44100);
    addCue(pTrack, CueType::HotCue, 0, 1000);
    addCue(pTrack, CueType::HotCue, 8, 2000);
    addCue(pTrack, CueType::HotCue, 12, 3000);
    QStringList warnings;
    const auto data = build(pTrack, 0, &warnings);
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(1, data->hotCues.size());
    EXPECT_EQ(1, data->hotCues[0].hotCue);
    ASSERT_EQ(1, warnings.size());
    EXPECT_EQ(QStringLiteral("The track has hot cues above H. "
                             "Players show hot cues A to H only."),
            warnings[0]);
}

TEST_F(AnlzTrackDataTest, HotCueHIsWrittenWithoutWarning) {
    const auto pTrack = makeTrack(44100);
    addCue(pTrack, CueType::HotCue, 7, 1000);
    QStringList warnings;
    const auto data = build(pTrack, 0, &warnings);
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(1, data->hotCues.size());
    EXPECT_EQ(8, data->hotCues[0].hotCue);
    EXPECT_TRUE(warnings.isEmpty());
}

TEST_F(AnlzTrackDataTest, MemoryCuesLimitedToTen) {
    const auto pTrack = makeTrack(44100);
    // Added in reverse order, so that the sort matters.
    for (int i = 12; i >= 1; --i) {
        addCue(pTrack, CueType::MemoryCue, Cue::kNoHotCue, i * 1000.0);
    }
    QStringList warnings;
    const auto data = build(pTrack, 0, &warnings);
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(10, data->memoryCues.size());
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(static_cast<quint32>((i + 1) * 1000), data->memoryCues[i].timeMs) << i;
    }
    ASSERT_EQ(1, warnings.size());
    EXPECT_EQ(QStringLiteral("The track has more than 10 memory cues. "
                             "The first 10 are written."),
            warnings[0]);
}

TEST_F(AnlzTrackDataTest, TenMemoryCuesGiveNoWarning) {
    const auto pTrack = makeTrack(44100);
    for (int i = 1; i <= 10; ++i) {
        addCue(pTrack, CueType::MemoryCue, Cue::kNoHotCue, i * 1000.0);
    }
    QStringList warnings;
    const auto data = build(pTrack, 0, &warnings);
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(10, data->memoryCues.size());
    EXPECT_TRUE(warnings.isEmpty());
}

TEST_F(AnlzTrackDataTest, MainCueCountsTowardTheTenMemoryCues) {
    const auto pTrack = makeTrack(44100);
    for (int i = 1; i <= 10; ++i) {
        addCue(pTrack, CueType::MemoryCue, Cue::kNoHotCue, i * 1000.0);
    }
    pTrack->setMainCuePosition(framesAtMs(44100, 500));
    QStringList warnings;
    const auto data = build(pTrack, 0, &warnings);
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(10, data->memoryCues.size());
    EXPECT_EQ(500u, data->memoryCues[0].timeMs);
    EXPECT_EQ(9000u, data->memoryCues[9].timeMs);
    EXPECT_EQ(1, warnings.size());
}

TEST_F(AnlzTrackDataTest, MainCueBecomesMemoryCueWhenMissing) {
    const auto pTrack = makeTrack(44100);
    addCue(pTrack, CueType::MemoryCue, Cue::kNoHotCue, 5000, -1, QStringLiteral("Break"));
    pTrack->setMainCuePosition(framesAtMs(44100, 2000));
    const auto data = build(pTrack);
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(2, data->memoryCues.size());
    EXPECT_EQ(2000u, data->memoryCues[0].timeMs);
    EXPECT_TRUE(data->memoryCues[0].comment.isEmpty());
    EXPECT_FALSE(data->memoryCues[0].color.has_value());
    EXPECT_FALSE(data->memoryCues[0].loopEndMs.has_value());
    EXPECT_EQ(5000u, data->memoryCues[1].timeMs);
}

TEST_F(AnlzTrackDataTest, MainCueIsNotDuplicatedByAMemoryCueAtTheSameTime) {
    {
        const auto pTrack = makeTrack(44100);
        addCue(pTrack, CueType::MemoryCue, Cue::kNoHotCue, 2000, -1, QStringLiteral("Mine"));
        pTrack->setMainCuePosition(framesAtMs(44100, 2000));
        const auto data = build(pTrack);
        ASSERT_TRUE(data.has_value());
        ASSERT_EQ(1, data->memoryCues.size());
        EXPECT_EQ(QStringLiteral("Mine"), data->memoryCues[0].comment);
    }
    {
        // Within 1 ms counts as the same time.
        const auto pTrack = makeTrack(44100);
        addCue(pTrack, CueType::MemoryCue, Cue::kNoHotCue, 2001);
        pTrack->setMainCuePosition(framesAtMs(44100, 2000));
        const auto data = build(pTrack);
        ASSERT_TRUE(data.has_value());
        EXPECT_EQ(1, data->memoryCues.size());
    }
    {
        // 2 ms apart is a different cue.
        const auto pTrack = makeTrack(44100);
        addCue(pTrack, CueType::MemoryCue, Cue::kNoHotCue, 2002);
        pTrack->setMainCuePosition(framesAtMs(44100, 2000));
        const auto data = build(pTrack);
        ASSERT_TRUE(data.has_value());
        EXPECT_EQ(2, data->memoryCues.size());
    }
}

TEST_F(AnlzTrackDataTest, NoMainCueAddsNoMemoryCue) {
    const auto pTrack = makeTrack(44100);
    const auto data = build(pTrack);
    ASSERT_TRUE(data.has_value());
    EXPECT_TRUE(data->memoryCues.isEmpty());
}

TEST_F(AnlzTrackDataTest, ClampsCueTimes) {
    const auto pTrack = makeTrack(44100);
    addCue(pTrack, CueType::HotCue, 0, 1000);
    addCue(pTrack, CueType::MemoryCue, Cue::kNoHotCue, 3000);
    // The end is before the start: a plain cue.
    addCue(pTrack, CueType::Loop, Cue::kNoHotCue, 8000, 6000);
    // The hot cue has an end that is not after the start: a plain cue.
    addCue(pTrack, CueType::Loop, 2, 20000, 20000);
    const auto data = build(pTrack, -5000);
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(-5000, data->timingOffsetMs);

    // The data holds audio time; the writer adds the offset and clamps at 0.
    ASSERT_EQ(2, data->hotCues.size());
    EXPECT_EQ(1000u, data->hotCues[0].timeMs);
    EXPECT_FALSE(data->hotCues[1].loopEndMs.has_value());
    ASSERT_EQ(2, data->memoryCues.size());
    EXPECT_EQ(3000u, data->memoryCues[0].timeMs);
    EXPECT_EQ(8000u, data->memoryCues[1].timeMs);
    EXPECT_FALSE(data->memoryCues[1].loopEndMs.has_value());

    const QByteArray ext = anlz::extBytes(*data);
    const auto hot = writtenHotCue(ext, 1);
    ASSERT_TRUE(hot.has_value());
    EXPECT_EQ(0u, hot->timeMs);
    const auto memory = writtenMemoryCues(ext);
    ASSERT_EQ(2u, memory.size());
    EXPECT_EQ(0u, memory[0].timeMs);
    EXPECT_EQ(3000u, memory[1].timeMs);
}

TEST_F(AnlzTrackDataTest, LoopEndTimesAreAudioTimeBeforeTheOffset) {
    const auto pTrack = makeTrack(44100);
    addCue(pTrack, CueType::Loop, Cue::kNoHotCue, 8000, 12000);
    const auto data = build(pTrack, 50);
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(1, data->memoryCues.size());
    EXPECT_EQ(8000u, data->memoryCues[0].timeMs);
    ASSERT_TRUE(data->memoryCues[0].loopEndMs.has_value());
    EXPECT_EQ(12000u, *data->memoryCues[0].loopEndMs);
    const auto memory = writtenMemoryCues(anlz::extBytes(*data));
    ASSERT_EQ(1u, memory.size());
    EXPECT_EQ(8050u, memory[0].timeMs);
    EXPECT_EQ(12050u, memory[0].loopTimeMs);
}

TEST_F(AnlzTrackDataTest, CueColoursBecomeRgb) {
    const auto pTrack = makeTrack(44100);
    addCue(pTrack, CueType::HotCue, 0, 1000, -1, QString(), RgbColor(0x336699));
    addCue(pTrack,
            CueType::MemoryCue,
            Cue::kNoHotCue,
            2000,
            -1,
            QStringLiteral("x"),
            RgbColor(0x0150F8));
    const auto data = build(pTrack);
    ASSERT_TRUE(data.has_value());
    ASSERT_TRUE(data->hotCues[0].color.has_value());
    EXPECT_EQ(qRgb(0x33, 0x66, 0x99), *data->hotCues[0].color);
    ASSERT_TRUE(data->memoryCues[0].color.has_value());
    EXPECT_EQ(qRgb(0x01, 0x50, 0xF8), *data->memoryCues[0].color);
    EXPECT_EQ(QStringLiteral("x"), data->memoryCues[0].comment);
}

TEST_F(AnlzTrackDataTest, MemoryCueWithTheDefaultColourHasNoColour) {
    const auto pTrack = makeTrack(44100);
    // addCue sets no colour: Mix Two gives the default colour.
    addCue(pTrack, CueType::MemoryCue, Cue::kNoHotCue, 1000);
    addCue(pTrack,
            CueType::MemoryCue,
            Cue::kNoHotCue,
            2000,
            -1,
            QString(),
            PredefinedColorPalettes::kDefaultCueColor);
    addCue(pTrack,
            CueType::Loop,
            Cue::kNoHotCue,
            3000,
            4000,
            QString(),
            PredefinedColorPalettes::kDefaultLoopColor);
    addCue(pTrack,
            CueType::MemoryCue,
            Cue::kNoHotCue,
            5000,
            -1,
            QString(),
            RgbColor(0x16C0F8));
    // A hot cue keeps its colour, also when it is the default one.
    addCue(pTrack,
            CueType::HotCue,
            0,
            6000,
            -1,
            QString(),
            PredefinedColorPalettes::kDefaultCueColor);
    const auto data = build(pTrack);
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(4, data->memoryCues.size());
    EXPECT_FALSE(data->memoryCues[0].color.has_value());
    EXPECT_FALSE(data->memoryCues[1].color.has_value());
    EXPECT_FALSE(data->memoryCues[2].color.has_value());
    ASSERT_TRUE(data->memoryCues[3].color.has_value());
    ASSERT_EQ(1, data->hotCues.size());
    ASSERT_TRUE(data->hotCues[0].color.has_value());
    EXPECT_EQ(rgbOf(PredefinedColorPalettes::kDefaultCueColor), *data->hotCues[0].color);

    // In the file: no colour (ID 0) and Aqua (ID 6).
    ParsedAnlz ext(anlz::extBytes(*data));
    auto* pList = ext.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES);
    ASSERT_NE(nullptr, pList);
    ASSERT_EQ(4u, pList->cues()->size());
    EXPECT_EQ(0u, pList->cues()->at(0)->color_id());
    EXPECT_EQ(0u, pList->cues()->at(1)->color_id());
    EXPECT_EQ(0u, pList->cues()->at(2)->color_id());
    EXPECT_EQ(6u, pList->cues()->at(3)->color_id());
}

TEST_F(AnlzTrackDataTest, BeatOneIsTheFirstBeatMarker) {
    // 128 BPM, the marker at 0.5 s: the beat at 500 ms is beat 1 and the
    // beat before it (31.25 ms) is beat 4.
    const auto pTrack = makeTrack(44100);
    const auto data = build(pTrack);
    ASSERT_TRUE(data.has_value());
    ASSERT_GT(data->beats.size(), 8);
    ASSERT_EQ(data->beats.size(), data->beatInBar.size());
    EXPECT_EQ(31, msOf(44100, data->beats[0].position));
    EXPECT_EQ(500, msOf(44100, data->beats[1].position));
    const std::vector<int> expected = {4, 1, 2, 3, 4, 1, 2, 3, 4};
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(expected[i], data->beatInBar[static_cast<int>(i)]) << i;
    }

    // In the file.
    ParsedAnlz dat(anlz::datBytes(*data));
    for (const auto& pSection : *dat.file().sections()) {
        if (pSection->fourcc() != rekordbox_anlz_t::SECTION_TAGS_BEAT_GRID) {
            continue;
        }
        auto* pGrid = static_cast<rekordbox_anlz_t::beat_grid_tag_t*>(pSection->body());
        EXPECT_EQ(4u, pGrid->beats()->at(0)->beat_number());
        EXPECT_EQ(500u, pGrid->beats()->at(1)->time());
        EXPECT_EQ(1u, pGrid->beats()->at(1)->beat_number());
    }
}

TEST_F(AnlzTrackDataTest, BeatOneIsTheFirstBeatMarkerOfABeatMap) {
    constexpr int kRate = 44100;
    const auto pTrack = makeTrack(kRate);
    // The first marker is at about 5.1 s, so the grid has beats before it.
    // 8 beats of 120 BPM, then 140 BPM.
    constexpr double kFirstMarker = 225000;
    constexpr double kTempoChange = kFirstMarker + 8 * 22050;
    pTrack->trySetBeats(mixxx::Beats::fromBeatMarkers(SampleRate(kRate),
            std::vector<mixxx::BeatMarker>{mixxx::BeatMarker(FramePos(kFirstMarker), 8)},
            FramePos(kTempoChange),
            mixxx::Bpm(140)));
    const auto data = build(pTrack);
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(data->beats.size(), data->beatInBar.size());
    int markerIndex = -1;
    for (int i = 0; i < data->beats.size(); ++i) {
        if (std::abs(data->beats[i].position.value() - kFirstMarker) < 1.0) {
            markerIndex = i;
        }
    }
    ASSERT_GT(markerIndex, 0);
    // Counting backward and forward from the marker.
    for (int i = 0; i < data->beats.size(); ++i) {
        const int fromMarker = i - markerIndex;
        const int expected = ((fromMarker % 4) + 4) % 4 + 1;
        EXPECT_EQ(expected, data->beatInBar[i]) << i;
    }
    EXPECT_EQ(1, data->beatInBar[markerIndex]);
    EXPECT_EQ(4, data->beatInBar[markerIndex - 1]);
    EXPECT_EQ(2, data->beatInBar[markerIndex + 1]);
    // The beat at the tempo change is 8 beats on: beat 1 again.
    EXPECT_EQ(1, data->beatInBar[markerIndex + 8]);
}

TEST_F(AnlzTrackDataTest, MainCueStaysWhenThereAreMoreThanTenMemoryCues) {
    const auto pTrack = makeTrack(44100);
    for (int i = 1; i <= 11; ++i) {
        addCue(pTrack, CueType::MemoryCue, Cue::kNoHotCue, i * 1000.0);
    }
    // The main cue would be the 12th memory cue.
    pTrack->setMainCuePosition(framesAtMs(44100, 20000));
    QStringList warnings;
    const auto data = build(pTrack, 0, &warnings);
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(10, data->memoryCues.size());
    for (int i = 0; i < 9; ++i) {
        EXPECT_EQ(static_cast<quint32>((i + 1) * 1000), data->memoryCues[i].timeMs) << i;
    }
    EXPECT_EQ(20000u, data->memoryCues[9].timeMs);
    EXPECT_EQ(1, warnings.size());
}

TEST_F(AnlzTrackDataTest, MainCueMemoryCueAtTheSameTimeStaysWhenTooMany) {
    const auto pTrack = makeTrack(44100);
    for (int i = 1; i <= 11; ++i) {
        addCue(pTrack,
                CueType::MemoryCue,
                Cue::kNoHotCue,
                i * 1000.0,
                -1,
                i == 11 ? QStringLiteral("Mine") : QString());
    }
    pTrack->setMainCuePosition(framesAtMs(44100, 11000));
    const auto data = build(pTrack);
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(10, data->memoryCues.size());
    EXPECT_EQ(QStringLiteral("Mine"), data->memoryCues[9].comment);
    EXPECT_EQ(11000u, data->memoryCues[9].timeMs);
}

TEST_F(AnlzTrackDataTest, RefusesTrackWithoutWaveform) {
    const auto pTrack = makeTrack(44100);
    QStringList warnings;
    QString error;
    const auto data = buildWith(pTrack, ConstWaveformPointer(), 0, &warnings, &error);
    EXPECT_FALSE(data.has_value());
    EXPECT_EQ(kNoWaveformMessage, error);
}

TEST_F(AnlzTrackDataTest, RefusesTrackWithEmptyWaveform) {
    const auto pTrack = makeTrack(44100);
    QString error;
    const auto data = buildWith(pTrack,
            QSharedPointer<Waveform>(new Waveform()),
            0,
            nullptr,
            &error);
    EXPECT_FALSE(data.has_value());
    EXPECT_EQ(kNoWaveformMessage, error);
}

TEST_F(AnlzTrackDataTest, RefusesWaveformThatIsNotFilled) {
    // The analyser attaches a zero-filled waveform with completion 0 before
    // it processes the track.
    const auto pTrack = makeTrack(44100);
    const QSharedPointer<Waveform> pWaveform(
            new Waveform(44100, 60 * 44100, 441, -1, 0));
    ASSERT_GT(pWaveform->getDataSize(), 0);
    ASSERT_EQ(0, pWaveform->getCompletion());
    QString error;
    EXPECT_FALSE(buildWith(pTrack, pWaveform, 0, nullptr, &error).has_value());
    EXPECT_EQ(kNoWaveformMessage, error);
}

TEST_F(AnlzTrackDataTest, RefusesWaveformThatIsHalfComplete) {
    const auto pTrack = makeTrack(44100);
    const QSharedPointer<Waveform> pWaveform = makeWaveform(44100, 60.0);
    pWaveform->setCompletion(pWaveform->getDataSize() / 2);
    QString error;
    EXPECT_FALSE(buildWith(pTrack, pWaveform, 0, nullptr, &error).has_value());
    EXPECT_EQ(kNoWaveformMessage, error);

    // Complete, the same waveform is accepted.
    pWaveform->setCompletion(pWaveform->getDataSize());
    EXPECT_TRUE(buildWith(pTrack, pWaveform).has_value());
}

TEST_F(AnlzTrackDataTest, RefusesTrackWithoutDuration) {
    const TrackPointer pTrack(Track::newTemporary());
    pTrack->setAudioProperties(mixxx::audio::ChannelCount(2),
            SampleRate(44100),
            mixxx::audio::Bitrate(),
            mixxx::Duration::fromSeconds(0));
    QString error;
    EXPECT_FALSE(buildWith(pTrack, makeWaveform(44100, 60.0), 0, nullptr, &error)
                    .has_value());
    EXPECT_EQ(kNoWaveformMessage, error);
}

TEST_F(AnlzTrackDataTest, RefusesTrackWithoutSampleRate) {
    const TrackPointer pTrack(Track::newTemporary());
    QString error;
    EXPECT_FALSE(buildWith(pTrack, makeWaveform(44100, 60.0), 0, nullptr, &error)
                    .has_value());
    EXPECT_EQ(QStringLiteral("The track has no valid sample rate. "
                             "Analyse the track again."),
            error);
}

TEST_F(AnlzTrackDataTest, TrackWithoutBeatsGivesEmptyGrid) {
    const auto pTrack = makeTrack(44100);
    pTrack->trySetBeats(mixxx::BeatsPointer());
    QStringList warnings;
    const auto data = build(pTrack, 0, &warnings);
    ASSERT_TRUE(data.has_value());
    EXPECT_TRUE(data->beats.isEmpty());
    EXPECT_TRUE(warnings.isEmpty());
}

TEST_F(AnlzTrackDataTest, TempoChangeGivesTwoTempos) {
    constexpr int kRate = 44100;
    const auto pTrack = makeTrack(kRate);
    // 8 beats of 120 BPM from the start, then 140 BPM.
    const mixxx::BeatMarker marker(FramePos(0), 8);
    const FramePos lastMarker(8 * 22050);
    pTrack->trySetBeats(mixxx::Beats::fromBeatMarkers(SampleRate(kRate),
            std::vector<mixxx::BeatMarker>{marker},
            lastMarker,
            mixxx::Bpm(140)));
    const auto data = build(pTrack);
    ASSERT_TRUE(data.has_value());
    ASSERT_GT(data->beats.size(), 20);
    for (int i = 0; i < data->beats.size(); ++i) {
        EXPECT_EQ(i < 8 ? 12000 : 14000, data->beats[i].tempo) << i;
    }
    EXPECT_NEAR(0.0, data->beats[0].position.value(), 0.5);
    EXPECT_NEAR(8 * 22050.0, data->beats[8].position.value(), 0.5);
}

TEST_F(AnlzTrackDataTest, BeatMapWithIntroHasSaneTempos) {
    constexpr int kRate = 44100;
    const auto pTrack = makeTrack(kRate);
    // The first marker is at about 5.1 s, so the grid has beats before it.
    // 8 beats of 120 BPM, then 140 BPM.
    constexpr double kFirstMarker = 225000;
    constexpr double kTempoChange = kFirstMarker + 8 * 22050;
    pTrack->trySetBeats(mixxx::Beats::fromBeatMarkers(SampleRate(kRate),
            std::vector<mixxx::BeatMarker>{mixxx::BeatMarker(FramePos(kFirstMarker), 8)},
            FramePos(kTempoChange),
            mixxx::Bpm(140)));
    const auto data = build(pTrack);
    ASSERT_TRUE(data.has_value());
    ASSERT_GT(data->beats.size(), 40);
    EXPECT_LT(data->beats.first().position.value(), 5.0 * kRate);
    EXPECT_LT(data->beats.first().position.value(), kFirstMarker);
    int beatsBeforeFirstMarker = 0;
    for (int i = 0; i < data->beats.size(); ++i) {
        const double position = data->beats[i].position.value();
        beatsBeforeFirstMarker += position < kFirstMarker - 0.5 ? 1 : 0;
        const int expected = position < kTempoChange - 0.5 ? 12000 : 14000;
        EXPECT_NEAR(expected, data->beats[i].tempo, 1) << i << " at " << position;
    }
    EXPECT_GT(beatsBeforeFirstMarker, 0);
}

} // namespace
