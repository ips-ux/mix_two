#include "effects/backends/builtin/beatfxenigmajeteffect.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"
#include "util/sample.h"

namespace {
// A rising pitch is a shortening delay, so the sweep runs from the long
// end to the short end on a log scale.
constexpr double kMinMs = 0.3;
constexpr double kMaxMs = 12.0;
constexpr CSAMPLE_GAIN kFeedback = 0.45f;
} // namespace

// static
QString BeatFxEnigmaJetEffect::getId() {
    return QStringLiteral("org.mixtwo.beatfx.enigmajet");
}

// static
EffectManifestPointer BeatFxEnigmaJetEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    beatfx::setCommonManifest(pManifest,
            getId(),
            QStringLiteral("ENIGMA JET"),
            QObject::tr("A flanger whose pitch seems to rise or fall without "
                        "end, one sweep per beat fraction."));
    pManifest->setAddDryToWet(false);
    beatfx::addBeatParameter(pManifest);
    beatfx::addDepthParameter(pManifest,
            QObject::tr("Counterclockwise: the original sound only. "
                        "Clockwise: more effect."));
    EffectManifestParameterPointer descend = pManifest->addParameter();
    descend->setId("descend");
    descend->setName(QObject::tr("Descend"));
    descend->setShortName(QObject::tr("Down"));
    descend->setDescription(QObject::tr("The pitch falls instead of rising "
                                        "(the DDJ-800's negative beat fractions)"));
    descend->setValueScaler(EffectManifestParameter::ValueScaler::Toggle);
    descend->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    descend->setRange(0, 0, 1);
    return pManifest;
}

void BeatFxEnigmaJetEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeat = parameters.value("beat");
    m_pDepth = parameters.value("depth");
    m_pDescend = parameters.value("descend");
}

void BeatFxEnigmaJetEffect::processChannel(BeatFxEnigmaJetState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const double sampleRate = engineParameters.sampleRate();
    const double cycleFrames = beatfx::beatFraction(m_pBeat->value()) *
            beatfx::beatSeconds(groupFeatures) * sampleRate;
    const double step = (m_pDescend->toBool() ? -1.0 : 1.0) / cycleFrames;
    const auto depth = static_cast<CSAMPLE_GAIN>(m_pDepth->value());
    const RampingValue<CSAMPLE_GAIN> mix(pState->prevDepth, depth, engineParameters.framesPerBuffer());
    const double logRange = std::log(kMaxMs / kMinMs);

    const int frames = engineParameters.framesPerBuffer();
    for (int f = 0; f < frames; ++f) {
        const CSAMPLE inL = pInput[f * 2];
        const CSAMPLE inR = pInput[f * 2 + 1];
        CSAMPLE wetL = 0, wetR = 0;
        for (int v = 0; v < 2; ++v) {
            double pos = pState->sweep + 0.5 * v;
            pos -= std::floor(pos);
            // pos 0 = longest delay, 1 = shortest: the pitch rises through it.
            const double ms = kMaxMs * std::exp(-logRange * pos);
            CSAMPLE dL, dR;
            pState->line.read(ms * 0.001 * sampleRate, &dL, &dR);
            const CSAMPLE window = static_cast<CSAMPLE>(0.5 - 0.5 * std::cos(2.0 * M_PI * pos));
            wetL += dL * window;
            wetR += dR * window;
        }
        pState->line.write(beatfx::clampSample(inL + wetL * kFeedback),
                beatfx::clampSample(inR + wetR * kFeedback));
        pState->sweep += step;
        pState->sweep -= std::floor(pState->sweep);
        const CSAMPLE_GAIN m = mix.getNth(f);
        pOutput[f * 2] = inL * (1 - m) + 0.5f * (inL + wetL) * m;
        pOutput[f * 2 + 1] = inR * (1 - m) + 0.5f * (inR + wetR) * m;
    }

    if (enableState == EffectEnableState::Disabling) {
        pState->clear();
    } else {
        pState->prevDepth = depth;
    }
}
