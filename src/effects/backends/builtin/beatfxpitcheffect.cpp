#include "effects/backends/builtin/beatfxpitcheffect.h"

#include <rubberband/RubberBandStretcher.h>

#include "engine/effects/engineeffectparameter.h"
#include "util/math.h"
#include "util/sample.h"

namespace {
// The nine BEAT steps, as percent of the original pitch (manual: -50 to
// +100 %; 1 beat = the centre = no change).
constexpr std::array<double, 9> kPercent = {-50, -37.5, -25, -12.5, 0, 25, 50, 75, 100};
} // namespace

BeatFxPitchState::BeatFxPitchState(const mixxx::EngineParameters& engineParameters)
        : EffectState(engineParameters) {
    planar[0] = mixxx::SampleBuffer(engineParameters.framesPerBuffer());
    planar[1] = mixxx::SampleBuffer(engineParameters.framesPerBuffer());
    audioParametersChanged(engineParameters);
}

BeatFxPitchState::~BeatFxPitchState() = default;

void BeatFxPitchState::audioParametersChanged(const mixxx::EngineParameters& engineParameters) {
    pStretcher = std::make_unique<RubberBand::RubberBandStretcher>(
            engineParameters.sampleRate(),
            engineParameters.channelCount(),
            RubberBand::RubberBandStretcher::OptionProcessRealTime);
    pStretcher->setMaxProcessSize(engineParameters.framesPerBuffer());
    pStretcher->setTimeRatio(1.0);
}

// static
QString BeatFxPitchEffect::getId() {
    return QStringLiteral("org.mixtwo.beatfx.pitch");
}

// static
EffectManifestPointer BeatFxPitchEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    beatfx::setCommonManifest(pManifest,
            getId(),
            QStringLiteral("PITCH"),
            QObject::tr("The pitch of the sound is changed, from -50 to +100 %."));
    pManifest->setAddDryToWet(false);
    beatfx::addBeatParameter(pManifest);
    beatfx::addDepthParameter(pManifest,
            QObject::tr("How far the pitch moves to the BEAT setting; all "
                        "the way counterclockwise is the original pitch"));
    return pManifest;
}

void BeatFxPitchEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeat = parameters.value("beat");
    m_pDepth = parameters.value("depth");
}

void BeatFxPitchEffect::processChannel(BeatFxPitchState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    Q_UNUSED(groupFeatures);
    int index = static_cast<int>(std::lround(m_pBeat->value()));
    index = math_clamp(index, 0, static_cast<int>(kPercent.size()) - 1);
    const double ratio = 1.0 + kPercent[index] / 100.0 * m_pDepth->value();
    pState->pStretcher->setPitchScale(ratio);

    const SINT frames = engineParameters.framesPerBuffer();
    SampleUtil::deinterleaveBuffer(pState->planar[0].data(), pState->planar[1].data(), pInput, frames);
    CSAMPLE* planar[2]{pState->planar[0].data(), pState->planar[1].data()};
    pState->pStretcher->process(planar, frames, false);
    const SINT toRead = math_min(static_cast<SINT>(pState->pStretcher->available()), frames);
    const SINT received = pState->pStretcher->retrieve(planar, toRead);
    SampleUtil::interleaveBuffer(pOutput, pState->planar[0].data(), pState->planar[1].data(), received);
    if (received < frames) {
        SampleUtil::clear(pOutput + received * 2, (frames - received) * 2);
    }

    if (enableState == EffectEnableState::Disabling) {
        pState->pStretcher->reset();
    }
}
