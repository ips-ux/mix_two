#include "library/rekordbox/anlzwriter.h"

#include <gtest/gtest.h>
#include <rekordbox_anlz.h>

#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QtDebug>
#include <QtGlobal>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "audio/frame.h"
#include "audio/types.h"
#include "library/rekordbox/anlztrackdata.h"
#include "library/rekordbox/anlzwaveform.h"
#include "waveform/waveform.h"

namespace {

using mixxx::audio::FramePos;
using mixxx::audio::SampleRate;
using mixxx::rekordbox::AnlzCue;
using mixxx::rekordbox::AnlzTrackData;
using mixxx::rekordbox::AnlzWaveforms;
using mixxx::rekordbox::GridBeat;
using mixxx::rekordbox::writeAnlzFiles;
namespace anlz = mixxx::rekordbox::anlz;

constexpr int kSampleRateHz = 44100;
constexpr SINT kFrames = static_cast<SINT>(kSampleRateHz) * 60;

/// The frame of a time in ms at 44.1 kHz (exact for the values used here).
FramePos framesAtMs(double ms) {
    return FramePos(ms * kSampleRateHz / 1000.0);
}

/// A parsed analysis file. The Kaitai objects read from the stream, so the
/// stream must live as long as the parsed file.
class ParsedAnlz {
  public:
    explicit ParsedAnlz(const QByteArray& bytes)
            : m_stream(std::string(bytes.constData(), static_cast<size_t>(bytes.size()))),
              m_kaitaiStream(&m_stream),
              m_anlz(&m_kaitaiStream) {
    }

    rekordbox_anlz_t& anlz() {
        return m_anlz;
    }

    /// The sections with a tag, in file order.
    std::vector<rekordbox_anlz_t::tagged_section_t*> sections(
            rekordbox_anlz_t::section_tags_t tag) {
        std::vector<rekordbox_anlz_t::tagged_section_t*> result;
        for (const auto& pSection : *m_anlz.sections()) {
            if (pSection->fourcc() == tag) {
                result.push_back(pSection.get());
            }
        }
        return result;
    }

    template<typename T>
    T* body(rekordbox_anlz_t::tagged_section_t* pSection) {
        return static_cast<T*>(pSection->body());
    }

    /// The cue list (PCOB) of a type.
    rekordbox_anlz_t::cue_tag_t* cueList(rekordbox_anlz_t::cue_list_type_t type) {
        for (auto* pSection : sections(rekordbox_anlz_t::SECTION_TAGS_CUES)) {
            auto* pTag = body<rekordbox_anlz_t::cue_tag_t>(pSection);
            if (pTag->type() == type) {
                return pTag;
            }
        }
        return nullptr;
    }

