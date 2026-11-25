#pragma once
// AI-generated (Claude Code). Start of AI-generated text.

#include <QtGlobal>
#include <optional>

#include "track/track_decl.h"

/// The preview deck as the click controller sees it. The view gives it a real
/// deck; a test gives it a fake.
class PreviewPlayerPort {
  public:
    virtual ~PreviewPlayerPort() = default;
    /// True when the track is completely loaded in the preview deck.
    virtual bool isLoaded(const TrackPointer& pTrack) const = 0;
    virtual bool isPlaying() const = 0;
    /// The play position in seconds of the loaded track.
    virtual double positionSeconds(const TrackPointer& pTrack) const = 0;
    /// Asks the deck to load the track without play. The load is not done when
    /// this returns.
    virtual void load(const TrackPointer& pTrack) = 0;
    virtual void seekAndPlay(const TrackPointer& pTrack, double seconds) = 0;
    virtual void stop() = 0;
};

/// The logic of a click on the artwork column or on the overview column of the
/// track list. The class owns the preview that it starts: it never stops, skips
/// or seeks a preview that another source (the preview button) started.
/// The class has no timer and no deck: the caller passes the time and the port.
class PreviewClickController {
  public:
    enum class Origin {
        Artwork,  // starts from the start; a click during the preview skips ahead
        Overview, // starts at the clicked position or at a cue mark
    };

    explicit PreviewClickController(PreviewPlayerPort* pPort)
            : m_pPort(pPort) {
    }

    /// True when the preview that this controller started plays this track.
    /// A click on it acts at once, with no wait for a double click.
    bool isImmediate(const TrackPointer& pTrack) const;

    /// A click of the user. Acts at once when `isImmediate()`, else waits for
    /// `fireDelayed()` so a double click can cancel it.
    void clickOrArm(const TrackPointer& pTrack,
            Origin origin,
            double startSeconds,
            double lengthSeconds,
            qint64 nowMillis);
    void cancelDelayed();
    bool isArmed() const {
        return m_armed;
    }
    /// Starts the armed click when at least `intervalMillis` have passed since
    /// it was armed. Returns true when it started.
    bool fireDelayed(qint64 nowMillis, qint64 intervalMillis);

    /// The preview deck has finished loading a track (PlayerInfo reports it).
    /// Applies the start time to the track of the click, never to another one.
    void onTrackLoaded();
    /// The track of the preview deck changed.
    void onDeckTrackChanged(const TrackPointer& pNewTrack);
    void onPlayChanged(bool playing);

    /// The pointer left the cell of the click. Stops the preview of an artwork click.
    void leftCell();
    /// Stops what this controller started (and drops an armed click).
    void stop();

    TrackPointer owner() const {
        return m_pOwner;
    }
    std::optional<Origin> ownerOrigin() const;

  private:
    void begin(const TrackPointer& pTrack, Origin origin, double startSeconds);
    void release();

    PreviewPlayerPort* const m_pPort;
    TrackPointer m_pOwner;
    Origin m_origin = Origin::Artwork;
    double m_startSeconds = 0.0;
    bool m_loadPending = false;
    bool m_started = false;

    bool m_armed = false;
    TrackPointer m_pArmedTrack;
    Origin m_armedOrigin = Origin::Artwork;
    double m_armedStart = 0.0;
    qint64 m_armedAt = 0;
};

// End of AI-generated text.
