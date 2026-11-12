#include "analyzer/analyzerwaveform.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "analyzer/analyzertrack.h"
#include "analyzer/constants.h"
#include "engine/filters/enginefilterbessel4.h"
#include "track/track.h"
#include "util/logger.h"
#include "waveform/waveform.h"
#include "waveform/waveformfactory.h"

namespace {

mixxx::Logger kLogger("AnalyzerWaveform");

// AI-generated (Claude Code). Start of AI-generated text.
// The band split points. With these, the three bands follow rekordbox's
// 3-band waveform most closely (measured on real tracks; the earlier split
// was 600 Hz and 4000 Hz).
// End of AI-generated text.
constexpr double kLowMidFreqHz = 150.0;

constexpr double kMidHighFreqHz = 2000.0;

// AI-generated (Claude Code). Start of AI-generated text.
// A local copy of WaveformStride::m_legacyOverallData and
// m_legacyFilteredData for the loop in processSamples.
struct LegacyPeaks {
    float overall[ChannelCount];
    float low[ChannelCount];
    float mid[ChannelCount];
    float high[ChannelCount];

    void loadFrom(const WaveformStride& stride) {
        for (int c = 0; c < ChannelCount; ++c) {
            overall[c] = stride.m_legacyOverallData[c];
            low[c] = stride.m_legacyFilteredData[c][Low];
            mid[c] = stride.m_legacyFilteredData[c][Mid];
            high[c] = stride.m_legacyFilteredData[c][High];
        }
    }

    void storeTo(WaveformStride* pStride) const {
        for (int c = 0; c < ChannelCount; ++c) {
            pStride->m_legacyOverallData[c] = overall[c];
            pStride->m_legacyFilteredData[c][Low] = low[c];
            pStride->m_legacyFilteredData[c][Mid] = mid[c];
            pStride->m_legacyFilteredData[c][High] = high[c];
        }
    }

    // The same rule as AnalyzerWaveform::storeIfGreater.
    void update(const CSAMPLE* pOverall,
            const CSAMPLE* pLow,
            const CSAMPLE* pMid,
            const CSAMPLE* pHigh) {
        for (int c = 0; c < ChannelCount; ++c) {
            overall[c] = std::max(overall[c], pOverall[c]);
            low[c] = std::max(low[c], pLow[c]);
            mid[c] = std::max(mid[c], pMid[c]);
            high[c] = std::max(high[c], pHigh[c]);
        }
    }
};
// End of AI-generated text.

} // namespace

AnalyzerWaveform::AnalyzerWaveform(
        UserSettingsPointer pConfig,
        const QSqlDatabase& dbConnection)
        : m_analysisDao(pConfig),
          m_replacedWaveformId(-1),
          m_replacedSummaryId(-1),
          m_legacyStrideFrames(0),
          m_waveformData(nullptr),
          m_waveformSummaryData(nullptr),
          m_stride(0, 0, 0, 0),
          m_currentStride(0),
          m_currentSummaryStride(0) {
    m_analysisDao.initialize(dbConnection);
}

AnalyzerWaveform::~AnalyzerWaveform() {
    kLogger.debug() << "~AnalyzerWaveform():";
    destroyFilters();
}