    /// The extended cue list (PCO2) of a type.
    rekordbox_anlz_t::cue_extended_tag_t* extendedCueList(
            rekordbox_anlz_t::cue_list_type_t type) {
        for (auto* pSection : sections(rekordbox_anlz_t::SECTION_TAGS_CUES_2)) {
            auto* pTag = body<rekordbox_anlz_t::cue_extended_tag_t>(pSection);
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

/// A section as it lies in the bytes of a file.
struct RawSection {
    QString fourcc;
    quint32 lenHeader;
    quint32 lenTag;
    QByteArray bytes;
};

quint32 u32At(const QByteArray& bytes, int offset) {
    return (static_cast<quint32>(static_cast<quint8>(bytes.at(offset))) << 24) |
            (static_cast<quint32>(static_cast<quint8>(bytes.at(offset + 1))) << 16) |
            (static_cast<quint32>(static_cast<quint8>(bytes.at(offset + 2))) << 8) |
            static_cast<quint32>(static_cast<quint8>(bytes.at(offset + 3)));
}

/// Splits a file after its 28 byte header into sections.
std::vector<RawSection> rawSections(const QByteArray& file) {
    std::vector<RawSection> result;
    int offset = static_cast<int>(u32At(file, 4));
    while (offset + 12 <= file.size()) {
        RawSection raw;
        raw.fourcc = QString::fromLatin1(file.mid(offset, 4));
        raw.lenHeader = u32At(file, offset + 4);
        raw.lenTag = u32At(file, offset + 8);
        raw.bytes = file.mid(offset, static_cast<int>(raw.lenTag));
        result.push_back(raw);
        offset += static_cast<int>(raw.lenTag);
    }
    return result;
}

QStringList fourccs(const std::vector<RawSection>& sections) {
    QStringList result;
    for (const auto& section : sections) {
        result << section.fourcc;
    }
    return result;
}

QList<quint32> lenHeaders(const std::vector<RawSection>& sections) {
    QList<quint32> result;
    for (const auto& section : sections) {
        result << section.lenHeader;
    }
    return result;
}

/// The n-th section with a four letter tag.
RawSection rawSectionOf(const QByteArray& file, const char* fourcc, int n = 0) {
    for (const auto& section : rawSections(file)) {
        if (section.fourcc == QLatin1String(fourcc) && n-- == 0) {
            return section;
        }
    }
    ADD_FAILURE() << "No section " << fourcc;
    return RawSection{};
}

/// Decodes the UTF-16BE bytes that Kaitai gives (it does not convert here).
QString fromUtf16Be(const std::string& bytes, bool dropTrailingNul) {
    QString result;
    for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
        result.append(QChar(static_cast<ushort>(
                (static_cast<quint8>(bytes[i]) << 8) |
                static_cast<quint8>(bytes[i + 1]))));
    }
    if (dropTrailingNul && result.endsWith(QChar(0))) {
        result.chop(1);
    }
    return result;
}

AnlzWaveforms sampleWaveforms() {
    Waveform waveform(kSampleRateHz, kFrames, 441, -1, 0);
    WaveformData* pData = waveform.data();
    for (int i = 0; i < waveform.getDataSize(); ++i) {
        const auto value = static_cast<unsigned char>(60 + (i % 100));
        pData[i].filtered.low = value;
        pData[i].filtered.mid = value;
        pData[i].filtered.high = value;
        pData[i].filtered.all = value;
    }
    return mixxx::rekordbox::anlzWaveformsFrom(waveform, kFrames, SampleRate(kSampleRateHz));
}

AnlzCue cue(int hotCue,
        quint32 timeMs,
        std::optional<quint32> loopEndMs = std::nullopt,
        const QString& comment = QString(),
        std::optional<QRgb> color = std::nullopt) {
    AnlzCue result;
    result.hotCue = hotCue;
    result.timeMs = timeMs;
    result.loopEndMs = loopEndMs;
    result.comment = comment;
    result.color = color;
    return result;
}

class AnlzWriterTest : public testing::Test {
  protected:
    void SetUp() override {
        m_data.stickPath = QStringLiteral("/Contents/Artist/Album/file.mp3");
        m_data.sampleRate = SampleRate(kSampleRateHz);
        m_data.frames = kFrames;
        // 128 BPM from 500 ms: 468.75 ms between the beats.
        for (int i = 0; i < 3; ++i) {
            m_data.beats.append(GridBeat{framesAtMs(500.0 + 468.75 * i), 12800});
            m_data.beatInBar.append(i + 1);
        }
        m_data.hotCues = {
                cue(1, 500, std::nullopt, QStringLiteral("Drop"), qRgb(0xF8, 0x09, 0x00)),
                cue(4, 10000, 11875u),
                cue(8, 30000),
        };
        m_data.memoryCues = {
                cue(0, 1000, std::nullopt, QString(), qRgb(0x16, 0xC0, 0xF8)),
                cue(0, 2000),
                cue(0, 5000, 6875u),
        };
        m_data.waveforms = sampleWaveforms();
        m_data.mp3PcmFrames = 0;
    }

    AnlzTrackData m_data;
};

TEST_F(AnlzWriterTest, DatHasSectionsInRekordboxOrder) {
    const QByteArray bytes = anlz::datBytes(m_data);

    EXPECT_EQ(QByteArray("PMAI"), bytes.left(4));
    EXPECT_EQ(28u, u32At(bytes, 4));
    EXPECT_EQ(static_cast<quint32>(bytes.size()), u32At(bytes, 8));
    EXPECT_EQ(1u, u32At(bytes, 12));
    EXPECT_EQ(0x10000u, u32At(bytes, 16));
    EXPECT_EQ(0x10000u, u32At(bytes, 20));
    EXPECT_EQ(0u, u32At(bytes, 24));

    const auto sections = rawSections(bytes);
    EXPECT_EQ((QStringList{"PPTH", "PVBR", "PQTZ", "PWAV", "PWV2", "PCOB", "PCOB"}),
            fourccs(sections));
    EXPECT_EQ((QList<quint32>{16, 16, 24, 20, 20, 24, 24}), lenHeaders(sections));

    // The sections fill the file to its end.
    int total = 28;
    for (const auto& section : sections) {
        total += static_cast<int>(section.lenTag);
    }
    EXPECT_EQ(bytes.size(), total);

    ParsedAnlz parsed(bytes);
    EXPECT_EQ(7u, parsed.anlz().sections()->size());
}

TEST_F(AnlzWriterTest, ExtHasSectionsInRekordboxOrder) {
    const QByteArray bytes = anlz::extBytes(m_data);

    EXPECT_EQ(QByteArray("PMAI"), bytes.left(4));
    EXPECT_EQ(28u, u32At(bytes, 4));
    EXPECT_EQ(static_cast<quint32>(bytes.size()), u32At(bytes, 8));

    const auto sections = rawSections(bytes);
    EXPECT_EQ((QStringList{"PPTH", "PWV3", "PCOB", "PCOB", "PCO2", "PCO2", "PWV5", "PWV4"}),
            fourccs(sections));
    EXPECT_EQ((QList<quint32>{16, 24, 24, 24, 20, 20, 24, 24}), lenHeaders(sections));
    EXPECT_FALSE(fourccs(sections).contains(QStringLiteral("PQT2")));

    ParsedAnlz parsed(bytes);
    EXPECT_EQ(8u, parsed.anlz().sections()->size());

    // The waveform bodies hold the bytes of the conversion.
    const AnlzWaveforms& w = m_data.waveforms;
    ASSERT_FALSE(w.detail.isEmpty());
    const auto pScroll = parsed.body<rekordbox_anlz_t::wave_scroll_tag_t>(
            parsed.sections(rekordbox_anlz_t::SECTION_TAGS_WAVE_SCROLL).at(0));
    EXPECT_EQ(1u, pScroll->len_entry_bytes());
    EXPECT_EQ(static_cast<quint32>(w.detail.size()), pScroll->len_entries());
    EXPECT_EQ(std::string(w.detail.constData(), static_cast<size_t>(w.detail.size())),
            pScroll->entries());
    const auto pColorScroll = parsed.body<rekordbox_anlz_t::wave_color_scroll_tag_t>(
            parsed.sections(rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_SCROLL).at(0));
    EXPECT_EQ(2u, pColorScroll->len_entry_bytes());
    EXPECT_EQ(static_cast<quint32>(w.detailEntries), pColorScroll->len_entries());
    const auto pColorPreview = parsed.body<rekordbox_anlz_t::wave_color_preview_tag_t>(
            parsed.sections(rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_PREVIEW).at(0));
    EXPECT_EQ(6u, pColorPreview->len_entry_bytes());
    EXPECT_EQ(1200u, pColorPreview->len_entries());
}

TEST_F(AnlzWriterTest, TwoExHasSectionsInRekordboxOrder) {
    const QByteArray bytes = anlz::twoExBytes(m_data);

    EXPECT_EQ(QByteArray("PMAI"), bytes.left(4));
    EXPECT_EQ(static_cast<quint32>(bytes.size()), u32At(bytes, 8));

    const auto sections = rawSections(bytes);
    EXPECT_EQ((QStringList{"PPTH", "PWV7", "PWV6", "PWVC"}), fourccs(sections));
    EXPECT_EQ((QList<quint32>{16, 24, 20, 14}), lenHeaders(sections));
    EXPECT_EQ(20u, sections.at(3).lenTag);
    EXPECT_EQ(QByteArray::fromHex("0000006400640064"), sections.at(3).bytes.mid(12));

    ParsedAnlz parsed(bytes);
    EXPECT_EQ(4u, parsed.anlz().sections()->size());
    const auto pBands = parsed.body<rekordbox_anlz_t::wave_3band_scroll_tag_t>(
            parsed.sections(rekordbox_anlz_t::SECTION_TAGS_WAVE_3BAND_SCROLL).at(0));
    EXPECT_EQ(3u, pBands->len_entry_bytes());
    EXPECT_EQ(static_cast<quint32>(m_data.waveforms.detailEntries), pBands->len_entries());
    const auto pBandsPreview = parsed.body<rekordbox_anlz_t::wave_3band_preview_tag_t>(
            parsed.sections(rekordbox_anlz_t::SECTION_TAGS_WAVE_3BAND_PREVIEW).at(0));
    EXPECT_EQ(3u, pBandsPreview->len_entry_bytes());
    EXPECT_EQ(1200u, pBandsPreview->len_entries());
}

TEST_F(AnlzWriterTest, HotCuesSplitBetweenDatAndExt) {
    ParsedAnlz dat(anlz::datBytes(m_data));
    auto* pDatHot = dat.cueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
    ASSERT_NE(nullptr, pDatHot);
    ASSERT_EQ(1u, pDatHot->cues()->size());
    EXPECT_EQ(1u, pDatHot->cues()->at(0)->hot_cue());
    EXPECT_EQ(0xffffffffu, pDatHot->memory_count());
    auto* pDatMemory = dat.cueList(rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES);
    ASSERT_NE(nullptr, pDatMemory);
    EXPECT_EQ(3u, pDatMemory->cues()->size());
    EXPECT_EQ(0u, pDatMemory->memory_count());

    ParsedAnlz ext(anlz::extBytes(m_data));
    auto* pExtHot = ext.cueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
    ASSERT_NE(nullptr, pExtHot);
    ASSERT_EQ(2u, pExtHot->cues()->size());
    EXPECT_EQ(4u, pExtHot->cues()->at(0)->hot_cue());
    EXPECT_EQ(8u, pExtHot->cues()->at(1)->hot_cue());
    EXPECT_EQ(0xffffffffu, pExtHot->memory_count());
    auto* pExtMemory = ext.cueList(rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES);
    ASSERT_NE(nullptr, pExtMemory);
    EXPECT_EQ(0u, pExtMemory->cues()->size());
    EXPECT_EQ(0xffffffffu, pExtMemory->memory_count());

    auto* pExtHot2 = ext.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
    ASSERT_NE(nullptr, pExtHot2);
    ASSERT_EQ(3u, pExtHot2->cues()->size());
    EXPECT_EQ(1u, pExtHot2->cues()->at(0)->hot_cue());
    EXPECT_EQ(4u, pExtHot2->cues()->at(1)->hot_cue());
    EXPECT_EQ(8u, pExtHot2->cues()->at(2)->hot_cue());
    auto* pExtMemory2 = ext.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES);
    ASSERT_NE(nullptr, pExtMemory2);
    EXPECT_EQ(3u, pExtMemory2->cues()->size());
}

TEST_F(AnlzWriterTest, HotCueEntryBytesMatchReference) {
    const QByteArray dat = anlz::datBytes(m_data);
    const QByteArray ext = anlz::extBytes(m_data);

    // PCPT of hot cue A at 500 ms: the first entry of the DAT hot list.
    const RawSection pcob = rawSectionOf(dat, "PCOB", 0);
    const QByteArray expectedPcpt = QByteArray::fromHex(
                                            "50435054"
                                            "0000001c"
                                            "00000038"
                                            "00000001"
                                            "00000000"
                                            "00010000"
                                            "ffff"
                                            "ffff"
                                            "01"
                                            "0003e8"
                                            "000001f4"
                                            "ffffffff") +
            QByteArray(16, '\0');
    ASSERT_EQ(56, expectedPcpt.size());
    // 12 bytes of tag head + 12 bytes of list head, then the entries.
    EXPECT_EQ(expectedPcpt, pcob.bytes.mid(24, 56));
    EXPECT_EQ(24u + 56u, pcob.lenTag);

    // PCP2 of the same cue, with the comment "Drop" and the colour F80900.
    const RawSection pco2 = rawSectionOf(ext, "PCO2", 0);
    const QByteArray expectedPcp2 = QByteArray::fromHex(
                                            "50435032"
                                            "00000010"
                                            "00000062"
                                            "00000001"
                                            "01"
                                            "0003e8"
                                            "000001f4"
                                            "ffffffff"
                                            "00"
                                            "01"
                                            "000000000000"
                                            "0000"
                                            "0000"
                                            "0000000a"
                                            "00440072006f0070"
                                            "0000"
                                            "00"
                                            "f80900") +
            QByteArray(40, '\0');
    // 88 + 10 bytes.
    ASSERT_EQ(98, expectedPcp2.size());
    EXPECT_EQ(expectedPcp2, pco2.bytes.mid(20, 98));
    ASSERT_GE(pco2.bytes.size(), 20 + 98);
    EXPECT_EQ(20u + 98u + 88u + 88u, pco2.lenTag); // plus hot cues D and H

    ParsedAnlz parsed(ext);
    auto* pList = parsed.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
    ASSERT_NE(nullptr, pList);
    const auto& entry = pList->cues()->at(0);
    EXPECT_EQ(98u, entry->len_entry());
    EXPECT_EQ(10u, entry->len_comment());
    EXPECT_EQ(0xf8, entry->color_red());
    EXPECT_EQ(0x09, entry->color_green());
    EXPECT_EQ(0x00, entry->color_blue());
    EXPECT_EQ(QStringLiteral("Drop"), fromUtf16Be(entry->comment(), true));
}

TEST_F(AnlzWriterTest, LoopEntriesCarryTheEnd) {
    // Hot cue D (4) is a loop from 10000 to 11875 ms.
    ParsedAnlz ext(anlz::extBytes(m_data));
    auto* pHot = ext.cueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
    ASSERT_NE(nullptr, pHot);
    const auto& pcpt = pHot->cues()->at(0);
    EXPECT_EQ(4u, pcpt->hot_cue());
    EXPECT_EQ(rekordbox_anlz_t::CUE_ENTRY_TYPE_LOOP, pcpt->type());
    EXPECT_EQ(10000u, pcpt->time());
    EXPECT_EQ(11875u, pcpt->loop_time());
    // Hot cue H (8) is a plain cue: type 1, no end.
    const auto& plain = pHot->cues()->at(1);
    EXPECT_EQ(rekordbox_anlz_t::CUE_ENTRY_TYPE_MEMORY_CUE, plain->type());
    EXPECT_EQ(30000u, plain->time());
    EXPECT_EQ(0xffffffffu, plain->loop_time());

    auto* pHot2 = ext.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
    ASSERT_NE(nullptr, pHot2);
    const auto& pcp2 = pHot2->cues()->at(1);
    EXPECT_EQ(4u, pcp2->hot_cue());
    EXPECT_EQ(rekordbox_anlz_t::CUE_ENTRY_TYPE_LOOP, pcp2->type());
    EXPECT_EQ(10000u, pcp2->time());
    EXPECT_EQ(11875u, pcp2->loop_time());
    // A hot cue with no colour writes 0 0 0 and colour ID 0.
    const auto& noColor = pHot2->cues()->at(2);
    EXPECT_EQ(0u, noColor->color_id());
    EXPECT_EQ(0, noColor->color_red());
    EXPECT_EQ(0, noColor->color_green());
    EXPECT_EQ(0, noColor->color_blue());

    // The memory loop in the DAT and EXT lists.
    ParsedAnlz dat(anlz::datBytes(m_data));
    auto* pMemory = dat.cueList(rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES);
    ASSERT_NE(nullptr, pMemory);
    ASSERT_EQ(3u, pMemory->cues()->size());
    EXPECT_EQ(0u, pMemory->cues()->at(2)->hot_cue());
    EXPECT_EQ(rekordbox_anlz_t::CUE_ENTRY_TYPE_LOOP, pMemory->cues()->at(2)->type());
    EXPECT_EQ(5000u, pMemory->cues()->at(2)->time());
    EXPECT_EQ(6875u, pMemory->cues()->at(2)->loop_time());
    auto* pMemory2 = ext.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES);
    ASSERT_NE(nullptr, pMemory2);
    EXPECT_EQ(rekordbox_anlz_t::CUE_ENTRY_TYPE_LOOP, pMemory2->cues()->at(2)->type());
    EXPECT_EQ(6875u, pMemory2->cues()->at(2)->loop_time());
}

TEST_F(AnlzWriterTest, MemoryCueColorBecomesNearestId) {
    m_data.memoryCues = {
            cue(0, 1000, std::nullopt, QString(), qRgb(0x16, 0xC0, 0xF8)), // Aqua
            cue(0, 2000, std::nullopt, QString(), qRgb(0x10, 0xC8, 0xF0)), // near Aqua
            cue(0, 3000),                                                  // none
            cue(0, 4000, std::nullopt, QString(), qRgb(0, 0, 0)),          // black
            cue(0, 5000, std::nullopt, QString(), qRgb(0xF8, 0x09, 0x00)), // Red
            cue(0, 6000, std::nullopt, QString(), qRgb(0x98, 0x08, 0xF8)), // Purple
    };
    ParsedAnlz ext(anlz::extBytes(m_data));
    auto* pList = ext.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES);
    ASSERT_NE(nullptr, pList);
    ASSERT_EQ(6u, pList->cues()->size());
    EXPECT_EQ(6u, pList->cues()->at(0)->color_id());
    EXPECT_EQ(6u, pList->cues()->at(1)->color_id());
    EXPECT_EQ(0u, pList->cues()->at(2)->color_id());
    EXPECT_EQ(0u, pList->cues()->at(3)->color_id());
    EXPECT_EQ(2u, pList->cues()->at(4)->color_id());
    EXPECT_EQ(8u, pList->cues()->at(5)->color_id());
    // A memory cue never writes the RGB bytes.
    EXPECT_EQ(0, pList->cues()->at(0)->color_red());
    EXPECT_EQ(0, pList->cues()->at(0)->color_green());
    EXPECT_EQ(0, pList->cues()->at(0)->color_blue());
}

