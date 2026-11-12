#include <gtest/gtest.h>

#include <QCryptographicHash>
#include <QDir>
#include <QtDebug>
#include <algorithm>
#include <cmath>
#include <vector>

#include "analyzer/analyzertrack.h"
#include "analyzer/analyzerwaveform.h"
#include "database/mixxxdb.h"
#include "library/dao/analysisdao.h"
#include "test/mixxxtest.h"
#include "track/track.h"
#include "util/db/dbconnectionpooled.h"
#include "util/db/dbconnectionpooler.h"
#include "waveform/waveform.h"

namespace {

constexpr std::size_t kBigBufSize = 2 * 1920; // Matches the WaveformSummary
constexpr std::size_t kCanarySize = 1024 * 4;
constexpr float kMagicFloat = 1234.567890f;
constexpr float kCanaryFloat = 0.0f;
constexpr int kChannelCount = 2;
const QString kReferenceBuffersPath = QStringLiteral("reference_buffers/");

class AnalyzerWaveformTest : public MixxxTest {
  protected:
    AnalyzerWaveformTest()
            : m_aw(config(), QSqlDatabase()) {
    }

    void SetUp() override {
        m_pTrack = Track::newTemporary();
        m_pTrack->setAudioProperties(
                mixxx::audio::ChannelCount(kChannelCount),
                mixxx::audio::SampleRate(44100),
                mixxx::audio::Bitrate(),
                mixxx::Duration::fromMillis(1000));

        // Memory layout for m_canaryBigBuf looks like
        //   [ canary | big buf | canary ]

        m_canaryBigBuf.resize(kBigBufSize + 2 * kCanarySize);
        for (std::size_t i = 0; i < kCanarySize; i++) {
            m_canaryBigBuf[i] = kCanaryFloat;
        }
        for (std::size_t i = kCanarySize; i < kCanarySize + kBigBufSize; i++) {
            m_canaryBigBuf[i] = kMagicFloat;
        }
        for (std::size_t i = kCanarySize + kBigBufSize; i < 2 * kCanarySize + kBigBufSize; i++) {
            m_canaryBigBuf[i] = kCanaryFloat;
        }
    }

    void assertWaveformReference(
            ConstWaveformPointer pWaveform,
            const QString& reference_title) {
        pWaveform->dump();

        QFile f(getTestDir().filePath(kReferenceBuffersPath + reference_title));
        bool pass = true;
        // If the file is not there, we will fail and write out the .actual
        // reference file.
        const QByteArray actual = pWaveform->toByteArray();

        ASSERT_TRUE(f.open(QFile::ReadOnly));
        const QByteArray reference = f.readAll();

        if (actual.size() == reference.size()) {
            for (int i = 0; i < actual.size(); ++i) {
                if (actual.at(i) != reference.at(i)) {
                    qDebug() << "#" << i << QString::number(actual[i], 16)
                             << QString::number(reference[i], 16);
                    pass = false;
                }
            }
        } else {
            qDebug() << "##" << actual.size() << reference.size();
            pass = false;
        }

        // Fail if either we didn't pass, or the comparison file was empty.
        if (!pass) {
            QString fname_actual = reference_title + ".actual";
            qWarning() << "Buffer does not match" << reference_title
                       << ", actual buffer written to "
                       << "reference_buffers/" + fname_actual;
            QFile actualFile(getTestDir().filePath(kReferenceBuffersPath + fname_actual));
            ASSERT_TRUE(actualFile.open(QFile::WriteOnly));
            actualFile.write(actual);
            actualFile.close();
            EXPECT_TRUE(false);
        }
        f.close();
    }

    void TearDown() override {
    }

