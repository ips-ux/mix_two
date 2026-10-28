#pragma once

#include <QObject>
#include <array>
#include <atomic>

#include "control/pollingcontrolproxy.h"
#include "engine/controls/enginecontrol.h"
#include "engine/sync/syncable.h"
#include "preferences/usersettings.h"

class BpmControl;
class Rotary;
class ControlTTRotary;
class ControlObject;
class ControlPotmeter;
class ControlPushButton;
class ControlProxy;
class PositionScratchController;

// RateControl is an EngineControl that is in charge of managing the rate of
// playback of a given channel of audio in the Mixxx engine. Using input from
// various controls, RateControl will calculate the current rate.
class RateControl : public EngineControl {
    Q_OBJECT
public:
  RateControl(const QString& group, UserSettingsPointer pConfig);

  // Enumerations which hold the state of the pitchbend buttons.
  // These enumerations can be used like a bitmask.
  enum class RampDirection {
      None,      // No buttons are held down
      Down,      // Down button is being held
      Up,        // Up button is being held
      DownSmall, // DownSmall button is being held
      UpSmall,   // UpSmall button is being held
  };

  enum class RampMode {
      Stepping = 0, // pitch takes a temporary step up/down a certain amount
      Linear = 1    // pitch moves up/down in a progressively linear fashion
  };

  void setBpmControl(BpmControl* bpmcontrol);

  // Returns the current engine rate.  "reportScratching" is used to tell
  // the caller that the user is currently scratching, and this is used to
  // disable keylock.
  double calculateSpeed(
          double baserate,
          double speed,
          bool paused,
          std::size_t samplesPerBuffer,
          bool* pReportScratching,
          bool* pReportReverse);

  // Set rate change when temp rate button is pressed
  static void setTemporaryRateChangeCoarseAmount(double v);
  static double getTemporaryRateChangeCoarseAmount();
  // Set rate change when temp rate small button is pressed
  static void setTemporaryRateChangeFineAmount(double v);
  static double getTemporaryRateChangeFineAmount();
  // Set rate change when perm rate button is pressed
  static void setPermanentRateChangeCoarseAmount(double v);
  static double getPermanentRateChangeCoarseAmount();
  // Set rate change when perm rate small button is pressed
  static void setPermanentRateChangeFineAmount(double v);
  static double getPermanentRateChangeFineAmount();
  // Set Rate Ramp Mode
  static void setRateRampMode(RampMode mode);
  static RampMode getRateRampMode();
  // Set Rate Ramp Sensitivity
  static void setRateRampSensitivity(int);
  static int getRateRampSensitivity();
  bool isReverseButtonPressed();
  // ReadAheadManager::getNextSamples() notifies us each time the play position
  // wrapped around during one buffer process (beatloop or track repeat) so
  // PositionScratchController can correctly interpret the sample position delta.
  void notifyWrapAround(mixxx::audio::FramePos triggerPos,
          mixxx::audio::FramePos targetPos);
  void notifySeek(mixxx::audio::FramePos position) override;
  void resetPositionScratchController();

public slots:
  void slotRateRangeChanged(double);
  void slotRateSliderChanged(double);
  void slotRateRatioChanged(double);
  void slotRatePhysicalChanged(double);
  void slotRateRangeNext(double);
  void slotReverseRollActivate(double);
  void slotControlRatePermDown(double);
  void slotControlRatePermDownSmall(double);
  void slotControlRatePermUp(double);
  void slotControlRatePermUpSmall(double);
  void slotControlFastForward(double);
  void slotControlFastBack(double);

private:
  void processTempRate(const size_t bufferSamples);
  double getJogFactor() const;
  double getWheelFactor() const;
  SyncMode getSyncMode() const;
  // Mix Two: the BPM the hardware tempo slider stands for, and whether the
  // deck plays at it.
  void updateTempoSlider();
  // Mix Two: the rekordbox-style platter (docs/ddj800-program/
  // SCRATCH-ENGINE.md, "Mix Two's version"). Returns the scratch rate while
  // the platter is touched.
  double calculatePlatterRate(double baserate, std::size_t samplesPerBuffer, double turn);
  // Starts a scratch's position lock and touch dead zone; the anchor is
  // taken in the next calculatePlatterRate().
  void startPlatterScratch();
  // Mix Two: runs the platter states for one buffer (SCRATCH-ENGINE.md
  // section 2.1). Returns true with *pRate set while the platter drives the
  // track (scratch or spin); otherwise the deck plays normally, with
  // m_platterBend applied.
  bool processPlatter(double baserate,
          double tempoRatio,
          bool paused,
          std::size_t samplesPerBuffer,
          double* pRate);
  // rekordbox's jog input stage (SCRATCH-ENGINE.md section 2.2): rebuilds
  // the 1 ms jog entries from the stamped turns, takes one buffer's worth and
  // applies the dead band. Returns the revolutions for this buffer; sets
  // *pJogFinished when the jog has been quiet for 56 ms.
  double takePlatterTurn(double bufferSeconds, bool touched, bool* pJogFinished);
  void clearPlatterQueue();
  // rekordbox's spin test: whether a spin at this buffer's speed goes on.
  bool platterSpinGoesOn(bool playing, double playRate, double speed);
  void setPlatterState(int state);

