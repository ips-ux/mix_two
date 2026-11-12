#pragma once

#include <cmath>
#include <limits>

#include "analyzer/analyzer.h"
#include "library/dao/analysisdao.h"
#include "util/performancetimer.h"
#include "util/sample.h"
#include "waveform/waveform.h"

//NOTS vrince some test to segment sound, to apply color in the waveform
//#define TEST_HEAT_MAP
#ifdef TEST_HEAT_MAP
class QImage;
#endif

class EngineFilterIIRBase;
class QSqlDatabase;

// AI-generated (Claude Code). Start of AI-generated text.
// The main waveform keeps the peak of each main stride (m_length frames).
// The overview (summary) is the mean of the peaks of the legacy strides
// (legacySamples frames, the main stride at kLegacyMainWaveformSampleRate),
// so that the overview does not change when the main point rate changes.
// End of AI-generated text.
struct WaveformStride {
    WaveformStride(double samples, double legacySamples, double averageSamples, int stemCount)
            : m_position(0),
              m_stemCount(stemCount),
              m_length(samples),
              m_legacyLength(legacySamples),
              m_averageLength(averageSamples),
              m_averagePosition(0),
              m_averageDivisor(0),
              m_postScaleConversion(static_cast<float>(
                      std::numeric_limits<unsigned char>::max())) {
        reset();
    }

    inline void reset() {
        m_position = 0;
        m_averageDivisor = 0;
        for (int i = 0; i < ChannelCount; ++i) {
            m_overallData[i] = 0.0f;
            m_legacyOverallData[i] = 0.0f;
            m_averageOverallData[i] = 0.0f;
            SampleUtil::clear(m_filteredData[i], BandCount);
            SampleUtil::clear(m_legacyFilteredData[i], BandCount);
            SampleUtil::clear(m_averageFilteredData[i], BandCount);
            if (m_stemCount > 0) {
                SampleUtil::clear(m_stemData[i], m_stemCount);
            } else {
                DEBUG_ASSERT(m_stemCount == 0);
            }
        }
    }

    inline void store(WaveformData* data) {
        for (int i = 0; i < ChannelCount; ++i) {
            WaveformData& datum = *(data + i);
            datum.filtered.all = static_cast<unsigned char>(std::min(255.0,
                    m_postScaleConversion * m_overallData[i] + 0.5));
            datum.filtered.low = static_cast<unsigned char>(std::min(255.0,
                    m_postScaleConversion * m_filteredData[i][Low] + 0.5));
            datum.filtered.mid = static_cast<unsigned char>(std::min(255.0,
                    m_postScaleConversion * m_filteredData[i][Mid] + 0.5));
            datum.filtered.high = static_cast<unsigned char>(std::min(255.0,
                    m_postScaleConversion * m_filteredData[i][High] + 0.5));
            for (int stemIdx = 0; stemIdx < m_stemCount; stemIdx++) {
                datum.stems[stemIdx] = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_stemData[i][stemIdx] + 0.5));
            }
        }
        // Reset the stride counters
        for (int i = 0; i < ChannelCount; ++i) {
            m_overallData[i] = 0.0f;
            for (int f = 0; f < BandCount; ++f) {
                m_filteredData[i][f] = 0.0f;
            }
            for (int stemIdx = 0; stemIdx < m_stemCount; ++stemIdx) {
                m_stemData[i][stemIdx] = 0.0f;
            }
        }
    }

    // AI-generated (Claude Code). Start of AI-generated text.
    // Adds the peaks of the current main stride to the legacy stride peaks.
    // It is used when each legacy stride is a whole number of main strides.
    // Calling it two times for the same main stride gives the same result.
    inline void foldMainIntoLegacy() {
        for (int i = 0; i < ChannelCount; ++i) {
            m_legacyOverallData[i] = std::max(m_legacyOverallData[i], m_overallData[i]);
            for (int f = 0; f < BandCount; ++f) {
                m_legacyFilteredData[i][f] =
                        std::max(m_legacyFilteredData[i][f], m_filteredData[i][f]);
            }
        }
    }

    // At the end of a legacy stride: add its peaks to the overview mean.
    inline void legacyStore() {
        m_averageDivisor++;
        for (int i = 0; i < ChannelCount; ++i) {
            m_averageOverallData[i] += m_legacyOverallData[i];
            m_legacyOverallData[i] = 0.0f;
            for (int f = 0; f < BandCount; ++f) {
                m_averageFilteredData[i][f] += m_legacyFilteredData[i][f];
                m_legacyFilteredData[i][f] = 0.0f;
            }
        }
    }
    // End of AI-generated text.

    inline void averageStore(WaveformData* data) {
        if (m_averageDivisor) {
            for (int i = 0; i < ChannelCount; ++i) {
                WaveformData& datum = *(data + i);
                datum.filtered.all = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_averageOverallData[i] / m_averageDivisor + 0.5));
                datum.filtered.low = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_averageFilteredData[i][Low] /
                                        m_averageDivisor +
                                0.5));
                datum.filtered.mid = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_averageFilteredData[i][Mid] /
                                        m_averageDivisor +
                                0.5));
                datum.filtered.high = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_averageFilteredData[i][High] /
                                        m_averageDivisor +
                                0.5));
            }
        } else {
            // This is the case if The Overview Waveform has more samples than the detailed waveform
            // AI-generated (Claude Code). Start of AI-generated text.
            // Use the running peak of the legacy stride.
            // End of AI-generated text.
            for (int i = 0; i < ChannelCount; ++i) {
                WaveformData& datum = *(data + i);
                datum.filtered.all = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_legacyOverallData[i] + 0.5));
                datum.filtered.low = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_legacyFilteredData[i][Low] + 0.5));
                datum.filtered.mid = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_legacyFilteredData[i][Mid] + 0.5));
                datum.filtered.high = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_legacyFilteredData[i][High] + 0.5));
            }
        }

        m_averageDivisor = 0;
        for (int i = 0; i < ChannelCount; ++i) {
            m_averageOverallData[i] = 0.0f;
            for (int f = 0; f < BandCount; ++f) {
                m_averageFilteredData[i][f] = 0.0f;
            }
        }
    }

    int m_position;
    int m_stemCount;
    double m_length;
    double m_legacyLength;
    double m_averageLength;
    int m_averagePosition;
    int m_averageDivisor;

    float m_overallData[ChannelCount];
    float m_filteredData[ChannelCount][BandCount];
    float m_stemData[ChannelCount][mixxx::kMaxSupportedStems];

    // AI-generated (Claude Code). Start of AI-generated text.
    // Peaks of the current legacy stride, for the overview only.
    // End of AI-generated text.
    float m_legacyOverallData[ChannelCount];
    float m_legacyFilteredData[ChannelCount][BandCount];

    float m_averageOverallData[ChannelCount];
    float m_averageFilteredData[ChannelCount][BandCount];

    float m_postScaleConversion;
};

