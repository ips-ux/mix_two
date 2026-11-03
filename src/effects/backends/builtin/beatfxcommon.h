#pragma once

// Mix Two: shared pieces of the Pioneer-style BEAT FX (DDJ-800 manual
// p.66-73, DJM-900NXS2 manual p.15-16 as the written spec). Every BEAT FX
// has the same two user controls: the BEAT fraction (the deck's BEAT < >
// buttons, 1/16 to 16 beats) and LEVEL/DEPTH (the meta knob).

#include <array>
#include <cmath>

#include "effects/backends/effectmanifest.h"
#include "effects/backends/effectprocessor.h"
#include "engine/effects/groupfeaturestate.h"
#include "util/samplebuffer.h"

namespace beatfx {

// The DDJ-800's BEAT fractions, in button order.
constexpr std::array<double, 9> kBeatFractions = {
        1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 1, 2, 4, 8, 16};
constexpr int kDefaultBeatIndex = 4; // 1 beat
// Without a tempo the effects assume 120 BPM, as rekordbox's BPM display
// starts there.
constexpr double kFallbackBeatSeconds = 0.5;

inline double beatFraction(double parameterValue) {
    int index = static_cast<int>(std::lround(parameterValue));
    if (index < 0) {
        index = 0;
    } else if (index >= static_cast<int>(kBeatFractions.size())) {
        index = static_cast<int>(kBeatFractions.size()) - 1;
    }
    return kBeatFractions[index];
}

inline double beatSeconds(const GroupFeatureState& groupFeatures) {
    if (groupFeatures.beat_length.has_value() && groupFeatures.beat_length->seconds > 0) {
        return groupFeatures.beat_length->seconds;
    }
    return kFallbackBeatSeconds;
}

inline void addBeatParameter(const EffectManifestPointer& pManifest) {
    EffectManifestParameterPointer beat = pManifest->addParameter();
    beat->setId("beat");
    beat->setName(QObject::tr("Beat"));
    beat->setShortName(QObject::tr("Beat"));
    beat->setDescription(QObject::tr(
            "Beat fraction: 1/16, 1/8, 1/4, 1/2, 1, 2, 4, 8 or 16 beats"));
    beat->setValueScaler(EffectManifestParameter::ValueScaler::Integral);
    beat->setUnitsHint(EffectManifestParameter::UnitsHint::Beats);
    beat->setRange(0, kDefaultBeatIndex, static_cast<double>(kBeatFractions.size() - 1));
}

inline void addDepthParameter(const EffectManifestPointer& pManifest, const QString& description) {
    EffectManifestParameterPointer depth = pManifest->addParameter();
    depth->setId("depth");
    depth->setName(QObject::tr("Level/Depth"));
    depth->setShortName(QObject::tr("Depth"));
    depth->setDescription(description);
    depth->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    depth->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    depth->setDefaultLinkType(EffectManifestParameter::LinkType::Linked);
    depth->setRange(0.0, 0.5, 1.0);
}

inline void setCommonManifest(const EffectManifestPointer& pManifest,
        const QString& id,
        const QString& name,
        const QString& description) {
    pManifest->setAddDryToWet(true);
    pManifest->setEffectRampsFromDry(true);
    pManifest->setId(id);
    pManifest->setName(name);
    pManifest->setShortName(name);
    pManifest->setAuthor("Mix Two");
    pManifest->setVersion("1.0");
    pManifest->setDescription(description);
}

/// A stereo (interleaved) circular delay line with linear interpolation,
/// sized in seconds at audioParametersChanged().
class DelayLine {
  public:
    void resize(const mixxx::EngineParameters& engineParameters, double seconds) {
        m_frames = static_cast<int>(seconds * engineParameters.sampleRate());
        m_buffer = mixxx::SampleBuffer(m_frames * 2);
        clear();
    }
    void clear() {
        m_buffer.clear();
        m_write = 0;
    }
    int frames() const {
        return m_frames;
    }
    void write(CSAMPLE left, CSAMPLE right) {
        m_buffer[m_write * 2] = left;
        m_buffer[m_write * 2 + 1] = right;
        m_write = (m_write + 1) % m_frames;
    }
    /// Reads `delayFrames` (fractional, >= 1) frames back from the frame
    /// written last.
    void read(double delayFrames, CSAMPLE* pLeft, CSAMPLE* pRight) const {
        if (delayFrames < 1) {
            delayFrames = 1;
        } else if (delayFrames > m_frames - 2) {
            delayFrames = m_frames - 2;
        }
        const int whole = static_cast<int>(delayFrames);
        const CSAMPLE frac = static_cast<CSAMPLE>(delayFrames - whole);
        const int a = (m_write - whole + m_frames * 2) % m_frames;
        const int b = (a - 1 + m_frames) % m_frames;
        *pLeft = m_buffer[a * 2] * (1 - frac) + m_buffer[b * 2] * frac;
        *pRight = m_buffer[a * 2 + 1] * (1 - frac) + m_buffer[b * 2 + 1] * frac;
    }

  private:
    mixxx::SampleBuffer m_buffer;
    int m_frames = 0;
    int m_write = 0;
};

inline CSAMPLE clampSample(CSAMPLE value) {
    return value > 1.0f ? 1.0f : (value < -1.0f ? -1.0f : value);
}

} // namespace beatfx
