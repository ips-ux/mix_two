// AI-generated (Claude Code). Start of AI-generated text.
#include "library/trackset/playtimegate.h"

bool PlayTimeGate::advance(const QString& deck,
        TrackId trackId,
        bool playing,
        double elapsedSeconds) {
    if (!trackId.isValid()) {
        m_decks.remove(deck);
        return false;
    }
    DeckState& state = m_decks[deck];
    if (state.trackId != trackId) {
        state = DeckState();
        state.trackId = trackId;
    }
    if (!playing || state.fired) {
        return false;
    }
    state.playedSeconds += elapsedSeconds;
    if (state.playedSeconds >= m_thresholdSeconds) {
        state.fired = true;
        return true;
    }
    return false;
}

void PlayTimeGate::reset(const QString& deck) {
    m_decks.remove(deck);
}
// End of AI-generated text.