bool AnalyzerWaveform::initialize(const AnalyzerTrack& track,
        mixxx::audio::SampleRate sampleRate,
        mixxx::audio::ChannelCount channelCount,
        SINT frameLength) {
    if (frameLength <= 0) {
        qWarning() << "AnalyzerWaveform::initialize - no waveform/waveform summary";
        return false;
    }

    // If we don't need to calculate the waveform/wavesummary, skip.
    if (!shouldAnalyze(track.getTrack())) {
        return false;
    }

    m_timer.start();

    // Now actually initialize the AnalyzerWaveform:
    destroyFilters();
    createFilters(sampleRate);

    // two visual sample per pixel in full width overview in full hd
    constexpr int summaryWaveformSamples = 2 * 1920;

    int stemCount = channelCount == mixxx::kAnalysisChannels
            ? 0
            : channelCount / mixxx::kAnalysisChannels;
    m_waveform = WaveformPointer(new Waveform(
            sampleRate, frameLength, kMainWaveformSampleRate, -1, stemCount));
    m_waveformSummary = WaveformPointer(new Waveform(sampleRate,
            frameLength,
            kLegacyMainWaveformSampleRate,
            summaryWaveformSamples,
            stemCount));

    // Now, that the Waveform memory is initialized, we can set set them to
    // the track. Be aware that other threads of Mixxx can touch them from
    // now.
    // AI-generated (Claude Code). Start of AI-generated text.
    // A waveform or overview that the track already shows stays until
    // storeResults sets the new one. A track without one shows the new one
    // while it fills in.
    // End of AI-generated text.
    if (!track.getTrack()->getWaveform()) {
        track.getTrack()->setWaveform(m_waveform);
    }
    if (!track.getTrack()->getWaveformSummary()) {
        track.getTrack()->setWaveformSummary(m_waveformSummary);
    }

    m_waveformData = m_waveform->data();
    m_waveformSummaryData = m_waveformSummary->data();

    // AI-generated (Claude Code). Start of AI-generated text.
    // The main stride length at kLegacyMainWaveformSampleRate, for the
    // overview. It comes from the Waveform constructor (with no frames, so
    // nearly no memory), so that it is bit for bit the value that the
    // overview used before; with fast floating point math a division here can
    // give a different last bit and move the stride boundaries.
    const double legacyAudioVisualRatio =
            Waveform(sampleRate, 0, kLegacyMainWaveformSampleRate, -1, 0)
                    .getAudioVisualRatio();
    // End of AI-generated text.
    m_stride = WaveformStride(m_waveform->getAudioVisualRatio(),
            legacyAudioVisualRatio,
            m_waveformSummary->getAudioVisualRatio(),
            stemCount);
    // AI-generated (Claude Code). Start of AI-generated text.
    // Whole-number stride lengths where the legacy stride is a multiple of
    // the main stride: the legacy peaks come from the main stride peaks.
    m_legacyStrideFrames = 0;
    const double mainAudioVisualRatio = m_waveform->getAudioVisualRatio();
    if (mainAudioVisualRatio >= 1 &&
            mainAudioVisualRatio == std::floor(mainAudioVisualRatio) &&
            legacyAudioVisualRatio == std::floor(legacyAudioVisualRatio)) {
        const int mainStrideFrames = static_cast<int>(mainAudioVisualRatio);
        const int legacyStrideFrames = static_cast<int>(legacyAudioVisualRatio);
        if (legacyStrideFrames % mainStrideFrames == 0) {
            m_legacyStrideFrames = legacyStrideFrames;
        }
    }
    // End of AI-generated text.

    m_currentStride = 0;
    m_currentSummaryStride = 0;
    m_channelCount = channelCount;

    //debug
    //m_waveform->dump();
    //m_waveformSummary->dump();

#ifdef TEST_HEAT_MAP
    test_heatMap = new QImage(256, 256, QImage::Format_RGB32);
    test_heatMap->fill(0xFFFFFFFF);
#endif
    return true;
}

