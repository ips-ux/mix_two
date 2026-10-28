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
  resolution of 6480 in rekordbox's own tick units.
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
- Other thresholds, 42/3240 and 1215/3240 of a revolution, belong to the
  spin and loop-adjust paths.

### Mix Two's version (clean room)

Same behaviour, computed per audio buffer in the engine instead of per MIDI
message: on touch, remember the track position; every buffer, aim for that
position plus all the revolutions turned since (× 1.8 s), at most 8× normal
speed. Aiming at the position gives the position lock and the carry-over of
the speed limit exactly. Dead zone and glitch filter as above. Used when
Preferences > Decks > Platter options > Scratch algorithm is "Rekordbox";
"Mixxx" keeps Mixxx's own scratching.

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
   by the DDJ-800 jogs when the scratch algorithm is "Rekordbox". Still to
   trace and build: the spin modes (release while turning, backspins) and the
   bend amount. Verify on hardware that one platter turn moves 1.8 s.
3. ~~Rendering~~: built 2026-10-27 as `EngineBufferScaleHalfbandLagrange`,
   chosen in Preferences > Decks > Platter options > Scratch algorithm
   ("Rekordbox (6.8.5)", the default; "Mixxx (2.7)" keeps the linear scaler).
   Still to build: the motion rules. Original plan: build Mix Two's scratch scaler: a 2× half-band (Hann-47 windowed sinc,
   generated from the formula) plus 4-point cubic Lagrange resampler, plus the motion rules above in the
   engine (not the mapping script), selected for 33⅓ / 45 RPM.
4. Verify against rekordbox by rendering the same platter movement in both
   (scripted jog MIDI into each app, same track) and comparing the output.
