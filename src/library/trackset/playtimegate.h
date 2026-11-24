#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <QHash>
#include <QString>

#include "track/trackid.h"

/// Counts the time that each deck plays its loaded track. It tells the caller
/// once, when a track has played for the threshold (60 seconds as rekordbox
/// does for its history). A paused deck adds no time. This class has no
/// timer and no control access, so a test can drive it with plain numbers.
class PlayTimeGate {
  public:
    static constexpr double kDefaultThresholdSeconds = 60.0;

    explicit PlayTimeGate(double thresholdSeconds = kDefaultThresholdSeconds)
            : m_thresholdSeconds(thresholdSeconds) {
    }

    /// Adds `elapsedSeconds` to the deck when `playing` is true. Returns true
    /// once for each load, when the count first reaches the threshold. A
    /// different track id on the deck starts a new count.
    bool advance(const QString& deck,
            TrackId trackId,
            bool playing,
            double elapsedSeconds);

    /// Forget the deck. Call it when the deck loads or ejects a track.
    void reset(const QString& deck);

  private:
    struct DeckState {
        TrackId trackId;
        double playedSeconds = 0.0;
        bool fired = false;
    };

    double m_thresholdSeconds;
    QHash<QString, DeckState> m_decks;
};
// End of AI-generated text.