bool AnalyzerWaveform::shouldAnalyze(TrackPointer pTrack) const {
    ConstWaveformPointer pTrackWaveform = pTrack->getWaveform();
    ConstWaveformPointer pTrackWaveformSummary = pTrack->getWaveformSummary();
    ConstWaveformPointer pLoadedTrackWaveform;
    ConstWaveformPointer pLoadedTrackWaveformSummary;
#ifdef __STEM__
    bool isStemTrack = !pTrack->getStemInfo().isEmpty();
#endif

    TrackId trackId = pTrack->getId();
    // AI-generated (Claude Code). Start of AI-generated text.
    // A waveform of an older version on the track stays on screen, but it is
    // analysed again.
    m_replacedWaveformId = -1;
    m_replacedSummaryId = -1;
    ConstWaveformPointer pReplacedWaveform;
    bool missingWaveform = pTrackWaveform.isNull() ||
            WaveformFactory::waveformVersionToVersionClass(
                    pTrackWaveform->getVersion()) ==
                    WaveformFactory::VC_SHOW_UNTIL_REPLACED;
    // End of AI-generated text.
    bool missingWavesummary = pTrackWaveformSummary.isNull();

    if (trackId.isValid() && (missingWaveform || missingWavesummary)) {
        QList<AnalysisDao::AnalysisInfo> analyses =
                m_analysisDao.getAnalysesForTrack(trackId);

        QListIterator<AnalysisDao::AnalysisInfo> it(analyses);
        while (it.hasNext()) {
            const AnalysisDao::AnalysisInfo& analysis = it.next();
            WaveformFactory::VersionClass vc;

            if (analysis.type == AnalysisDao::TYPE_WAVEFORM) {
                vc = WaveformFactory::waveformVersionToVersionClass(analysis.version);
                if (missingWaveform && vc == WaveformFactory::VC_USE) {
                    pLoadedTrackWaveform = ConstWaveformPointer(
                            WaveformFactory::loadWaveformFromAnalysis(analysis));
                    missingWaveform = false;
                } else if (vc == WaveformFactory::VC_SHOW_UNTIL_REPLACED &&
                        m_replacedWaveformId == -1) {
                    // AI-generated (Claude Code). Start of AI-generated text.
                    // Keep the first stored analysis of an older version
                    // until the new one is stored. Show it if the track has
                    // no waveform.
                    m_replacedWaveformId = analysis.analysisId;
                    if (pTrackWaveform.isNull()) {
                        pReplacedWaveform = ConstWaveformPointer(
                                WaveformFactory::loadWaveformFromAnalysis(analysis));
                    }
                    // End of AI-generated text.
                } else if (vc != WaveformFactory::VC_KEEP) {
                    // remove all other Analysis except that one we should keep
                    m_analysisDao.deleteAnalysis(analysis.analysisId);
                }
            }
            if (analysis.type == AnalysisDao::TYPE_WAVESUMMARY) {
                vc = WaveformFactory::waveformSummaryVersionToVersionClass(analysis.version);
                if (vc == WaveformFactory::VC_USE && m_replacedSummaryId == -1) {
                    // AI-generated (Claude Code). Start of AI-generated text.
                    // Keep the first usable stored overview. If the track is
                    // analysed, storeResults removes it after the new one is
                    // saved.
                    m_replacedSummaryId = analysis.analysisId;
                    if (missingWavesummary) {
                        pLoadedTrackWaveformSummary = ConstWaveformPointer(
                                WaveformFactory::loadWaveformFromAnalysis(analysis));
                        missingWavesummary = false;
                    }
                    // End of AI-generated text.
                } else if (vc != WaveformFactory::VC_KEEP) {
                    // remove all other Analysis except that one we should keep
                    m_analysisDao.deleteAnalysis(analysis.analysisId);
                }
            }
        }
    }

#ifdef __STEM__
    // If the waveform was generated without stem information but the track has
    // some, we need to regenerate the waveform.
    const bool waveformHasStemData = (!pTrackWaveform.isNull() &&
                                             pTrackWaveform->hasStem()) ||
            (!pLoadedTrackWaveform.isNull() &&
                    pLoadedTrackWaveform->hasStem());
    if (!missingWaveform && !waveformHasStemData && isStemTrack) {
        missingWaveform = true;
    }
#endif

    // AI-generated (Claude Code). Start of AI-generated text.
    if (m_replacedWaveformId != -1 && pLoadedTrackWaveform) {
        // A waveform of the current version is stored too. The older one is
        // not necessary.
        m_analysisDao.deleteAnalysis(m_replacedWaveformId);
        m_replacedWaveformId = -1;
        pReplacedWaveform.clear();
    }
    // End of AI-generated text.

    // If we don't need to calculate the waveform/wavesummary, skip.
    if (!missingWaveform && !missingWavesummary) {
        kLogger.debug() << "loadStored - Stored waveform loaded";
        if (pLoadedTrackWaveform) {
            pTrack->setWaveform(pLoadedTrackWaveform);
        }
        if (pLoadedTrackWaveformSummary) {
            pTrack->setWaveformSummary(pLoadedTrackWaveformSummary);
        }
        return false;
    }
    // AI-generated (Claude Code). Start of AI-generated text.
    // Show the waveform of the older version and the stored overview during
    // the analysis.
    if (pReplacedWaveform) {
        pTrack->setWaveform(pReplacedWaveform);
    }
    if (pLoadedTrackWaveformSummary) {
        pTrack->setWaveformSummary(pLoadedTrackWaveformSummary);
    }
    // End of AI-generated text.
    return true;
}

