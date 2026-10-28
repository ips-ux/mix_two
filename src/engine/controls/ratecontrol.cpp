#include "engine/controls/ratecontrol.h"

#include <QtDebug>
#include <array>
#include <algorithm>
#include <cmath>

#include "control/controlobject.h"
#include "control/controlpotmeter.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"
#include "control/controlttrotary.h"
#include "engine/controls/bpmcontrol.h"
#include "engine/controls/enginecontrol.h"
#include "engine/positionscratchcontroller.h"
#include "mixer/playermanager.h"
#include "moc_ratecontrol.cpp"
#include "util/rotary.h"
#include "util/time.h"
#include "vinylcontrol/defs_vinylcontrol.h"

namespace {
constexpr int kRateSensitivityMin = 100;
constexpr int kRateSensitivityMax = 2500;
} // namespace

// Static default values for rate buttons (percents)
ControlValueAtomic<double> RateControl::m_dTemporaryRateChangeCoarse;
ControlValueAtomic<double> RateControl::m_dTemporaryRateChangeFine;
ControlValueAtomic<double> RateControl::m_dPermanentRateChangeCoarse;
ControlValueAtomic<double> RateControl::m_dPermanentRateChangeFine;
int RateControl::m_iRateRampSensitivity;
RateControl::RampMode RateControl::m_eRateRampMode;

namespace {

// rekordbox tempo ranges: ±6, ±10, ±16 % and WIDE.
constexpr std::array<double, 4> kSyncRateRanges = {0.06, 0.10, 0.16, 1.00};

// The slider BPM counts as matched below this difference.
constexpr double kTempoSliderMatchBpm = 0.05;

// Platter (rekordbox-style scratching, SCRATCH-ENGINE.md):
// one revolution at 33 1/3 rpm is 1.8 s of track.
constexpr double kPlatterSecondsPerRevolution = 60.0 / (100.0 / 3.0);
// Speed limit; what it cuts off follows in the next buffers.
constexpr double kPlatterMaxSpeed = 8.0;
// Faster than this is a glitch and is dropped.
constexpr double kPlatterGlitchSpeed = 25.0;
// After a touch, motion is discarded for up to 60 ms until it adds up to
// 9/3240 of a revolution at 0.05x or faster: the landing hand stays still.
constexpr double kPlatterDeadZoneRevolutions = 9.0 / 3240.0;
constexpr double kPlatterDeadZoneSeconds = 0.060;
constexpr double kPlatterDeadZoneMinSpeed = 0.05;
// Release, spin, disable and bend (SCRATCH-ENGINE.md section 2.1).
enum PlatterState : int {
    kPlatterIdle = 0,
    kPlatterScratch = 1,
    kPlatterSpin = 2,
    kPlatterDisable = 3,
    kPlatterBend = 4,
};
// While spinning or disabled, faster than this is a glitch.
constexpr double kPlatterSpinGlitchSpeed = 50.0;
// Jog input stage (SCRATCH-ENGINE.md section 2.2), as rekordbox runs the
// DDJ-800: the steps are queued as 1 ms entries and taken one audio buffer at
// a time. A buffer passes its steps only from 2 (touched) or 3 (untouched)
// of the deck's 6480 per revolution, or if the previous buffer passed that
// much; smaller movements are held back.
constexpr double kPlatterDeadBandTouched = 2.0 / 6480.0;
constexpr double kPlatterDeadBandUntouched = 3.0 / 6480.0;
// No steps passed for this long: the jog has finished. The queue is
// emptied, the next movement starts a new delay, and disable ends. (The
// DDJ-800 never reports "stopped"; it just stops sending.)
constexpr double kPlatterQuietSeconds = 0.056;
// A spin hands back to play after this many buffers slower than play.
constexpr int kPlatterSpinSlowCount = 3;
// While spinning, an empty buffer repeats the last speed; this many in a row
// end the spin.
constexpr int kPlatterSpinEmptyBuffers = 2;
// Pushing a disabled platter this far against the spin frees it again.
constexpr double kPlatterDisableEndRevolutions = 1215.0 / 3240.0;
// Bend: from 0.5x platter speed, 1.5 / (1 - 0.5) = 3 % per 1x, up to +42 %.
constexpr double kPlatterBendMinSpeed = 0.5;
constexpr double kPlatterBendPercentPerSpeed = 1.5 / (1.0 - kPlatterBendMinSpeed);
constexpr double kPlatterBendMaxPercent = 42.0;
constexpr double kPlatterBendTimeoutSeconds = 0.056;

/// The jog turn bin of a millisecond (the clock may be negative in tests).
std::size_t platterTurnBin(qint64 ms) {
    constexpr qint64 bins = 256;
    return static_cast<std::size_t>(((ms % bins) + bins) % bins);
}

/// The smallest rekordbox range that holds a tempo change of `needed`,
/// never narrower than `current`.
double fittingRateRange(double current, double needed) {
    if (needed <= current + 1e-9) {
        return current;
    }
    for (double range : kSyncRateRanges) {
        if (range >= current && needed <= range + 1e-9) {
            return range;
        }
    }
    return std::max(current, kSyncRateRanges.back());
}

} // namespace

const double RateControl::kWheelMultiplier = 40.0;
const double RateControl::kPausedJogMultiplier = 18.0;

