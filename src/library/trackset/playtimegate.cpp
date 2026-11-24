// AI-generated (Claude Code). Start of AI-generated text.
#include "library/trackset/playtimegate.h"

bool PlayTimeGate::advance(const QString& deck,
        TrackId trackId,
        bool counting,
        double elapsedSeconds,
        double trackSeconds) {
    if (!trackId.isValid()) {
        m_decks.remove(deck);
        return false;
    }
    DeckState& state = m_decks[deck];
    if (state.trackId != trackId) {
        state = DeckState();
        state.trackId = trackId;
    }
    if (!counting || state.fired) {
        return false;
    }
    state.playedSeconds += elapsedSeconds;
    double limit = m_thresholdSeconds;
    if (trackSeconds > 0.0 && trackSeconds < m_thresholdSeconds) {
        limit = trackSeconds * kShortTrackPlayedFraction;
    }
    if (state.playedSeconds >= limit) {
        state.fired = true;
        return true;
    }
    return false;
}

void PlayTimeGate::reset(const QString& deck) {
    m_decks.remove(deck);
}
// End of AI-generated text.
