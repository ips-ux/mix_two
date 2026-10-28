#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <vector>

#include "engine/bufferscalers/enginebufferscalehalfbandlagrange.h"
#include "engine/bufferscalers/enginebufferscalelinear.h"
#include "engine/readaheadmanager.h"
#include "test/mixxxtest.h"

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kSampleRate = 44100;
constexpr int kChannels = 2;
constexpr SINT kBufferFrames = 512;

/// A track in memory, read in playback direction like the real
/// ReadAheadManager: forward for a positive rate, backward for a negative one.
class FakeReadAheadManager : public ReadAheadManager {
  public:
    explicit FakeReadAheadManager(std::vector<CSAMPLE> track, SINT startFrame = 0)
            : m_track(std::move(track)),
              m_frame(startFrame) {
    }

    SINT getNextSamples(double rate,
            CSAMPLE* pBuffer,
            SINT requestedSamples,
            mixxx::audio::ChannelCount channelCount) override {
        const SINT frames = requestedSamples / channelCount;
        const SINT trackFrames = static_cast<SINT>(m_track.size()) / channelCount;
        for (SINT i = 0; i < frames; ++i) {
            const SINT frame = rate >= 0 ? m_frame + i : m_frame - 1 - i;
            for (int ch = 0; ch < channelCount; ++ch) {
                pBuffer[i * channelCount + ch] = (frame >= 0 && frame < trackFrames)
                        ? m_track[frame * channelCount + ch]
                        : 0.0f;
            }
        }
        m_frame += rate >= 0 ? frames : -frames;
        return frames * channelCount;
    }

  private:
    std::vector<CSAMPLE> m_track;
    SINT m_frame;
};

std::vector<CSAMPLE> sineTrack(double frequency, SINT frames) {
    std::vector<CSAMPLE> track(frames * kChannels);
    for (SINT i = 0; i < frames; ++i) {
        const auto v = static_cast<CSAMPLE>(0.5 * std::sin(2 * kPi * frequency * i / kSampleRate));
        track[i * kChannels] = v;
        track[i * kChannels + 1] = -v;
    }
    return track;
}

void setRate(EngineBufferScale* pScaler, double rate) {
    double tempo = rate;
    double pitch = rate;
    pScaler->setScaleParameters(1.0, &tempo, &pitch);
}

/// Plays `buffers` buffers at a constant rate and returns the left channel.
std::vector<CSAMPLE> play(EngineBufferScale* pScaler, double rate, int buffers) {
    pScaler->setSignal(mixxx::audio::SampleRate(kSampleRate), mixxx::audio::ChannelCount(kChannels));
    pScaler->clear();
    setRate(pScaler, rate);
    setRate(pScaler, rate); // twice: no ramp from the previous rate
    std::vector<CSAMPLE> out(kBufferFrames * kChannels);
    std::vector<CSAMPLE> left;
    for (int b = 0; b < buffers; ++b) {
        pScaler->scaleBuffer(out.data(), static_cast<SINT>(out.size()));
        for (SINT i = 0; i < kBufferFrames; ++i) {
            left.push_back(out[i * kChannels]);
        }
    }
    return left;
}

/// RMS difference from the ideal resampled sine, skipping the start-up.
double sineError(const std::vector<CSAMPLE>& left, double frequency, double rate) {
    double sum = 0;
    int n = 0;
    for (std::size_t i = 64; i < left.size(); ++i) {
        const double ideal = 0.5 * std::sin(2 * kPi * frequency * (i * rate) / kSampleRate);
        sum += (left[i] - ideal) * (left[i] - ideal);
        ++n;
    }
    return std::sqrt(sum / n);
}

TEST(EngineBufferScaleHalfbandLagrangeTest, FilterMeetsTheSpec) {
    // The full half-band filter on the 2x grid: 0.5 in the centre, taps / 2
    // at the odd offsets.
    const auto& taps = EngineBufferScaleHalfbandLagrange::midpointTaps();
    double sum = 0;
    for (double tap : taps) {
        sum += tap;
    }
    EXPECT_NEAR(0.5, sum, 1e-4); // a constant passes unchanged

    auto responseDb = [&](double f) { // f: 0..1 of the 2x grid's Nyquist
        std::complex<double> h = 0.5;
        for (int i = 1; i <= EngineBufferScaleHalfbandLagrange::kTapsPerSide; ++i) {
            h += taps[i - 1] * std::cos(kPi * f * (2 * i - 1)); // both sides, halved
        }
        return 20 * std::log10(std::abs(h));
    };
    for (double f = 0; f <= 0.40; f += 0.005) {
        EXPECT_NEAR(0.0, responseDb(f), 0.04) << "passband at " << f;
    }
    EXPECT_NEAR(-6.02, responseDb(0.5), 0.1);
    for (double f = 0.60; f <= 1.0; f += 0.005) {
        EXPECT_LT(responseDb(f), -47.0) << "stopband at " << f;
    }
}