RateControl::RateControl(const QString& group, UserSettingsPointer pConfig)
        : EngineControl(group, pConfig),
          m_pBpmControl(nullptr),
          m_pSampleRate(QStringLiteral("[App]"), QStringLiteral("samplerate")),
          m_pRateRatio(std::make_unique<ControlObject>(
                  ConfigKey(group, QStringLiteral("rate_ratio")),
                  true,
                  false,
                  false,
                  1.0)),
          m_pRateDir(std::make_unique<ControlObject>(
                  ConfigKey(group, QStringLiteral("rate_dir")))),
          m_pRateRange(std::make_unique<ControlPotmeter>(
                  ConfigKey(group, QStringLiteral("rateRange")), 0.01, 4.00)),
          // We need the sample rate so we can guesstimate something close
          // what latency is.
          // Allow rate slider to go out of bounds so that sync lock rate
          // adjustments are not capped.
          m_pRateSlider(std::make_unique<ControlPotmeter>(
                  ConfigKey(group, QStringLiteral("rate")), -1.0, 1.0, true)),
          // Search rate. Rate used when searching in sound. This overrules the
          // playback rate
          m_pRateSearch(std::make_unique<ControlPotmeter>(
                  ConfigKey(group, QStringLiteral("rateSearch")), -300., 300.)),
          m_pRatePhysical(std::make_unique<ControlObject>(
                  ConfigKey(group, QStringLiteral("rate_physical")))),
          m_pRateRangeNext(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("rate_range_next")))),
          m_pPlatterTouch(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("platter_touch")))),
          m_pPlatterTurn(std::make_unique<ControlObject>(
                  ConfigKey(group, QStringLiteral("platter_turn")))),
          m_pPlatterState(std::make_unique<ControlObject>(
                  ConfigKey(group, QStringLiteral("platter_state")))),
          m_platterState(kPlatterIdle),
          m_platterQueue{},
          m_platterQueueHead(0),
          m_platterQueueCount(0),
          m_platterLastTickMs(-1),
          m_platterJogActive(false),
          m_platterLastEntryDirection(0),
          m_platterCarry(0.0),
          m_platterPassedLast(false),
          m_platterLastPassClock(0.0),
          m_platterClock(0.0),
          m_platterSpeed(0.0),
          m_platterDirection(0),
          m_platterReversed(false),
          m_platterSpinDirection(1),
          m_platterSlowCount(0),
          m_platterSpinEmpty(0),
          m_platterAgainstSpin(0.0),
          m_platterPlayingAtTouch(false),
          m_platterBend(0.0),
          m_platterBendClock(0.0),
          m_platterActive(false),
          m_platterDeadZone(false),
          m_platterAnchorPending(false),
          m_platterAnchor(0.0),
          m_platterTurned(0.0),
          m_platterDeadTurn(0.0),
          m_platterSinceTouch(0.0),
          m_pTempoSliderBpm(std::make_unique<ControlObject>(
                  ConfigKey(group, QStringLiteral("tempo_slider_bpm")))),
          m_pTempoSliderMatched(std::make_unique<ControlObject>(
                  ConfigKey(group, QStringLiteral("tempo_slider_matched")),
                  true,
                  false,
                  false,
                  1.0)),
          m_ratePhysicalKnown(false),
          // Temporary rate-change buttons
          m_pButtonRateTempDown(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("rate_temp_down")))),
          m_pButtonRateTempDownSmall(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("rate_temp_down_small")))),
          m_pButtonRateTempUp(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("rate_temp_up")))),
          m_pButtonRateTempUpSmall(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("rate_temp_up_small")))),
          // Permanent rate-change buttons
          m_pButtonRatePermDown(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("rate_perm_down")))),
          m_pButtonRatePermDownSmall(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("rate_perm_down_small")))),
          m_pButtonRatePermUp(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("rate_perm_up")))),
          m_pButtonRatePermUpSmall(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("rate_perm_up_small")))),
          // Reverse button
          m_pReverseButton(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("reverse")))),
          m_pReverseRollButton(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("reverseroll")))),
          m_pForwardButton(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("fwd")))),
          m_pBackButton(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("back")))),
          // Wheel to control playback position/speed
          m_pWheel(std::make_unique<ControlTTRotary>(
                  ConfigKey(group, QStringLiteral("wheel")))),
          // Scratch controller, this is an accumulator which is useful for
          // controllers that return individual +1 or -1s, these get added up
          // and cleared when we read
          m_pScratch2(std::make_unique<ControlObject>(
                  ConfigKey(group, QStringLiteral("scratch2")))),
          m_pScratch2Enable(std::make_unique<ControlPushButton>(
                  ConfigKey(group, QStringLiteral("scratch2_enable")))),
          m_pScratch2Scratching(std::make_unique<ControlPushButton>(ConfigKey(
                  group, QStringLiteral("scratch2_indicates_scratching")))),
          m_pScratchController(
                  std::make_unique<PositionScratchController>(group)),
          m_pJog(std::make_unique<ControlObject>(
                  ConfigKey(group, QStringLiteral("jog")))),
          // FIXME: The filter length should be dependent on sample rate/block size or something
          m_pJogFilter(std::make_unique<Rotary>(25)),
          m_pVCEnabled(nullptr),
          m_pVCScratching(nullptr),
          m_pVCMode(nullptr),
          m_syncMode(group, QStringLiteral("sync_mode")),
          m_slipEnabled(group, QStringLiteral("slip_enabled")),
          m_wrapAroundCount(0),
          m_jumpPos(mixxx::audio::FramePos()),
          m_targetPos(mixxx::audio::FramePos()),
          m_bTempStarted(false),
          m_tempRateRatio(0.0),
          m_dRateTempRampChange(0.0) {
    // Vinyl control COs are only created for main decks
    if (PlayerManager::isDeckGroup(getGroup())) {
        m_pVCEnabled = ControlObject::getControl(
                ConfigKey(getGroup(), QStringLiteral("vinylcontrol_enabled")),
                ControlFlag::NoAssertIfMissing);
        m_pVCScratching = ControlObject::getControl(
                ConfigKey(getGroup(), QStringLiteral("vinylcontrol_scratching")),
                ControlFlag::NoAssertIfMissing);
        m_pVCMode = ControlObject::getControl(
                ConfigKey(getGroup(), QStringLiteral("vinylcontrol_mode")),
                ControlFlag::NoAssertIfMissing);
    }
    // This is the resulting rate ratio that can be used for display or calculations.
    // The track original rate ratio is 1.
    connect(m_pRateRatio.get(),
            &ControlObject::valueChanged,
            this,
            &RateControl::slotRateRatioChanged,
            Qt::DirectConnection);

    connect(m_pRateDir.get(),
            &ControlObject::valueChanged,
            this,
            &RateControl::slotRateRangeChanged,
            Qt::DirectConnection);
    connect(m_pRateRange.get(),
            &ControlObject::valueChanged,
            this,
            &RateControl::slotRateRangeChanged,
            Qt::DirectConnection);

    connect(m_pRateSlider.get(),
            &ControlObject::valueChanged,
            this,
            &RateControl::slotRateSliderChanged,
            Qt::DirectConnection);

    m_pTempoSliderBpm->setReadOnly();
    m_pTempoSliderMatched->setReadOnly();
    connect(m_pRatePhysical.get(),
            &ControlObject::valueChanged,
            this,
            &RateControl::slotRatePhysicalChanged,
            Qt::DirectConnection);
    connect(m_pRateRangeNext.get(),
            &ControlObject::valueChanged,
            this,
            &RateControl::slotRateRangeNext,
            Qt::DirectConnection);
    m_pPlatterTouch->setButtonMode(mixxx::control::ButtonMode::Toggle);
    m_pPlatterState->setReadOnly();
    // Collect the turns here (any thread), by arrival millisecond; the
    // engine queues them as rekordbox's 1 ms jog timer does.
    for (auto& bin : m_platterTurnBins) {
        bin.store(0.0);
    }
    m_pPlatterTurn->connectValueChangeRequest(
            this,
            [this](double revolutions) {
                const qint64 ms = mixxx::Time::elapsed().toIntegerMillis();
                auto& bin = m_platterTurnBins[platterTurnBin(ms)];
                double pending = bin.load();
                while (!bin.compare_exchange_weak(pending, pending + revolutions)) {
                }
            },
            Qt::DirectConnection);

    m_pReverseButton->set(0);

    connect(m_pForwardButton.get(),
            &ControlObject::valueChanged,
            this,
            &RateControl::slotControlFastForward,
            Qt::DirectConnection);
    m_pForwardButton->set(0);

    connect(m_pBackButton.get(),
            &ControlObject::valueChanged,
            this,
            &RateControl::slotControlFastBack,
            Qt::DirectConnection);
    m_pBackButton->set(0);

    connect(m_pReverseRollButton.get(),
            &ControlObject::valueChanged,
            this,
            &RateControl::slotReverseRollActivate,
            Qt::DirectConnection);

    // Permanent rate-change buttons
    m_pButtonRatePermDown->setKbdRepeatable(true);
    connect(m_pButtonRatePermDown.get(),
            &ControlObject::valueChanged,
            this,
            &RateControl::slotControlRatePermDown,
            Qt::DirectConnection);

    m_pButtonRatePermDownSmall->setKbdRepeatable(true);
    connect(m_pButtonRatePermDownSmall.get(),
            &ControlObject::valueChanged,
            this,
            &RateControl::slotControlRatePermDownSmall,
            Qt::DirectConnection);

    m_pButtonRatePermUp->setKbdRepeatable(true);
    connect(m_pButtonRatePermUp.get(),
            &ControlObject::valueChanged,
            this,
            &RateControl::slotControlRatePermUp,
            Qt::DirectConnection);

    m_pButtonRatePermUpSmall->setKbdRepeatable(true);
    connect(m_pButtonRatePermUpSmall.get(),
            &ControlObject::valueChanged,
            this,
            &RateControl::slotControlRatePermUpSmall,
            Qt::DirectConnection);

    // Scratch enable toggle
    m_pScratch2Enable->set(0);

    // Enable by default, because it was always scratching before introducing
    // this control.
    m_pScratch2Scratching->set(1.0);

    //     // Update Internal Settings
    //     // Set Pitchbend Mode
    //     m_eRateRampMode = static_cast<RampMode>(
    //         getConfig()->getValue(ConfigKey("[Controls]","RateRamp"),
    //                               static_cast<int>(RampMode::Stepping)));

    //     // Set the Sensitivity
    //     m_iRateRampSensitivity =
    //             getConfig()->getValueString(ConfigKey("[Controls]","RateRampSensitivity")).toInt();
}