TEST_F(AnlzWriterTest, BeatGridEntries) {
    ParsedAnlz dat(anlz::datBytes(m_data));
    const auto sections = dat.sections(rekordbox_anlz_t::SECTION_TAGS_BEAT_GRID);
    ASSERT_EQ(1u, sections.size());
    auto* pGrid = dat.body<rekordbox_anlz_t::beat_grid_tag_t>(sections.at(0));
    EXPECT_EQ(0x80000u, pGrid->_unnamed1());
    ASSERT_EQ(3u, pGrid->num_beats());
    const auto& beats = *pGrid->beats();
    EXPECT_EQ(1u, beats[0]->beat_number());
    EXPECT_EQ(2u, beats[1]->beat_number());
    EXPECT_EQ(3u, beats[2]->beat_number());
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(12800u, beats[static_cast<size_t>(i)]->tempo());
    }
    // 500, 968.75, 1437.5 ms rounded.
    EXPECT_EQ(500u, beats[0]->time());
    EXPECT_EQ(969u, beats[1]->time());
    EXPECT_EQ(1438u, beats[2]->time());
    EXPECT_EQ(24u + 3u * 8u, sections.at(0)->len_tag());
}

TEST_F(AnlzWriterTest, BeatNumbersAreTheOnesOfTheData) {
    // The first beat of the file is beat 3 of its bar; the writer does not
    // number the beats itself.
    m_data.beats.clear();
    m_data.beatInBar.clear();
    const std::vector<unsigned> expected = {3, 4, 1, 2, 3, 4};
    for (size_t i = 0; i < expected.size(); ++i) {
        m_data.beats.append(GridBeat{framesAtMs(500.0 * static_cast<double>(i)), 12000});
        m_data.beatInBar.append(static_cast<int>(expected[i]));
    }
    ParsedAnlz dat(anlz::datBytes(m_data));
    auto* pGrid = dat.body<rekordbox_anlz_t::beat_grid_tag_t>(
            dat.sections(rekordbox_anlz_t::SECTION_TAGS_BEAT_GRID).at(0));
    ASSERT_EQ(expected.size(), pGrid->beats()->size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(expected[i], pGrid->beats()->at(i)->beat_number());
    }
}

