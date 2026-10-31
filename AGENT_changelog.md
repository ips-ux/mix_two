<!-- AI-generated (Claude Code, 2026-10-27). Review before relying on it. -->

# AGENT_changelog: Mix Two

> **AI-generated:** Claude Code wrote this file from the git history and the
> program docs in `docs/ddj800-program/`. It is the change record for AI
> agents picking up work in this fork. The end-user version, in ASD-STE100
> Simplified Technical English, is [USER_changelog.md](USER_changelog.md).

## How to use and maintain this file

- **Read first** on a new task: [Working agreements](#working-agreements),
  [DECISIONS.md](docs/ddj800-program/DECISIONS.md) (the owner's rules), then
  the newest entry. Per-feature status lives in the bucket files
  ([BUCKETS.md](docs/ddj800-program/BUCKETS.md)); update the bucket row when
  you change a feature, then run `python tools/buckets_summary.py`.
- **Add an entry** for every change you commit: newest first, with the date,
  commit, and for each item the surface (controls, config keys, UI strings),
  files, tests, and a **Status** from the ladder in DECISIONS.md §3
  (`Built`, `Checked` with evidence, or `docs only`). Never write
  `Deck-verified` or `CDJ-verified` yourself: only the owner sets those.
  Record what you did not verify.
- Mirror user-visible changes in USER_changelog.md (STE rules are in its
  header). Internal-only changes (tests, refactors, tooling) go here only.
- Base: upstream Mixxx `main` at `ee17a27266` (2026-10-25). Diff against it
  with `git diff ee17a27266 HEAD`.

## Working agreements

- **Git:** `origin` = `github.com/ips-ux/mix_two` (fork), `upstream` =
  `github.com/mixxxdj/mixxx`. Agents commit and push to `origin/main` as part
  of a task ([AGENTS.md](AGENTS.md)). Never open PRs or issues upstream.
  AI-written commit bodies, comments and docs carry a start and end
  disclaimer.
- **Clean room:** never copy decompiled Pioneer code, tables, binaries,
  models or images. Write a behavioural spec, then implement it. Do not touch
  `clatc.dll` (DRM). Firmware files may be analysed statically (owner,
  2026-10-27), never run, flashed, modified or redistributed. RE output
  stays in `build/re/`.
- **Engine owns behaviour; mappings stay thin.** New behaviour is a control
  (`[Group],key`) used by both the skin and the controller script. The DDJ-800
  script sets LEDs from the engine's answer, not from the button press.
- **The owner's spec is the default.** No opt-in setting for specified
  behaviour; settings only where Pioneer offers a real choice or several
  algorithms coexist ([FUNCTIONS.md](docs/ddj800-program/FUNCTIONS.md)).
- **Engine thread:** no allocations or locks in audio callbacks.
- **Build and test (Windows):** `tools\mixtwo_windows.bat configure | build
  [target] | test [gtest args] | run [--nobuild]`; no arguments (a
  double-click) = `run`, pausing on failure. `run` uses a separate dev
  profile (`MIXTWO_PROFILE`, default `build\mixtwo-profile`). `mixxx-test`
  sets `MIXXX_DISABLE_PORTAUDIO` because Pioneer's ASIO driver crashes after
  repeated load/unload (ROADMAP F16); keep it set for any test binary.
- **Not verified in this changelog's first pass:** the build and the test
  suite were not re-run when the history was committed. Status below comes
  from the program docs written when each item was built.

---

## 2026-10-31: AUDIO: HARDWARE | PC toggle, smaller MT

- Toolbar: "AUDIO:" label (headphone-jack icon `btn__audio_jack.svg`) and a
  two-side toggle. The sides push new controls `[Master],audio_target_hardware`
  / `audio_target_pc` (AudioTargetManager::select(); the side in use does
  nothing) and display `audio_target` (PC through `<Not/>`). Lit: HARDWARE
  #ff9a1f, PC #3d9be9. The single-button icons are deleted.
- MT: 24 px (was 34, radius 12 px), moved out of SyncKeySyncBox between two
  equal expanding spacers, so it sits half way between KEY and the tempo
  slider. Seen on screen (PaleMoon).

## 2026-10-31: HARDWARE / PC audio target and Audio Routing

- Owner chose "play through the DDJ-800" after the firmware trace (FW 2.7:
  MASTER, BOOTH, HEADPHONES cannot be read at start). The persistence from
  d01054873d is reverted (a saved value cannot follow a deck that was moved
  while unplugged).
- `src/soundio/audiotargetmanager.{h,cpp}`: `[Master],audio_target`
  (1 = HARDWARE, 0 = PC; saved as `[Soundcard] audio_target`, default
  HARDWARE). HARDWARE = the DJ unit (`kHardwareUnits`, now "DDJ-800") with
  Main 1/2, Headphones 3/4, Booth 5/6 at the device's own sample rate; Mix
  Two's gain / booth_gain / headGain at 1.0 and headMix -1 (cue only; the
  deck blends master in hardware). PC = the previous interface (name kept
  in `[Soundcard] audio_target_pc_device`, matched by maker under another
  API). Each target has a file: `soundconfig_hardware.xml`,
  `soundconfig_pc.xml`; `soundconfig.xml` is always the active one.
  `prepareStartup()` (CoreServices, before setupDevices) falls back to PC
  when no DJ unit is present. Hooks: `SoundManagerConfig::setFileName`,
  `SoundManager::setConfigBeforeSetup`.
- Audio Routing (Preferences > Sound Hardware, Windows): ASIO or WASAPI;
  sets `[Soundcard],audio_routing`, and the manager moves the active target
  to the new API at once. WASAPI is shared mode (other apps can play); the
  DDJ-800's WASAPI endpoint is stereo unless Windows sets it to 5.1.
- DDJ-800 mapping: the level knobs act only on PC; the
  `levelKnobsSoftware` option is gone. Mock test: 36 checks pass.
- Skin: "HARDWARE AUDIO" / "PC AUDIO" button with icons in the toolbar next
  to REC (rekordbox's top-bar PC MASTER OUT position).
- Verified: first start switched to DDJ-800 ASIO (44.1 kHz; a first try at
  48 kHz failed with "Invalid sample rate", fixed by using the device's own
  rate); the owner toggled PC and back (target files written). Not checked:
  WASAPI on the deck, Linux, a start without the DDJ-800.

## 2026-10-30: MASTER / BOOTH / headphone knobs keep their last value

- Owner: MASTER and BOOTH do not start at their hardware positions. Cause
  (FW §2.7): the position reply (SysEx cmd 0x03) leaves out MASTER, BOOTH
  and both headphone knobs; the HID input report is not enabled or decoded
  (FW §2.6), so there is no way to read them at start.
- Fix: `[Master],gain`, `booth_gain`, `headGain` and `headMix` now persist
  in mixxx.cfg (`ControlAudioTaperPot` gained a `bPersist` argument; it no
  longer resets a persistent value to 1.0). Verified: start, quit, the four
  keys are in `[Master]` of the profile's mixxx.cfg. Still wrong if a knob
  moved while Mix Two was off, until that knob moves (MIX-002 note).
- Applies to every user and controller, not only the DDJ-800.

## 2026-10-30: mixer scales and the SOUND COLOR FX row

- 2-deck mixer rebuilt: `channel_left/right.xml` are now knob columns only
  (TRIM, HI/MID/LOW, divider, COLOR, CUE); the faders and meters moved to a
  centre block in `mixer_2decks.xml` with the "SOUND COLOR FX" label level
  with the COLOR knobs (fixed heights must match: divider 1 + 6 + 41 + 4 +
  16 + 3). CUE is centred under each knob column; the crossfader row keeps
  only the orientation dots. COLOR knobs are always visible now (the
  `loaded_chain_preset` visibility binding is gone).
- Scales are drawn in the knob background SVGs: TRIM
  (`knob_bg_regular_trim`), MIXING (`knob_bg_main_scale`), headphone LEVEL
  (`knob_bg_main_level`), MASTER LEVEL and BOOTH (`knob_bg_main_level_red`,
  red from 75 %). Corner labels: `#KnobCornerLabel` (Open Sans); "-inf" is
  an SVG (`knobs/label_minus_inf.svg`) because fonts draw the infinity sign
  too small. TRIM ends: -inf / +12 (Mix Two's pregain range, not the
  DDJ-800's +9). New template `controls/knob_label_above_level.xml`.
- Seen on screen (PaleMoon). Not checked: Classic scheme, 4-deck mixer.

## 2026-10-30: mixer follow-ups (owner, after the screenshot)

- Channel CUE (PFL) in the 2- and 4-deck mixers is now a "CUE" text button
  styled as MASTER CUE (`#ChannelCueButton`, grey off / orange on), as on
  the DDJ-800 photo (`.resource/Gear Images/ddj800.jpg`).
- CFX buttons in a 2 x 2 grid (DUB ECHO, SWEEP / NOISE, FILTER) as on the
  hardware; they stay under HEADPHONES (owner's placement; the hardware has
  them above).
- QuickEffect star button and dot removed from the COLOR knobs (2- and
  4-deck); their 18 px slot stays so the knobs line up.
- 2-deck TRIM moved to the top of the knob column (above HI); a 38 px
  spacer keeps the fader where it was. 4-deck already had TRIM on top.
- Seen on screen (new build, PaleMoon): layout, CFX right-click menu (ticks
  the current preset). Not checked: CUE on state, Classic scheme.

## 2026-10-30: mixer look; CFX buttons pick their effect

- **Labels:** round SYNC shows GRID / BPM (was GRID LOCK / BPM SYNC), KEY
  SYNC shows KEY (`decks/rate_controls.xml`).
- **PFL under COLOR:** the 2-deck mixer's QuickEffect selectors are gone;
  the PFL buttons sit in their place under the COLOR knobs; a 39 px spacer
  keeps the meters level with the faders. 4-deck: selector removed (PFL was
  already under the knob).
- **CFX right-click (FX-060):** new widget `WSoundColorFxButton`
  (`<SoundColorFxButton><SoundColorFx>N</SoundColorFx>`) opens a menu of
  the QuickEffect chain presets. `EffectsManager` stores
  `[SoundColorFX] cfxN_preset` in mixxx.cfg (defaults Filter Echo, Moog
  Filter, White Noise, Filter: Mixxx stand-ins, not Pioneer CFX) and loads
  it into every deck when the button turns on, keeping `super1`. The
  mapping's `cfxNPreset` settings and its preset loading are removed.
- **Knobs:** every pointer white (`KnobColor*` = white; new white pointer
  SVGs for regular / main / small / fx); TRIM body orange
  (`knob_bg_regular_trim.svg`), COLOR body light blue
  (`knob_bg_regular_colorfx.svg`, arc `ArcColorQuickEffect` #4f9fd6). Both
  schemes.
- **CUE off state:** PaleMoon PFL off icon #4c4c4c, on = orange icon on a
  dark warm face; Classic PFL off icon #5e5e5e; MASTER CUE off = grey text
  and border, on = orange.
- **Test:** mock harness 35 checks pass; `mixxx-lib` builds. `mixxx.exe`
  not linked (Mix Two was running): nothing seen on screen yet.

## 2026-10-30: DDJ-800 level knobs, Sound Color FX buttons on screen

- **Cause of "MASTER and headphone knobs do nothing":** the owner's
  `soundconfig.xml` sends Master to the Audient USB ASIO interface and has
  no Headphones output. The DDJ-800 applies MASTER / BOOTH / HEADPHONES in
  hardware to its own outputs only, so the knobs changed nothing audible;
  MASTER stopped when 1d3e9f8a05 set `masterLevelSoftware` to off.
- **Fix (MIX-003..006):** option renamed `levelKnobsSoftware` (so the new
  default applies over a stored `false`), default **on**; it now also binds
  HEADPHONES MIXING `B6 0C` -> `[Master],headMix` and LEVEL `B6 0D` ->
  `[Master],headGain`. With the DDJ-800 as sound card, turn it off (else
  levels apply twice).
- **Sound Color FX on screen (FX-059, CTL):** new engine controls
  `[SoundColorFX],cfx1..cfx4` in `EffectsManager` (toggle; one on at a
  time; switches every deck's QuickEffect `enabled`). The script reads and
  writes them (presets, LEDs) instead of its own `activeCfx`, so the deck
  and the screen share one state. LateNight: HEADPHONES module moved to the
  top of its column; four rectangular grey buttons (lit blue) DUB ECHO,
  SWEEP, NOISE, FILTER under it (`#SoundColorFxButton`, both schemes).
- **Test:** mock harness 34 checks pass; `mixxx-test` builds; effects tests
  pass. `mixxx.exe` link not done (Mix Two was running). Not tried on deck
  or on screen.

## 2026-10-30: DDJ-800 key shift, keyboard, slip reverse, search, SHIFT + RELOOP

- **Key shift pads (CTL-083):** rb6's CSV leaves the pads to rekordbox
  itself, so the manual decides (p.55): top +4..+7, bottom 0..+3, the same
  as keyboard mode. PAGE moves the pads 8 semitones (`keyShiftBase`) instead
  of sending `pitch_up` / `pitch_down`.
- **Keyboard mode (CTL-075):** SHIFT + HOT CUE clears the keyboard hot cue;
  the first pad picks hot cue 1-8 and sets it (`hotcue_N_set`) if empty
  (manual p.53); pads light the existing hot cues until the pick.
- **SLIP REVERSE (CTL-051):** ends after 8 beats while held (timer from
  `bpm`, manual p.21); release stops the timer; the LED follows `reverseroll`.
- **Super Fast Search (CTL-020):** CC 0x29 (sent while TRACK SEARCH is held,
  per FIRMWARE.md 2.2) moves `playposition`, 30 s per jog turn
  (`superFastSearchSecondsPerTurn`, a guess). Not confirmed on the deck.
- **SHIFT + RELOOP/EXIT (CTL-033):** `reloop_andstop` removed; rb6 maps it to
  Active Loop, which the engine does not have yet (PERF-013), so it is
  unbound.
- **Test:** `node tools/ddj800_mapping_test.js`: 27 checks pass; the previous
  script fails 11 of the 12 new ones.
- **Status:** `Checked` (mock harness), CTL-020 `Built`. Not tested on the
  deck.

## 2026-10-28: DDJ-800 Beat Loop pads, hot cues I-P, zoom direction

- **Beat Loop pads (CTL-080):** the script set `beatloop_toggle`, which the
  engine does not have, so the pads only changed the size. A pad now sends
  `beatloop_<size>_toggle` (starts a loop of that size; the same pad ends it).
- **Hot cues I-P (CTL-061):** rekordbox's CSV shows the deck pages the hot
  cue pads itself: page 2 sends notes 0x08-0x0F (PAD9-16), PAGE is "PAGE
  DOWN / UP". The script folded those notes onto pads 1-8 and made PAGE call
  `memory_cue_prev/next` (it moved the playhead). Hot cue mode now uses
  pads 0-15, PAGE does nothing in the engine, and the LEDs for hot cues
  1-16 go to both pages' notes. Other pad modes are unchanged (they still
  fold page 2 onto pads 1-8 and use their PAGE scaling).
- **SHIFT + BROWSE zoom (CTL-008):** right turns sent `waveform_zoom_up`,
  which is the engine's zoom out; right now sends `waveform_zoom_down`.
- **Test:** `node tools/ddj800_mapping_test.js`, a mock-engine harness: 15
  checks pass (the previous script fails 10 of them); every one of the 1509
  bound handlers runs without an error.
- **Also:** USER_changelog jog resolution (6480) and MASTER / BOOTH LEVEL
  default (off) corrected (TRU-056, TRU-057).
- **Status:** `Checked` (mock harness). Not tested on the deck.

## 2026-10-28: owner interview, inventories and buckets

- **What:** the owner answered 25 program questions; the answers are in
  [DECISIONS.md](docs/ddj800-program/DECISIONS.md) (rekordbox 6 default,
  rekordbox 7 additions, export in both formats, managed library and backup,
  Linux as a first-class target, skins, status ladder, STE for all text).
- **Inventories:** the DDJ-800, CDJ-3000, DJM-900NXS2 and rekordbox 5.5 /
  6.6 / 7.0 manuals were compared with the code (882 source rows) and merged
  into 17 bucket files in `docs/ddj800-program/buckets/`; every source row is
  in exactly one bucket row (checked by script). `tools/buckets_summary.py`
  writes [BUCKETS.md](docs/ddj800-program/BUCKETS.md).
- **Retired / trimmed:** PARITY.md is retired (kept for `PAR-` citations);
  ROADMAP.md §3 and §6 now point to the buckets.
- **Status:** docs only. Old "Have" claims became `Built` at most; owner
  deck tests from earlier entries wait for the owner's confirmation
  (TRU-059 to TRU-063).

## 2026-10-28: DDJ-800 position request; mixer and audio traced

- **Owner's report:** the mixer and level knobs do not control Mix Two;
  wants the knob positions read at startup.
- **Traced (worker, firmware + rekordbox capture, FIRMWARE.md 2.7):** the
  deck sends every mixer control as MIDI exactly as the mapping expects
  (MSB then LSB). SysEx cmd 0x03 `F0 00 40 05 00 00 02 09 00 03 01 F7`
  makes it resend all positions (not MASTER, BOOTH, headphones); no
  keep-alive needed. Audio: rekordbox sends MASTER on OUT 1/2, PHONES 3/4,
  BOOTH 5/6, and mixes in software; the deck applies MASTER / BOOTH /
  headphone levels in hardware.
- **Mapping:** `init` ends with the send-all request
  (`PioneerDDJ800.SYSEX_SEND_ALL`). `masterLevelSoftware` defaults to false
  (the deck applies the level in hardware when it is the sound card).
- **Not found:** why the mixer knobs do nothing on the owner's deck. A Node
  run of the whole `init` with the deck's exact bytes reaches pregain, EQ,
  volume, crossfader and CFX; the log shows no script errors; Mixxx's MIDI
  dispatch has no filter that would treat them differently from the jog.
  Next: a `--controller-debug` log of the owner moving each control.
- **Status:** script-checked; not tried on the deck.

## 2026-10-28: fine grid slide (`beats_translate_ms`)

- **Owner:** rekordbox's slide is gentle (place the grid on a kick
  transient, zoomed in) but responsive (no delay, no endless spinning).
  The previous mapping moved 5 ms steps per 8 ticks (about 4 s per turn).
- **Engine (`BpmControl`):** new `[ChannelN],beats_translate_ms`
  (`ControlEncoder`, no-op filtering off so repeats count): moves the grid
  by that many ms. `Beats::tryTranslate` floors to whole frames, so the
  slot moves to the nearest whole frame and carries the remainder
  (`m_translateBeatsMsRemainder`). Listed in the controller control picker.
- **Mapping:** `gridTurn` slide sends ticks x `gridSlideMsPerTick` (0.05 ms)
  per message, nothing held back; GRID ADJUST unchanged (8-tick steps).
- **Tests:** `BeatsTranslateTest.TranslateByMilliseconds`; Node harness
  (scratchpad) for the mapping.
- **Open:** rekordbox's real distance per turn; 0.05 ms/tick is a choice.
- **Status:** unit-tested; not felt on the deck.

## 2026-10-28: SHIFT + jog slides the beat grid (needle search reverted)

- **Owner's correction:** in rekordbox, SHIFT + jog freely shifts the beat
  grid; it is not a needle search. The worker's "byte 3 flag = SHIFT" was a
  wrong inference (that HID path is something else, not identified).
- **Mapping:** `needleSearch` removed. SHIFT + top (CC 0x1F) and SHIFT +
  side (CC 0x26) call the new `PioneerDDJ800.gridTurn(deck, ticks, true)`,
  which `jogTurn` also uses for GRID SLIDE / GRID ADJUST (one 5 ms
  `beats_translate_*` / `beats_adjust_*` step per `gridAdjustTicks` = 8).
- **Open:** rekordbox's slide distance per jog turn is not measured; Mix
  Two moves the grid 4.05 s per turn (810 steps of 5 ms).
- **Tests:** Node harness with a stubbed `engine` (scratchpad): SHIFT +
  top / side slide, no seek / jog / scratch, GRID ADJUST and plain jog
  unchanged. ESLint not installed; not run.
- **Status:** script-checked; not felt on the deck.

## 2026-10-28: SHIFT + jog as needle search (wrong, reverted above)

- **Owner's choice:** add rekordbox's SHIFT + jog (traced in the previous
  entry; SHIFT flag Inferred, read over HID in rekordbox).
- **Mapping only (`PioneerDDJ800.needleSearch`):** SHIFT + top (CC 0x1F)
  adds ticks / `jogResolution` to `state.needle` (a 0..1 fraction, read from
  `playposition` at each SHIFT touch, note 0x67, since playposition updates
  at 15 Hz) and sets `playposition`. SHIFT + side (CC 0x26) is no longer
  bound. The old SHIFT search (`jog` x150) is gone. Mixxx's
  `LoopingControl::notifySeek` already disables a loop the seek leaves.
- **Tests:** a Node check with a stubbed `engine` (scratchpad, not in the
  repo): accumulation, clamp, new touch re-reads, SHIFT + side unbound.
  ESLint is not installed on this machine; not run.
- **Status:** script-checked; not felt on the deck.

## 2026-10-28: paused deck's jog scratches (rekordbox)

- **Owner's choice:** match rekordbox for the paused side jog.
- **rekordbox traced (worker, clean room, SCRATCH-ENGINE.md 2.1 "Paused
  deck"):** a not-playing deck's untouched movement starts an untouched
  scratch (no bend threshold), same maths as a touched scratch, 3-step dead
  band, touch dead zone, no spin, ends after 56 ms quiet; touch restarts it
  as touched; PLAY hands over to bend (Inferred). SHIFT + touched jog =
  needle search, one turn = whole track (not built).
- **Engine (`RateControl::processPlatter`):** new
  `m_platterUntouchedScratch`; `startPlatterScratch()` factored out of
  `calculatePlatterRate` (anchors in the same buffer when paused).
- **Mapping (`jogTurn`):** every turn goes to `platter_turn` unless Mixxx's
  scratch is on; the `jog` (ticks x 0.8, x18 paused) path is gone.
- **Known difference:** vinyl off + touched + paused scratches as untouched
  (3-step band, no release spin); rekordbox uses the touched scratch.
- **Tests:** `UntouchedPlatterScratchesAPausedDeck`,
  `PlayDuringAnUntouchedScratchBends`.
- **Status:** unit-tested; not felt on the deck.

## 2026-10-28: jog bend fixed on the deck (side jog while playing)

- **Owner's report:** backspin good, jogging forward/backward "very very
  wrong"; rekordbox's bend is nearly transparent, no artifacts.
- **Cause:** the owner's profile (`build/mixtwo-profile/mixxx.cfg`) has
  `[App] scratch_algorithm 0` ("Mixxx"), so the mapping sent the side jog
  of a playing deck to Mixxx's `jog` at 0.8 per tick: at 6480 ticks/turn
  that is roughly 200x too strong. The engine platter was not in use (the
  backspin was Mixxx's scratch). Profile also has key lock on decks 1-2
  with `keylock_engine 0` (SoundTouch).
- **Mapping:** a playing deck's untouched turns always go to
  `platter_turn` (rekordbox bend) unless Mixxx's scratch is active.
- **rekordbox bend traced (worker, clean room):** formula confirmed (3 %
  per 1x, threshold 0.5x, hold below it, 56 ms timeout per callback);
  new: instant ratio offset clamped ±0.42, rate = 1 + tempo + bend
  (additive, not limited by tempo range), speed changes ramped over at most
  512 samples, slope path unused. SCRATCH-ENGINE.md 2.1.
- **Engine:** bend clamped to ±42 % (was +42 % only); added to the rate
  instead of scaled by the tempo ratio. New test
  `BackwardBendIsCappedAt42Percent`.
- **Not changed here:** paused deck side jog (done in the next entry).
- **Status:** unit-tested and render-checked; not felt on the deck.

## 2026-10-28: rekordbox's jog input stage in the engine

- **Engine (`RateControl`):** `platter_turn` arrivals are binned per
  millisecond by `mixxx::Time` (256 lock-free atomic bins, any thread);
  `takePlatterTurn` rebuilds the 1 ms ticks per buffer into a 100-entry
  queue (first entry 2 x ceil(buffer ms), 0.5 ms on direction change, zero =
  forward), drains one buffer of queue time, applies the dead band
  (`kPlatterDeadBandTouched` 2/6480 rev, `...Untouched` 3/6480, or the
  previous buffer passed). `processPlatter`: touch-on before / touch-off
  after the buffer's steps; release spins if that buffer moved, did not
  reverse and is not slow; spin ends on reversal, 3 slow buffers or 2 empty
  buffers (first repeats the speed); `kPlatterQuietSeconds` 0.056 ends
  disable and resets the queue. Removed: speed window, spin position
  correction (`m_platterSpinTarget`), `measurePlatter` history.
- **Owner's choice:** match rekordbox including its ~2-buffer jog delay.
- **Tests:** `PlatterTest` runs on `mixxx::Time` test mode (one buffer of
  time per buffer). New: `MovementReachesTheTrackAboutTwoBuffersLate`,
  `BackspinRunsOnUntilTwoEmptyBuffers`; `GlitchesAreDropped` and
  `SpinStopsWithThePlatter` updated to rekordbox's behaviour (glitch sliver
  passes; one repeated buffer at the spin end). `PlatterRenderTest` runs on
  the test clock (messages at their own time); `render_scratch.py` default
  buffer 256 frames. Full suite: see commit.
- **Result vs capture:** per move -10 / +18 / +11 ms at 256 frames (128:
  -3 / +37 / -5; 512: -28 / -22 / +30; before: -1 / +41 / +21).
  `rekordbox3.settings` `AudioBufferSize` = 256.
- **Status:** unit-tested and render-checked; not felt on the deck.

## 2026-10-28: rekordbox jog input timing traced (spin run-on, hand-back)

- **Docs:** `docs/ddj800-program/SCRATCH-ENGINE.md` new §2.2 (jog input
  timing, answers to the backspin run-on / slow release / "1.8× hand-back",
  and the exact changes for `src/engine/controls/ratecontrol.cpp`), new §2.3
  (CDJ-3000 over HID, vinyl speed adjust, Master Tempo), §2 and §2.1
  corrected (rules are per audio buffer, not per jog message), "Next steps"
  items 4–5. `FIRMWARE.md` §3, §6 items 4/10 and §7 rows updated (the
  bursty-MIDI hypothesis is withdrawn). Ledger rows F26–F30 in ROADMAP §1
  (F25 marked superseded).
- **Findings:** rekordbox runs the DDJ-800 through the DDJ-1000 HID map and
  reads its jogs from the HID report (absolute counters); steps are summed
  per 1 ms timer tick, queued, drained per audio buffer with a 2/3-step dead
  band; ~2-buffer input delay; a spin ends after two empty buffers; hand-back
  at play speed (1 + tempo change) for 3 buffers; 56 ms quiet resets the
  jog. CDJ-3000 map: 9720 steps/rev, 3 ms per report.
- **Method:** static analysis of rekordbox 6.8.5 only, existing Ghidra
  project (`build/re/`, new helper scripts `ListClasses.java`,
  `DumpAsm.java`, an `ext:` target in `DumpTargets.java`); REA CLI 6.8.0
  `inspect-pe-imports` evidence
  `ev_a50335a0d496c5ba5f953d5798f8fd7b2ca00917686e2db178d8041dd87b69de`.
  No firmware decryption, no DRM or licensing code touched.
- **Not verified:** rekordbox's audio buffer size during the 2026-10-27
  capture (it sets the delay); how rekordbox enables the DDJ-800 HID report.
  **No engine change** in this pass.
- **Status:** docs only.

## 2026-10-27: firmware static analysis (DDJ-800 v1.02, CDJ-3000 v3.22)

- **New doc:** `docs/ddj800-program/FIRMWARE.md` (method, part inventory,
  findings, implications, unknowns). Ledger rows F18–F25 in ROADMAP §1;
  links from ROADMAP §6 "Hardware and driver work" and SCRATCH-ENGINE
  "Next steps" item 5.
- **DDJ-800:** updater CAB opened with 7-Zip; `.upd` container split by our
  own script; SH-2A images turned into ELF from their section tables and
  analysed in Ghidra 12.1.4 (projects and scripts in `build/re/firmware/`).
  Jog = ×4 quadrature count, no scaling (6480 confirmed), one message per
  1 ms frame, value 0x40 ± steps clamped to ±63, nothing sent when still;
  touch is a plain digital line; JOG ADJUST is mechanical; USB descriptors,
  HID report layout (per-deck OUT streams = jog display) and the rekordbox
  verification SysEx documented. Mix Two must not implement the handshake.
- **CDJ-3000:** the UPD is a LUKS1 volume; not decrypted (access control).
  CDJ-3000 questions stay open; rekordbox's `HidMapCDJ3000` noted as the
  lawful alternative.
- **No engine change.** Implications listed in FIRMWARE.md §6 (constants in
  `ratecontrol.cpp` confirmed; hypothesis for rekordbox's spin hand-back to
  test by PC-side MIDI timing).
- **Status:** docs only. Firmware was never run; nothing was sent to a device.

## 2026-10-27: scratch gap vs rekordbox: scaler read log, spin lag, quiet time

- **Scaler bug (all decks with the "Rekordbox" algorithm):**
  `EngineBufferScaleHalfbandLagrange::rewindReadAhead` reported only the
  re-read frames as consumed at a direction change; the backward
  read-ahead stayed in `ReadAheadManager`'s read log. The engine position
  (walked from the log) drifted from the audio with every direction change,
  and the hand-back after a scratch played the leftovers out as -1x for
  ~70 ms. It now reports unconsumed-before + re-read - unconsumed-after
  (returns `double`). Test:
  `EngineBufferScaleHalfbandLagrangeTest.PositionFromTheReadLogSurvivesDirectionChanges`
  (fake reader with a read log; was 791 frames off after 40 changes).
- **Spin lag:** `RateControl::processPlatter` spin now keeps
  `m_platterSpinTarget` (where the platter's steps put the track, from the
  scratch target at release) and adds (target - (position + speed x
  buffer)) / 20 ms to the measured speed. Before, the 20 ms speed window
  replayed the last 10 ms before release (~60 ms after a fast backspin).
- **Quiet time:** `kPlatterQuietSeconds` 50 ms -> 1.8 / 6480 / 0.05 =
  5.6 ms (spec definition at the measured resolution).
- **Tests:** `PlatterSpinTest.ReleaseWhileTurningSpins` and
  `SpinStopsWithThePlatter` now assert from before the touch (every step
  counts once; the spin stops where the platter stopped). Full suite: see
  commit.
- **Tools:** `PlatterRenderTest` `MIXTWO_RENDER_POSITIONS` CSV (time,
  exact position, `platter_state`, speed); `render_scratch.py render
  --positions`.
- **Result vs rekordbox capture:** -1 / +41 / +21 ms per move (was -122 /
  -215 / +66). Open: rekordbox spins ~37 ms past the last step on a
  backspin, hands back immediately on a slow release, and hands a forward
  spin back at ~1.8x platter speed. rekordbox's audio buffer is 256 frames
  (`rekordbox3.settings`), so per-buffer jog handling does not explain it.
- **Status:** unit-tested and render-checked; not felt on the deck.

## 2026-10-27: double-click starts Mix Two

- `tools/mixtwo_windows.bat` with no arguments builds and runs, like `run`
  (`--nobuild` stays command-line only). If the build tools, the build or
  Mix Two fail, the window pauses so the error can be read (only in the
  no-argument case). Unknown arguments still print the usage.
- **Status:** failure path, usage and build tested; the successful
  double-click start not run here (it opens the app).

## 2026-10-27: rekordbox scratch comparison by USB capture; jog resolution 6480

- **Capture:** the owner played the DDJ-800 in rekordbox with REC running
  and USBPcapCMD recording the root hub (`Z:\Program Files\USBPcap\ddj800.pcap`,
  2.8 GB, not in the repo; REC take `.resource/01 Testing-2026-10-27.wav`,
  ignored by git). Target remains rekordbox 6; the measured resolution
  equals 6.8.5's constant.
- **Finding:** the DDJ-800 sends **6480** jog ticks per turn (1.8 s of
  track per 6490 / 6360 ticks in two moves). Mapping default was 720.
- **Mapping:** `Pioneer-DDJ-800-script.js` `jogResolution` default 6480;
  `Pioneer-DDJ-800.midi.xml` option default 6480, max 4096 -> 16384. A
  profile that saved 720 keeps it until changed in Preferences. The
  untouched `jog` search factor (`ticks * 0.8`) is unchanged (raw ticks).
- **Tools:** `usbpcap_to_jog.py` streams pcap and skips records > 1051
  bytes; `scratch_trace.py` prints 1×-play stretches and per-move offset
  change (robust to wrong-bar matches); `render_scratch.py` uses 6480 and
  has `render --resolution`; `PlatterRenderTest` reads
  `MIXTWO_RENDER_JOG_RESOLUTION` (default 6480). Scripts and renders were
  regenerated; release spin +1.351 s, backspin -5.134 s, bend +0.147 s, as
  before.
- **Result:** Mix Two at 6480 vs rekordbox: -122 ms after a scratch, -215
  ms after a backspin, +66 ms after a forward spin (at 720: +407 ms,
  +1397 ms). Table in `tools/soundparity/README.md`.
- **Status:** render-checked against rekordbox; affected tests rerun (see
  commit); not felt on the deck. Open: the 0.1–0.2 s per move.

## 2026-10-27: scratch spin, disable and bend; render harness

- **Engine:** `RateControl::processPlatter` runs rekordbox's five platter
  states per buffer (idle, scratch, spin, disable, bend); new read-only
  control `[ChannelN],platter_state` (0–4). Spec traced from rekordbox
  6.8.5 and written in our own words:
  [SCRATCH-ENGINE.md §2.1](docs/ddj800-program/SCRATCH-ENGINE.md).
  Constants: spin glitch 50×, speed window 20 ms, quiet 50 ms, spin slow
  count 3, disable end 1215/3240 rev, bend 3 %/1× from 0.5×, cap +42 %
  (one-sided, as rekordbox), bend timeout 56 ms. The glitch filter moved
  from `calculatePlatterRate` (which now takes the turn) to
  `processPlatter`. Bend is added as a user tweak (sync accepts it).
- **Mapping:** `Pioneer-DDJ-800-script.js` `jogTurn` sends untouched turns
  to `platter_turn` while the deck plays or `platter_state` ≠ 0 (with the
  "Rekordbox" algorithm); a paused, idle deck keeps `jog` search.
- **Tests:** 9 new in `src/test/platter_test.cpp` (`PlatterSpinTest`,
  using the engine's exact `getPlayPos()`, since `playposition` only
  updates 15×/s). Full suite: 1384/1384 passed.
- **Render harness:** `src/test/platterrender_test.cpp`
  (`PlatterRenderTest`, env-gated `MIXTWO_RENDER_*`) and
  `tools/soundparity/render_scratch.py` (`scripts`, `wrap`, `render`,
  `all`). Real track: `.resource/jvckpot.mp3` (ignored by git; never
  commit it or the renders). Output in `build/soundparity/scratch/`. A
  cross-correlation check put every render within 50 ms of the hand-worked
  track position. The `mixxx` algorithm in the harness only swaps the
  scaler; the motion is always the engine's platter.
- **Test fix:** `ControllerScriptEngineLegacyTimerTest` used a potmeter
  clamped to 50 for Qt timer ids; reused ids carry serial bits (> 1e6)
  when the whole binary runs in one process (our `test` helper, unlike
  ctest). Now an unbounded `ControlObject`.
- **Status:** unit-tested and render-checked; not tested on the deck. Open:
  rekordbox renders of the same movements (no replay path yet, see
  `tools/soundparity/README.md`), feel on hardware, 1.8 s/turn.

## 2026-10-27: changelogs

- Added this file and [USER_changelog.md](USER_changelog.md). **Status:** docs
  only.

## 2026-10-27: `e78f87dfa8` roadmap handoff

- [ROADMAP.md](docs/ddj800-program/ROADMAP.md) §6: remaining work by
  priority (P1/P2/P3, hardware and driver, design, check-in-rekordbox list,
  waiting on owner). F13 and §5 updated for the remotes and commit policy.
- [SCRATCH-ENGINE.md](docs/ddj800-program/SCRATCH-ENGINE.md): removed the
  stale "motion rules still to build" line.
- **Status:** docs only.

## 2026-10-26 – 2026-10-27: `59ace09228` first fork commit

Everything below landed in one commit on top of upstream. 136 files,
+9565 / −875. Grouped by area.

### Git and agent policy
- Repo connected to `origin`/`upstream`; local `main` tracks `origin/main`.
  `core.filemode=false`, `core.symlinks=false`, `core.autocrlf=false`
  (Windows checkout: ignore executable-bit diffs; the two symlinked SVGs in
  `res/skins/LateNight/palemoon/buttons/` are plain files locally).
- [AGENTS.md](AGENTS.md): new section "Commits and Pushes in This Fork";
  PR ban kept, scoped to upstream.

### Controllers: DDJ-800 mapping
- **Files:** `res/controllers/Pioneer-DDJ-800.midi.xml`,
  `res/controllers/Pioneer-DDJ-800-script.js` (built from rekordbox 6.8.5
  `DDJ-800.midi.csv`; DDJ-400/FLX4 as structural template).
- Every CSV input bound via `midi.makeInputHandler`. Intentional gaps: mic
  knobs (hardware), controller-settings values, Related Tracks, FX tap.
- Behaviour fixed after the first hardware test: SYNC press toggles;
  SHIFT+SYNC = MASTER; BROWSE press never loads (opens the load prompt);
  MASTER LEVEL `B608` / BOOTH LEVEL `B609` drive main/booth gain; MASTER CUE
  `96 63` uses the engine control `[Master],main_cue`; MEMORY / DELETE / CALL
  use memory cues; KEY SYNC LED follows the engine.
- **Mapping options:** `vinylModeDefault` (true), `jogResolution` (720,
  100–4096), `inputSelectPassthrough` (false), `masterLevelSoftware` (true),
  `cfx1Preset`–`cfx4Preset` (−1 = default; DUB ECHO, SWEEP, NOISE, FILTER).
- **Not done:** jog display / on-jog indicator, HID (MI_03), DDJ-800
  preference page; need runtime capture (ROADMAP WS-A, U2, U3).
- **Status:** verified on hardware for most controls (2026-10-26 test);
  later fixes unit-tested / mock-harness only.

### Controllers: plug-and-play, hotplug, open retry
- **Mapping device match:** `<product protocol="midi" device_name="…"/>` or
  `device_name_regex` in `res/schema.xml`; parsed in
  `legacycontrollermappingfilehandler.cpp` / `controllermappinginfo.*`;
  port names normalised by `MidiUtils::canonicalDeviceName`;
  `MidiController::matchMapping`.
- **Auto-enable:** `ControllerManager` enables a never-configured controller
  with the first matching mapping (user mappings before system ones).
  Config `[Controller],auto_enable_matching_mappings` (default on,
  `kAutoEnableMatchingMappingsCfgKey`).
- **MIDI hotplug:** `src/controllers/midi/midiportwatcher.*` polls the OS
  port list once a second (WinMM / CoreMIDI / `/proc/asound/seq/clients`); a
  change held for two polls restarts PortMidi and replaces all PortMidi
  controllers. Config `[Controller],midi_hotplug_enabled` (default on).
  Replaced controllers are deleted only after prefs pages and QML proxies are
  rebuilt; `slotApplyMapping` / `openController` reject stale pointers.
- **Open retry:** a controller that fails to open is retried for 30 s (F15).
- **Trade-offs:** any MIDI change reopens all MIDI controllers (scripts
  re-init); HID/bulk not hotplugged; rescans pause during MIDI learn; the
  DDJ-800 audio interface is still selected by hand.
- **Tests:** `src/test/midiproductmatch_test.cpp`,
  `src/test/midiportwatcher_test.cpp`.
- **Status:** verified on hardware.

### Startup: rekordbox guard
- `src/coreservices.cpp`: `isRekordboxRunning()` (Toolhelp snapshot for
  `rekordbox.exe`, Windows only) and `CoreServices::askToCloseRekordbox()`
  before audio drivers load: Retry / Start Anyway / Quit. Skipped when
  `MIXXX_DISABLE_PORTAUDIO` is set.
- `src/soundio/portaudioenumerator.cpp`: honours `MIXXX_DISABLE_PORTAUDIO`.
- `src/sources/soundsourceproxy.cpp`: repeated provider registration is a
  no-op (second `CoreServices` in one process).
- **Status:** compiled; reason in ROADMAP F16.

### Engine: sync (Pioneer model)
- Controls/config: `[Master],sync_type` (Grid Lock = BEAT SYNC, default /
  BPM Sync), `[Master],sync_grid` (1/8, 1/4, 1/2, Whole), per-deck
  `sync_indicator`, `sync_leader_toggle`. UI: Preferences › Decks › Deck
  options › **Sync options** (`dlgprefdeck*`). Mixxx's old "Sync mode
  (Dynamic tempo tracks)" renamed **Dynamic tempo handling**.
- Phase lock no longer depends on quantize.
- MASTER is **explicit** (reverts upstream's soft-leader downgrade, Mixxx
  issue #11788), so a stopped deck can be MASTER; pressing MASTER on the
  master returns to automatic choice (EngineSync re-pick bug fixed). Sticky:
  a playing master keeps MASTER until it stops; a zero fader does not move it.
- Only the master's tempo slider sets tempo; followers ignore theirs.
- Half/double picked from the joining track's **own BPM**, always on (owner
  decision; rekordbox has a checkbox). Threshold is Mixxx's √2 (verify).
- Known differences: the MASTER deck also has sync on; on-screen buttons
  outside the deck hub still use Mixxx tap/hold.
- **Files:** `src/engine/sync/*`, `src/engine/controls/bpmcontrol.*`.
- **Tests:** `src/test/enginesynctest.cpp` (updated).
- **Status:** unit-tested; partly hardware-tested.

### Engine: tempo
- `[ChannelN],rate_range_next` steps ±6 / ±10 / ±16 / WIDE (±100 %); while
  synced the range widens to fit the master (never narrows; verify).
- Soft takeover on the DDJ tempo slider; `tempo_slider_bpm`,
  `tempo_slider_matched`, `rate_physical`.
- `src/widget/wraterange.*`: clickable range in the deck hub.
- **Files:** `src/engine/controls/ratecontrol.*`.
- **Status:** unit-tested.

### Engine: key sync
- `[ChannelN],keysync_enabled`: on/off mode, follows the tempo master's key
  and its key changes; fallback order MASTER → sync target → any loaded deck
  with an analysed key (playing first). Manual key change or KEY RESET turns
  it off; stays off without an analysed key; the synced key is kept on off.
- **Files:** `src/engine/controls/keycontrol.*`.
- **Tests:** `src/test/keysync_test.cpp`.
- **Status:** unit-tested. Verify vs rekordbox: same vs compatible key,
  restore on off, which deck leads.

### Engine: memory cues
- `CueType::MemoryCue` (point or loop, no count limit) in
  `src/track/cueinfo.*`; deck controls `memory_cue_set` / `_next` / `_prev` /
  `_delete` / `_count`; first `kMaxNumberOfMemoryCueMarks` (64, in
  `src/util/defs.h`) published as `memory_cue_N_position` for red "M" marks
  (`src/waveform/renderers/waveformmarkset.*`).
- rekordbox USB imports keep memory cues as memory cues.
- **Files:** `src/engine/controls/cuecontrol.*`.
- **Tests:** `src/test/cuecontrol_test.cpp`.
- **Status:** unit-tested. Open: Memory Cue deck panel, comments on deck,
  CALL while playing (verify).

### Engine: scratch (rekordbox model)
- Rendering: `src/engine/bufferscalers/enginebufferscalehalfbandlagrange.*`,
  2× half-band (Hann-47 windowed sinc, 43 taps, generated from the formula)
  + 4-point cubic Lagrange. Renders all playback with key lock off.
- Motion: `RateControl::calculatePlatterRate`, controls
  `[ChannelN],platter_touch` / `platter_turn`; position-locked, 60 ms touch
  dead zone, 25× glitch filter, ±8× limit with carry-over, fixed 33⅓
  (1.8 s per turn). DDJ-800 jogs use it.
- Config `[App],scratch_algorithm`; UI Preferences › Decks › **Platter
  options › Scratch algorithm**: "Rekordbox (6.8.5)" (default) / "Mixxx
  (2.7)" (linear).
- Spec: [SCRATCH-ENGINE.md](docs/ddj800-program/SCRATCH-ENGINE.md).
- **Tests:** `src/test/enginebufferscalehalfbandlagrange_test.cpp`,
  `src/test/platter_test.cpp`.
- **Status:** unit-tested. Spin, disable and bend added later the same day
  (entry above).

### Mixer
- `[Master],main_cue`: main mix joins the cue bus (`src/engine/enginemixer.*`).
- Skin: BOOTH knob replaces Mixxx's balance knob (owner decision).
- **Tests:** `src/test/enginemixertest.cpp`.
- **Status:** unit-tested; hardware button mapped.

### Library: playlist folders
- Schema **40 → 41** (`src/database/mixxxdb.cpp`): `PlaylistFolders` table
  and `Playlists.folder_id`. One-way upgrade of the user's database.
- `src/library/dao/playlistdao.*`, `src/library/trackset/playlistfeature.*`,
  `baseplaylistfeature.*`, `src/widget/wlibrarysidebar.*`,
  `src/widget/wtrackmenu.cpp`: Create New Folder / Rename Folder / Delete
  Folder, Move to Folder submenu, defaults "Untitled Folder" / "Untitled
  Playlist", new/duplicated playlists go into the right-clicked folder,
  folders stay expanded, Add to Playlist shows folders as submenus.
- **Tests:** `src/test/playlistfolders_test.cpp`.
- **Status:** unit-tested. UI gaps in ROADMAP §6 P1.

### Library: load prompt
- `src/library/loadprompt.*`, `src/widget/wloadprompt.*`,
  `src/library/librarycontrol.*`. Controls `[Library],load_prompt`,
  `load_prompt_push`, `load_prompt_select`, `load_prompt_deck`,
  `load_prompt_load`, `load_prompt_cancel`. BROWSE push opens "Load to the
  Deck"; pick with LOAD, click, or turn + push; 5 s timeout. No load-method
  preference (owner decision).
- **Tests:** `src/test/loadprompt_test.cpp`.
- **Status:** unit-tested. Look is provisional.

### Library: rekordbox import fixes
- `src/library/rekordbox/rekordboxbeats.*`: one tempo segment per run of
  beats with the same stored PQTZ tempo (was one per ms-rounded beat → BPM
  flicker, sync wobble). Beats within 1 ms of rekordbox's grid.
- `src/library/rekordbox/rekordboxfeature.cpp`, `rekordboxanlz.h`: tracks
  with only `.DAT` (no `.EXT`) keep the grid (a shared flag was split).
- Takes effect when the track is loaded again from the rekordbox view.
- **Tests:** `src/test/rekordboxbeats_test.cpp`,
  `src/test/rekordboxanlz_test.cpp`.
- **Status:** unit-tested.

### Skin: LateNight rework
- Decks mirrored from the centre out; deck hub `decks/deck_hub.xml` (large
  BPM, slider BPM, tempo % and range, platter, CUE/reverse, PLAY); rate
  column with 34 px domed SYNC / KEY SYNC / MT buttons
  (`decks/rate_controls.xml`, `decks/transport_buttons.xml`); MT lock button
  by the waveform removed; mixer split into `mixer/mixer_headphones.xml` and
  `mixer/mixer_main.xml` (old `mixer_main_headphone.xml` deleted); new
  `controls/knob_label_above.xml`; `btn__mt_light*.svg` for classic and
  palemoon; QSS in `style*.qss`; slots per deck in `skin.xml`.
- Font: `res/fonts/Silkscreen-Bold.ttf` (SIL OFL 1.1, licence alongside), 8 px.
- Tooltips: `src/skin/legacy/tooltips.cpp`.
- Design rules: [DESIGN.md](docs/ddj800-program/DESIGN.md).
- **Status:** compiled; built to the owner's direction, layout still
  provisional until the rekordbox-layout skin.

### Tooling and tests
- `tools/mixtwo_windows.bat`: Windows build/test/run helper.
- `tools/soundparity/`: `make_signals.py` (test-signal pack),
  `compare.py` (chirp alignment, null depth, third-octave diff, tail length);
  Python 3 + numpy + scipy. Plan: [SOUND.md](docs/ddj800-program/SOUND.md).
- Test harness: `src/test/main.cpp`, `mixxxtest.*`, `coreservicestest.cpp`,
  QML smoke tests adjusted for the above.
- `CMakeLists.txt`: new sources and tests registered.

### Docs
- `docs/ddj800-program/`: ROADMAP, PARITY (218 rows), SCRATCH-ENGINE,
  SOUND, DESIGN, FUNCTIONS.

<!-- End of AI-generated document. -->
