#include "effects/backends/builtin/beatfxreverbeffect.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/sample.h"

namespace {
// The nine BEAT steps span the manual's 1 to 100 % extent; mapped onto
// the plate's decay so the top step rings long and the bottom is a room.
constexpr float kMinDecay = 0.15f;
constexpr float kMaxDecay = 0.96f;
constexpr float kBandwidth = 0.75f;
constexpr float kDamping = 0.35f;
} // namespace

// static
QString BeatFxReverbEffect::getId() {
    return QStringLiteral("org.mixtwo.beatfx.reverb");
}

// static
EffectManifestPointer BeatFxReverbEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    beatfx::setCommonManifest(pManifest,
            getId(),
            QStringLiteral("REVERB"),
            QObject::tr("A reverberation is added to the sound. The BEAT "
                        "buttons set its extent."));
    beatfx::addBeatParameter(pManifest);
    beatfx::addDepthParameter(pManifest,
            QObject::tr("Balance between the original sound and the effect sound"));
    return pManifest;
}

void BeatFxReverbEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeat = parameters.value("beat");
    m_pDepth = parameters.value("depth");
}

void BeatFxReverbEffect::processChannel(BeatFxReverbState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    Q_UNUSED(groupFeatures);
    const double step = m_pBeat->value() / static_cast<double>(beatfx::kBeatFractions.size() - 1);
    const float decay = kMinDecay + static_cast<float>(step) * (kMaxDecay - kMinDecay);
    const auto send = static_cast<float>(m_pDepth->value());

    if (pState->sampleRate != engineParameters.sampleRate()) {
        pState->reverb.setSamplerate(engineParameters.sampleRate());
        pState->sampleRate = engineParameters.sampleRate();
    }
    if (enableState == EffectEnableState::Enabling) {
        pState->reverb.activate();
    }
    pState->reverb.processBuffer(pInput,
            pOutput,
            engineParameters.samplesPerBuffer(),
            kBandwidth,
            decay,
            kDamping,
            send,
            pState->sendPrevious);

    if (enableState == EffectEnableState::Disabling) {
        SampleUtil::applyRampingGain(pOutput, 1.0, 0.0, engineParameters.samplesPerBuffer());
        pState->sendPrevious = 0;
    } else {
        pState->sendPrevious = send;
    }
}