TEST_F(AnlzWriterTest, BeatWithoutNumberIsBeatOne) {
    m_data.beatInBar.clear();
    ParsedAnlz dat(anlz::datBytes(m_data));
    auto* pGrid = dat.body<rekordbox_anlz_t::beat_grid_tag_t>(
            dat.sections(rekordbox_anlz_t::SECTION_TAGS_BEAT_GRID).at(0));
    ASSERT_EQ(3u, pGrid->beats()->size());
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(1u, pGrid->beats()->at(i)->beat_number());
    }
}

TEST_F(AnlzWriterTest, WritesEmptyBeatGrid) {
    m_data.beats.clear();
    const QByteArray dat = anlz::datBytes(m_data);
    const RawSection pqtz = rawSectionOf(dat, "PQTZ");
    EXPECT_EQ(24u, pqtz.lenTag);
    EXPECT_EQ(24u, pqtz.lenHeader);

    ParsedAnlz parsedDat(dat);
    auto* pGrid = parsedDat.body<rekordbox_anlz_t::beat_grid_tag_t>(
            parsedDat.sections(rekordbox_anlz_t::SECTION_TAGS_BEAT_GRID).at(0));
    EXPECT_EQ(0u, pGrid->num_beats());
    ParsedAnlz parsedExt(anlz::extBytes(m_data));
    ParsedAnlz parsedTwoEx(anlz::twoExBytes(m_data));
}