void RateControl::setBpmControl(BpmControl* bpmcontrol) {
    m_pBpmControl = bpmcontrol;
    // BpmControl owns local_bpm, so it exists from here on.
    m_pLocalBpm = std::make_unique<ControlProxy>(getGroup(), QStringLiteral("local_bpm"), this);
    m_pLocalBpm->connectValueChanged(this, [this] { updateTempoSlider(); }, Qt::DirectConnection);
    updateTempoSlider();
}

//static
void RateControl::setRateRampMode(RampMode mode) {
    m_eRateRampMode = mode;
}

//static
RateControl::RampMode RateControl::getRateRampMode() {
    return m_eRateRampMode;
}

//static
void RateControl::setRateRampSensitivity(int sense) {
    // Reverse the actual sensitivity value passed.
    // That way the gui works in an intuitive manner.
    sense = kRateSensitivityMax - sense + kRateSensitivityMin;
    if (sense < kRateSensitivityMin) {
        m_iRateRampSensitivity = kRateSensitivityMin;
    } else if (sense > kRateSensitivityMax) {
        m_iRateRampSensitivity = kRateSensitivityMax;
    } else {
        m_iRateRampSensitivity = sense;
    }
}

//static
void RateControl::setTemporaryRateChangeCoarseAmount(double v) {
    m_dTemporaryRateChangeCoarse.setValue(v);
}