class AnalyzerWaveform : public Analyzer {
  public:
    AnalyzerWaveform(
            UserSettingsPointer pConfig,
            const QSqlDatabase& dbConnection);
    ~AnalyzerWaveform() override;

    bool initialize(const AnalyzerTrack& track,
            mixxx::audio::SampleRate sampleRate,
            mixxx::audio::ChannelCount channelCount,
            SINT frameLength) override;
    bool processSamples(const CSAMPLE* buffer, SINT count) override;
    void storeResults(TrackPointer tio) override;
    void cleanup() override;

  private:
    bool shouldAnalyze(TrackPointer tio) const;

    void storeCurrentStridePower();
    void resetCurrentStride();

    void createFilters(mixxx::audio::SampleRate sampleRate);
    void destroyFilters();
    void storeIfGreater(float* pDest, float source);

    mutable AnalysisDao m_analysisDao;
    // AI-generated (Claude Code). Start of AI-generated text.
    // The stored analysis of an older waveform version that the new analysis
    // replaces. storeResults removes it after the new waveform is saved.
    // -1 if there is none.
    mutable int m_replacedWaveformId;
    // The same for the stored overview (summary) that the new one replaces.
    mutable int m_replacedSummaryId;
    // The legacy stride length in frames if it is a whole multiple of a
    // whole-number main stride, else 0. See processSamples.
    int m_legacyStrideFrames;
    // End of AI-generated text.

    WaveformPointer m_waveform;
    WaveformPointer m_waveformSummary;
    WaveformData* m_waveformData;
    WaveformData* m_waveformSummaryData;

    WaveformStride m_stride;

    int m_currentStride;
    int m_currentSummaryStride;
    mixxx::audio::ChannelCount m_channelCount;

    struct Filters {
        std::unique_ptr<EngineFilterIIRBase> low;
        std::unique_ptr<EngineFilterIIRBase> mid;
        std::unique_ptr<EngineFilterIIRBase> high;
    };

    Filters m_filters;

    struct Buffers {
        std::vector<float> low;
        std::vector<float> mid;
        std::vector<float> high;

        SINT size;

        Buffers()
                : low(),
                  mid(),
                  high(),
                  size(0) {
        }
    };

    Buffers m_buffers;

    PerformanceTimer m_timer;

#ifdef TEST_HEAT_MAP
    QImage* test_heatMap;
#endif
};