TEST_F(AnlzWriterTest, TimingOffsetIsAdded) {
    m_data.beats = {
            GridBeat{framesAtMs(0.0), 12800},
            GridBeat{framesAtMs(500.0), 12800},
    };
    m_data.timingOffsetMs = 26;
    {
        ParsedAnlz dat(anlz::datBytes(m_data));
        auto* pGrid = dat.body<rekordbox_anlz_t::beat_grid_tag_t>(
                dat.sections(rekordbox_anlz_t::SECTION_TAGS_BEAT_GRID).at(0));
        ASSERT_EQ(2u, pGrid->beats()->size());
        EXPECT_EQ(26u, pGrid->beats()->at(0)->time());
        EXPECT_EQ(526u, pGrid->beats()->at(1)->time());
    }
    // A negative offset never gives a time below 0.
    m_data.timingOffsetMs = -100;
    {
        ParsedAnlz dat(anlz::datBytes(m_data));
        auto* pGrid = dat.body<rekordbox_anlz_t::beat_grid_tag_t>(
                dat.sections(rekordbox_anlz_t::SECTION_TAGS_BEAT_GRID).at(0));
        EXPECT_EQ(0u, pGrid->beats()->at(0)->time());
        EXPECT_EQ(400u, pGrid->beats()->at(1)->time());
    }
    // The offset moves the cue starts and the loop ends too, in PCPT and in
    // PCP2 (the cue times of AnlzCue are audio time).
    m_data.timingOffsetMs = 26;
    {
        ParsedAnlz dat(anlz::datBytes(m_data));
        auto* pDatHot = dat.cueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
        ASSERT_NE(nullptr, pDatHot);
        EXPECT_EQ(526u, pDatHot->cues()->at(0)->time());
        auto* pDatMemory = dat.cueList(rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES);
        ASSERT_NE(nullptr, pDatMemory);
        EXPECT_EQ(1026u, pDatMemory->cues()->at(0)->time());
        EXPECT_EQ(0xffffffffu, pDatMemory->cues()->at(0)->loop_time());
        EXPECT_EQ(5026u, pDatMemory->cues()->at(2)->time());
        EXPECT_EQ(6901u, pDatMemory->cues()->at(2)->loop_time());

        ParsedAnlz ext(anlz::extBytes(m_data));
        auto* pExtHot = ext.cueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
        ASSERT_NE(nullptr, pExtHot);
        EXPECT_EQ(10026u, pExtHot->cues()->at(0)->time());
        EXPECT_EQ(11901u, pExtHot->cues()->at(0)->loop_time());
        EXPECT_EQ(30026u, pExtHot->cues()->at(1)->time());
        EXPECT_EQ(0xffffffffu, pExtHot->cues()->at(1)->loop_time());
        auto* pHot2 = ext.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
        ASSERT_NE(nullptr, pHot2);
        EXPECT_EQ(526u, pHot2->cues()->at(0)->time());
        EXPECT_EQ(0xffffffffu, pHot2->cues()->at(0)->loop_time());
        EXPECT_EQ(10026u, pHot2->cues()->at(1)->time());
        EXPECT_EQ(11901u, pHot2->cues()->at(1)->loop_time());
        auto* pMemory2 = ext.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES);
        ASSERT_NE(nullptr, pMemory2);
        EXPECT_EQ(5026u, pMemory2->cues()->at(2)->time());
        EXPECT_EQ(6901u, pMemory2->cues()->at(2)->loop_time());
    }
    // A negative offset never gives a time below 0: a cue at 50 ms with -100.
    m_data.timingOffsetMs = -100;
    m_data.hotCues[0].timeMs = 50;
    {
        ParsedAnlz dat(anlz::datBytes(m_data));
        auto* pDatHot = dat.cueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
        ASSERT_NE(nullptr, pDatHot);
        EXPECT_EQ(0u, pDatHot->cues()->at(0)->time());
        ParsedAnlz ext(anlz::extBytes(m_data));
        auto* pHot2 = ext.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
        ASSERT_NE(nullptr, pHot2);
        EXPECT_EQ(0u, pHot2->cues()->at(0)->time());
        EXPECT_EQ(9900u, pHot2->cues()->at(1)->time());
        EXPECT_EQ(11775u, pHot2->cues()->at(1)->loop_time());
    }
}