//static
void RateControl::setTemporaryRateChangeFineAmount(double v) {
    m_dTemporaryRateChangeFine.setValue(v);
}

//static
void RateControl::setPermanentRateChangeCoarseAmount(double v) {
    m_dPermanentRateChangeCoarse.setValue(v);
}

//static
void RateControl::setPermanentRateChangeFineAmount(double v) {
    m_dPermanentRateChangeFine.setValue(v);
}

//static
double RateControl::getTemporaryRateChangeCoarseAmount() {
    return m_dTemporaryRateChangeCoarse.getValue();
}

//static
double RateControl::getTemporaryRateChangeFineAmount() {
    return m_dTemporaryRateChangeFine.getValue();
}

//static
double RateControl::getPermanentRateChangeCoarseAmount() {
    return m_dPermanentRateChangeCoarse.getValue();
}

//static
double RateControl::getPermanentRateChangeFineAmount() {
    return m_dPermanentRateChangeFine.getValue();
}

void RateControl::slotRateRangeChanged(double) {
    // update RateSlider with the new Range value butdo not change m_pRateRatio
    slotRateRatioChanged(m_pRateRatio->get());
}

void RateControl::slotRateSliderChanged(double v) {
    double rateRatio = 1.0 + m_pRateDir->get() * m_pRateRange->get() * v;
    m_pRateRatio->set(rateRatio);
    updateTempoSlider();
}

void RateControl::slotRateRatioChanged(double v) {
    double rateRange = m_pRateRange->get();
    if (rateRange > 0.0 && getSyncMode() != SyncMode::None) {
        // Mix Two (rekordbox): while synced, the tempo range widens to the
        // smallest one that holds the synced tempo.
        const double fitting = fittingRateRange(rateRange, std::fabs(v - 1));
        if (fitting != rateRange) {
            rateRange = fitting;
            m_pRateRange->set(rateRange);
        }
    }
    if (rateRange > 0.0) {
        double newRate = m_pRateDir->get() * (v - 1) / rateRange;
        m_pRateSlider->set(newRate);
    } else {
        m_pRateSlider->set(0);
    }
    updateTempoSlider();
}

void RateControl::slotRateRangeNext(double v) {
    if (v <= 0.0) {
        return;
    }
    const double current = m_pRateRange->get();
    double next = kSyncRateRanges.front();
    for (double range : kSyncRateRanges) {
        if (range > current + 0.001) {
            next = range;
            break;
        }
    }
    m_pRateRange->set(next);
    // Our own set() does not notify us. Keeps the tempo, moves the slider.
    slotRateRangeChanged(next);
}

double RateControl::calculatePlatterRate(
        double baserate, std::size_t samplesPerBuffer, double turn) {
    const double outputRate = m_pSampleRate.get();
    const double outputFrames = static_cast<double>(samplesPerBuffer) / 2.0;
    if (outputRate <= 0.0 || baserate <= 0.0 || outputFrames <= 0.0) {
        return 0.0;
    }
    const double bufferSeconds = outputFrames / outputRate;
    const double position = frameInfo().currentPosition.value();
    if (!m_platterActive) {
        // Touched: stop the track. The renderer eases into the stop over
        // this buffer, so the anchor is taken in the next one.
        m_platterActive = true;
        m_platterDeadZone = true;
        m_platterAnchorPending = true;
        m_platterTurned = 0.0;
        m_platterDeadTurn = 0.0;
        m_platterSinceTouch = 0.0;
        m_platterSinceTouch += bufferSeconds;
        return 0.0;
    }
    if (m_platterAnchorPending) {
        m_platterAnchorPending = false;
        m_platterAnchor = position;
    }
    m_platterSinceTouch += bufferSeconds;

    const double turnSpeed =
            std::fabs(turn) * kPlatterSecondsPerRevolution / bufferSeconds;
    if (m_platterDeadZone) {
        m_platterDeadTurn += turn;
        if (m_platterSinceTouch >= kPlatterDeadZoneSeconds ||
                (std::fabs(m_platterDeadTurn) >= kPlatterDeadZoneRevolutions &&
                        turnSpeed >= kPlatterDeadZoneMinSpeed)) {
            // Out of the dead zone: this buffer's turn counts, what came
            // before is discarded.
            m_platterDeadZone = false;
        } else {
            turn = 0.0;
        }
    }
    m_platterTurned += turn;

    // Aim at where the platter puts the track; the speed limit only delays
    // getting there, so nothing is lost and the audio stays locked.
    const double trackRate = baserate * outputRate;
    const double target = m_platterAnchor +
            m_platterTurned * kPlatterSecondsPerRevolution * trackRate;
    const double rate = (target - position) / (outputFrames * baserate);
    return std::clamp(rate, -kPlatterMaxSpeed, kPlatterMaxSpeed);
}

void RateControl::clearPlatterQueue() {
    m_platterQueueHead = 0;
    m_platterQueueCount = 0;
    m_platterJogActive = false;
    m_platterLastEntryDirection = 0;
    m_platterCarry = 0.0;
    m_platterPassedLast = false;
}