void AnalyzerWaveform::createFilters(mixxx::audio::SampleRate sampleRate) {
    // m_filter[Low] = new EngineFilterButterworth8Low(sampleRate, kLowMidFreqHz);
    // m_filter[Mid] = new EngineFilterButterworth8Band(sampleRate, kLowMidFreqHz, kMidHighFreqHz);
    // m_filter[High] = new EngineFilterButterworth8High(sampleRate, kMidHighFreqHz);
    m_filters = {
            std::make_unique<EngineFilterBessel4Low>(sampleRate, kLowMidFreqHz),
            std::make_unique<EngineFilterBessel4Band>(sampleRate, kLowMidFreqHz, kMidHighFreqHz),
            std::make_unique<EngineFilterBessel4High>(sampleRate, kMidHighFreqHz)};

    // settle filters for silence in preroll to avoids ramping (Issue #7776)
    m_filters.low->assumeSettled();
    m_filters.mid->assumeSettled();
    m_filters.high->assumeSettled();
}

void AnalyzerWaveform::destroyFilters() {
    m_filters = {};
}

bool AnalyzerWaveform::processSamples(const CSAMPLE* pIn, SINT count) {
    VERIFY_OR_DEBUG_ASSERT(m_waveform) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(m_waveformSummary) {
        return false;
    }

    SINT numFrames = count / m_channelCount;
    count = numFrames * mixxx::audio::ChannelCount::stereo();
    int stemCount = 0;

    const CSAMPLE* pWaveformInput = pIn;
    CSAMPLE* pMixedChannel = nullptr;

    if (m_channelCount > mixxx::audio::ChannelCount::stereo()) {
        DEBUG_ASSERT(0 == m_channelCount % mixxx::audio::ChannelCount::stereo());

        pMixedChannel = SampleUtil::alloc(count);
        VERIFY_OR_DEBUG_ASSERT(pMixedChannel) {
            return false;
        }
        SampleUtil::mixMultichannelToStereo(pMixedChannel, pIn, numFrames, m_channelCount);
        stemCount = m_channelCount / mixxx::audio::ChannelCount::stereo();
        pWaveformInput = pMixedChannel;
    }

    // This should only append once if count is constant
    if (count > m_buffers.size) {
        m_buffers.low.resize(count);
        m_buffers.mid.resize(count);
        m_buffers.high.resize(count);
        m_buffers.size = count;
    }

    m_filters.low->process(pWaveformInput, &m_buffers.low[0], count);
    m_filters.mid->process(pWaveformInput, &m_buffers.mid[0], count);
    m_filters.high->process(pWaveformInput, &m_buffers.high[0], count);

    m_waveform->setSaveState(Waveform::SaveState::NotSaved);
    m_waveformSummary->setSaveState(Waveform::SaveState::NotSaved);

    // AI-generated (Claude Code). Start of AI-generated text.
    // If each legacy stride is a whole number of main strides (for example
    // 100 = 5 x 20 frames at 44100 Hz), the legacy peaks are the maximum of
    // the main stride peaks: fold them in at each main stride end.
    // Otherwise update the legacy peaks for each frame. They stay in local
    // variables in the loop, because through the m_stride members the
    // compiler must read them from memory again after each write of
    // waveform data.
    const bool legacyFromMainStrides = m_legacyStrideFrames > 0;
    LegacyPeaks legacyPeaks;
    if (!legacyFromMainStrides) {
        legacyPeaks.loadFrom(m_stride);
    }
    // End of AI-generated text.
    for (SINT i = 0; i < count; i += 2) {
        // Take max value, not average of data
        CSAMPLE cover[2] = {fabs(pWaveformInput[i]), fabs(pWaveformInput[i + 1])};
        CSAMPLE clow[2] = {fabs(m_buffers.low[i]), fabs(m_buffers.low[i + 1])};
        CSAMPLE cmid[2] = {fabs(m_buffers.mid[i]), fabs(m_buffers.mid[i + 1])};
        CSAMPLE chigh[2] = {fabs(m_buffers.high[i]), fabs(m_buffers.high[i + 1])};

        // This is for if you want to experiment with averaging instead of
        // maxing.
        // m_stride.m_overallData[Right] += buffer[i]*buffer[i];
        // m_stride.m_overallData[Left] += buffer[i + 1]*buffer[i + 1];
        // m_stride.m_filteredData[Right][Low] += m_buffers.low[i]*m_buffers.low[i];
        // m_stride.m_filteredData[Left][Low] += m_buffers.low[i + 1]*m_buffers.low[i + 1];
        // m_stride.m_filteredData[Right][Mid] += m_buffers.mid[i]*m_buffers.mid[i];
        // m_stride.m_filteredData[Left][Mid] += m_buffers.mid[i + 1]*m_buffers.mid[i + 1];
        // m_stride.m_filteredData[Right][High] += m_buffers.high[i]*m_buffers.high[i];
        // m_stride.m_filteredData[Left][High] += m_buffers.high[i + 1]*m_buffers.high[i + 1];

        // Record the max across this stride.
        storeIfGreater(&m_stride.m_overallData[Left], cover[Left]);
        storeIfGreater(&m_stride.m_overallData[Right], cover[Right]);
        storeIfGreater(&m_stride.m_filteredData[Left][Low], clow[Left]);
        storeIfGreater(&m_stride.m_filteredData[Right][Low], clow[Right]);
        storeIfGreater(&m_stride.m_filteredData[Left][Mid], cmid[Left]);
        storeIfGreater(&m_stride.m_filteredData[Right][Mid], cmid[Right]);
        storeIfGreater(&m_stride.m_filteredData[Left][High], chigh[Left]);
        storeIfGreater(&m_stride.m_filteredData[Right][High], chigh[Right]);
        if (!legacyFromMainStrides) {
            legacyPeaks.update(cover, clow, cmid, chigh);
        }

        for (int s = 0; s < stemCount; s++) {
            CSAMPLE cstem[2] = {
                    fabs(pIn[i * stemCount + s * mixxx::kAnalysisChannels]),
                    fabs(pIn[i * stemCount + s * mixxx::kAnalysisChannels +
                            1])};
            storeIfGreater(&m_stride.m_stemData[Left][s], cstem[Left]);
            storeIfGreater(&m_stride.m_stemData[Right][s], cstem[Right]);
        }

        m_stride.m_position++;

        if (fmod(m_stride.m_position, m_stride.m_length) < 1) {
            VERIFY_OR_DEBUG_ASSERT(m_currentStride + ChannelCount <= m_waveform->getDataSize()) {
                qWarning() << "AnalyzerWaveform::process - currentStride > waveform size";
                return false;
            }
            if (legacyFromMainStrides) {
                m_stride.foldMainIntoLegacy();
            }
            m_stride.store(m_waveformData + m_currentStride);
            m_currentStride += ChannelCount;
            m_waveform->setCompletion(m_currentStride);
            // With whole-number stride lengths, fmod(position, length) < 1
            // is the same as position % length == 0.
            if (legacyFromMainStrides && m_stride.m_position % m_legacyStrideFrames == 0) {
                m_stride.legacyStore();
            }
        }

        if (!legacyFromMainStrides &&
                fmod(m_stride.m_position, m_stride.m_legacyLength) < 1) {
            legacyPeaks.storeTo(&m_stride);
            m_stride.legacyStore();
            legacyPeaks.loadFrom(m_stride);
        }

        if (fmod(m_stride.m_position, m_stride.m_averageLength) < 1) {
            VERIFY_OR_DEBUG_ASSERT(m_currentSummaryStride + ChannelCount <= m_waveformSummary->getDataSize()) {
                qWarning() << "AnalyzerWaveform::process - current summary stride > waveform summary size";
                return false;
            }
            // The running legacy peak (for an overview with more points than
            // legacy strides) includes the current part of a stride.
            if (legacyFromMainStrides) {
                m_stride.foldMainIntoLegacy();
            } else {
                legacyPeaks.storeTo(&m_stride);
            }
            m_stride.averageStore(m_waveformSummaryData + m_currentSummaryStride);
            m_currentSummaryStride += ChannelCount;
            m_waveformSummary->setCompletion(m_currentSummaryStride);

#ifdef TEST_HEAT_MAP
            QPointF point(m_stride.m_filteredData[Right][High],
                    m_stride.m_filteredData[Right][Mid]);

            float norm = sqrt(point.x() * point.x() + point.y() * point.y());
            point /= norm;

            point *= m_stride.m_filteredData[Right][Low];
            test_heatMap->setPixel(point.toPoint(), 0xFF0000FF);
#endif
        }
    }

    if (!legacyFromMainStrides) {
        legacyPeaks.storeTo(&m_stride);
    }

    //kLogger.debug() << "process - m_waveform->getCompletion()" << m_waveform->getCompletion() << "off" << m_waveform->getDataSize();
    //kLogger.debug() << "process - m_waveformSummary->getCompletion()" << m_waveformSummary->getCompletion() << "off" << m_waveformSummary->getDataSize();
    if (pMixedChannel) {
        SampleUtil::free(pMixedChannel);
    }
    return true;
}