TEST_F(AnlzWriterTest, UnicodePathAndComment) {
    const QString path = QString::fromUtf16(
            u"/Contents/Ünïcødé/日本語/\U0001F3A7.mp3");
    const QString comment = QString::fromUtf16(u"\U0001F3A7 drop");
    m_data.stickPath = path;
    m_data.hotCues[0].comment = comment;

    for (const QByteArray& bytes : {anlz::datBytes(m_data),
                 anlz::extBytes(m_data),
                 anlz::twoExBytes(m_data)}) {
        const RawSection ppth = rawSectionOf(bytes, "PPTH");
        ASSERT_EQ(16u, ppth.lenHeader);
        // The surrogate pair of the headphones is two code units.
        EXPECT_EQ(static_cast<quint32>(2 * path.size() + 2), u32At(ppth.bytes, 12));
        EXPECT_EQ(16u + 2u * static_cast<quint32>(path.size()) + 2u, ppth.lenTag);

        ParsedAnlz parsed(bytes);
        auto* pPath = parsed.body<rekordbox_anlz_t::path_tag_t>(
                parsed.sections(rekordbox_anlz_t::SECTION_TAGS_PATH).at(0));
        EXPECT_EQ(path, fromUtf16Be(pPath->path(), false));
    }

    ParsedAnlz ext(anlz::extBytes(m_data));
    auto* pHot = ext.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
    ASSERT_NE(nullptr, pHot);
    const auto& entry = pHot->cues()->at(0);
    EXPECT_EQ(static_cast<quint32>(2 * comment.size() + 2), entry->len_comment());
    EXPECT_EQ(88u + entry->len_comment(), entry->len_entry());
    EXPECT_EQ(comment, fromUtf16Be(entry->comment(), true));
    // The cues that follow are still at the right place.
    EXPECT_EQ(4u, pHot->cues()->at(1)->hot_cue());
    EXPECT_EQ(10000u, pHot->cues()->at(1)->time());
}

TEST_F(AnlzWriterTest, EmptyPathAndCommentHaveNoBytes) {
    m_data.stickPath.clear();
    const QByteArray dat = anlz::datBytes(m_data);
    const RawSection ppth = rawSectionOf(dat, "PPTH");
    EXPECT_EQ(16u, ppth.lenTag);
    EXPECT_EQ(0u, u32At(ppth.bytes, 12));

    ParsedAnlz ext(anlz::extBytes(m_data));
    auto* pHot = ext.extendedCueList(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES);
    ASSERT_NE(nullptr, pHot);
    const auto& noComment = pHot->cues()->at(1);
    EXPECT_EQ(0u, noComment->len_comment());
    EXPECT_EQ(88u, noComment->len_entry());
}

