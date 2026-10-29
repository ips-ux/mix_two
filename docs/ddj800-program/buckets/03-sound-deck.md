<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 03: Sound: deck

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** Critical  ·  **Scope:** how the deck sounds and feels: the scratch engine and jog feel, Master Tempo, key shift quality, vinyl brake / start ramps, tempo ranges and pitch bend. Matched to rekordbox 6.8.5, plus CDJ-3000 improvements its manual shows (DEC-§1). The jog *mapping* is bucket 01; deck behaviour (cues, loops, sync) is bucket 09.

Reading notes:
- Src prefixes as in bucket 01 (`A:` = inventory A, `C-` / `D-` = inventories C / D, `PAR-`, `RM§6`, `AGL`, `DEC-§n`); `SE` = SCRATCH-ENGINE.md, `SND` = SOUND.md, `F<n>` = ROADMAP finding.
- No CDJ or DJM is available for recordings (DEC-§1, Q12): every sound row is measured against rekordbox 6.8.5 only. The CDJ-3000 manual says nothing about Master Tempo quality, DSP, jog resolution or ramp lengths (inventory D).
- Clean room (FUNCTIONS.md, SND): specs in our own words, implementation from the spec.
- **Owner's profile:** `build/mixtwo-profile/mixxx.cfg` has `[App] scratch_algorithm 0` ("Mixxx") and key lock engine SoundTouch (AGL 2026-10-28). Deck tests of the rows below need "Rekordbox (6.8.5)" selected, or they test Mixxx's engine.