double RateControl::takePlatterTurn(double bufferSeconds, bool touched, bool* pJogFinished) {
    *pJogFinished = false;
    const double bufferMs = bufferSeconds * 1000.0;
    // An entry lasts at least 1 ms and at most two buffers (rounded up to
    // whole ms); the first entry of a movement gets the maximum, which is
    // what delays every movement by about two buffers.
    const double maxEntryMs = std::max(1.0, 2.0 * std::ceil(bufferMs));
    const auto queueEntry = [this, maxEntryMs](double turn) {
        double ms = 1.0;
        if (!m_platterJogActive) {
            m_platterJogActive = true;
            m_platterLastPassClock = m_platterClock;
            ms = maxEntryMs;
        }
        // rekordbox's quirk: no movement counts as forward, and a change of
        // direction between entries shortens the entry to 0.5 ms.
        const int direction = turn < 0.0 ? -1 : 1;
        if (m_platterLastEntryDirection != 0 && direction != m_platterLastEntryDirection) {
            ms = 0.5;
        }
        m_platterLastEntryDirection = direction;
        if (m_platterQueueCount < kPlatterQueueSize) {
            m_platterQueue[(m_platterQueueHead + m_platterQueueCount) % kPlatterQueueSize] =
                    PlatterEntry{turn, ms};
            ++m_platterQueueCount;
        }
    };

    // The 1 ms ticks since the last buffer: each takes what arrived in the
    // millisecond before it; while the jog is active, even nothing is queued.
    const qint64 nowMs = mixxx::Time::elapsed().toIntegerMillis();
    if (m_platterLastTickMs < 0 || nowMs - m_platterLastTickMs >= kPlatterTurnBins) {
        // The first buffer, or the engine stalled: take everything at once.
        double turn = 0.0;
        for (auto& bin : m_platterTurnBins) {
            turn += bin.exchange(0.0);
        }
        if (turn != 0.0) {
            queueEntry(turn);
        }
    } else {
        for (qint64 tick = m_platterLastTickMs + 1; tick <= nowMs; ++tick) {
            const double turn =
                    m_platterTurnBins[platterTurnBin(tick - 1)].exchange(0.0);
            if (m_platterJogActive || turn != 0.0) {
                queueEntry(turn);
            }
        }
    }
    m_platterLastTickMs = nowMs;

    // This buffer takes exactly its own length of queue time; an entry's
    // steps are spread evenly over its duration.
    double turn = 0.0;
    double needMs = bufferMs;
    while (needMs > 1e-9 && m_platterQueueCount > 0) {
        PlatterEntry& entry = m_platterQueue[m_platterQueueHead];
        const double usedMs = std::min(needMs, entry.ms);
        const double part = entry.ms > 0.0 ? entry.turn * usedMs / entry.ms : entry.turn;
        turn += part;
        entry.turn -= part;
        entry.ms -= usedMs;
        needMs -= usedMs;
        if (entry.ms <= 1e-9) {
            m_platterQueueHead = (m_platterQueueHead + 1) % kPlatterQueueSize;
            --m_platterQueueCount;
        }
    }

    // Dead band.
    turn += m_platterCarry;
    m_platterCarry = 0.0;
    const double band =
            (touched ? kPlatterDeadBandTouched : kPlatterDeadBandUntouched) * (1.0 - 1e-9);
    const bool enough = std::fabs(turn) >= band;
    if (!enough && !m_platterPassedLast) {
        m_platterCarry = turn;
        turn = 0.0;
    }
    m_platterPassedLast = enough;
    if (turn != 0.0) {
        m_platterLastPassClock = m_platterClock;
    }

    if (m_platterJogActive && m_platterClock - m_platterLastPassClock > kPlatterQuietSeconds) {
        clearPlatterQueue();
        *pJogFinished = true;
    }
    return turn;
}

bool RateControl::platterSpinGoesOn(bool playing, double playRate, double speed) {
    if (speed == 0.0) {
        return false;
    }
    // Slower than the deck plays, in its direction: hand back to play.
    if (playing && playRate != 0.0 && (speed > 0.0) == (playRate > 0.0) &&
            std::fabs(speed) < std::fabs(playRate)) {
        ++m_platterSlowCount;
        return m_platterSlowCount < kPlatterSpinSlowCount;
    }
    m_platterSlowCount = 0;
    return true;
}

void RateControl::setPlatterState(int state) {
    if (state != m_platterState) {
        m_platterState = state;
        m_pPlatterState->forceSet(state);
    }
}

