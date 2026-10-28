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
- **Speed from the platter:** per jog message, speed = revolutions turned ÷
  elapsed minutes ÷ 33⅓. 1.0 is normal playback; one revolution is 1.8 s of
  track. (45 exists in the code; Pioneer jogs use 33⅓, and Mix Two has no
  setting for it.)
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

- **Release.** When the hand lets go, rekordbox looks at the platter speed
  of the last jog message. If the platter is still turning (any speed above
  zero) and that message did not reverse direction, the deck goes into
  **spin**; otherwise the scratch stops.
- **Spin.** The track plays at the speed of the free-running platter (speed
  limit ±8×; anything implying more than **50×** is dropped as a glitch).
  There is no position lock: the platter's speed drives the track. Spin ends
  when:
  - the platter stops (a jog update arrives with no movement),
  - the platter changes direction, or
  - the deck is playing in the platter's direction and the platter has been
    slower than the deck's play speed (tempo included) for **3** jog
    messages in a row. The count resets when the platter is faster again.
    This is how a forward spin hands back to normal play.
  The 3-message count is set for every controller (`SetSpinCount(3)`), so
  the platter's speed is passed on even while it is counting.
- **Disable.** After a spin ends (and after a release without spin), the
  platter usually keeps coasting. If the deck was playing when it was
  touched, rekordbox ignores the platter until either it is pushed **against
  the spin direction by more than 1215/3240 of a revolution** (0.375 rev), or
  the jog goes quiet. A deck that was paused at the touch goes straight back
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
- **"The jog goes quiet"** means no movement for **50 ms**. At the DDJ-800's
  720 steps per revolution that is one step slower than 0.05×, the same
  floor as the touch dead zone. It ends a spin (the platter has stopped), a
  disable phase, and decides at release whether the platter still turns.
- Spin renders like scratching (key lock off, the scratch scaler).
- Bend adds (bend ÷ 100) × the deck's tempo to the play rate, as a user
  tweak, so a synced deck accepts it like any other nudge.
- Deliberately the same as rekordbox: the one-sided +42 % cap.

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
   mapping's old 720, which was 9× off). Next: find the 0.1–0.2 s (dead
   zone, ±8× limit, spin hand-back) with more captures of single moves.
