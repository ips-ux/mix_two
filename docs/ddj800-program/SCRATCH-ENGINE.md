# rekordbox 6 scratch engine: behavioural spec (clean room)

> AI-generated (Claude Code), 2026-10-27. Written from static analysis of
> rekordbox 6.8.5 (`rekordbox.exe`, SHA-256 31898b14…dae0f0) with Ghidra 12.1.4.
> This file describes **behaviour and parameters only**, in our own words.
> No decompiled code is copied here or into Mix Two (ground rule: clean room).
> The Ghidra project and dumps live in `build/re/` and must never ship.
> End of AI-generated notice.

## The chain

| Stage | rekordbox class | Job |
|---|---|---|
| 1. Platter input | `djplay::MidiJogTouch` | MIDI jog touch and rotation messages from the controller. |
| 2. Motion | `DjController::ScratchProcessor`, `ScratchProcessor3` (`ScratchProcessorType` picks one) | Turns platter ticks into a playback speed, per jog message. |
| 3. Transport | `rbxfrm::StretchBehavior` (`reqScratch` / `doScratch`, `reqStopScratch`, bend and vinyl-speed tasks) | Switches the player between normal play and scratch, with a short fade task on the switch. |
| 4. Rendering | `rbxfrm::f2dsp::TimeStretchScratch` | Reads the track at the commanded speed and renders audio. |

## 2. Motion (ScratchProcessor)

Traced fully for the DDJ-800 (2026-10-27).

- **Which processor:** rekordbox special-cases the DDJ-800 by name: processor
  type 1, the plain `ScratchProcessor` **without** a feedback filter
  (`Integral2` / `Integral3` smoothing is for other controllers), and a jog
  resolution of 6480 in rekordbox's own tick units. These are the
  DDJ-800's MIDI jog ticks: a USB capture of the deck in rekordbox
  (2026-10-27) showed 1.8 s of track per about 6480 ticks.
- **Speed from the platter:** per **audio buffer**, not per jog message
  (corrected 2026-10-28, section 2.2): speed = revolutions handed to the
  motion stage for this buffer ÷ buffer length in minutes ÷ 33⅓. 1.0 is
  normal playback; one revolution is 1.8 s of track. (45 exists in the code;
  Pioneer jogs use 33⅓, and Mix Two has no setting for it.)
- **Modes:** idle, scratch (platter touched), two spin modes (released while
  turning) and bend (not touched: rate = 1 + 0.01 × bend amount).
- **Touch dead zone ("JogTouchAdjust"):** for the first **60 ms** after a
  touch, motion is discarded until it adds up to **9/3240 of a revolution**
  (about 1°) and the speed is at least **0.05×**. The hand landing on the
  platter does not move the track.
- **Glitch filter:** a message implying more than **25×** (scratch) or
  **50×** (spin) is dropped.
- **Speed limit:** ±**8×**; whatever the limit cuts off is carried into the
  next message, so no movement is lost.