bool RateControl::processPlatter(double baserate,
        double tempoRatio,
        bool paused,
        std::size_t samplesPerBuffer,
        double* pRate) {
    const double outputRate = m_pSampleRate.get();
    const double bufferSeconds = outputRate > 0.0
            ? static_cast<double>(samplesPerBuffer) / 2.0 / outputRate
            : 0.0;
    m_platterClock += bufferSeconds;
    const bool touchedNow = m_pPlatterTouch->toBool();
    // A touch applies before this buffer's steps, a release after them, so
    // the release is decided on the motion this buffer still had under the
    // hand.
    const bool touched = touchedNow || m_platterState == kPlatterScratch;
    bool jogFinished = false;
    double turn = takePlatterTurn(bufferSeconds, touched, &jogFinished);
    double speed = bufferSeconds > 0.0 ? turn * kPlatterSecondsPerRevolution / bufferSeconds : 0.0;
    if (turn != 0.0) {
        const double glitchSpeed =
                (m_platterState == kPlatterSpin || m_platterState == kPlatterDisable)
                ? kPlatterSpinGlitchSpeed
                : kPlatterGlitchSpeed;
        if (std::fabs(speed) > glitchSpeed) {
            turn = 0.0; // a glitch, not a hand
            speed = 0.0;
        }
    }
    const bool moved = turn != 0.0;
    if (moved) {
        const int direction = turn > 0.0 ? 1 : -1;
        m_platterReversed = m_platterDirection != 0 && direction != m_platterDirection;
        m_platterDirection = direction;
        m_platterSpeed = speed;
    }

    const bool playing = !paused;
    const double playRate = m_pReverseButton->toBool() ? -tempoRatio : tempoRatio;

    // A spin or a scratch ends here. A deck that played at the touch
    // ignores the coasting platter for a while.
    const auto stopDriving = [this] {
        if (m_platterPlayingAtTouch) {
            m_platterAgainstSpin = 0.0;
            setPlatterState(kPlatterDisable);
        } else {
            setPlatterState(kPlatterIdle);
        }
    };

    if (touched) {
        if (m_platterState != kPlatterScratch) {
            m_platterActive = false; // calculatePlatterRate anchors anew
            m_platterPlayingAtTouch = playing;
            m_platterBend = 0.0;
            setPlatterState(kPlatterScratch);
        }
        *pRate = calculatePlatterRate(baserate, samplesPerBuffer, turn);
        if (!touchedNow) {
            // Released: spin on if this buffer still moved.
            m_platterActive = false;
            m_platterSlowCount = 0;
            if (moved && !m_platterReversed && platterSpinGoesOn(playing, playRate, speed)) {
                m_platterSpinDirection = speed > 0.0 ? 1 : -1;
                m_platterSpinEmpty = 0;
                setPlatterState(kPlatterSpin);
            } else {
                m_platterSpinDirection = m_platterDirection != 0 ? m_platterDirection : 1;
                stopDriving();
            }
        }
        return true;
    }

    if (m_platterState == kPlatterSpin) {
        if (moved) {
            m_platterSpinEmpty = 0;
            if (m_platterReversed || !platterSpinGoesOn(playing, playRate, speed)) {
                stopDriving();
            }
        } else if (++m_platterSpinEmpty >= kPlatterSpinEmptyBuffers) {
            stopDriving(); // the platter has stopped
        }
    }

    if (m_platterState == kPlatterSpin) {
        // The platter's speed drives the track; an empty buffer repeats the
        // last one.
        *pRate = std::clamp(m_platterSpeed, -kPlatterMaxSpeed, kPlatterMaxSpeed);
        return true;
    }

    if (m_platterState == kPlatterDisable) {
        if (moved && (turn > 0.0 ? 1 : -1) != m_platterSpinDirection) {
            m_platterAgainstSpin += std::fabs(turn);
            if (m_platterAgainstSpin > kPlatterDisableEndRevolutions) {
                setPlatterState(kPlatterIdle);
            }
        } else if (jogFinished) {
            setPlatterState(kPlatterIdle); // the jog has gone quiet
        }
        return false;
    }

    // Idle or bend: the untouched platter bends the tempo.
    if (moved && std::fabs(speed) > kPlatterBendMinSpeed) {
        // Capped above at 42 %, then limited to ±42 % by the player.
        m_platterBend = std::clamp(speed * kPlatterBendPercentPerSpeed,
                                -kPlatterBendMaxPercent,
                                kPlatterBendMaxPercent) /
                100.0;
        m_platterBendClock = m_platterClock;
        setPlatterState(kPlatterBend);
    } else if (m_platterState == kPlatterBend &&
            m_platterClock - m_platterBendClock > kPlatterBendTimeoutSeconds) {
        m_platterBend = 0.0;
        setPlatterState(kPlatterIdle);
    }
    return false;
}

void RateControl::slotRatePhysicalChanged(double) {
    m_ratePhysicalKnown = true;
    updateTempoSlider();
}

void RateControl::updateTempoSlider() {
    const double localBpm = m_pLocalBpm ? m_pLocalBpm->get() : 0.0;
    if (localBpm <= 0.0) {
        m_pTempoSliderBpm->forceSet(0.0);
        m_pTempoSliderMatched->forceSet(1.0);
        return;
    }
    const double position = m_ratePhysicalKnown ? m_pRatePhysical->get() : m_pRateSlider->get();
    const double sliderBpm =
            localBpm * (1.0 + m_pRateDir->get() * m_pRateRange->get() * position);
    const double bpm = localBpm * m_pRateRatio->get();
    m_pTempoSliderBpm->forceSet(sliderBpm);
    m_pTempoSliderMatched->forceSet(
            std::fabs(sliderBpm - bpm) < kTempoSliderMatchBpm ? 1.0 : 0.0);
}

void RateControl::slotReverseRollActivate(double v) {
    if (v > 0.0) {
        m_slipEnabled.set(1);
        m_pReverseButton->set(1);
    } else {
        m_pReverseButton->set(0);
        m_slipEnabled.set(0);
    }
}

void RateControl::slotControlFastForward(double v) {
    //qDebug() << "slotControlFastForward(" << v << ")";
    if (v > 0.0) {
        m_pRateSearch->set(4.0);
    } else {
        m_pRateSearch->set(0.0);
    }
}

void RateControl::slotControlFastBack(double v) {
    //qDebug() << "slotControlFastBack(" << v << ")";
    if (v > 0.0) {
        m_pRateSearch->set(-4.0);
    } else {
        m_pRateSearch->set(0.0);
    }
}

