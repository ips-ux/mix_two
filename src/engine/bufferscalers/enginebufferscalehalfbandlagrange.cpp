#include "engine/bufferscalers/enginebufferscalehalfbandlagrange.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "engine/readaheadmanager.h"
#include "moc_enginebufferscalehalfbandlagrange.cpp"

namespace {

constexpr int kTaps = EngineBufferScaleHalfbandLagrange::kTapsPerSide;
// Frames the filter needs behind and ahead of floor(position).
constexpr SINT kHistoryFrames = kTaps;
constexpr SINT kLookaheadFrames = kTaps + 1;
// Stream frames kept in memory; reads beyond this are done in steps.
constexpr SINT kCapacityFrames = 8192;
// Smallest top-up read, so the ReadAheadManager is not called per frame.
constexpr SINT kMinReadFrames = 64;
// Half-band window: 47 points, so offsets -23 ... 23.
constexpr double kWindowHalfLength = 23.0;
constexpr double kPi = 3.14159265358979323846;

double halfbandTap(int offset) {
    const double x = offset / 2.0;
    const double sinc = std::sin(kPi * x) / (kPi * x);
    const double hann = 0.5 * (1.0 + std::cos(kPi * offset / kWindowHalfLength));
    return 0.5 * sinc * hann;
}

/// 4-point cubic Lagrange through y(-1), y(0), y(1), y(2), evaluated at u in [0, 1).
inline double lagrange4(double u, double ym1, double y0, double y1, double y2) {
    const double um1 = u - 1.0;
    const double um2 = u - 2.0;
    const double up1 = u + 1.0;
    return ym1 * (-u * um1 * um2 / 6.0) +
            y0 * (up1 * um1 * um2 / 2.0) +
            y1 * (-up1 * u * um2 / 2.0) +
            y2 * (up1 * u * um1 / 6.0);
}

} // namespace

// static
const std::array<double, EngineBufferScaleHalfbandLagrange::kTapsPerSide>&
EngineBufferScaleHalfbandLagrange::midpointTaps() {
    static const auto taps = [] {
        std::array<double, kTaps> result{};
        for (int i = 1; i <= kTaps; ++i) {
            // The midpoint sits at the odd offsets of the 2x grid; 2x for the
            // zero-stuffed interpolation gain.
            result[i - 1] = 2.0 * halfbandTap(2 * i - 1);
        }
        return result;
    }();
    return taps;
}

EngineBufferScaleHalfbandLagrange::EngineBufferScaleHalfbandLagrange(
        ReadAheadManager* pReadAheadManager)
        : m_pReadAheadManager(pReadAheadManager),
          m_channelCount(0),
          m_frameCount(0),
          m_position(0.0),
          m_rate(1.0),
          m_oldRate(1.0),
          m_bClear(false) {
    onSignalChanged();
    clear();
}

void EngineBufferScaleHalfbandLagrange::onSignalChanged() {
    // Only grow, as EngineBufferScaleLinear does, to keep allocations out of
    // the real-time thread after the first stem track.
    const int channelCount = getOutputSignal().getChannelCount();
    const auto samples = static_cast<std::size_t>(kCapacityFrames * channelCount);
    if (m_frames.size() < samples) {
        m_frames.resize(samples);
        m_discard.resize(samples);
    }
    if (channelCount != m_channelCount) {
        m_channelCount = channelCount;
        clear();
    }
}

void EngineBufferScaleHalfbandLagrange::setScaleParameters(double base_rate,
        double* pTempoRatio,
        double* pPitchRatio) {
    Q_UNUSED(pPitchRatio);
    m_oldRate = m_rate;
    m_rate = base_rate * *pTempoRatio;
}

void EngineBufferScaleHalfbandLagrange::clear() {
    m_bClear = true;
    // Silence behind the first frame read after a seek.
    std::fill(m_frames.begin(),
            m_frames.begin() + std::min<std::size_t>(m_frames.size(),
                                       static_cast<std::size_t>(
                                               kHistoryFrames * m_channelCount)),
            0.0f);
    m_frameCount = kHistoryFrames;
    m_position = static_cast<double>(kHistoryFrames);
}

void EngineBufferScaleHalfbandLagrange::compact() {
    const SINT keepFrom = static_cast<SINT>(std::floor(m_position)) - kHistoryFrames;
    if (keepFrom <= 0) {
        return;
    }
    const SINT keepFrames = m_frameCount - keepFrom;
    std::memmove(m_frames.data(),
            m_frames.data() + keepFrom * m_channelCount,
            static_cast<std::size_t>(keepFrames * m_channelCount) * sizeof(CSAMPLE));
    m_frameCount = keepFrames;
    m_position -= keepFrom;
}

void EngineBufferScaleHalfbandLagrange::ensureFrames(SINT lastIndex, double rate) {
    int failedReads = 0;
    while (m_frameCount <= lastIndex) {
        if (m_frameCount + kMinReadFrames > kCapacityFrames) {
            compact();
            // Never ask for more than fits behind the kept history.
            lastIndex = std::min(lastIndex, kCapacityFrames - 1);
            if (m_frameCount > lastIndex) {
                break;
            }
        }
        const SINT wanted = std::min(kCapacityFrames - m_frameCount,
                std::max(lastIndex + 1 - m_frameCount, kMinReadFrames));
        const SINT samplesRead = m_pReadAheadManager->getNextSamples(rate,
                m_frames.data() + m_frameCount * m_channelCount,
                wanted * m_channelCount,
                mixxx::audio::ChannelCount(m_channelCount));
        const SINT framesRead = samplesRead / m_channelCount;
        if (framesRead <= 0) {
            // As EngineBufferScaleLinear: one empty read can be a loop jump;
            // a second one means there is nothing (e.g. a broken file), so
            // continue with silence instead of spinning.
            if (++failedReads > 1) {
                const SINT missing = lastIndex + 1 - m_frameCount;
                std::fill_n(m_frames.data() + m_frameCount * m_channelCount,
                        missing * m_channelCount,
                        0.0f);
                m_frameCount += missing;
            }
            continue;
        }
        m_frameCount += framesRead;
    }
}