  // Set rate change of the temporary pitch rate
  void setRateTemp(double v);
  // Add a value to the temporary pitch rate
  void addRateTemp(double v);
  // Subtract a value from the temporary pitch rate
  void subRateTemp(double v);
  // Reset the temporary pitch rate
  void resetRateTemp(void);
  // Get the 'Raw' Temp Rate
  double getTempRate(void);

  // For Sync Lock
  BpmControl* m_pBpmControl;

  PollingControlProxy m_pSampleRate;
  std::unique_ptr<ControlObject> m_pRateRatio;
  std::unique_ptr<ControlObject> m_pRateDir;
  std::unique_ptr<ControlObject> m_pRateRange;
  std::unique_ptr<ControlPotmeter> m_pRateSlider;
  std::unique_ptr<ControlPotmeter> m_pRateSearch;
  // Mix Two: position of the controller's tempo slider, -1..1 like "rate".
  // It can differ from "rate" while SYNC holds the tempo or before the
  // slider has caught up (soft takeover).
  std::unique_ptr<ControlObject> m_pRatePhysical;
  // Mix Two: steps through the rekordbox ranges (±6, ±10, ±16 %, WIDE).
  std::unique_ptr<ControlPushButton> m_pRateRangeNext;
  std::unique_ptr<ControlObject> m_pTempoSliderBpm;
  std::unique_ptr<ControlObject> m_pTempoSliderMatched;
  std::unique_ptr<ControlProxy> m_pLocalBpm;
  bool m_ratePhysicalKnown;
  // [ChannelN],platter_touch: the platter is touched (scratching).
  std::unique_ptr<ControlPushButton> m_pPlatterTouch;
  // [ChannelN],platter_turn: each set adds that many revolutions turned
  // (negative backwards). The value itself stays 0.
  std::unique_ptr<ControlObject> m_pPlatterTurn;
  // Turns by arrival millisecond (any thread adds, the engine takes them as
  // rekordbox's 1 ms ticks would).
  static constexpr int kPlatterTurnBins = 256; // as platterTurnBin()
  std::array<std::atomic<double>, kPlatterTurnBins> m_platterTurnBins;
  // [ChannelN],platter_state (read-only): 0 idle, 1 scratch, 2 spin,
  // 3 disable, 4 bend.
  std::unique_ptr<ControlObject> m_pPlatterState;
  int m_platterState;
  // The jog input stage: a queue of 1 ms entries (no allocations).
  struct PlatterEntry {
      double turn; // revolutions
      double ms;   // how long the entry lasts
  };
  static constexpr int kPlatterQueueSize = 100;
  std::array<PlatterEntry, kPlatterQueueSize> m_platterQueue;
  int m_platterQueueHead;
  int m_platterQueueCount;
  qint64 m_platterLastTickMs;      // last 1 ms tick taken; -1 before the first
  bool m_platterJogActive;         // ticks are queued (until 56 ms quiet)
  int m_platterLastEntryDirection; // of the last queued entry; 0 none
  double m_platterCarry;           // dead band: revolutions held back
  bool m_platterPassedLast;        // the previous buffer passed the dead band
  double m_platterLastPassClock;   // m_platterClock when steps last passed
  double m_platterClock;           // seconds of audio processed
  double m_platterSpeed;           // speed of the last buffer that moved
  int m_platterDirection;          // direction of the last movement (+1 / -1)
  bool m_platterReversed;          // the last movement reversed direction
  int m_platterSpinDirection;
  int m_platterSlowCount;        // spin: buffers slower than play, in a row
  int m_platterSpinEmpty;        // spin: buffers without movement, in a row
  double m_platterAgainstSpin;   // disable: revolutions against the spin
  bool m_platterPlayingAtTouch;
  bool m_platterUntouchedScratch; // the jog on a paused deck, not a touch
  double m_platterBend;          // fraction of the tempo added by a bend
  double m_platterBendClock;     // m_platterClock at the last bend update
  bool m_platterActive;
  bool m_platterDeadZone;
  // The renderer eases into the stop over the first touched buffer; the
  // anchor is taken after that, where the track came to rest.
  bool m_platterAnchorPending;
  double m_platterAnchor;     // track frames at touch
  double m_platterTurned;     // revolutions applied since touch
  double m_platterDeadTurn;   // revolutions discarded in the dead zone
  double m_platterSinceTouch; // seconds since touch