void RateControl::slotControlRatePermDown(double v) {
    // Adjusts temp rate down if button pressed
    if (v > 0.0) {
        m_pRateSlider->set(m_pRateSlider->get() -
                m_pRateDir->get() * m_dPermanentRateChangeCoarse.getValue() / (100 * m_pRateRange->get()));
        slotRateSliderChanged(m_pRateSlider->get());
    }
}

void RateControl::slotControlRatePermDownSmall(double v) {
    // Adjusts temp rate down if button pressed
    if (v > 0.0) {
        m_pRateSlider->set(m_pRateSlider->get() -
                m_pRateDir->get() * m_dPermanentRateChangeFine.getValue() / (100. * m_pRateRange->get()));
        slotRateSliderChanged(m_pRateSlider->get());
    }
}

void RateControl::slotControlRatePermUp(double v) {
    // Adjusts temp rate up if button pressed
    if (v > 0.0) {
        m_pRateSlider->set(m_pRateSlider->get() +
                m_pRateDir->get() * m_dPermanentRateChangeCoarse.getValue() / (100. * m_pRateRange->get()));
        slotRateSliderChanged(m_pRateSlider->get());
    }
}

void RateControl::slotControlRatePermUpSmall(double v) {
    // Adjusts temp rate up if button pressed
    if (v > 0.0) {
        m_pRateSlider->set(m_pRateSlider->get() +
                           m_pRateDir->get() * m_dPermanentRateChangeFine.getValue() / (100. * m_pRateRange->get()));
        slotRateSliderChanged(m_pRateSlider->get());
    }
}

double RateControl::getWheelFactor() const {
    return m_pWheel->get();
}

double RateControl::getJogFactor() const {
    // FIXME: Sensitivity should be configurable separately?
    constexpr double jogSensitivity = 0.1; // Nudges during playback
    double jogValue = m_pJog->get();

    // Since m_pJog is an accumulator, reset it since we've used its value.
    if (jogValue != 0.) {
        m_pJog->set(0.);
    }

    double jogValueFiltered = m_pJogFilter->filter(jogValue);
    double jogFactor = jogValueFiltered * jogSensitivity;

    if (util_isnan(jogValue) || util_isnan(jogFactor)) {
        jogFactor = 0.0;
    }

    return jogFactor;
}

SyncMode RateControl::getSyncMode() const {
    return syncModeFromDouble(m_syncMode.get());
}

double RateControl::calculateSpeed(double baserate,
        double speed,
        bool paused,
        std::size_t samplesPerBuffer,
        bool* pReportScratching,
        bool* pReportReverse) {
    *pReportScratching = false;
    *pReportReverse = false;

    processTempRate(samplesPerBuffer);

    // Mix Two: a scratching or spinning platter overrides everything else,
    // as on Pioneer; an untouched one bends the tempo.
    double platterRate = 0.0;
    if (processPlatter(baserate, speed, paused, samplesPerBuffer, &platterRate)) {
        *pReportScratching = true;
        return platterRate;
    }
    // rekordbox adds the bend to 1 + the tempo change; it does not scale it.
    const double platterBend = paused ? 0.0 : m_platterBend;

    double rate;
    const double searching = m_pRateSearch->get();
    if (searching != 0) {
        // If searching is in progress, it overrides everything else
        rate = searching;
    } else {
        double wheelFactor = getWheelFactor();
        double jogFactor = getJogFactor();
        bool bVinylControlEnabled = m_pVCEnabled && m_pVCEnabled->toBool();
        bool useScratch2Value = m_pScratch2Enable->toBool();

        // By default scratch2_enable is enough to determine if the user is
        // scratching or not. Moving platter controllers have to disable
        // "scratch2_indicates_scratching" if they are not scratching,
        // to allow things like key-lock.
        if (useScratch2Value && m_pScratch2Scratching->toBool()) {
            *pReportScratching = true;
        }

        if (bVinylControlEnabled) {
            if (m_pVCScratching && m_pVCScratching->toBool()) {
                *pReportScratching = true;
            }
            rate = speed;
        } else {
            double scratchFactor = m_pScratch2->get();
            // Don't trust values from m_pScratch2
            if (util_isnan(scratchFactor)) {
                scratchFactor = 0.0;
            }
            if (paused) {
                // Stopped. Wheel, jog and scratch controller all scrub through audio.
                if (useScratch2Value) {
                    rate = scratchFactor + jogFactor + wheelFactor * kWheelMultiplier;
                } else {
                    rate = jogFactor * kPausedJogMultiplier + wheelFactor;
                }
            } else {
                // The buffer is playing, so calculate the buffer rate.

                // There are four rate effects we apply: wheel, scratch, jog and temp.
                // Wheel:   a linear additive effect (no spring-back)
                // Scratch: a rate multiplier
                // Jog:     a linear additive effect whose value is filtered (springs back)
                // Temp:    pitch bend

                // New scratch behavior - overrides playback speed (and old behavior)
                if (useScratch2Value) {
                    rate = scratchFactor;
                } else {
                    // add temp rate, but don't go backwards
                    rate = math_max(speed + getTempRate(), 0.0);
                    rate += wheelFactor;
                }
                rate += jogFactor + platterBend;
            }
        }

        double currentSample = frameInfo().currentPosition.toEngineSamplePos();
        // Let PositionScratchController also know if the play pos wrapped around
        // (beatloop or track repeat) so it can correctly interpret the sample position delta.
        m_pScratchController->process(currentSample,
                rate,
                samplesPerBuffer,
                baserate,
                m_wrapAroundCount,
                m_jumpPos,
                m_targetPos);
        // Reset count after use.
        m_wrapAroundCount = 0;

        // If waveform scratch is enabled, override all other controls
        if (m_pScratchController->isEnabled()) {
            rate = m_pScratchController->getRate();
            *pReportScratching = true;
        } else {
            // If sync lock is on, respond to it -- but vinyl and scratch mode always override.
            if (toSynchronized(getSyncMode()) && !paused &&
                    !bVinylControlEnabled && !useScratch2Value) {
                if (m_pBpmControl == nullptr) {
                    qDebug() << "ERROR: calculateRate m_pBpmControl is null during sync lock";
                    return 1.0;
                }

                double userTweak = 0.0;
                if (!*pReportScratching) {
                    // Only report user tweak if the user is not scratching.
                    userTweak = getTempRate() + wheelFactor + jogFactor + platterBend;
                }
                rate = m_pBpmControl->calcSyncedRate(userTweak);
            }
            // If we are reversing (and not scratching,) flip the rate.  This is ok even when syncing.
            // Reverse with vinyl is only ok if absolute mode isn't on.
            int vcmode = m_pVCMode ? static_cast<int>(m_pVCMode->get()) : MIXXX_VCMODE_ABSOLUTE;
            // TODO(owen): Instead of just ignoring reverse mode, should we
            // disable absolute mode instead?
            if (m_pReverseButton->toBool() && !m_pScratch2Enable->toBool() &&
                    (!bVinylControlEnabled ||
                            vcmode != MIXXX_VCMODE_ABSOLUTE)) {
                rate = -rate;
                *pReportReverse = true;
            }
        }
    }
    return rate;
}

