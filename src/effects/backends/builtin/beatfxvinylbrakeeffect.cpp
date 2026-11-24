// AI-generated (Claude Code). Start of AI-generated text.
#include "effects/backends/builtin/beatfxvinylbrakeeffect.h"

#include <algorithm>

#include "engine/effects/engineeffectparameter.h"

QString BeatFxVinylBrakeEffect::getId() {
    return QStringLiteral("org.mixtwo.beatfx.vinylbrake");
}

EffectManifestPointer BeatFxVinylBrakeEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    beatfx::setCommonManifest(pManifest,
            getId(),
            QStringLiteral("VINYL BRAKE"),
            QObject::tr("The sound slows down and stops, as a record does "
                        "when you stop the turntable. The beat fraction "
                        "sets the brake time."));
    // The output is the whole sound of the channel: the engine does not add
    // the dry sound.
    pManifest->setAddDryToWet(false);
    beatfx::addBeatParameter(pManifest);
    beatfx::addDepthParameter(pManifest,
            QObject::tr("Counterclockwise: normal speed. Clockwise: the "
                        "sound slows down until it stops."));
    return pManifest;
}

void BeatFxVinylBrakeEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeat = parameters.value("beat");
    m_pDepth = parameters.value("depth");
}

void BeatFxVinylBrakeEffect::processChannel(BeatFxVinylBrakeState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const int frames = engineParameters.framesPerBuffer();
    const int size = pState->frames;
    // Brake time T = beat fraction x beat length. The speed moves 1 / T per
    // second, so it goes from 1 to 0 (and back) in T.
    const double brakeSeconds = std::max(1e-3,
            beatfx::beatFraction(m_pBeat->value()) * beatfx::beatSeconds(groupFeatures));
    const double step = 1.0 / (brakeSeconds * engineParameters.sampleRate());
    const double target = std::clamp(1.0 - m_pDepth->value(), 0.0, 1.0);
    // A lag of more than this would read outside the recorded sound.
    const double maxLag = size - 3;
    const double crossfadeStep = 1.0 / pState->crossfadeFrames;

    double speed = pState->speed;
    double lag = pState->lag;
    double crossfade = pState->crossfade;
    int write = pState->writeIndex;
    CSAMPLE* pBuffer = pState->buffer.data();

    for (int f = 0; f < frames; ++f) {
        const CSAMPLE inL = pInput[f * 2];
        const CSAMPLE inR = pInput[f * 2 + 1];

        // When the speed leaves 0, the read position starts again from the
        // live input.
        if (speed <= 0.0 && target > 0.0) {
            lag = 0.0;
            crossfade = 0.0;
        }
        if (speed < target) {
            speed = std::min(target, speed + step);
        } else if (speed > target) {
            speed = std::max(target, speed - step);
        }

        pBuffer[write * 2] = inL;
        pBuffer[write * 2 + 1] = inR;

        if (lag <= 0.0 && speed >= 1.0) {
            // Normal speed, no lag: the input goes through unchanged.
            pOutput[f * 2] = inL;
            pOutput[f * 2 + 1] = inR;
        } else {
            const int whole = static_cast<int>(lag);
            const auto frac = static_cast<CSAMPLE>(lag - whole);
            int a = write - whole;
            if (a < 0) {
                a += size;
            }
            const int b = a == 0 ? size - 1 : a - 1;
            const CSAMPLE lagL = pBuffer[a * 2] * (1 - frac) + pBuffer[b * 2] * frac;
            const CSAMPLE lagR = pBuffer[a * 2 + 1] * (1 - frac) + pBuffer[b * 2 + 1] * frac;
            // Silence at speed 0, with a short fade before it.
            const auto gain = static_cast<CSAMPLE>(std::min(
                    1.0, speed / BeatFxVinylBrakeState::kStopFadeSpeed));
            const auto live = static_cast<CSAMPLE>(crossfade);
            pOutput[f * 2] = lagL * gain * (1 - live) + inL * live;
            pOutput[f * 2 + 1] = lagR * gain * (1 - live) + inR * live;
        }

        write = write + 1 == size ? 0 : write + 1;

        // The read position moves `speed` frames while the input moves 1.
        lag += 1.0 - speed;
        if (speed >= 1.0 && lag > 0.0) {
            crossfade += crossfadeStep;
            if (crossfade >= 1.0) {
                lag = 0.0;
                crossfade = 0.0;
            }
        } else if (crossfade > 0.0) {
            crossfade = std::max(0.0, crossfade - crossfadeStep);
        }
        if (lag > maxLag) {
            // The lag is out of the buffer: the speed counts as 0 from here.
            lag = 0.0;
            speed = 0.0;
            crossfade = 0.0;
        }
    }

    if (enableState == EffectEnableState::Disabling) {
        pState->clear();
    } else {
        pState->speed = speed;
        pState->lag = lag;
        pState->crossfade = crossfade;
        pState->writeIndex = write;
    }
}
// End of AI-generated text.