TEST(EngineBufferScaleHalfbandLagrangeTest, NormalSpeedIsTransparent) {
    const auto track = sineTrack(997, 8192);
    FakeReadAheadManager reader(track);
    EngineBufferScaleHalfbandLagrange scaler(&reader);
    const auto left = play(&scaler, 1.0, 8);
    for (SINT i = 0; i < static_cast<SINT>(left.size()); ++i) {
        ASSERT_FLOAT_EQ(track[i * kChannels], left[i]) << "frame " << i;
    }
}

TEST(EngineBufferScaleHalfbandLagrangeTest, HalfSpeedHalvesThePitch) {
    FakeReadAheadManager reader(sineTrack(1000, 8192));
    EngineBufferScaleHalfbandLagrange scaler(&reader);
    EXPECT_LT(sineError(play(&scaler, 0.5, 8), 1000, 0.5), 1e-4);
}

TEST(EngineBufferScaleHalfbandLagrangeTest, MuchCleanerThanLinearAtHighFrequencies) {
    // A 10 kHz tone played 1.37x faster: linear interpolation leaves clearly
    // audible error, the band-limited scaler far less.
    constexpr double kFrequency = 10000;
    constexpr double kRate = 1.37;
    FakeReadAheadManager readerHq(sineTrack(kFrequency, 16384));
    EngineBufferScaleHalfbandLagrange halfband(&readerHq);
    FakeReadAheadManager readerLinear(sineTrack(kFrequency, 16384));
    EngineBufferScaleLinear linear(&readerLinear);

    const double errorHalfband = sineError(play(&halfband, kRate, 16), kFrequency, kRate);
    const double errorLinear = sineError(play(&linear, kRate, 16), kFrequency, kRate);
    EXPECT_LT(errorHalfband * 10, errorLinear)
            << "half-band " << errorHalfband << ", linear " << errorLinear;
}

TEST(EngineBufferScaleHalfbandLagrangeTest, ReversingRetracesTheTrack) {
    // A ramp makes the track position audible: forward, then backward.
    std::vector<CSAMPLE> ramp(8192 * kChannels);
    for (SINT i = 0; i < 8192; ++i) {
        ramp[i * kChannels] = ramp[i * kChannels + 1] = static_cast<CSAMPLE>(i) / 8192;
    }
    FakeReadAheadManager reader(ramp, 4096);
    EngineBufferScaleHalfbandLagrange scaler(&reader);
    scaler.setSignal(mixxx::audio::SampleRate(kSampleRate), mixxx::audio::ChannelCount(kChannels));
    scaler.clear();
    std::vector<CSAMPLE> out(kBufferFrames * kChannels);

    setRate(&scaler, 1.0);
    setRate(&scaler, 1.0);
    double advanced = scaler.scaleBuffer(out.data(), static_cast<SINT>(out.size()));
    advanced += scaler.scaleBuffer(out.data(), static_cast<SINT>(out.size()));
    const CSAMPLE forwardEnd = out[(kBufferFrames - 1) * kChannels];
    EXPECT_NEAR(2.0 * kBufferFrames, advanced, 1e-9);

    setRate(&scaler, -1.0); // direction change inside this buffer
    scaler.scaleBuffer(out.data(), static_cast<SINT>(out.size()));
    setRate(&scaler, -1.0);
    scaler.scaleBuffer(out.data(), static_cast<SINT>(out.size()));
    // The turn decelerates over half a buffer and accelerates back over the
    // other half, so the next buffer starts where the forward play ended.
    EXPECT_NEAR(forwardEnd, out[0], 4.0 / 8192);
    // Going back: the ramp falls, one frame per output frame.
    for (SINT i = 1; i < kBufferFrames; ++i) {
        EXPECT_LT(out[i * kChannels], out[(i - 1) * kChannels]) << "frame " << i;
        EXPECT_NEAR(1.0 / 8192,
                out[(i - 1) * kChannels] - out[i * kChannels],
                1e-5);
    }
}

TEST(EngineBufferScaleHalfbandLagrangeTest, StereoChannelsStaySeparate) {
    FakeReadAheadManager reader(sineTrack(440, 8192));
    EngineBufferScaleHalfbandLagrange scaler(&reader);
    scaler.setSignal(mixxx::audio::SampleRate(kSampleRate), mixxx::audio::ChannelCount(kChannels));
    scaler.clear();
    setRate(&scaler, 0.73);
    setRate(&scaler, 0.73);
    std::vector<CSAMPLE> out(kBufferFrames * kChannels);
    for (int b = 0; b < 4; ++b) {
        scaler.scaleBuffer(out.data(), static_cast<SINT>(out.size()));
    }
    for (SINT i = 0; i < kBufferFrames; ++i) {
        EXPECT_FLOAT_EQ(-out[i * kChannels], out[i * kChannels + 1]);
    }
}

} // namespace
