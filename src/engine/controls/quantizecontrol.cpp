#include "engine/controls/quantizecontrol.h"

#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "engine/controls/enginecontrol.h"
#include "moc_quantizecontrol.cpp"
#include "preferences/usersettings.h"
#include "track/track.h"
#include "util/assert.h"

#include <cmath>

// AI-generated (Claude Code). Start of AI-generated text.
namespace {

// Mix Two: the beat values that [ChannelN],quantize_beats accepts, in the
// order of the right-click cycle on the deck's QUANTIZE cell.
constexpr double kQuantizeBeatsValues[] = {1.0, 0.125, 0.25, 0.5};
constexpr double kQuantizeBeatsDefault = 1.0;

bool isValidQuantizeBeats(double value) {
    for (double valid : kQuantizeBeatsValues) {
        if (value == valid) {
            return true;
        }
    }
    return false;
}

} // namespace

mixxx::audio::FramePos quantizeSubdivided(mixxx::audio::FramePos prevBeat,
        mixxx::audio::FramePos nextBeat,
        mixxx::audio::FramePos position,
        double stepBeats) {
    VERIFY_OR_DEBUG_ASSERT(prevBeat.isValid() && nextBeat.isValid() && position.isValid()) {
        return position;
    }
    const mixxx::audio::FrameDiff_t beatLength = nextBeat - prevBeat;
    if (beatLength <= 0) {
        return prevBeat;
    }
    if (!(stepBeats > 0.0) || stepBeats >= 1.0) {
        // Whole beat: same rule as Mixxx's closest beat (a tie goes to next).
        return (nextBeat - position > position - prevBeat) ? prevBeat : nextBeat;
    }
    const double fraction = (position - prevBeat) / beatLength;
    const double steps = std::round(fraction / stepBeats);
    return prevBeat + steps * stepBeats * beatLength;
}
// End of AI-generated text.

QuantizeControl::QuantizeControl(const QString& group,
        UserSettingsPointer pConfig)
        : EngineControl(group, pConfig) {
    m_pCOQuantizeEnabled = new ControlPushButton(ConfigKey(group, "quantize"), true, 1.0);
    m_pCOQuantizeEnabled->setButtonMode(mixxx::control::ButtonMode::Toggle);
    m_pCONextBeat = new ControlObject(ConfigKey(group, "beat_next"));
    m_pCONextBeat->setKbdRepeatable(true);
    m_pCONextBeat->set(mixxx::audio::kInvalidFramePos.toEngineSamplePosMaybeInvalid());
    m_pCOPrevBeat = new ControlObject(ConfigKey(group, "beat_prev"));
    m_pCOPrevBeat->setKbdRepeatable(true);
    m_pCOPrevBeat->set(mixxx::audio::kInvalidFramePos.toEngineSamplePosMaybeInvalid());
    m_pCOClosestBeat = new ControlObject(ConfigKey(group, "beat_closest"));
    m_pCOClosestBeat->set(mixxx::audio::kInvalidFramePos.toEngineSamplePosMaybeInvalid());

    // AI-generated (Claude Code). Start of AI-generated text.
    // Mix Two: the beat value that quantized actions snap to, stored per deck.
    m_pCOQuantizeBeats = new ControlObject(ConfigKey(group, "quantize_beats"),
            true,  // bIgnoreNops
            false, // bTrack
            true,  // bPersist
            kQuantizeBeatsDefault);
    if (!isValidQuantizeBeats(m_pCOQuantizeBeats->get())) {
        // A stored value that is not 1/8, 1/4, 1/2 or 1.
        m_pCOQuantizeBeats->set(kQuantizeBeatsDefault);
    }
    m_pCOQuantizeBeats->connectValueChangeRequest(this,
            &QuantizeControl::slotQuantizeBeatsChangeRequest,
            Qt::DirectConnection);
    m_pCOQuantizeBeatsCycle = new ControlPushButton(ConfigKey(group, "quantize_beats_cycle"));
    connect(m_pCOQuantizeBeatsCycle,
            &ControlObject::valueChanged,
            this,
            &QuantizeControl::slotQuantizeBeatsCycle,
            Qt::DirectConnection);
    // End of AI-generated text.
}

QuantizeControl::~QuantizeControl() {
    delete m_pCOQuantizeEnabled;
    delete m_pCONextBeat;
    delete m_pCOPrevBeat;
    delete m_pCOClosestBeat;
    delete m_pCOQuantizeBeats;
    delete m_pCOQuantizeBeatsCycle;
}

// AI-generated (Claude Code). Start of AI-generated text.
void QuantizeControl::slotQuantizeBeatsChangeRequest(double value) {
    if (!isValidQuantizeBeats(value)) {
        return;
    }
    // The engine thread sees the new value in setFrameInfo() and moves
    // beat_closest there; this slot can run in the GUI thread.
    m_pCOQuantizeBeats->setAndConfirm(value);
}

void QuantizeControl::slotQuantizeBeatsCycle(double value) {
    if (value <= 0.0) {
        return;
    }
    const double current = m_pCOQuantizeBeats->get();
    constexpr int kCount = sizeof(kQuantizeBeatsValues) / sizeof(kQuantizeBeatsValues[0]);
    int next = 0;
    for (int i = 0; i < kCount; ++i) {
        if (kQuantizeBeatsValues[i] == current) {
            next = (i + 1) % kCount;
            break;
        }
    }
    m_pCOQuantizeBeats->setAndConfirm(kQuantizeBeatsValues[next]);
}
// End of AI-generated text.