- **Position lock:** each message adds the difference between the speed it
  asked for last time and the speed the player actually achieved (averaged
  over the player's blocks since then). The audio stays locked to where the
  platter is, not only to how fast it turns.
- The threshold 42/3240 of a revolution belongs to the bend entry and
  1215/3240 to the end of the disable phase (section 2.1 below).

### Mix Two's version (clean room)

Same behaviour, computed per audio buffer in the engine instead of per MIDI
message: on touch, remember the track position; every buffer, aim for that
position plus all the revolutions turned since (× 1.8 s), at most 8× normal
speed. Aiming at the position gives the position lock and the carry-over of
the speed limit exactly. Dead zone and glitch filter as above. Used when
Preferences > Decks > Platter options > Scratch algorithm is "Rekordbox";
"Mixxx" keeps Mixxx's own scratching.

## 2.1 Release, spin, disable and bend

Traced for the DDJ-800 (2026-10-27, same Ghidra project). The motion stage
has five states: **idle**, **scratch** (touched), **spin**, **disable** and
**bend**.

(2026-10-28: every "jog message" or "update" below is in fact **one audio
buffer** of the input stage in section 2.2. The motion stage never sees
individual DDJ-800 messages.)

- **Release.** When the hand lets go, rekordbox looks at the platter speed
  of the last buffer (which, because of the input delay in 2.2, shows the
  platter as it was one to two buffers **before** the release). If that
  speed is not zero and that buffer did not reverse direction, and the
  slow check below does not already fail, the deck goes into **spin**;
  otherwise the scratch stops. The release itself counts as the first slow
  check.
- **Spin.** The track plays at the speed of the free-running platter (speed
  limit ±8×; anything implying more than **50×** is dropped as a glitch).
  There is no position lock: the platter's speed drives the track. Spin ends
  when:
  - the platter stops: a buffer with **no** movement first repeats the
    previous buffer's speed once; a **second** empty buffer in a row ends
    the spin (zero speed),
  - the platter changes direction, or
  - the deck is playing in the platter's direction and the platter has been
    slower than the deck's play speed for **3** buffers in a row (the third
    slow buffer ends it). Play speed = 1 + the deck's tempo change (tempo
    included, sign from the play direction). The count resets when the
    platter is faster again. When platter and deck turn in opposite
    directions (every backspin of a playing deck) there is no slow check at
    all. This is how a forward spin hands back to normal play.
  - the input stage goes quiet for 56 ms (section 2.2), or the platter is
    touched.
  The 3 count is set for every controller (`SetSpinCount(3)`), so the
  platter's speed is passed on even while it is counting. (With a count
  above 3, slow buffers would not drive the track.)
- **Disable.** After a spin ends (and after a release without spin), the
  platter usually keeps coasting. If the deck was playing when it was
  touched, rekordbox ignores the platter until either it is pushed **against
  the spin direction by more than 1215/3240 of a revolution** (0.375 rev), or
  the jog goes quiet (56 ms, section 2.2). A deck that was paused at the touch goes straight back
  to idle. While disabled the deck plays normally.
- **Bend** (platter not touched). From idle, a platter faster than **0.5×**
  (or more than 42/3240 of a revolution within 60 ms) starts a bend. The
  bend amount is 1.5 ÷ (1 − 0.5) = **3 % per 1× of platter speed**, capped
  at **+42 %** (the cap is one-sided in rekordbox), and is only updated
  while the platter is faster than 0.5×. The player plays at
  (1 + bend ÷ 100) × its normal speed. The bend ends **56 ms** after the
  last update. Touching the platter cancels a bend.
- **Constants** (DDJ-800, rekordbox defaults): jog resolution 6480 units;
  scratch glitch 25×, spin glitch 50×, speed limit 8×, touch dead zone 60 ms
  / 9/3240 rev / 0.05×, bend threshold 0.5×, bend gain 1.5, bend cap 42,
  bend timeout 56 ms, spin count 3, disable-end 1215/3240 rev.

### Mix Two's version (clean room)

The engine keeps the same five states per deck, exposed read-only as
`[ChannelN],platter_state` (0 idle, 1 scratch, 2 spin, 3 disable, 4 bend).
With the "Rekordbox" scratch algorithm the DDJ-800 mapping sends every jog
turn to `platter_turn` while the deck plays, is touched, or is not idle; a
paused, idle deck keeps Mixxx's jog search.

- **Platter speed** is measured over the last 20 ms of buffers (turns ÷
  time), because Mix Two sees the jog per audio buffer, not per MIDI
  message. Rules that rekordbox applies per jog message are applied per
  buffer that received movement.
- **"The jog goes quiet"** means no movement for **5.6 ms**: at the
  DDJ-800's 6480 steps per revolution, one step slower than 0.05×, the same
  floor as the touch dead zone. (It was 50 ms while the mapping assumed 720
  steps.) The DDJ-800 never sends a "no movement" message, so this timer is
  the only way to see that the platter stopped. It ends a spin, a disable
  phase, and decides at release whether the platter still turns.
