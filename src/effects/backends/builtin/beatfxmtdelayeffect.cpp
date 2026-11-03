#include "effects/backends/builtin/beatfxmtdelayeffect.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"
#include "util/sample.h"

namespace {
constexpr int kTaps = 7;
// Each later tap is a little quieter, as on the manual's level diagram.
constexpr CSAMPLE_GAIN kTapGain = 0.55f;
constexpr CSAMPLE_GAIN kTapDecay = 0.9f;
} // namespace

// static
QString BeatFxMtDelayEffect::getId() {
    return QStringLiteral("org.mixtwo.beatfx.mtdelay");
}

// static
EffectManifestPointer BeatFxMtDelayEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    beatfx::setCommonManifest(pManifest,
            getId(),
            QStringLiteral("MULTI TAP DELAY"),
            QObject::tr("A delayed sound plays up to 7 times, 1/8 of the "
                        "beat fraction apart."));
    beatfx::addBeatParameter(pManifest);
    beatfx::addDepthParameter(pManifest,
            QObject::tr("MIN to centre: level of the odd delays. Centre to "
                        "MAX: level of the even delays."));
    return pManifest;
}

void BeatFxMtDelayEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeat = parameters.value("beat");
    m_pDepth = parameters.value("depth");
}

void BeatFxMtDelayEffect::processChannel(BeatFxMtDelayState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const double tapSeconds = beatfx::beatFraction(m_pBeat->value()) *
            beatfx::beatSeconds(groupFeatures) / 8.0;
    double tapFrames = tapSeconds * engineParameters.sampleRate();
    const double maxTap = (pState->line.frames() - 2) / static_cast<double>(kTaps);
    if (tapFrames > maxTap) {
        tapFrames = maxTap;
    }
    const auto depth = static_cast<CSAMPLE_GAIN>(m_pDepth->value());
    const CSAMPLE_GAIN oddTarget = std::min(1.0f, depth * 2.0f);
    const CSAMPLE_GAIN evenTarget = std::max(0.0f, (depth - 0.5f) * 2.0f);
    const RampingValue<CSAMPLE_GAIN> odd(pState->prevOdd, oddTarget, engineParameters.framesPerBuffer());
    const RampingValue<CSAMPLE_GAIN> even(pState->prevEven, evenTarget, engineParameters.framesPerBuffer());

    const int frames = engineParameters.framesPerBuffer();
    for (int f = 0; f < frames; ++f) {
        pState->line.write(pInput[f * 2], pInput[f * 2 + 1]);
        CSAMPLE sumL = 0, sumR = 0;
        CSAMPLE_GAIN gain = kTapGain;
        const CSAMPLE_GAIN oddGain = odd.getNth(f);
        const CSAMPLE_GAIN evenGain = even.getNth(f);
        for (int tap = 1; tap <= kTaps; ++tap) {
            CSAMPLE l, r;
            pState->line.read(tapFrames * tap, &l, &r);
            const CSAMPLE_GAIN level = gain * ((tap % 2 == 1) ? oddGain : evenGain);
            sumL += l * level;
            sumR += r * level;
            gain *= kTapDecay;
        }
        pOutput[f * 2] = beatfx::clampSample(sumL);
        pOutput[f * 2 + 1] = beatfx::clampSample(sumR);
    }

    if (enableState == EffectEnableState::Disabling) {
        SampleUtil::applyRampingGain(pOutput, 1.0, 0.0, engineParameters.samplesPerBuffer());
        pState->clear();
    } else {
        pState->prevOdd = oddTarget;
        pState->prevEven = evenTarget;
    }
}
