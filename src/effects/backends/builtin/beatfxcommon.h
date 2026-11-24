#pragma once

// Mix Two: shared pieces of the Pioneer-style BEAT FX (DDJ-800 manual
// p.66-73, DJM-900NXS2 manual p.15-16 as the written spec). Every BEAT FX
// has the same two user controls: the BEAT fraction (the deck's BEAT < >
// buttons, 1/16 to 16 beats; FLANGER and PHASER go up to 64 beats, ENIGMA JET
// and MOBIUS go from -64 to 64 beats) and LEVEL/DEPTH (the meta knob).

#include <QString>
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

// AI-generated (Claude Code). Start of AI-generated text.
// The wide range of FLANGER and PHASER: 1/16 to 64 beats (DDJ-800 manual
// p.71).
constexpr std::array<double, 11> kWideBeatFractions = {
        1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 1, 2, 4, 8, 16, 32, 64};
constexpr int kDefaultWideBeatIndex = 4; // 1 beat
// The signed range of ENIGMA JET and MOBIUS: -64 to -1/16 beats (the pitch
// falls) and 1/16 to 64 beats (the pitch rises), in rising order (DDJ-800
// manual p.70, 73, 74).
constexpr std::array<double, 22> kSignedBeats = {-64,
        -32,
        -16,
        -8,
        -4,
        -2,
        -1,
        -1.0 / 2,
        -1.0 / 4,
        -1.0 / 8,
        -1.0 / 16,
        1.0 / 16,
        1.0 / 8,
        1.0 / 4,
        1.0 / 2,
        1,
        2,
        4,
        8,
        16,
        32,
        64};
constexpr int kDefaultSignedBeatIndex = 15; // +1 beat
// End of AI-generated text.
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

// AI-generated (Claude Code). Start of AI-generated text.
// The two helpers below turn a parameter value (an index, clamped at both
// ends) into beats.
inline double wideBeatFraction(double parameterValue) {
    int index = static_cast<int>(std::lround(parameterValue));
    if (index < 0) {
        index = 0;
    } else if (index >= static_cast<int>(kWideBeatFractions.size())) {
        index = static_cast<int>(kWideBeatFractions.size()) - 1;
    }
    return kWideBeatFractions[index];
}

inline double signedBeats(double parameterValue) {
    int index = static_cast<int>(std::lround(parameterValue));
    if (index < 0) {
        index = 0;
    } else if (index >= static_cast<int>(kSignedBeats.size())) {
        index = static_cast<int>(kSignedBeats.size()) - 1;
    }
    return kSignedBeats[index];
}

/// The beats at `index` of a beat parameter with `steps` steps (9, 11 or 22).
/// Returns false for any other step count or an index out of range.
inline bool beatStepValue(int steps, int index, double* pBeats) {
    if (index < 0 || index >= steps) {
        return false;
    }
    if (steps == static_cast<int>(kBeatFractions.size())) {
        *pBeats = kBeatFractions[index];
    } else if (steps == static_cast<int>(kWideBeatFractions.size())) {
        *pBeats = kWideBeatFractions[index];
    } else if (steps == static_cast<int>(kSignedBeats.size())) {
        *pBeats = kSignedBeats[index];
    } else {
        return false;
    }
    return true;
}

/// The label of a beat value as the DDJ-800 shows it: "1/16", "1/2", "1",
/// "64", with a leading minus for a negative value.
inline QString beatLabel(double beats) {
    const double magnitude = std::abs(beats);
    const QString sign = beats < 0 ? QStringLiteral("-") : QString();
    if (magnitude < 1) {
        return sign + QStringLiteral("1/") + QString::number(std::lround(1.0 / magnitude));
    }
    return sign + QString::number(std::lround(magnitude));
}
// End of AI-generated text.

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

// AI-generated (Claude Code). Start of AI-generated text.
// The first parameter of FLANGER and PHASER: 11 steps, 1/16 to 64 beats,
// default 1 beat.
inline void addWideBeatParameter(const EffectManifestPointer& pManifest) {
    EffectManifestParameterPointer beat = pManifest->addParameter();
    beat->setId("beat");
    beat->setName(QObject::tr("Beat"));
    beat->setShortName(QObject::tr("Beat"));
    beat->setDescription(QObject::tr(
            "Beat fraction: 1/16, 1/8, 1/4, 1/2, 1, 2, 4, 8, 16, 32 or 64 beats"));
    beat->setValueScaler(EffectManifestParameter::ValueScaler::Integral);
    beat->setUnitsHint(EffectManifestParameter::UnitsHint::Beats);
    beat->setRange(0,
            kDefaultWideBeatIndex,
            static_cast<double>(kWideBeatFractions.size() - 1));
}

// The first parameter of ENIGMA JET and MOBIUS: 22 steps, -64 to -1/16 and
// 1/16 to 64 beats, default +1 beat. A negative value makes the pitch fall.
inline void addSignedBeatParameter(const EffectManifestPointer& pManifest) {
    EffectManifestParameterPointer beat = pManifest->addParameter();
    beat->setId("beat");
    beat->setName(QObject::tr("Beat"));
    beat->setShortName(QObject::tr("Beat"));
    beat->setDescription(QObject::tr(
            "Beat fraction: 1/16 to 64 beats raises the pitch. "
            "-1/16 to -64 beats lowers the pitch."));
    beat->setValueScaler(EffectManifestParameter::ValueScaler::Integral);
    beat->setUnitsHint(EffectManifestParameter::UnitsHint::Beats);
    beat->setRange(0, kDefaultSignedBeatIndex, static_cast<double>(kSignedBeats.size() - 1));
}
// End of AI-generated text.

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
