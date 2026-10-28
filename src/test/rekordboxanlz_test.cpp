#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <cmath>

#include "library/rekordbox/rekordboxanlz.h"
#include "track/beats.h"
#include "track/cue.h"
#include "track/track.h"

namespace {

constexpr auto kSampleRate = mixxx::audio::SampleRate(44100);

/// Minimal rekordbox ANLZ files, laid out as lib/rekordbox-metadata/rekordbox_anlz.ksy
/// describes them (big endian).
class AnlzWriter {
  public:
    void u1(quint8 v) {
        m_bytes.append(static_cast<char>(v));
    }
    void u2(quint16 v) {
        char b[2];
        qToBigEndian(v, b);
        m_bytes.append(b, 2);
    }
    void u4(quint32 v) {
        char b[4];
        qToBigEndian(v, b);
        m_bytes.append(b, 4);
    }
    void tag(const char* fourcc) {
        m_bytes.append(fourcc, 4);
    }
    void zeros(int n) {
        m_bytes.append(QByteArray(n, '\0'));
    }

    /// PQTZ: `count` beats of a constant tempo, times rounded to whole ms.
    void beatGrid(double bpm, double firstBeatMs, int count) {
        tag("PQTZ");
        u4(24);
        u4(24 + 8 * count);
        u4(0);
        u4(0x80000);
        u4(count);
        for (int i = 0; i < count; ++i) {
            u2(static_cast<quint16>(i % 4 + 1));
            u2(static_cast<quint16>(std::lround(bpm * 100)));
            u4(static_cast<quint32>(std::lround(firstBeatMs + i * 60000.0 / bpm)));
        }
    }

    /// PCOB with one memory cue.
    void memoryCue(quint32 timeMs) {
        tag("PCOB");
        u4(24);
        u4(24 + 56);
        u4(0); // memory cues
        zeros(2);
        u2(1); // one cue
        u4(1);
        tag("PCPT");
        u4(28);
        u4(56);
        u4(0); // not a hot cue
        u4(1); // enabled
        u4(0x10000);
        u2(0xffff);
        u2(0xffff);
        u1(1); // memory cue
        zeros(3);
        u4(timeMs);
        u4(0xffffffff); // no loop
        zeros(16);
    }

    bool save(const QString& path) const {
        QByteArray file("PMAI");
        char b[4];
        qToBigEndian<quint32>(28, b);
        file.append(b, 4);
        qToBigEndian<quint32>(static_cast<quint32>(28 + m_bytes.size()), b);
        file.append(b, 4);
        file.append(QByteArray(28 - 12, '\0'));
        file.append(m_bytes);
        QFile out(path);
        return out.open(QIODevice::WriteOnly) && out.write(file) == file.size();
    }

  private:
    QByteArray m_bytes;
};

TrackPointer newTrack() {
    TrackPointer pTrack(Track::newTemporary());
    pTrack->setAudioProperties(mixxx::audio::ChannelCount(2),
            kSampleRate,
            mixxx::audio::Bitrate(),
            mixxx::Duration::fromSeconds(300));
    return pTrack;
}

class RekordboxAnlzTest : public testing::Test {
  protected:
    QString datPath() const {
        return m_dir.filePath(QStringLiteral("ANLZ0000.DAT"));
    }
    QString extPath() const {
        return m_dir.filePath(QStringLiteral("ANLZ0000.EXT"));
    }
    QTemporaryDir m_dir;
};

TEST_F(RekordboxAnlzTest, LegacyFileAloneGivesGridAndCues) {
    AnlzWriter dat;
    dat.beatGrid(132.0, 120, 400);
    dat.memoryCue(120);
    ASSERT_TRUE(dat.save(datPath()));

    const auto pTrack = newTrack();
    mixxx::rekordbox::readAnalysisFiles(pTrack, kSampleRate, 0, datPath());

    const auto pBeats = pTrack->getBeats();
    ASSERT_TRUE(pBeats);
    EXPECT_NEAR(132.0, pTrack->getBpm(), 0.005);
    EXPECT_NEAR(120 * kSampleRate / 1000.0, pTrack->getMainCuePosition().value(), 1.0);
}

TEST_F(RekordboxAnlzTest, GridFromLegacyFileCuesFromExtendedFile) {
    AnlzWriter dat;
    dat.beatGrid(128.0, 50, 400);
    dat.memoryCue(999); // ignored: the .EXT has the cues
    ASSERT_TRUE(dat.save(datPath()));
    AnlzWriter ext;
    ext.beatGrid(90.0, 50, 400); // ignored: grids come from the .DAT
    ASSERT_TRUE(ext.save(extPath()));

    const auto pTrack = newTrack();
    mixxx::rekordbox::readAnalysisFiles(pTrack, kSampleRate, 0, datPath());

    ASSERT_TRUE(pTrack->getBeats());
    EXPECT_NEAR(128.0, pTrack->getBpm(), 0.005);
    EXPECT_FALSE(pTrack->findCueByType(mixxx::CueType::MainCue) &&
            std::fabs(pTrack->getMainCuePosition().value() - 999 * kSampleRate / 1000.0) < 1.0);
}

} // namespace