  protected:
    AnalyzerWaveform m_aw;
    TrackPointer m_pTrack;
    std::vector<CSAMPLE> m_canaryBigBuf;
};

// Basic test to make sure we don't alter the input buffer and don't step out of bounds.
TEST_F(AnalyzerWaveformTest, canary) {
    m_aw.initialize(AnalyzerTrack(m_pTrack),
            m_pTrack->getSampleRate(),
            m_pTrack->getChannels(),
            kBigBufSize / kChannelCount);
    m_aw.processSamples(&m_canaryBigBuf[kCanarySize], kBigBufSize);
    m_aw.storeResults(m_pTrack);
    m_aw.cleanup();
    std::size_t i = 0;
    for (; i < kCanarySize; i++) {
        EXPECT_FLOAT_EQ(m_canaryBigBuf[i], kCanaryFloat);
    }
    for (; i < kCanarySize + kBigBufSize; i++) {
        EXPECT_FLOAT_EQ(m_canaryBigBuf[i], kMagicFloat);
    }
    for (; i < 2 * kCanarySize + kBigBufSize; i++) {
        EXPECT_FLOAT_EQ(m_canaryBigBuf[i], kCanaryFloat);
    }

    // Small reference, compare bitwise
    assertWaveformReference(m_pTrack->getWaveform(), "AnalyzerWaveformsTest");

    // The summary is always big, so we check only the metadata
    ConstWaveformPointer pWaveformSummary = m_pTrack->getWaveformSummary();
    ASSERT_NE(pWaveformSummary, nullptr);
    EXPECT_EQ(pWaveformSummary->getDataSize(), 3842);
    EXPECT_EQ(pWaveformSummary->getCompletion(), 3842);
    EXPECT_DOUBLE_EQ(pWaveformSummary->getAudioVisualRatio(), 1.0);
}

// AI-generated (Claude Code). Start of AI-generated text.
constexpr SINT kOneSecondFrames = 44100;
constexpr int kTrackSampleRate = 44100;
const QString kOldWaveformVersion = QStringLiteral("Waveform-6.1");
const QString kNewWaveformVersion = QStringLiteral("Waveform-7.0-mixtwo");
const QString kSummaryVersion = QStringLiteral("WaveformSummary-5.0");

// The overview size for one second of audio at 44100 Hz, measured before the
// main waveform went to 2205 points per second. It must not change.
constexpr int kOneSecondSummaryDataSize = 3842;

TEST_F(AnalyzerWaveformTest, MainWaveformHas2205PointsPerSecond) {
    ASSERT_TRUE(m_aw.initialize(AnalyzerTrack(m_pTrack),
            m_pTrack->getSampleRate(),
            m_pTrack->getChannels(),
            kOneSecondFrames));

    ConstWaveformPointer pWaveform = m_pTrack->getWaveform();
    ASSERT_NE(pWaveform, nullptr);
    EXPECT_DOUBLE_EQ(pWaveform->getVisualSampleRate(), 2205.0);
    EXPECT_DOUBLE_EQ(pWaveform->getAudioVisualRatio(), 20.0);

    ConstWaveformPointer pWaveformSummary = m_pTrack->getWaveformSummary();
    ASSERT_NE(pWaveformSummary, nullptr);
    EXPECT_EQ(pWaveformSummary->getDataSize(), kOneSecondSummaryDataSize);
    m_aw.cleanup();
}

// An analysis database in memory and a track with an ID in it.
class AnalyzerWaveformDb {
  public:
    explicit AnalyzerWaveformDb(const UserSettingsPointer& pConfig)
            : m_mixxxDb(pConfig, true),
              m_dbConnectionPooler(m_mixxxDb.connectionPool()),
              m_analysisDao(pConfig) {
        m_dbConnection = mixxx::DbConnectionPooled(m_mixxxDb.connectionPool());
        EXPECT_TRUE(MixxxDb::initDatabaseSchema(m_dbConnection));
        m_analysisDao.initialize(m_dbConnection);
    }

    const QSqlDatabase& dbConnection() const {
        return m_dbConnection;
    }

    AnalysisDao& analysisDao() {
        return m_analysisDao;
    }

    int countRows(TrackId trackId, AnalysisDao::AnalysisType type) {
        return static_cast<int>(
                m_analysisDao.getAnalysesForTrackByType(trackId, type).size());
    }

    static TrackPointer newTrack() {
        TrackPointer pTrack = Track::newDummy(
                QDir::temp().filePath(QStringLiteral("analyzerwaveformtest.mp3")),
                TrackId(QVariant(1)));
        pTrack->setAudioProperties(
                mixxx::audio::ChannelCount(kChannelCount),
                mixxx::audio::SampleRate(kTrackSampleRate),
                mixxx::audio::Bitrate(),
                mixxx::Duration::fromMillis(1000));
        return pTrack;
    }

    // Stores a waveform and an overview as an older Mix Two version did.
    void storeOldAnalyses(TrackId trackId) {
        Waveform waveform(kTrackSampleRate, kOneSecondFrames, 441, -1, 0);
        waveform.setCompletion(waveform.getDataSize());
        AnalysisDao::AnalysisInfo analysis;
        analysis.trackId = trackId;
        analysis.type = AnalysisDao::TYPE_WAVEFORM;
        analysis.version = kOldWaveformVersion;
        analysis.description = QStringLiteral("Waveform 6.1");
        analysis.data = waveform.toByteArray();
        ASSERT_TRUE(m_analysisDao.saveAnalysis(&analysis));

        Waveform summary(kTrackSampleRate, kOneSecondFrames, 441, 2 * 1920, 0);
        summary.setCompletion(summary.getDataSize());
        AnalysisDao::AnalysisInfo summaryAnalysis;
        summaryAnalysis.trackId = trackId;
        summaryAnalysis.type = AnalysisDao::TYPE_WAVESUMMARY;
        summaryAnalysis.version = kSummaryVersion;
        summaryAnalysis.description = QStringLiteral("WaveformSummary 5.0");
        summaryAnalysis.data = summary.toByteArray();
        ASSERT_TRUE(m_analysisDao.saveAnalysis(&summaryAnalysis));
    }