## Scratch and jog feel

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| DECK-001 | Scratch motion: position lock, touch dead zone, spin / backspin, hand-back, disable | rb6 | - | traced 6.8.5 | - | Built | ratecontrol.cpp (`platter_touch` / `platter_turn` / `platter_state`); SE §2, §2.1; PlatterTest | Built to rekordbox's traced rules. Render harness vs a USB capture of rekordbox: per move -10 / +18 / +11 ms at 256 frames (SE next steps 4). **Not felt on the deck.** The owner's "backspin good" (2026-10-28) was Mixxx's scratch (profile set to "Mixxx"), so it does not count for this row. | A:E-002, C-247, PAR-B6, RM§6 P1 scratch |
| DECK-002 | Scratch rendering: 2x half-band + cubic Lagrange | rb6 | - | traced 6.8.5 (F17) | - | Built | `EngineBufferScaleHalfbandLagrange`; SE §4; `tools/soundparity/render_scratch.py` | Filter generated from its textbook formula (clean room). Renders all playback with key lock off, as rekordbox. | SE §4, F17 |
| DECK-003 | Jog input stage: 1 ms bins, queue drained per audio buffer, dead band, 56 ms quiet reset | rb6 | - | traced 6.8.5 (F27, F28) | - | Built | ratecontrol.cpp (`takePlatterTurn`); SE §2.2; tests `MovementReachesTheTrackAboutTwoBuffersLate`, `BackspinRunsOnUntilTwoEmptyBuffers` | Owner's choice: match rekordbox including its ~2-buffer jog delay. Best fit at 256 frames, the owner's rekordbox buffer. rekordbox reads the jog over HID absolute counters, Mix Two over MIDI (clamp +-63 steps per ms); HID input is HWL. | AGL 2026-10-28 jog input stage, F27 |
| DECK-004 | Pitch bend with the jog (side, or top with VINYL off) | rb6 | Y p104 | Y p109 | Y p126 | Built | ratecontrol.cpp (`processPlatter`); SE §2.1; test `BackwardBendIsCappedAt42Percent` | 3 % per 1x platter speed, threshold 0.5x, +-42 % cap, added to the rate (not limited by tempo range). Fixed 2026-10-28 after the owner reported side-jog bend "very very wrong" (profile used Mixxx's jog x0.8). Not felt on the deck since. | A:E-003, C-083 |
| DECK-005 | Jog on a paused deck | rb6 | - | traced 6.8.5 | - | Built | SE §2.1 "Paused deck"; tests `UntouchedPlatterScratchesAPausedDeck`, `PlayDuringAnUntouchedScratchBends` | 1.8 s per turn. Known difference: VINYL off + touched + paused scratches as untouched (rekordbox uses the touched scratch). | A:E-004 |
| DECK-006 | Slip scratching (release resumes from the background position) | rb6 | Y p135 | Y p144 | Y p161 | Partial | Mixxx slip + platter path | Unverified with the new platter engine; needs a deck test. | A:F-007 |
| DECK-007 | Jog resolution and input timing | rb6 | - | 6480 steps/turn (DDJ-800) | - | Built | XML:25, JS:200 (6480); FW §2.2; ROADMAP F19, F29 | Measured by USB capture 2026-10-27 (was 720, 9x off). rekordbox drives a CDJ-3000 at 9720 steps/turn, 3 ms per report, no dead band (F29): reference only. The player's own values are unknown (F24). | D-014 |
| DECK-008 | Scratch algorithm choice: "Rekordbox (6.8.5)" (default) or "Mixxx (2.7)" | Improve | - | N (one algorithm) | N | Built | Preferences > Decks > Platter options | Owner's improvement (PAR-B6): lets the DJ choose and leaves room for more algorithms. Existing profiles keep their stored value: the owner's profile still says "Mixxx". | AGL 2026-10-27 scratch algorithm |
| DECK-009 | JOG rotation speed fixed at 33 1/3 | rb6 | Y p182 | Y p200 | Y p228 | Built | SE §2; DESIGN "Later" | rb6 default only; owner 2026-10-27: no 33 1/3 / 45 setting. Revisit only if DVS is built. | C-245, DESIGN Later platter speed |
| DECK-010 | JOG pref: after CUE + spinning the jog, return to the cue and pause | rb6 | Y p182 | Y p200 | Y p228 | Partial | - | Unverified: Pioneer cue mode with the rekordbox platter model is not tested. | C-079 |
| DECK-011 | JOG FEELING ADJUST (weight) | DDJ-800 | - | - | - | Skip | FW §2.4; ROADMAP F20 | Deck hardware handles it (purely mechanical, no message). CDJ-3000 JOG ADJUST is mechanical too. | A:E-001, D-011 |
| DECK-012 | Jog touch sensitivity setting | Skip | - | N | N | Skip | ROADMAP F20 | No setting in rb6 or the CDJ-3000; DDJ-800 touch is a digital line. Nothing to build. | D-013 |
| DECK-013 | Half tempo while scratching above a BPM threshold | Owner? | N | N | str 7.2.19 | Build | - | rb7.2 addition. Open question 2. | C-207 |

## Tempo and pitch

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| DECK-014 | Tempo ranges +-6 / 10 / 16 % / WIDE and their step resolution | rb6 | Y p126 | Y p133 | Y p150 | Partial | ratecontrol.cpp:37-38, 141 (`rate_range_next`) | Ranges match. Steps 0.02 / 0.04 / 0.04 / 0.5 % are not matched (continuous slider). CDJ-3000 uses 0.05 % at +-10 / 16: keep rb6. Check in rb6 whether the range ever narrows back on its own (Mix Two only widens while synced). | C-081, D-001, PAR-B8 |
| DECK-015 | Pitch bend on a synced follower | rb6 | - | unknown | - | Build | CDJ-3000 p66 | CDJ-3000: bending a synced non-master drops BEAT SYNC to BPM sync. rb6 wins: check rb6 first, then match. Unverified in Mix Two (Grid Lock on nudge). | D-051 |

## Master Tempo and key shift

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| DECK-016 | Master Tempo (key lock) sound | rb6 | Y p126 | Y p133 | Y p150 | Partial | keylock + MT button (JS:562-563); SE §2.3; F30 | Function works; the sound is Rubber Band / SoundTouch, not rekordbox's own `f2dsp::TimeStretch` (algorithm family not identified). Measure rekordbox by the SND method, then build or tune. The CDJ-3000 manual gives nothing to learn here. | A:D-006, C-082, D-003, PAR-B7, SND deck sound, F30, RM§6 P1 other deck sound |
| DECK-017 | Key shift sound (KEY SHIFT and KEYBOARD pads, semitone buttons) | rb6 | Y p137 | Y p146 | Y p164 | Partial | Mixxx key engine | Same engine as DECK-016; match rekordbox by measurement. | A:N-004, SND key shift |

## Vinyl brake and start

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| DECK-018 | Vinyl Speed Adjust: Touch/Brake and Release/Start ramps (also slip braking and slip pause in vinyl mode) | rb6 | Y p182 | Y p200 | Y p227 | Partial | Mixxx `brake` / `softStart` script functions (not wired); SE §2.3; F30 | Needs wiring. rekordbox: linear ramp in speed per ms, default 1.0/ms (near instant); the preference table values are unknown (read them from the running process). Heard on every vinyl-mode stop and start. CDJ-3000 links both ramps to one knob (TOUCH&RELEASE): open question 1. Needed for My Settings export (bucket 04). | A:F-009, A:AB-028, C-244, D-012, D-060, PAR-M4, RM§6 P2 vinyl speed adjust |

## Open questions for the owner

1. **Vinyl Speed Adjust controls (DECK-018).** rb6 has two separate settings
   (Touch/Brake, Release/Start); the CDJ-3000 can link both to one value.
   Options: (a) rb6's two settings only; (b) rb6's two plus a "link" option.
   *Recommendation:* (b); the link value is also needed for My Settings
   export (VINYL SPEED ADJUST).
2. **Half tempo while scratching (DECK-013).** rekordbox 7.2 can halve the
   scratch tempo above a BPM threshold. Options: (a) adopt as an rb7-add
   option; (b) skip, the scratch target is rb6.8.5. *Recommendation:* (b).

<!-- End of AI-generated text. -->
