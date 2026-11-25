// AI-generated (Claude Code). Start of AI-generated text.
#include "library/tabledelegates/previewclickcontroller.h"

#include "library/tabledelegates/previewclick.h"

bool PreviewClickController::isImmediate(const TrackPointer& pTrack) const {
    return pTrack && m_pOwner == pTrack && m_started && !m_loadPending &&
            m_pPort->isLoaded(pTrack) && m_pPort->isPlaying();
}

void PreviewClickController::clickOrArm(const TrackPointer& pTrack,
        Origin origin,
        double startSeconds,
        double lengthSeconds,
        qint64 nowMillis) {
    if (!pTrack) {
        return;
    }
    if (isImmediate(pTrack)) {
        const double target = origin == Origin::Artwork
                ? previewclick::skipAhead(m_pPort->positionSeconds(pTrack),
                          previewclick::kSkipAheadSeconds,
                          lengthSeconds)
                : startSeconds;
        m_pPort->seekAndPlay(pTrack, target);
        return;
    }
    m_armed = true;
    m_pArmedTrack = pTrack;
    m_armedOrigin = origin;
    m_armedStart = startSeconds;
    m_armedAt = nowMillis;
}

void PreviewClickController::cancelDelayed() {
    m_armed = false;
    m_pArmedTrack.reset();
}

bool PreviewClickController::fireDelayed(qint64 nowMillis, qint64 intervalMillis) {
    if (!m_armed || nowMillis - m_armedAt < intervalMillis) {
        return false;
    }
    const TrackPointer pTrack = m_pArmedTrack;
    const Origin origin = m_armedOrigin;
    const double start = m_armedStart;
    cancelDelayed();
    begin(pTrack, origin, start, nowMillis);
    return true;
}

void PreviewClickController::begin(const TrackPointer& pTrack,
        Origin origin,
        double startSeconds,
        qint64 nowMillis) {
    // A preview that another source plays is not ours: leave it alone.
    if (!m_pOwner && m_pPort->isPlaying() && m_pPort->isLoaded(pTrack)) {
        return;
    }
    m_pOwner = pTrack;
    m_origin = origin;
    m_startSeconds = startSeconds;
    m_started = false;
    if (m_pInFlight) {
        // The deck still loads an earlier click. The new track loads after it.
        m_loadPending = true;
        m_loadStartedAt = nowMillis;
        return;
    }
    if (m_pPort->isLoaded(pTrack)) {
        m_loadPending = false;
        m_started = true;
        m_pPort->seekAndPlay(pTrack, startSeconds);
        return;
    }
    m_loadPending = true;
    m_loadStartedAt = nowMillis;
    m_pInFlight = pTrack;
    m_pPort->load(pTrack);
}

void PreviewClickController::onTrackLoaded() {
    if (!m_loadPending || m_pInFlight || !m_pOwner || !m_pPort->isLoaded(m_pOwner)) {
        return;
    }
    m_loadPending = false;
    m_started = true;
    m_pPort->seekAndPlay(m_pOwner, m_startSeconds);
}

void PreviewClickController::onDeckTrackChanged(const TrackPointer& pNewTrack) {
    if (!pNewTrack) {
        // The load call of the deck reports an empty deck first.
        if (m_pOwner && !m_loadPending) {
            release();
        }
        return;
    }
    if (m_pInFlight && pNewTrack == m_pInFlight) {
        m_pInFlight.reset();
        if (m_pOwner && m_pOwner != pNewTrack) {
            // A later click wants another track: load it now.
            m_pInFlight = m_pOwner;
            m_pPort->load(m_pOwner);
        }
        return;
    }
    if (m_pOwner && pNewTrack != m_pOwner) {
        release();
    }
}

bool PreviewClickController::expireLoad(qint64 nowMillis, qint64 timeoutMillis) {
    if ((!m_loadPending && !m_pInFlight) || nowMillis - m_loadStartedAt < timeoutMillis) {
        return false;
    }
    m_pInFlight.reset();
    release();
    return true;
}

void PreviewClickController::onPlayChanged(bool playing) {
    if (!playing && m_started && !m_loadPending) {
        release();
    }
}

void PreviewClickController::leftCell() {
    const std::optional<Origin> origin = m_armed ? std::optional<Origin>(m_armedOrigin)
                                                 : ownerOrigin();
    if (origin && *origin == Origin::Artwork) {
        stop();
    }
}

void PreviewClickController::stop() {
    cancelDelayed();
    if (m_pOwner && m_pPort->isLoaded(m_pOwner)) {
        m_pPort->stop();
    }
    release();
}

std::optional<PreviewClickController::Origin> PreviewClickController::ownerOrigin() const {
    if (!m_pOwner) {
        return std::nullopt;
    }
    return m_origin;
}

void PreviewClickController::release() {
    m_pOwner.reset();
    m_started = false;
    m_loadPending = false;
}
// End of AI-generated text.