  private:
    MixxxDb m_mixxxDb;
    mixxx::DbConnectionPooler m_dbConnectionPooler;
    QSqlDatabase m_dbConnection;
    AnalysisDao m_analysisDao;
};

// Gives one second of a constant signal to an initialized analyzer and stores
// the result.
void analyseOneSecond(AnalyzerWaveform* pAnalyzer, const TrackPointer& pTrack) {
    std::vector<CSAMPLE> samples(kOneSecondFrames * kChannelCount, 0.25f);
    pAnalyzer->processSamples(samples.data(), static_cast<SINT>(samples.size()));
    pAnalyzer->storeResults(pTrack);
    pAnalyzer->cleanup();
}

TEST_F(AnalyzerWaveformTest, OldWaveformStaysUntilNewIsStored) {
    AnalyzerWaveformDb db(config());
    TrackPointer pTrack = AnalyzerWaveformDb::newTrack();
    db.storeOldAnalyses(pTrack->getId());

    AnalyzerWaveform analyzer(config(), db.dbConnection());
    // initialize returns false when shouldAnalyze returns false.
    ASSERT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack),
            pTrack->getSampleRate(),
            pTrack->getChannels(),
            kOneSecondFrames));
    ConstWaveformPointer pShownWaveform = pTrack->getWaveform();
    ASSERT_NE(pShownWaveform, nullptr);
    EXPECT_EQ(pShownWaveform->getVersion(), kOldWaveformVersion);
    // The stored overview stays on the track during the analysis too.
    ConstWaveformPointer pShownSummary = pTrack->getWaveformSummary();
    ASSERT_NE(pShownSummary, nullptr);
    EXPECT_EQ(pShownSummary->getVersion(), kSummaryVersion);

    analyseOneSecond(&analyzer, pTrack);
    pShownWaveform = pTrack->getWaveform();
    ASSERT_NE(pShownWaveform, nullptr);
    EXPECT_EQ(pShownWaveform->getVersion(), kNewWaveformVersion);
    pShownSummary = pTrack->getWaveformSummary();
    ASSERT_NE(pShownSummary, nullptr);
    EXPECT_EQ(pShownSummary->getCompletion(), pShownSummary->getDataSize());
    EXPECT_EQ(db.countRows(pTrack->getId(), AnalysisDao::TYPE_WAVEFORM), 1);
    EXPECT_EQ(db.countRows(pTrack->getId(), AnalysisDao::TYPE_WAVESUMMARY), 1);
}

TEST_F(AnalyzerWaveformTest, NewWaveformIsNotAnalysedAgain) {
    AnalyzerWaveformDb db(config());
    TrackPointer pTrack = AnalyzerWaveformDb::newTrack();
    db.storeOldAnalyses(pTrack->getId());
    {
        AnalyzerWaveform analyzer(config(), db.dbConnection());
        ASSERT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack),
                pTrack->getSampleRate(),
                pTrack->getChannels(),
                kOneSecondFrames));
        analyseOneSecond(&analyzer, pTrack);
    }
    // Count before the next analyzers look at the database.
    EXPECT_EQ(db.countRows(pTrack->getId(), AnalysisDao::TYPE_WAVESUMMARY), 1);

    // The same track, and the same track loaded again without waveforms in
    // memory, need no new analysis.
    AnalyzerWaveform sameTrackAnalyzer(config(), db.dbConnection());
    EXPECT_FALSE(sameTrackAnalyzer.initialize(AnalyzerTrack(pTrack),
            pTrack->getSampleRate(),
            pTrack->getChannels(),
            kOneSecondFrames));
    TrackPointer pReloadedTrack = AnalyzerWaveformDb::newTrack();
    AnalyzerWaveform reloadedTrackAnalyzer(config(), db.dbConnection());
    EXPECT_FALSE(reloadedTrackAnalyzer.initialize(AnalyzerTrack(pReloadedTrack),
            pReloadedTrack->getSampleRate(),
            pReloadedTrack->getChannels(),
            kOneSecondFrames));

    int waveformRows = 0;
    const QList<AnalysisDao::AnalysisInfo> analyses =
            db.analysisDao().getAnalysesForTrack(pTrack->getId());
    for (const auto& analysis : analyses) {
        if (analysis.type == AnalysisDao::TYPE_WAVEFORM) {
            ++waveformRows;
            EXPECT_EQ(analysis.version, kNewWaveformVersion);
        }
    }
    EXPECT_EQ(waveformRows, 1);
    EXPECT_EQ(db.countRows(pTrack->getId(), AnalysisDao::TYPE_WAVESUMMARY), 1);
}