  std::unique_ptr<ControlPushButton> m_pButtonRateTempDown;
  std::unique_ptr<ControlPushButton> m_pButtonRateTempDownSmall;
  std::unique_ptr<ControlPushButton> m_pButtonRateTempUp;
  std::unique_ptr<ControlPushButton> m_pButtonRateTempUpSmall;

  std::unique_ptr<ControlPushButton> m_pButtonRatePermDown;
  std::unique_ptr<ControlPushButton> m_pButtonRatePermDownSmall;
  std::unique_ptr<ControlPushButton> m_pButtonRatePermUp;
  std::unique_ptr<ControlPushButton> m_pButtonRatePermUpSmall;

  std::unique_ptr<ControlPushButton> m_pReverseButton;
  std::unique_ptr<ControlPushButton> m_pReverseRollButton;
  std::unique_ptr<ControlObject> m_pForwardButton;
  std::unique_ptr<ControlObject> m_pBackButton;

  std::unique_ptr<ControlTTRotary> m_pWheel;
  std::unique_ptr<ControlObject> m_pScratch2;
  std::unique_ptr<ControlPushButton> m_pScratch2Enable;
  std::unique_ptr<ControlObject> m_pScratch2Scratching;

  std::unique_ptr<PositionScratchController> m_pScratchController;

  std::unique_ptr<ControlObject> m_pJog;
  std::unique_ptr<Rotary> m_pJogFilter;

  ControlObject* m_pVCEnabled;
  ControlObject* m_pVCScratching;
  ControlObject* m_pVCMode;

  PollingControlProxy m_syncMode;
  PollingControlProxy m_slipEnabled;

  // Values used when temp and perm rate buttons are pressed
  static ControlValueAtomic<double> m_dTemporaryRateChangeCoarse;
  static ControlValueAtomic<double> m_dTemporaryRateChangeFine;
  static ControlValueAtomic<double> m_dPermanentRateChangeCoarse;
  static ControlValueAtomic<double> m_dPermanentRateChangeFine;

  int m_wrapAroundCount;
  mixxx::audio::FramePos m_jumpPos;
  mixxx::audio::FramePos m_targetPos;

  // This is true if we've already started to ramp the rate
  bool m_bTempStarted;
  // Set the Temporary Rate Change Mode
  static RampMode m_eRateRampMode;
  // The Rate Temp Sensitivity, the higher it is the slower it gets
  static int m_iRateRampSensitivity;
  // Factor applied to the deprecated "wheel" rate value.
  static const double kWheelMultiplier;
  // Factor applied to jogwheels when the track is paused to speed up seeking.
  static const double kPausedJogMultiplier;
  // Temporary pitchrate, added to the permanent rate for calculateRate
  double m_tempRateRatio;
  // Speed for temporary rate change
  double m_dRateTempRampChange;
};