void RateControl::processTempRate(const std::size_t bufferSamples) {
    // Code to handle temporary rate change buttons.
    // We support two behaviors, the standard ramped pitch bending
    // and pitch shift stepping, which is the old behavior.

    RampDirection rampDirection = RampDirection::None;
    if (m_pButtonRateTempUp->toBool()) {
        rampDirection = RampDirection::Up;
    } else if (m_pButtonRateTempDown->toBool()) {
        rampDirection = RampDirection::Down;
    } else if (m_pButtonRateTempUpSmall->toBool()) {
        rampDirection = RampDirection::UpSmall;
    } else if (m_pButtonRateTempDownSmall->toBool()) {
        rampDirection = RampDirection::DownSmall;
    }

    if (rampDirection != RampDirection::None) {
        if (m_eRateRampMode == RampMode::Stepping) {
            if (!m_bTempStarted) {
                m_bTempStarted = true;
                // old temporary pitch shift behavior
                double change = m_dTemporaryRateChangeCoarse.getValue() / 100.0;
                double csmall = m_dTemporaryRateChangeFine.getValue() / 100.0;

                switch (rampDirection) {
                case RampDirection::Up:
                    setRateTemp(change);
                    break;
                case RampDirection::Down:
                    setRateTemp(-change);
                    break;
                case RampDirection::UpSmall:
                    setRateTemp(csmall);
                    break;
                case RampDirection::DownSmall:
                    setRateTemp(-csmall);
                    break;
                case RampDirection::None:
                default:
                    DEBUG_ASSERT(false);
                }
            }
        } else if (m_eRateRampMode == RampMode::Linear) {
            if (!m_bTempStarted) {
                m_bTempStarted = true;
                double latrate = bufferSamples / m_pSampleRate.get();
                m_dRateTempRampChange = latrate / (m_iRateRampSensitivity / 100.0);
            }

            switch (rampDirection) {
            case RampDirection::Up:
            case RampDirection::UpSmall:
                addRateTemp(m_dRateTempRampChange * m_pRateRange->get());
                break;
            case RampDirection::Down:
            case RampDirection::DownSmall:
                subRateTemp(m_dRateTempRampChange * m_pRateRange->get());
                break;
            case RampDirection::None:
            default:
                DEBUG_ASSERT(false);
            }
        }
    } else if (m_bTempStarted) {
        m_bTempStarted = false;
        resetRateTemp();
    }
}

double RateControl::getTempRate() {
    // qDebug() << m_tempRateRatio;
    return m_tempRateRatio;
}

void RateControl::setRateTemp(double v) {
    m_tempRateRatio = v;
    if (m_tempRateRatio < -1.0) {
        m_tempRateRatio = -1.0;
    } else if (m_tempRateRatio > 1.0) {
        m_tempRateRatio = 1.0;
    } else if (util_isnan(m_tempRateRatio)) {
        m_tempRateRatio = 0;
    }
}

void RateControl::addRateTemp(double v)
{
    setRateTemp(m_tempRateRatio + v);
}

void RateControl::subRateTemp(double v)
{
    setRateTemp(m_tempRateRatio - v);
}

void RateControl::resetRateTemp(void)
{
    setRateTemp(0.0);
}

bool RateControl::isReverseButtonPressed() {
    if (m_pReverseButton) {
        return m_pReverseButton->toBool();
    }
    return false;
}

void RateControl::notifyWrapAround(mixxx::audio::FramePos triggerPos,
        mixxx::audio::FramePos targetPos) {
    VERIFY_OR_DEBUG_ASSERT(triggerPos.isValid() && targetPos.isValid()) {
        m_wrapAroundCount = 0;
        // no need to reset the position, they're not used if count is 0.
        return;
    }
    m_wrapAroundCount++;
    m_jumpPos = triggerPos;
    m_targetPos = targetPos;
}

void RateControl::notifySeek(mixxx::audio::FramePos position) {
    m_pScratchController->notifySeek(position);
}

void RateControl::resetPositionScratchController() {
    // Resets the scratch state to avoid engine freeze due to insanley high rate
    // reported on track load while scratching.
    // https://github.com/mixxxdj/mixxx/issues/15082
    m_pScratchController->reset();
}
