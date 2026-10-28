#pragma once

#include <array>
#include <vector>

#include "engine/bufferscalers/enginebufferscale.h"

class ReadAheadManager;

/// Mix Two: the "Rekordbox" scratch algorithm (Preferences > Decks > Platter
/// options).
///
/// Like EngineBufferScaleLinear this is varispeed (pitch follows speed, as on
/// a turntable), but band-limited: the track is oversampled 2x with a
/// half-band filter, and the read position is interpolated with 4-point
/// cubic Lagrange on the oversampled signal. That is the design rekordbox 6
/// uses (docs/ddj800-program/SCRATCH-ENGINE.md); the filter is generated here
/// from its formula, nothing is taken from rekordbox.
///
/// It reads the same playback-ordered stream from the ReadAheadManager as the
/// linear scaler, keeps 11 frames behind and 12 ahead of the read position for
/// the filter, and handles a change of direction the same way: the buffer is
/// split at zero speed and the unplayed read-ahead is rewound.
class EngineBufferScaleHalfbandLagrange : public EngineBufferScale {
    Q_OBJECT
  public:
    /// Non-zero half-band taps per side (odd offsets 1, 3, ..., 21).
    static constexpr int kTapsPerSide = 11;

    explicit EngineBufferScaleHalfbandLagrange(ReadAheadManager* pReadAheadManager);
    ~EngineBufferScaleHalfbandLagrange() override = default;

    double scaleBuffer(
            CSAMPLE* pOutputBuffer,
            SINT iOutputBufferSize) override;
    void clear() override;

    void setScaleParameters(double base_rate,
            double* pTempoRatio,
            double* pPitchRatio) override;

    /// Weights of the point halfway between frames n and n + 1:
    ///   mid = sum over i = 1..kTapsPerSide of taps[i - 1] * (x[n + i] + x[n + 1 - i]).
    /// A half-band sinc with a 47-point Hann window, scaled for 2x
    /// interpolation (the weights sum to 1/2, so a constant passes unchanged).
    static const std::array<double, kTapsPerSide>& midpointTaps();

  private:
    void onSignalChanged() override;

    /// Renders `frames` output frames while the speed moves linearly from
    /// |rateFrom| to |rateTo|. Returns the frames advanced in the stream.
    double scale(CSAMPLE* pOutput, SINT frames, double rateFrom, double rateTo);
    /// Reads from the ReadAheadManager until frame `lastIndex` is available.
    void ensureFrames(SINT lastIndex, double rate);
    /// Drops consumed frames that the filter no longer needs.
    void compact();
    /// Rewinds the ReadAheadManager over the read-ahead that was not played,
    /// for a change of direction. Returns the frames it read doing so.
    SINT rewindReadAhead(double newRate);

    CSAMPLE frameSample(SINT frame, int channel) const {
        return m_frames[static_cast<std::size_t>(frame * m_channelCount + channel)];
    }
    CSAMPLE midpointSample(SINT frame, int channel) const;

    ReadAheadManager* m_pReadAheadManager;
    /// The stream in playback order, interleaved.
    std::vector<CSAMPLE> m_frames;
    std::vector<CSAMPLE> m_discard;
    int m_channelCount;
    SINT m_frameCount;
    /// Read position in frames of m_frames.
    double m_position;
    double m_rate;
    double m_oldRate;
    bool m_bClear;
};