- **Spin follows the platter's steps.** The spin is driven by the platter
  speed, which is measured over 20 ms and so runs 10 ms late. On its own it
  replayed the last 10 ms before the release and overshot every spin (about
  60 ms of track after a fast backspin). The spin therefore also keeps the
  position the platter's steps reach, and corrects towards it over the same
  20 ms, landing on it at the end of each buffer as the scratch does.
  (2026-10-28: rekordbox does not measure per jog message either; it hands
  the motion stage exact per-buffer counts from a queue, section 2.2.)
- Spin renders like scratching (key lock off, the scratch scaler).
- Bend adds (bend ÷ 100) × the deck's tempo to the play rate, as a user
  tweak, so a synced deck accepts it like any other nudge.
- Deliberately the same as rekordbox: the one-sided +42 % cap.

## 2.2 Jog input timing: from the deck to the motion stage

> AI-generated (Claude Code), 2026-10-28. Static analysis of rekordbox 6.8.5
> (same binary and Ghidra project as above; REA evidence
> `ev_a50335a0d496c5ba5f953d5798f8fd7b2ca00917686e2db178d8041dd87b69de`
> records the binary's identity and its WINMM imports `timeGetTime`,
> `timeSetEvent`, `midiInOpen`; the code paths were decompiled with the
> existing headless Ghidra project, outputs in `build/re/out/timing/`).
> Behaviour in our own words; no code or tables copied. **Observed** = read
> in the code; **Inferred** = deduced. End of AI-generated notice.

This section answers why rekordbox, compared with summing the jog steps,
(a) runs a backspin ~37 ms past the platter's last step, (b) does not spin
at all after a slow creeping release, and (c) seemed to hand a forward spin
back while the platter still turned at ~1.8×.

### Which code handles the DDJ-800 jog (Observed)

