#pragma once

// Mix Two: shared pieces of the Pioneer-style SOUND COLOR FX (DJM-900NXS2
// manual p.15, DDJ-800 manual p.76 as the written spec). Each has one
// control, COLOR: centre-detented, no effect at the centre, one behaviour
// to the left and another to the right.

#include <cmath>

#include "effects/backends/effectmanifest.h"
#include "effects/backends/effectprocessor.h"
#include "util/types.h"

namespace colorfx {

inline void setCommonManifest(const EffectManifestPointer& pManifest,
        const QString& id,
        const QString& name,
        const QString& description,
        const QString& colorDescription) {
    pManifest->setAddDryToWet(false);
    pManifest->setEffectRampsFromDry(true);
    pManifest->setId(id);
    pManifest->setName(name);
    pManifest->setShortName(name);
    pManifest->setAuthor("Mix Two");
    pManifest->setVersion("1.0");
    pManifest->setDescription(description);

    EffectManifestParameterPointer color = pManifest->addParameter();
    color->setId("color");
    color->setName(QObject::tr("Color"));
    color->setShortName(QObject::tr("Color"));
    color->setDescription(colorDescription);
    color->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    color->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    color->setNeutralPointOnScale(0.5);
    color->setDefaultLinkType(EffectManifestParameter::LinkType::Linked);
    color->setRange(0.0, 0.5, 1.0);
}

/// How far the knob is from the centre, 0..1, and which side.
struct Turn {
    double amount; // 0 at the centre, 1 at either end
    bool right;
};

inline Turn turn(double color) {
    if (color >= 0.5) {
        return {std::min(1.0, (color - 0.5) * 2.0), true};
    }
    return {std::min(1.0, (0.5 - color) * 2.0), false};
}

/// Log sweep between two frequencies: t = 0 gives `from`, 1 gives `to`.
inline double sweepHz(double from, double to, double t) {
    return from * std::pow(to / from, t);
}

/// A second-order biquad filter for one stereo channel pair (direct form I).
class Biquad {
  public:
    enum class Type {
        LowPass,
        HighPass,
        BandPass,
    };

    void set(Type type, double hz, double q, double sampleRate) {
        const double w = 2.0 * M_PI * std::min(hz, sampleRate * 0.45) / sampleRate;
        const double c = std::cos(w);
        const double s = std::sin(w);
        const double alpha = s / (2.0 * q);
        double b0, b1, b2;
        switch (type) {
        case Type::LowPass:
            b0 = (1 - c) / 2;
            b1 = 1 - c;
            b2 = (1 - c) / 2;
            break;
        case Type::HighPass:
            b0 = (1 + c) / 2;
            b1 = -(1 + c);
            b2 = (1 + c) / 2;
            break;
        default:
            b0 = alpha;
            b1 = 0;
            b2 = -alpha;
            break;
        }
        const double a0 = 1 + alpha;
        m_b0 = static_cast<CSAMPLE>(b0 / a0);
        m_b1 = static_cast<CSAMPLE>(b1 / a0);
        m_b2 = static_cast<CSAMPLE>(b2 / a0);
        m_a1 = static_cast<CSAMPLE>(-2 * c / a0);
        m_a2 = static_cast<CSAMPLE>((1 - alpha) / a0);
    }

    void clear() {
        for (int ch = 0; ch < 2; ++ch) {
            m_x[ch][0] = m_x[ch][1] = 0;
            m_y[ch][0] = m_y[ch][1] = 0;
        }
    }

    /// Processes one frame in place (left, right).
    void run(CSAMPLE* pLeft, CSAMPLE* pRight) {
        *pLeft = step(0, *pLeft);
        *pRight = step(1, *pRight);
    }

  private:
    CSAMPLE step(int ch, CSAMPLE x) {
        const CSAMPLE y = m_b0 * x + m_b1 * m_x[ch][0] + m_b2 * m_x[ch][1] -
                m_a1 * m_y[ch][0] - m_a2 * m_y[ch][1];
        m_x[ch][1] = m_x[ch][0];
        m_x[ch][0] = x;
        m_y[ch][1] = m_y[ch][0];
        m_y[ch][0] = y;
        return y;
    }

    CSAMPLE m_b0 = 1, m_b1 = 0, m_b2 = 0, m_a1 = 0, m_a2 = 0;
    CSAMPLE m_x[2][2] = {{0, 0}, {0, 0}};
    CSAMPLE m_y[2][2] = {{0, 0}, {0, 0}};
};

inline CSAMPLE clampSample(CSAMPLE value) {
    return value > 1.0f ? 1.0f : (value < -1.0f ? -1.0f : value);
}

} // namespace colorfx