// A deterministic stereo test signal: an exponential sine sweep with a slow
// level change, and short bursts of noise from a fixed linear congruential
// generator. The two channels are different.
std::vector<CSAMPLE> makeSweepAndNoise(int sampleRate, SINT frames) {
    std::vector<CSAMPLE> samples(static_cast<std::size_t>(frames) * kChannelCount);
    const double seconds = static_cast<double>(frames) / sampleRate;
    quint32 noiseState = 12345u;
    double phase = 0.0;
    for (SINT i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        const double frequency = 40.0 * std::pow(300.0, t / seconds);
        phase += 2.0 * M_PI * frequency / sampleRate;
        const double level = 0.5 + 0.4 * std::sin(2.0 * M_PI * 0.3 * t);
        const double sweep = level * std::sin(phase);
        noiseState = noiseState * 1664525u + 1013904223u;
        const double noise = static_cast<double>(noiseState >> 8) / (1 << 23) - 1.0;
        const double burst = std::fmod(t, 0.5) < 0.1 ? noise : 0.0;
        samples[2 * i] = static_cast<CSAMPLE>(0.6 * sweep + 0.4 * burst);
        samples[2 * i + 1] = static_cast<CSAMPLE>(0.4 * sweep - 0.3 * burst);
    }
    return samples;
}

// The hashes of the "all" values of the overviews in the first reference
// files, before the band split points changed.
const QByteArray kSummaryAllHash44100 =
        QByteArrayLiteral("f939d5213589c1e876f41d991d8670ff22eecc32");
const QByteArray kSummaryAllHash48000 =
        QByteArrayLiteral("91a67f8215ca32b9b6e486c487e86a47dcb46b5b");
const QByteArray kSummaryAllHashShort44100 =
        QByteArrayLiteral("fd6080c2acac3fc04a4aefa1b627aa6363240bf1");
const QByteArray kSummaryAllHashShort48000 =
        QByteArrayLiteral("8524bcfbb31592d0d105ebf711f6c23d3c7c61c4");

// The overview of the test signal. The analyser gets the signal in blocks of
// 4096 frames, as the analyser thread does.
ConstWaveformPointer analyseSummary(
        const UserSettingsPointer& pConfig, int sampleRate, int seconds = 30) {
    constexpr SINT kBlockFrames = 4096;
    const SINT frames = static_cast<SINT>(seconds) * sampleRate;
    TrackPointer pTrack = Track::newTemporary();
    pTrack->setAudioProperties(
            mixxx::audio::ChannelCount(kChannelCount),
            mixxx::audio::SampleRate(sampleRate),
            mixxx::audio::Bitrate(),
            mixxx::Duration::fromSeconds(seconds));
    AnalyzerWaveform analyzer(pConfig, QSqlDatabase());
    EXPECT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack),
            pTrack->getSampleRate(),
            pTrack->getChannels(),
            frames));
    const std::vector<CSAMPLE> samples = makeSweepAndNoise(sampleRate, frames);
    for (SINT frame = 0; frame < frames; frame += kBlockFrames) {
        const SINT blockFrames = std::min(kBlockFrames, frames - frame);
        analyzer.processSamples(&samples[static_cast<std::size_t>(frame) * kChannelCount],
                blockFrames * kChannelCount);
    }
    analyzer.storeResults(pTrack);
    analyzer.cleanup();
    return pTrack->getWaveformSummary();
}

// The SHA-1 of the "all" values of every point, as hex.
QByteArray allValuesHash(const ConstWaveformPointer& pWaveform) {
    QByteArray all;
    all.reserve(pWaveform->getDataSize());
    for (int i = 0; i < pWaveform->getDataSize(); ++i) {
        all.append(static_cast<char>(pWaveform->getAll(i)));
    }
    return QCryptographicHash::hash(all, QCryptographicHash::Sha1).toHex();
}

