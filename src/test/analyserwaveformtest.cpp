#include <gtest/gtest.h>

#include <QDir>
#include <QtDebug>
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

    analyseOneSecond(&analyzer, pTrack);
    pShownWaveform = pTrack->getWaveform();
    ASSERT_NE(pShownWaveform, nullptr);
    EXPECT_EQ(pShownWaveform->getVersion(), kNewWaveformVersion);
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
}
// End of AI-generated text.

} // namespace