- rekordbox does **not** use a separate DDJ-800 jog class. The DDJ-800 is
  run by the DDJ-1000 HID map (`djplay::HidMapDDJ1000`), special-cased by
  name: jog resolution 6480, scratch processor type 1. Type 1 builds the
  chain **AudioCallbackSumTimerProcessor → ScratchProcessor** (no feedback
  filter). Only the DDJ-1000/DDJ-800 and DDJ-FLX10 maps build this
  "sum timer" input stage. (`djplay::MidiMapDDJ800` only handles indicators
  such as the jog display; the CSV's jog rows are commented out, F6.)
- That map reads the jogs from the deck's **HID input report**: byte 2
  carries touch and "moved / forward" flags per side, bytes 5–6 and 7–8 the
  **absolute 16-bit jog counters**; the step count is the counter
  difference, the sign comes from the forward flag, the left side is deck 1
  or 3 and the right deck 2 or 4 by a status bit in byte 4. This is exactly
  the report layout found in the firmware (FIRMWARE.md §2.6). So rekordbox
  is not limited by the MIDI ±63 clamp. (Inferred: rekordbox switches the
  deck into the mode that sends this report; how it does that is unknown.
  The USB capture parser only decoded MIDI, so it would not have shown the
  HID reports.)
- The generic MIDI jog item (`djplay::MidiJogRotate`) passes each message's
  value − 0x40 with the message's time stamp (JUCE's WinMM input: the
  driver's millisecond stamp plus the start time). The sum-timer stage
  **ignores that time stamp**, so for the DDJ-800 the result is the same
  whether the steps come over HID or MIDI.

### The input stage, step by step (Observed)

1. **Arrival:** each report or message adds its steps to a per-deck
   accumulator and marks the jog *active*. Touch on/off is only latched.
2. **1 ms tick:** the HID map runs a multimedia timer (`timeSetEvent`,
   period 1 ms, periodic) that ticks all four decks. While a deck's jog is
   active, each tick takes the accumulated steps (even zero) and appends
   one entry (steps, duration) to a 100-entry queue:
   - duration = milliseconds since the previous tick, from a 1 ms wall
     clock (`_ftime` over `GetSystemTimePreciseAsFileTime`, truncated to
     whole ms), at least **1 ms** and at most **2 × the audio buffer length
     rounded up to whole ms**;
   - if the entry's direction differs from the previous entry's, the
     duration is **0.5 ms** instead. Zero steps count as "forward", so for a
     platter turning backwards with gaps every switch between an empty tick
     and a backward tick is shortened to 0.5 ms (rekordbox's own quirk);
   - the first tick after the jog was inactive measures the time since the
     last active tick, so it is clamped to the maximum: the first entry of
     every movement lasts **2 audio buffers**.
3. **Audio callback** (buffer length D ms):
   - applies a pending touch-on first;
   - takes exactly D ms worth of queue entries; each entry's steps are
     spread evenly over its duration, and a partly used entry keeps the
     rest for the next buffer;
   - **dead band:** the buffer's steps (plus any carried steps) are passed
     on only if they add up to at least **2 steps** while touched, **3
     steps** while not touched, or if the previous buffer passed that
     much; otherwise they are carried to the next buffer and the motion
     stage gets 0;
   - calls the motion stage with (steps, D), so speed = steps ÷ D;
   - applies a pending touch-off **after** that, so the release decision
     uses this buffer's speed;
   - if no buffer has passed any steps for more than **56 ms** (wall clock),
     it tells the motion stage the jog has finished (ends a scratch that
     needs it, ends **disable** → idle), empties the queue, drops the
     carry and marks the jog inactive.
4. The motion stage (sections 2 and 2.1) runs once per buffer with those
   steps. Its own periodic tick has no spin timeout; the 56 ms above is the
   only time-out.

Consequences:

- **Input delay.** Because the first entry lasts two buffers and the
  queue is drained at exactly real time, the platter's motion reaches the
  track about **1–2 audio buffers late** for the whole movement (it only
  resets after the 56 ms quiet). Touch is not delayed. Steps are never lost
  (except ≤ 2 carried steps at the 56 ms reset).
- **Per-buffer quantisation.** Every rule of 2.1 sees buffers, not
  messages.

### (a) Backspin runs ~37 ms past the last step (mechanism Observed, numbers Inferred)

After the platter's last step the queue still holds the last 1–2 buffers
of motion, which play out first (the backspin "keeps following the
platter"). Then the first empty buffer repeats the previous speed and the
second empty buffer ends the spin. Run-on ≈ delay (≈ 2 buffers, less the
part of the first buffer already consumed) + 2 buffers. A backspin of a
playing deck never meets the slow check (opposite directions), so only the
"two empty buffers" rule ends it. With a ~10–11 ms buffer (e.g. 512
samples at 48 kHz) this gives the measured order: the forward spin's
offset (+21 ms, below) is the delay alone, the backspin's (+41 ms) the
delay plus the two buffers. rekordbox's buffer size during the capture is
unknown.

### (b) Slow creeping release: no spin (mechanism Observed, case Inferred)

The release decision uses the speed of the buffer processed just before
the touch-off, and that buffer shows the platter **1–2 buffers before the
release**, still under the hand. A hand that rests on the platter before
letting go gives 0 there (or less than the 2-step dead band), so there is
no spin and normal play resumes at once. The creep that starts after the
release only reaches the motion stage when the deck is already in disable
(ignored until 56 ms after it stops, or 0.375 rev against the spin). Even
if a creep was already under way, steps below the dead band per buffer read
as 0, and two such buffers end a spin.

### (c) The "hand-back at ~1.8×" is the input delay (Inferred)

The slow comparison is |platter speed of the buffer| < |1 + tempo change|,
three buffers in a row (Observed; the play speed is read from the
transport, base tempo diff + 1). There is no higher threshold. While
spinning, the track follows the platter **delayed** by the input delay;
when the spin hands back, the last 1–2 buffers of platter motion are still
queued and are ignored by the disable state, and normal play takes over.
Compared with a no-delay model the deck therefore lands one delay
(≈ 2 buffers ≈ 21 ms) behind, which the earlier analysis read as an early
hand-back at ~1.8×. Same cause as most of (a).

### Other jog input variants (Observed, not used by the DDJ-800)

- `AudioCallbackTimeStampProcessor` (1–5): the same queue, but each entry's
  duration is the gap between the controllers' own message time stamps
  (default 1 ms, gaps over 2.1 ms replaced by 1 ms); used by most other
  HID maps.
- `AudioCallbackProcessor` (CDJ-3000, below): each report counts as a fixed
  3 ms; no dead band.

### What this means for Mix Two (src/engine/controls/ratecontrol.cpp)

Mix Two already runs per audio buffer, so it can copy rekordbox's model
directly instead of approximating per-message timing. Intended behaviour:

1. **Jog queue instead of the speed window.** Remove
   `kPlatterSpeedWindowSeconds` (20 ms) and the spin's position correction
   towards `m_platterSpinTarget`. Time-stamp `platter_turn` arrivals with a
   1 ms clock and build 1 ms entries (steps, duration) as in step 2 above:
   duration = ms since the previous entry, clamped to [1 ms, 2 × ceil(buffer
   ms)], 0.5 ms when the direction differs from the previous entry (zero
   counts as forward), first entry after inactivity = the maximum, at most
   100 entries. Each buffer takes exactly its own length of queue time;
   speed = steps ÷ buffer length. (Mix Two's buffer length sets the delay,
   as rekordbox's does; use the same buffer size as the owner's rekordbox
   setting to match it.)
2. **Dead band** (new constants): pass a buffer's steps (with carry) only
   if ≥ **2** steps touched / ≥ **3** untouched, or if the previous buffer
   passed ≥ that; otherwise carry them and pass 0.
3. **Spin start in `processPlatter`:** decide on the current buffer's
   output, processed before the touch-off (apply touch-on before and
   touch-off after the buffer's steps): spin if its speed ≠ 0, it did not
   reverse direction, and the slow check (which counts the release as its
   first slow buffer) does not fail. Do not use "is the platter still
   moving now" (`kPlatterQuietSeconds`).
4. **Spin end:** an empty buffer repeats the previous buffer's speed once;
   a second empty buffer in a row ends the spin. `kPlatterSpinSlowCount`
   stays **3** but counts buffers; no slow check when platter and play
   directions differ; play speed = 1 + tempo change.
5. **Quiet:** `kPlatterQuietSeconds` becomes **0.056 s** of no passed
   steps (wall clock or buffer clock). It ends disable (→ idle), empties
   the queue, drops the carry and makes the next movement start a new
   delay. It no longer ends spins (rule 4 does) or decides the release
   (rule 3).
6. Keep the glitch limits (25× / 50×) and the ±8× limit on the per-buffer
   speed.
7. Optional, later: read the jogs from the DDJ-800 HID report (absolute
   counters, no ±63 clamp) as rekordbox does; needs the unknown switch that
   makes the deck send it.

## 2.3 CDJ-3000 over HID, vinyl speed adjust, Master Tempo (2026-10-28)

> AI-generated (Claude Code), 2026-10-28, same method and rules as 2.2.
> End of AI-generated notice.

- **`djplay::HidMapCDJ3000`** (Observed): jog from the HID input report, a
  16-bit absolute counter (difference between reports, with a direction
  bit and a "valid" bit), touch from a status bit. Resolution **9720** steps
  per revolution. Input stage `AudioCallbackProcessor`: every report counts
  as a fixed **3 ms** of motion, queued and drained per audio buffer as in
  2.2, **no dead band**, same 56 ms quiet reset. Then the same
  `ScratchProcessor` with the same rules and constants; spin count 3, or 5
  together with a count smoother when a per-player flag is set (flag meaning
  unknown). So a CDJ-3000 in rekordbox has the same release/spin/hand-back
  rules, with a finer jog and no dead band. (The player's own firmware stays
  unknown: FIRMWARE.md §4.)
- **Vinyl speed adjust (touch/brake, release/start)** (Observed:
  `rbxfrm::VinylSpeedAdjustor`, `StretchBehavior::SpeedAdjustTask`): per
  audio block the deck's speed ramps **linearly**, start: + a × N / sample
  rate × 1000, until it reaches the play speed; brake: − b × the same, until
  it reaches 0; a and b are in **speed per millisecond** (rekordbox names it
  "VinylSpeedPerMs"), default 1.0 per ms (practically instant), otherwise
  taken from a table chosen by the preference (`VinylAdjustBrake.Left/Right`
  commands, "Touch/Brake", "ReleaseStart"). The table values were not
  found (unknown; reading them from the running process would settle it).
  The release-FX "VINYL BRAKE" (`BeatFxVinylBrake`, `VinylBrakeCore`) is a
  separate effect, not traced.
- **Master Tempo**: rekordbox's own `rbxfrm::f2dsp::TimeStretch` /
  `TimeStretchManager` (with a "simple key lock" mode and several master
  tempo types) and `PitchTimeStretchFGPR`; no third-party time-stretch
  library name appears in the binary or its DLLs (élastique, Rubber Band,
  SoundTouch searched). Algorithm family not identified (not traced deeper,
  as agreed).

## 4. Rendering (TimeStretchScratch)

Fully specified (static analysis plus a read of the running process,
2026-10-27). In short: **2× oversampling with a half-band filter, then 4-point
cubic Lagrange interpolation** on the oversampled signal.

1. **Half-band filter.** The track is treated as if upsampled by 2: the
   original samples stay on the even points, and each in-between point is
   computed by a half-band FIR. The filter is a textbook design and Mix Two
   generates its own copy from the formula:
   - ideal half-band impulse response h(k) = 0.5 · sinc(k / 2),
   - multiplied by a **Hann window of 47 points** centred on k = 0,
   - so k = −23…+23, of which the even k ≠ 0 are zero and the end points
     vanish: **43 taps, 11 non-zero coefficients per side**, centre 0.5.
   - Check figures for our own design: passband flat within 0.03 dB to
     0.8 × the original Nyquist (17.6 kHz at 44.1 kHz), −6 dB exactly at the
     original Nyquist, stopband −48 dB or better from 1.2 × the original
     Nyquist. (Our generated taps agree with rekordbox's to float precision,
     2.5e-8.)
2. **Cubic Lagrange interpolation.** The read position, measured in
   half-samples (t from 0 to 2 across one original sample), is interpolated
   from four consecutive points of the oversampled signal: the original
   sample before, the midpoint, the next original sample and the next
   midpoint, with the standard 4-point Lagrange weights
   −(t³ − 3t² + 2t)/6, (t³ − 2t² − t + 2)/2, −(t³ − t² − 2t)/2, (t³ − t)/6.
   The constants seen in the running process are 1/6, 0.5, 2 and −1.
3. **SIMD layout:** four lanes at a time (the coefficients are identical in
   every lane), with separate loops for forward and reverse play.
4. **Read position smoothing:** jumps in the read position are eased with a
   1/16 step per block. A rate within 0.0001 of 1.0 is treated as exactly 1.0.
5. **Not yet traced:** whether very fast movement (well above 2×) switches to
   another path (two further loops in the same routine), and the gain
   normalisation between the original and the interpolated points.

## What Mix Two does today, for comparison

- Speed from the jog with an alpha-beta filter in the mapping script
  (`engine.scratchEnable`), no dead zone, no position feedback.
- Rendering with `EngineBufferScaleLinear`: 2-point linear interpolation, which
  aliases audibly at scratch speeds. Key lock is dropped while scratching.

## Next steps

1. ~~Filter and polynomial~~: done, see section 4.
2. ~~Motion~~: traced (section 2) and built 2026-10-27 in `RateControl`
   (`calculatePlatterRate`, controls `platter_touch` / `platter_turn`), driven
   by the DDJ-800 jogs when the scratch algorithm is "Rekordbox". Spin,
   disable and bend traced and built 2026-10-27 (section 2.1,
   `[ChannelN],platter_state`). Jog resolution measured 2026-10-27: 6480
   ticks per turn (item 4; the mapping default was 720). Still to feel on
   the deck: scratching and spins against rekordbox.
3. ~~Rendering~~: built 2026-10-27 as `EngineBufferScaleHalfbandLagrange`,
   chosen in Preferences > Decks > Platter options > Scratch algorithm
   ("Rekordbox (6.8.5)", the default; "Mixxx (2.7)" keeps the linear scaler).
   The motion rules are built too (item 2). Original plan: build Mix Two's scratch scaler: a 2× half-band (Hann-47 windowed sinc,
   generated from the formula) plus 4-point cubic Lagrange resampler, plus the motion rules above in the
   engine (not the mapping script), selected for 33⅓ / 45 RPM.
4. Verify against rekordbox by rendering the same platter movement in both
   (scripted jog MIDI into each app, same track) and comparing the output.
   Mix Two side built 2026-10-27: `tools/soundparity/render_scratch.py`
   writes the jog scripts (baby scratch, forward/back, release spin,
   backspin, paused spin, bend) as DDJ-800 MIDI and renders them through
   the real engine (`PlatterRenderTest`) with the owner's real track
   `.resource/jvckpot.mp3`. A cross-correlation check of the renders put
   the track within 50 ms of the position worked out by hand from each
   script. rekordbox side done 2026-10-27 by USB capture
   (`usbpcap_to_jog.py`, `scratch_trace.py`, README there): the same hand
   movement, rendered in Mix Two, matches rekordbox to 10 ms in normal play
   and early scratching, and lands 0.1–0.2 s short of rekordbox after each
   move (-122 ms after a scratch, -215 ms after a backspin, +66 ms after a
   forward spin). The capture also gave the jog resolution (6480, not the
   mapping's old 720, which was 9× off).
   Then found and fixed (2026-10-27): the rekordbox scaler left its
   read-ahead in the engine's read log at every change of direction, so
   after a scratch the hand-back played the leftovers out as the track going
   backwards for ~70 ms; the spin replayed the last 10 ms before the release
   (speed window lag); and the "quiet" time was still 50 ms from the 720
   assumption. Now: **-1 ms** after the scratch, **+41 ms** after the
   backspin, **+21 ms** after the forward spin. Explained 2026-10-28
   (section 2.2): rekordbox's jog input is delayed by about two audio
   buffers and its spin ends two empty buffers after the queue runs dry;
   the release decision sees the platter as it was under the hand. To
   do: build the queue model (2.2, "What this means for Mix Two") and
   re-render the captured moves with it.
5. Firmware check (2026-10-27, [FIRMWARE.md](FIRMWARE.md) §2.2, §3, §6): the
   DDJ-800 firmware sends the raw ×4 encoder count (6480 per turn confirmed),
   at most one message per jog per 1 ms frame, clamped to ±63 steps, and
   **nothing** when the count did not change. rekordbox trace done
   2026-10-28 (section 2.2): rekordbox reads the DDJ-800 jogs from the HID
   report (absolute counters), sums them per 1 ms timer tick, queues them,
   and drains the queue per audio buffer into the ScratchProcessor; message
   time stamps are ignored, the only time-out is 56 ms without movement,
   and a spin ends after two empty buffers. The bursty-MIDI hypothesis is
   withdrawn. Next: (1) implement the queue model in `ratecontrol.cpp`
   (constants listed in 2.2); (2) re-render the 2026-10-27 capture with it
   at 256/512/1024-sample buffers and see which buffer reproduces +41 ms /
   +21 ms (and ask the owner which buffer size rekordbox was set to);
   (3) a USB capture that also decodes HID IN (EP 0x87) and OUT (EP 0x06)
   reports, to see the jog report and the switch that enables it. The
   CDJ-3000 update is encrypted and was not used.