void QuantizeControl::trackLoaded(TrackPointer pNewTrack) {
    if (pNewTrack) {
        m_pBeats = pNewTrack->getBeats();
        // Initialize prev and next beat as if current position was zero.
        // If there is a cue point, the value will be updated.
        lookupBeatPositions(mixxx::audio::kStartFramePos);
        updateClosestBeat(mixxx::audio::kStartFramePos);
    } else {
        m_pBeats.reset();
        m_pCOPrevBeat->set(mixxx::audio::kInvalidFramePos.toEngineSamplePosMaybeInvalid());
        m_pCONextBeat->set(mixxx::audio::kInvalidFramePos.toEngineSamplePosMaybeInvalid());
        m_pCOClosestBeat->set(mixxx::audio::kInvalidFramePos.toEngineSamplePosMaybeInvalid());
    }
}

void QuantizeControl::trackBeatsUpdated(mixxx::BeatsPointer pBeats) {
    m_pBeats = pBeats;
    const mixxx::audio::FramePos currentPosition = frameInfo().currentPosition;
    lookupBeatPositions(currentPosition);
    updateClosestBeat(currentPosition);
}

void QuantizeControl::setFrameInfo(mixxx::audio::FramePos currentPosition,
        mixxx::audio::FramePos trackEndPosition,
        mixxx::audio::SampleRate sampleRate) {
    FrameInfo frameInf = frameInfo();
    // AI-generated (Claude Code). Start of AI-generated text.
    // Mix Two: a new beat value also moves beat_closest (engine thread only).
    // End of AI-generated text.
    const double quantizeBeats = m_pCOQuantizeBeats->get();
    if (frameInf.currentPosition != currentPosition ||
            frameInf.trackEndPosition != trackEndPosition ||
            frameInf.sampleRate != sampleRate ||
            quantizeBeats != m_appliedQuantizeBeats) {
        m_appliedQuantizeBeats = quantizeBeats;
        EngineControl::setFrameInfo(currentPosition, trackEndPosition, sampleRate);
        playPosChanged(currentPosition);
    }
}

void QuantizeControl::playPosChanged(mixxx::audio::FramePos position) {
    DEBUG_ASSERT(position.isValid());
    // kInitialPlayPosition is used during track loading to indicate
    // that no valid playback position has been set yet.
    // It may happen this is called before we have valid position, so
    // skip processing to avoid passing an extreme value to the beat
    // lookup functions.
    // The correct position will be set once the track is fully loaded and
    // the engine processes the initial seek (e.g. to the cue point).
    if (position == kInitialPlayPosition) {
        return;
    }

    // We only need to update the prev or next if the current sample is
    // out of range of the existing beat positions or if we've been forced to
    // do so.
    const auto prevBeatPosition =
            mixxx::audio::FramePos::fromEngineSamplePosMaybeInvalid(
                    m_pCOPrevBeat->get());
    const auto nextBeatPosition =
            mixxx::audio::FramePos::fromEngineSamplePosMaybeInvalid(
                    m_pCONextBeat->get());
    if (!prevBeatPosition.isValid() || position < prevBeatPosition ||
            !nextBeatPosition.isValid() || position > nextBeatPosition) {
        lookupBeatPositions(position);
    }
    updateClosestBeat(position);
}

void QuantizeControl::lookupBeatPositions(mixxx::audio::FramePos position) {
    DEBUG_ASSERT(position.isValid());
    mixxx::BeatsPointer pBeats = m_pBeats;
    if (pBeats) {
        mixxx::audio::FramePos prevBeatPosition;
        mixxx::audio::FramePos nextBeatPosition;
        pBeats->findPrevNextBeats(position, &prevBeatPosition, &nextBeatPosition, false);
        // FIXME: -1.0 is a valid frame position, should we set the COs to NaN?
        m_pCOPrevBeat->set(prevBeatPosition.toEngineSamplePosMaybeInvalid());
        m_pCONextBeat->set(nextBeatPosition.toEngineSamplePosMaybeInvalid());
    }
}

void QuantizeControl::updateClosestBeat(mixxx::audio::FramePos position) {
    DEBUG_ASSERT(position.isValid());
    if (!m_pBeats) {
        return;
    }
    const auto prevBeatPosition =
            mixxx::audio::FramePos::fromEngineSamplePosMaybeInvalid(
                    m_pCOPrevBeat->get());
    const auto nextBeatPosition =
            mixxx::audio::FramePos::fromEngineSamplePosMaybeInvalid(
                    m_pCONextBeat->get());

    // Calculate closest beat by hand since we want the beat locations themselves
    // and duplicating the work by calling the standard API would double
    // the number of mutex locks.
    if (!prevBeatPosition.isValid()) {
        if (nextBeatPosition.isValid()) {
            m_pCOClosestBeat->set(nextBeatPosition.toEngineSamplePos());
        } else {
            // Likely no beat information -- can't set closest beat value.
        }
    } else if (!nextBeatPosition.isValid()) {
        m_pCOClosestBeat->set(prevBeatPosition.toEngineSamplePos());
    } else {
        // AI-generated (Claude Code). Start of AI-generated text.
        // Mix Two: with a beat value below 1 the closest point is on the
        // subdivision grid; beat_prev and beat_next stay whole beats.
        // End of AI-generated text.
        const mixxx::audio::FramePos currentClosestBeatPosition =
                quantizeSubdivided(prevBeatPosition,
                        nextBeatPosition,
                        position,
                        m_pCOQuantizeBeats->get());
        const auto closestBeatPosition =
                mixxx::audio::FramePos::fromEngineSamplePosMaybeInvalid(
                        m_pCOClosestBeat->get());
        if (closestBeatPosition != currentClosestBeatPosition) {
            m_pCOClosestBeat->set(currentClosestBeatPosition.toEngineSamplePos());
        }
    }
}