CSAMPLE EngineBufferScaleHalfbandLagrange::midpointSample(SINT frame, int channel) const {
    const auto& taps = midpointTaps();
    double sum = 0.0;
    for (int i = 1; i <= kTaps; ++i) {
        sum += taps[i - 1] *
                (frameSample(frame + i, channel) + frameSample(frame + 1 - i, channel));
    }
    return static_cast<CSAMPLE>(sum);
}

double EngineBufferScaleHalfbandLagrange::rewindReadAhead(double newRate) {
    // What was read but not yet played; the engine still has it in its read
    // log (it turns the frames we report into a position by walking that log
    // in order).
    const double unconsumedBefore = m_frameCount - m_position;
    const SINT playedUpTo = static_cast<SINT>(std::floor(m_position));
    const SINT unplayed = m_frameCount - (playedUpTo + 1);
    SINT framesRead = 0;
    int failedReads = 0;
    while (framesRead < unplayed && failedReads <= 1) {
        const SINT wanted = std::min(unplayed - framesRead, kCapacityFrames);
        const SINT samples = m_pReadAheadManager->getNextSamples(newRate,
                m_discard.data(),
                wanted * m_channelCount,
                mixxx::audio::ChannelCount(m_channelCount));
        if (samples <= 0) {
            ++failedReads;
            continue;
        }
        framesRead += samples / m_channelCount;
    }
    // The stream now continues backwards from the read position, mirrored
    // around it: keep what was played, and read on from its mirror image.
    if (unplayed > 0) {
        m_frameCount = playedUpTo + 1;
    }
    m_position = 2.0 * playedUpTo + 1.0 - m_position;
    // Report the backward read-ahead and its forward re-read as consumed, so
    // the log keeps exactly what the buffer holds unplayed. Reporting only
    // the re-read left the backward read-ahead in the log on every direction
    // change; after a scratch the engine's position lagged the audio and the
    // hand-back played the leftovers out as the track going backwards.
    return unconsumedBefore + framesRead - (m_frameCount - m_position);
}

double EngineBufferScaleHalfbandLagrange::scale(
        CSAMPLE* pOutput, SINT frames, double rateFrom, double rateTo) {
    if (frames <= 0) {
        return 0.0;
    }
    // The ReadAheadManager takes the direction from the sign of the rate.
    const double readRate = rateTo != 0.0 ? rateTo : rateFrom;
    double speed = std::fabs(rateFrom);
    const double speedStep = (std::fabs(rateTo) - speed) / frames;

    // Read the whole buffer's worth up front (as far as it fits), then top up.
    const double estimate = speed * frames + speedStep * frames * (frames - 1) / 2.0;
    compact();
    ensureFrames(std::min(static_cast<SINT>(std::floor(m_position + estimate)) +
                         kLookaheadFrames,
                         kCapacityFrames - 1),
            readRate);

    double advanced = 0.0;
    for (SINT outFrame = 0; outFrame < frames; ++outFrame) {
        SINT n = static_cast<SINT>(std::floor(m_position));
        if (n + kLookaheadFrames >= m_frameCount) {
            compact();
            n = static_cast<SINT>(std::floor(m_position));
            ensureFrames(n + kLookaheadFrames, readRate);
        }
        // Position on the 2x grid: even points are frames, odd ones midpoints.
        const double q = 2.0 * (m_position - n);
        CSAMPLE* pOut = pOutput + outFrame * m_channelCount;
        if (q < 1.0) {
            for (int ch = 0; ch < m_channelCount; ++ch) {
                pOut[ch] = static_cast<CSAMPLE>(lagrange4(q,
                        midpointSample(n - 1, ch),
                        frameSample(n, ch),
                        midpointSample(n, ch),
                        frameSample(n + 1, ch)));
            }
        } else {
            for (int ch = 0; ch < m_channelCount; ++ch) {
                pOut[ch] = static_cast<CSAMPLE>(lagrange4(q - 1.0,
                        frameSample(n, ch),
                        midpointSample(n, ch),
                        frameSample(n + 1, ch),
                        midpointSample(n + 1, ch)));
            }
        }
        m_position += speed;
        advanced += speed;
        speed += speedStep;
    }
    return advanced;
}

double EngineBufferScaleHalfbandLagrange::scaleBuffer(
        CSAMPLE* pOutputBuffer,
        SINT iOutputBufferSize) {
    if (iOutputBufferSize == 0 || m_channelCount == 0) {
        return 0.0;
    }
    if (m_bClear) {
        m_oldRate = m_rate; // If cleared, don't ramp the rate.
        m_bClear = false;
    }
    const double rateFrom = m_oldRate;
    const double rateTo = m_rate;
    m_oldRate = m_rate;
    const SINT frames = iOutputBufferSize / m_channelCount;

    if (rateFrom * rateTo < 0) {
        // Direction change: slow down to zero over the first half, rewind the
        // unplayed read-ahead, then speed up the other way.
        const SINT firstHalf = frames / 2;
        double framesRead = scale(pOutputBuffer, firstHalf, rateFrom, 0.0);
        framesRead += rewindReadAhead(rateTo);
        framesRead += scale(pOutputBuffer + firstHalf * m_channelCount,
                frames - firstHalf,
                0.0,
                rateTo);
        return framesRead;
    }
    return scale(pOutputBuffer, frames, rateFrom, rateTo);
}