// The reference overviews were made by the code before the main waveform
// went to 2205 points per second, then made again when the band split
// points changed. The "all" values do not use the band filters: their hashes
// are from the overviews before both changes and must not change.
TEST_F(AnalyzerWaveformTest, SummaryIsUnchangedAt44100) {
    ConstWaveformPointer pSummary = analyseSummary(config(), 44100);
    ASSERT_NE(pSummary, nullptr);
    assertWaveformReference(pSummary, "AnalyzerWaveformSummary44100");
    EXPECT_EQ(allValuesHash(pSummary), kSummaryAllHash44100);
}

TEST_F(AnalyzerWaveformTest, SummaryIsUnchangedAt48000) {
    ConstWaveformPointer pSummary = analyseSummary(config(), 48000);
    ASSERT_NE(pSummary, nullptr);
    assertWaveformReference(pSummary, "AnalyzerWaveformSummary48000");
    EXPECT_EQ(allValuesHash(pSummary), kSummaryAllHash48000);
}

// A short track: the overview has more points than legacy strides, so some
// overview points use the running peak of a legacy stride.
TEST_F(AnalyzerWaveformTest, SummaryIsUnchangedForShortTrack) {
    ConstWaveformPointer pSummary44100 = analyseSummary(config(), 44100, 2);
    ASSERT_NE(pSummary44100, nullptr);
    assertWaveformReference(pSummary44100, "AnalyzerWaveformSummaryShort44100");
    EXPECT_EQ(allValuesHash(pSummary44100), kSummaryAllHashShort44100);
    ConstWaveformPointer pSummary48000 = analyseSummary(config(), 48000, 2);
    ASSERT_NE(pSummary48000, nullptr);
    assertWaveformReference(pSummary48000, "AnalyzerWaveformSummaryShort48000");
    EXPECT_EQ(allValuesHash(pSummary48000), kSummaryAllHashShort48000);
}

struct Bands {
    int low = 0;
    int mid = 0;
    int high = 0;
};

// The largest band values of the main waveform for one second of a sine.
// The first quarter second is not used, so that the filters have settled.
Bands sinePeaks(const UserSettingsPointer& pConfig, double frequency) {
    TrackPointer pTrack = Track::newTemporary();
    pTrack->setAudioProperties(
            mixxx::audio::ChannelCount(kChannelCount),
            mixxx::audio::SampleRate(kTrackSampleRate),
            mixxx::audio::Bitrate(),
            mixxx::Duration::fromMillis(1000));
    AnalyzerWaveform analyzer(pConfig, QSqlDatabase());
    EXPECT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack),
            pTrack->getSampleRate(),
            pTrack->getChannels(),
            kOneSecondFrames));
    std::vector<CSAMPLE> samples(static_cast<std::size_t>(kOneSecondFrames) * kChannelCount);
    for (SINT i = 0; i < kOneSecondFrames; ++i) {
        const auto value = static_cast<CSAMPLE>(
                0.5 * std::sin(2.0 * M_PI * frequency * i / kTrackSampleRate));
        samples[2 * i] = value;
        samples[2 * i + 1] = value;
    }
    analyzer.processSamples(samples.data(), static_cast<SINT>(samples.size()));
    analyzer.storeResults(pTrack);
    analyzer.cleanup();

    Bands peaks;
    ConstWaveformPointer pWaveform = pTrack->getWaveform();
    EXPECT_NE(pWaveform, nullptr);
    if (!pWaveform) {
        return peaks;
    }
    for (int i = pWaveform->getDataSize() / 4; i < pWaveform->getDataSize(); ++i) {
        peaks.low = std::max<int>(peaks.low, pWaveform->getLow(i));
        peaks.mid = std::max<int>(peaks.mid, pWaveform->getMid(i));
        peaks.high = std::max<int>(peaks.high, pWaveform->getHigh(i));
    }
    return peaks;
}

// The bands split as rekordbox's 3-band waveform does: low below 150 Hz,
// high above 2000 Hz.
TEST_F(AnalyzerWaveformTest, BandSplit) {
    const Bands bass = sinePeaks(config(), 100.0);
    EXPECT_GT(bass.low, bass.mid);
    EXPECT_GT(bass.low, bass.high);

    const Bands middle = sinePeaks(config(), 1000.0);
    EXPECT_GT(middle.mid, middle.low);
    EXPECT_GT(middle.mid, middle.high);

    const Bands treble = sinePeaks(config(), 8000.0);
    EXPECT_GT(treble.high, treble.low);
    EXPECT_GT(treble.high, treble.mid);

    // Below the earlier split at 600 Hz, but above the split at 150 Hz.
    const Bands lowMid = sinePeaks(config(), 400.0);
    EXPECT_GT(lowMid.mid, lowMid.low);
}
// End of AI-generated text.

} // namespace