TEST_F(AnlzWriterTest, PvbrHoldsMp3Frames) {
    const RawSection wav = rawSectionOf(anlz::datBytes(m_data), "PVBR");
    EXPECT_EQ(1620u, wav.lenTag);
    EXPECT_EQ(0u, u32At(wav.bytes, wav.bytes.size() - 4));

    m_data.mp3PcmFrames = 7304832;
    const QByteArray dat = anlz::datBytes(m_data);
    const RawSection pvbr = rawSectionOf(dat, "PVBR");
    EXPECT_EQ(1620u, pvbr.lenTag);
    EXPECT_EQ(16u, pvbr.lenHeader);
    EXPECT_EQ(7304832u, u32At(pvbr.bytes, pvbr.bytes.size() - 4));
    // The 400 index entries are 0.
    EXPECT_EQ(QByteArray(1600, '\0'), pvbr.bytes.mid(12, 1600));

    ParsedAnlz parsed(dat);
    auto* pVbr = parsed.body<rekordbox_anlz_t::vbr_tag_t>(
            parsed.sections(rekordbox_anlz_t::SECTION_TAGS_VBR).at(0));
    EXPECT_EQ(400u, pVbr->index()->size());
}

TEST_F(AnlzWriterTest, WritesThreeFilesIntoNewFolder) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString folder = tempDir.filePath(QStringLiteral("P001/0000ABCD"));

    QString error;
    ASSERT_TRUE(writeAnlzFiles(m_data, folder, &error)) << qPrintable(error);

    const auto readFile = [&folder](const char* name) {
        QFile file(QDir(folder).filePath(QString::fromLatin1(name)));
        EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << name;
        return file.readAll();
    };
    EXPECT_EQ(anlz::datBytes(m_data), readFile("ANLZ0000.DAT"));
    EXPECT_EQ(anlz::extBytes(m_data), readFile("ANLZ0000.EXT"));
    EXPECT_EQ(anlz::twoExBytes(m_data), readFile("ANLZ0000.2EX"));

    // A second write into the same folder replaces the files.
    m_data.stickPath = QStringLiteral("/Contents/Other/file.mp3");
    ASSERT_TRUE(writeAnlzFiles(m_data, folder, &error)) << qPrintable(error);
    EXPECT_EQ(anlz::datBytes(m_data), readFile("ANLZ0000.DAT"));

    // A folder that cannot exist: a file is the parent. (A read-only folder
    // is not a portable test: on Windows the read-only attribute of a folder
    // does not stop the creation of files in it.)
    QFile blocker(tempDir.filePath(QStringLiteral("blocker")));
    ASSERT_TRUE(blocker.open(QIODevice::WriteOnly));
    blocker.write("not a folder");
    blocker.close();
    error.clear();
    EXPECT_FALSE(writeAnlzFiles(m_data, tempDir.filePath(QStringLiteral("blocker/P001")), &error));
    EXPECT_TRUE(error.startsWith(
            QStringLiteral("Cannot write the analysis files:")))
            << qPrintable(error);
    // The error message is optional.
    EXPECT_FALSE(writeAnlzFiles(m_data, tempDir.filePath(QStringLiteral("blocker/P001")), nullptr));
    EXPECT_FALSE(QFileInfo::exists(tempDir.filePath(QStringLiteral("blocker/P001"))));

    // For tools/anlz_verify.py: also write the files into a folder that the
    // caller names. Never into the repository.
    const QString sampleDir = qEnvironmentVariable("MIXTWO_ANLZ_SAMPLE_DIR");
    if (!sampleDir.isEmpty()) {
        EXPECT_TRUE(writeAnlzFiles(m_data, sampleDir, &error)) << qPrintable(error);
    }
}

TEST_F(AnlzWriterTest, FailureOnTheSecondFileChangesNothing) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString folder = tempDir.filePath(QStringLiteral("P001/0000ABCD"));
    const QDir dir(folder);

    // An old set is in the folder.
    QString error;
    ASSERT_TRUE(writeAnlzFiles(m_data, folder, &error)) << qPrintable(error);
    const auto readFile = [&dir](const char* name) {
        QFile file(dir.filePath(QString::fromLatin1(name)));
        EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << name;
        return file.readAll();
    };
    const QByteArray oldDat = readFile("ANLZ0000.DAT");
    const QByteArray oldTwoEx = readFile("ANLZ0000.2EX");

    // The .EXT file cannot be written: a folder that is not empty has its
    // name.
    ASSERT_TRUE(QFile::remove(dir.filePath(QStringLiteral("ANLZ0000.EXT"))));
    ASSERT_TRUE(dir.mkdir(QStringLiteral("ANLZ0000.EXT")));
    QFile inner(dir.filePath(QStringLiteral("ANLZ0000.EXT/inner")));
    ASSERT_TRUE(inner.open(QIODevice::WriteOnly));
    inner.close();

    m_data.stickPath = QStringLiteral("/Contents/Other/file.mp3");
    EXPECT_FALSE(writeAnlzFiles(m_data, folder, &error));
    // The message names the file.
    EXPECT_TRUE(error.startsWith(QStringLiteral(
            "Cannot write the analysis files: "
            "ANLZ0000.EXT: ")))
            << qPrintable(error);
    // The failure came before the first commit: the new .DAT file is not in
    // place, and the old files and the folder are as they were. No temporary
    // file of QSaveFile stays.
    EXPECT_EQ(oldDat, readFile("ANLZ0000.DAT"));
    EXPECT_EQ(oldTwoEx, readFile("ANLZ0000.2EX"));
    EXPECT_EQ(3, dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size())
            << qPrintable(dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot)
                               .join(QLatin1Char(',')));
}

