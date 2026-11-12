// AI-generated (Claude Code). Start of AI-generated text.
// Disabled benchmark for the cost of the waveform analysis. It decodes every
// audio file in the folder named by MIXTWO_COST_TRACKS into memory, then times
// only AnalyzerWaveform (initialize, processSamples and building the byte
// array). It prints one COST line per file and a COST TOTAL line.
// Run it with --gtest_also_run_disabled_tests.
// End of AI-generated text.

#include <gtest/gtest.h>

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QStringList>
#include <QtDebug>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>

#include "analyzer/analyzertrack.h"
#include "analyzer/analyzerwaveform.h"
#include "analyzer/constants.h"
#include "sources/audiosourcestereoproxy.h"
#include "sources/soundsourceproxy.h"
#include "test/mixxxtest.h"
#include "test/soundsourceproviderregistration.h"
#include "track/track.h"
#include "waveform/waveform.h"

namespace {

// Same level as AnalysisDao::saveAnalysis uses (qCompress default level).
constexpr int kStoreCompressionLevel = -1;

struct CostResult {
    double audioSeconds = 0.0;
    double analysisMs = 0.0;
    qsizetype byteArraySize = 0;
    qsizetype storedSize = 0;
};

} // namespace

class AnalyzerWaveformCostTest : public MixxxTest, SoundSourceProviderRegistration {
  protected:
    // Returns false if the file cannot be decoded.
    bool measureFile(const QString& filePath, CostResult* pResult) {
        TrackPointer pTrack = Track::newTemporary(filePath);

        mixxx::AudioSource::OpenParams openParams;
        openParams.setChannelCount(mixxx::kAnalysisMaxChannels);
        mixxx::AudioSourcePointer pAudioSource =
                SoundSourceProxy(pTrack).openAudioSource(openParams);
        if (!pAudioSource) {
            return false;
        }
        if (pAudioSource->getSignalInfo().getChannelCount() % mixxx::kAnalysisChannels) {
            pAudioSource = std::make_shared<mixxx::AudioSourceStereoProxy>(
                    pAudioSource,
                    mixxx::kAnalysisFramesPerChunk);
        }

        const auto sampleRate = pAudioSource->getSignalInfo().getSampleRate();
        const auto channelCount = pAudioSource->getSignalInfo().getChannelCount();
        const SINT frameLength = pAudioSource->frameLength();

        // Decode the whole track first: decode time is not part of the cost.
        std::vector<CSAMPLE> samples(static_cast<std::size_t>(frameLength) * channelCount);
        SINT framesDecoded = 0;
        while (framesDecoded < frameLength) {
            const SINT chunkFrames =
                    std::min(mixxx::kAnalysisFramesPerChunk, frameLength - framesDecoded);
            const auto range = mixxx::IndexRange::forward(
                    pAudioSource->frameIndexRange().start() + framesDecoded,
                    chunkFrames);
            const auto readable = pAudioSource->readSampleFrames(
                    mixxx::WritableSampleFrames(
                            range,
                            mixxx::SampleBuffer::WritableSlice(
                                    samples.data() + framesDecoded * channelCount,
                                    chunkFrames * channelCount)));
            const SINT readFrames = readable.frameLength();
            if (readFrames <= 0) {
                break;
            }
            framesDecoded += readFrames;
        }
        if (framesDecoded <= 0) {
            return false;
        }

        AnalyzerWaveform analyzer(config(), QSqlDatabase());
        const auto start = std::chrono::steady_clock::now();
        if (!analyzer.initialize(AnalyzerTrack(pTrack), sampleRate, channelCount, framesDecoded)) {
            return false;
        }
        SINT processedFrames = 0;
        while (processedFrames < framesDecoded) {
            const SINT chunkFrames =
                    std::min(mixxx::kAnalysisFramesPerChunk, framesDecoded - processedFrames);
            analyzer.processSamples(
                    samples.data() + processedFrames * channelCount,
                    chunkFrames * channelCount);
            processedFrames += chunkFrames;
        }
        ConstWaveformPointer pWaveform = pTrack->getWaveform();
        if (!pWaveform) {
            return false;
        }
        const QByteArray byteArray = pWaveform->toByteArray();
        const auto end = std::chrono::steady_clock::now();

        pResult->audioSeconds = static_cast<double>(framesDecoded) / sampleRate;
        pResult->analysisMs =
                std::chrono::duration<double, std::milli>(end - start).count();
        pResult->byteArraySize = byteArray.size();
        pResult->storedSize = qCompress(byteArray, kStoreCompressionLevel).size();
        analyzer.cleanup();
        return true;
    }
};

TEST_F(AnalyzerWaveformCostTest, DISABLED_Measure) {
    const QString folder = qEnvironmentVariable("MIXTWO_COST_TRACKS");
    if (folder.isEmpty()) {
        GTEST_SKIP() << "MIXTWO_COST_TRACKS is not set";
    }
    const QDir dir(folder);
    ASSERT_TRUE(dir.exists()) << "Folder does not exist: " << folder.toStdString();

    const QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
    double totalSeconds = 0.0;
    double totalMs = 0.0;
    qint64 totalByteArray = 0;
    qint64 totalStored = 0;
    int fileCount = 0;
    for (const QFileInfo& fileInfo : files) {
        if (!SoundSourceProxy::isFileSuffixSupported(fileInfo.suffix())) {
            continue;
        }
        CostResult result;
        if (!measureFile(fileInfo.absoluteFilePath(), &result)) {
            ADD_FAILURE() << "Cannot analyse " << fileInfo.fileName().toStdString();
            continue;
        }
        std::printf("COST %.3f %.3f %lld %lld\n",
                result.audioSeconds,
                result.analysisMs,
                static_cast<long long>(result.byteArraySize),
                static_cast<long long>(result.storedSize));
        totalSeconds += result.audioSeconds;
        totalMs += result.analysisMs;
        totalByteArray += result.byteArraySize;
        totalStored += result.storedSize;
        ++fileCount;
    }
    std::printf("COST TOTAL %d %.3f %.3f %lld %lld\n",
            fileCount,
            totalSeconds,
            totalMs,
            static_cast<long long>(totalByteArray),
            static_cast<long long>(totalStored));
    std::fflush(stdout);
    EXPECT_GT(fileCount, 0);
}