void AnalyzerWaveform::cleanup() {
    m_waveform.clear();
    m_waveformData = nullptr;
    m_waveformSummary.clear();
    m_waveformSummaryData = nullptr;
}

void AnalyzerWaveform::storeResults(TrackPointer pTrack) {
    // Force completion to waveform size
    if (m_waveform) {
        m_waveform->setSaveState(Waveform::SaveState::SavePending);
        m_waveform->setCompletion(m_waveform->getDataSize());
        m_waveform->setVersion(WaveformFactory::currentWaveformVersion());
        m_waveform->setDescription(WaveformFactory::currentWaveformDescription());
    }

    // Force completion to waveform size
    if (m_waveformSummary) {
        m_waveformSummary->setSaveState(Waveform::SaveState::SavePending);
        m_waveformSummary->setCompletion(m_waveformSummary->getDataSize());
        m_waveformSummary->setVersion(WaveformFactory::currentWaveformSummaryVersion());
        m_waveformSummary->setDescription(WaveformFactory::currentWaveformSummaryDescription());
    }

#ifdef TEST_HEAT_MAP
    test_heatMap->save("heatMap.png");
#endif
    // Ensure that the analyses get saved. This is also called from
    // TrackDAO.updateTrack(), but it can happen that we analyze only the
    // waveforms (i.e. if the config setting was disabled in a previous scan)
    // and then it is not called. The other analyzers have signals which control
    // the update of their data.
    m_analysisDao.saveTrackAnalyses(
            pTrack->getId(),
            m_waveform,
            m_waveformSummary);
    // AI-generated (Claude Code). Start of AI-generated text.
    // Remove the stored waveform of the older version only when the new one
    // is saved. If waveform caching is off, nothing is saved or removed.
    if (m_replacedWaveformId != -1 && m_waveform &&
            m_waveform->saveState() == Waveform::SaveState::Saved) {
        m_analysisDao.deleteAnalysis(m_replacedWaveformId);
    }
    if (m_replacedSummaryId != -1 && m_waveformSummary &&
            m_waveformSummary->saveState() == Waveform::SaveState::Saved) {
        m_analysisDao.deleteAnalysis(m_replacedSummaryId);
    }
    m_replacedWaveformId = -1;
    m_replacedSummaryId = -1;
    // End of AI-generated text.

    // Set waveforms on track AFTER they'been written to disk in order to have
    // a consistency when OverviewCache asks AnalysisDAO for a waveform summary.
    pTrack->setWaveform(m_waveform);
    pTrack->setWaveformSummary(m_waveformSummary);

    kLogger.debug() << "Waveform generation for track" << pTrack->getId() << "done"
                    << m_timer.elapsed().debugSecondsWithUnit();
}

void AnalyzerWaveform::storeIfGreater(float* pDest, float source) {
    if (*pDest < source) {
        *pDest = source;
    }
}