TEST_F(AnlzWriterTest, ReferenceStickSectionsParse) {
    // Needs a local copy of the analysis files of a stick that rekordbox wrote
    // (the folder with PIONEER/USBANLZ in it). Nothing from it goes into the
    // repository, and every file is opened read-only.
    const QString referenceDir = qEnvironmentVariable("MIXTWO_DLP_REFERENCE");
    if (referenceDir.isEmpty()) {
        GTEST_SKIP() << "MIXTWO_DLP_REFERENCE is not set.";
    }

    const QHash<QString, QStringList> expectedOrder = {
            {QStringLiteral("DAT"),
                    {"PPTH", "PVBR", "PQTZ", "PWAV", "PWV2", "PCOB", "PCOB"}},
            {QStringLiteral("EXT"),
                    {"PPTH", "PWV3", "PCOB", "PCOB", "PCO2", "PCO2", "PWV5", "PWV4"}},
            {QStringLiteral("2EX"), {"PPTH", "PWV7", "PWV6", "PWVC"}},
    };

    int fileCount = 0;
    QDirIterator tracks(QDir(referenceDir).filePath(QStringLiteral("PIONEER/USBANLZ")),
            {QStringLiteral("ANLZ0000.DAT")},
            QDir::Files,
            QDirIterator::Subdirectories);
    while (tracks.hasNext()) {
        const QDir trackDir = QFileInfo(tracks.next()).dir();
        for (auto it = expectedOrder.cbegin(); it != expectedOrder.cend(); ++it) {
            const QString name = trackDir.dirName() + QStringLiteral(".") + it.key();
            QFile file(trackDir.filePath(QStringLiteral("ANLZ0000.") + it.key()));
            ASSERT_TRUE(file.open(QIODevice::ReadOnly)) << qPrintable(name);
            const QByteArray bytes = file.readAll();
            file.close();
            ++fileCount;

            // Kaitai does not know PQT2 and PWVC: the parser keeps the body of
            // such a section as bytes. The fourcc is read as a number and
            // changed to four letters, so that it also works for those.
            QStringList actual;
            try {
                ParsedAnlz parsed(bytes);
                for (const auto& pSection : *parsed.anlz().sections()) {
                    const auto tag = static_cast<quint32>(pSection->fourcc());
                    actual << QString::fromLatin1(QByteArray::fromHex(
                            QByteArray::number(tag, 16).rightJustified(8, '0')));
                }
            } catch (const std::exception& error) {
                // The name is the folder of the track and the file type, never
                // the path of the music file in the PPTH section.
                ADD_FAILURE() << "Kaitai cannot parse " << qPrintable(name) << ": "
                              << error.what();
                continue;
            }

            // rekordbox writes a PQT2 section after the second PCO2 of the
            // .EXT file, and may write a PSSI section at its end. Mix Two
            // writes neither.
            if (it.key() == QLatin1String("EXT")) {
                actual.removeAll(QStringLiteral("PQT2"));
                if (!actual.isEmpty() && actual.last() == QLatin1String("PSSI")) {
                    actual.removeLast();
                }
            }
            EXPECT_EQ(it.value(), actual) << qPrintable(name);
        }
    }
    EXPECT_GT(fileCount, 0) << "No ANLZ0000.DAT file under PIONEER/USBANLZ.";
    RecordProperty("checked_files", fileCount);
    qInfo() << "Reference stick: checked" << fileCount << "analysis files.";
}

TEST_F(AnlzWriterTest, MsFromFramesRounds) {
    const SampleRate rate(kSampleRateHz);
    EXPECT_EQ(1000u, anlz::msFromFrames(FramePos(44100), rate, 0));
    EXPECT_EQ(500u, anlz::msFromFrames(FramePos(22049), rate, 0));
    EXPECT_EQ(0u, anlz::msFromFrames(FramePos(44100), rate, -2000));
    EXPECT_EQ(1026u, anlz::msFromFrames(FramePos(44100), rate, 26));
    EXPECT_EQ(0u, anlz::msFromFrames(FramePos(-44100), rate, 0));
    // A position that is not valid or not finite gives 0.
    EXPECT_EQ(0u, anlz::msFromFrames(mixxx::audio::kInvalidFramePos, rate, 0));
    EXPECT_EQ(0u, anlz::msFromFrames(mixxx::audio::kInvalidFramePos, rate, 25));
    EXPECT_EQ(1000u, anlz::msFromFrames(FramePos(48000), SampleRate(48000), 0));
}

TEST_F(AnlzWriterTest, SectionAndFileHeaders) {
    const QByteArray body = QByteArray::fromHex("01020304");
    const QByteArray section = anlz::section("PTST", 16, body);
    EXPECT_EQ(QByteArray::fromHex("5054535400000010"
                                  "00000010"
                                  "01020304"),
            section);

    const QByteArray file = anlz::fileBytes({section, section});
    EXPECT_EQ(QByteArray("PMAI"), file.left(4));
    EXPECT_EQ(28u, u32At(file, 4));
    EXPECT_EQ(static_cast<quint32>(28 + 2 * section.size()), u32At(file, 8));
    EXPECT_EQ(file.size(), static_cast<int>(u32At(file, 8)));
    EXPECT_EQ(section + section, file.mid(28));
}

} // namespace
