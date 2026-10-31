<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 02: Sound: effects

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** Critical  ·  **Scope:** every rekordbox effect family as it sounds and behaves in the engine: Beat FX (all 34, the DDJ-800's 14 first), Sound Color FX, Release FX, Pad FX, Merge FX, FX beat / BPM / tap / quantize, FX frequency and effect parameters, with the DJM-900NXS2 manual's descriptions as written spec input. Controller wiring of the FX section is in bucket 01 (CTL); the FX display on the DDJ-800 is in bucket 11 (HWL).

Citation notes: inventory A rows outside its section 1 reuse letters that clash with inventories B, C and D, so they are cited with an `A:` prefix (`A:V-001` = inventory A, row V-001). `C-162.nn` = a row of inventory C's Beat FX roster table. `RM6` = ROADMAP.md §6; `SND` = SOUND.md; `DEC-§n` = DECISIONS.md section n.

Reference rules applied here: Beat FX are recorded from **rekordbox 6.8.5** (DEC-§1, Q10), not from rekordbox 5.8.7 as SOUND.md still says. No CDJ or DJM is available for recordings (DEC-§1, Q12), so the DJM-900NXS2 manual is a written spec only. SOUND COLOR FX button 2 is **SWEEP** (DEC-§1). Mixxx's built-in effects with similar names never count as a match (SND).

## Sound parity method and infrastructure

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| FX-001 | Test-signal pack and comparison tool | rb6 | — | — | — | Built | `tools/soundparity/make_signals.py`, `compare.py` (AGENT_changelog "Tooling and tests") | Impulses, sweeps, pink noise, clicks, drum loops; compare = chirp alignment, null depth, third-octave diff, tail length. Not yet run against any FX capture. | SND Method 1-2, SND Order 1 |
| FX-002 | Oracle captures of rekordbox effects (black box, own REC to WAV, MIDI replay) | rb6 | — | Y 6.8.5 | — | Build | SND Method 2; RM6 "Waiting on the owner" | Reference is **rekordbox 6.8.5** (DEC-§1 Q10); SND still names 5.8.7 as the oracle: fix SND (TRU). Owner records the captures. No CDJ/DJM captures (Q12). | SND Method 2, RM6 waiting-on-owner, DEC-§1 |
| FX-003 | Written behavioural spec per effect (clean room) | rb6 | — | — | — | Build | SND Method 3; inventory D §3 item 1 | Spec inputs: rb6 static facts, captures, DDJ-800 manual p66-76, DJM-900NXS2 manual p13-17 (rows below carry them as notes). Never transcribe decompiled DSP (SND boundary). | SND Method 3, D §4 X7 |
| FX-004 | Pioneer effects as Mix Two built-ins with rekordbox names, parameters and order; selector loads by identity | rb6 | Y 5.8.7 CSV | Y 6.8.5 CSV | Y 7.2.19 CSV | Partial | `Pioneer-DDJ-800-script.js:466-471` loads "visible effect N" | Today the DDJ-800 FX SELECT loads by position in the visible effects list (needs wiring once the effects exist). Order of the 14 is identical in rb5/6/7 CSVs. Mapping side: CTL. Selector mapping row: CTL-087 (C-163). | SND Method 4 |
| FX-005 | Acceptance: Mix Two render vs capture, then owner blind A/B | rb6 | — | — | — | Build | SND Method 5 | A row reaches Checked only with a compare.py result cited; owner A/B gives Deck-verified. | SND Method 5 |
| FX-006 | Effects beyond Pioneer (Improve), after parity | Improve | — | — | — | Build | SND "Ideas beyond Pioneer" | Only after parity and marked as Improve. Idea list is empty (Reddit not reachable from the tools); see open question 6. | SND Ideas, SND Order 6 |

## Effect families, units and controls

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| FX-007 | Effect families: BEAT FX, SOUND COLOR FX, RELEASE FX, MERGE FX, PAD FX | rb6 | ≠ p150 (no MERGE FX) | Y p160 | Y p176 | Partial | Mixxx effect units + quick effects | rb6 has four families plus Pad FX. MERGE FX: FX-079. | C-155 |
| FX-008 | Extra Beat FX targets: MIC, crossfader A, crossfader B | DJM-900NXS2 | N | N | N | Build | DJM p13 | rb6 targets (CH1-4, sampler, master; [M] cancels the rest) are built: see CTL-088. The DJM-900NXS2 adds MIC and crossfader-side targets: not rb6; open question 2. | D-126 |
| FX-009 | Two Beat FX units (FX1, FX2), each assignable | rb6 | Y p151 | Y p161 | Y p177 | Partial | `beatFxUnit` (script:117) | Mixxx has 4 units; the DDJ-800 drives unit 1. Second unit needs the screen FX panel (LEG). | C-157 |
| FX-010 | Beat FX multi mode: up to 3 effects per unit, one LEVEL/DEPTH, beat buttons | rb6 | Y p115, p151 | Y p120, p161 | Y p137, p177 | Partial | Mixxx unit = 3 slots + mix knob | Build the single/multi switch (RM6 P1 Sound "Beat FX modes"). | C-158, PAR-D1 |
| FX-011 | Beat FX single mode: one effect, 5 parameter knobs with on/off each, SNAPSHOT | rb6 | Y p114, p152 | Y p119, p162 | Y p136, p178 | Partial | chain presets approximate a snapshot | SNAPSHOT does not store LEVEL/DEPTH. rb7 names the button SNAPSHOT (label only). Per-effect knob assignment is unknown (C-162 note): find it in rb6.8.5. | C-159 |
| FX-012 | FX BPM: AUTO (deck BPM, default) or TAP | rb6 | Y p153 | Y p163 | Y p179 | Build | script:499-501 (SHIFT + BEAT < resets the meta knob); no FX tap | rb6 AUTO = deck grid BPM (better than the DJM's audio-measured AUTO, 70-180 BPM); keep rb6. DDJ-800: SHIFT + BEAT < = AUTO, SHIFT + BEAT > = TAP (manual p28/p66; p65 contradicts). Inventory status Partial is overstated (A conflict 18). | C-160, A:T-006, A:T-007, D-128, PAR-D3, RM6 P1 Sound |
| FX-013 | Beat buttons set the beat fraction (or the parameter value) | rb6 | Y p115 | Y p120 | Y p137 | Partial | script:491-498 (meta +/- 0.125) | Today BEAT < > step a meta knob, not beat fractions. Needs a beat parameter on each Pioneer effect (ranges in the roster rows). | C-161, A:T-008 |
| FX-014 | LEVEL/DEPTH: effect-specific amount (balance, feedback, duty, pitch, oscillator level) | rb6 | Y | Y | Y | Partial | script:480-482 (unit `mix`) | Dry/wet only today. What DEPTH does per effect is in the roster rows. | A:T-013 |
| FX-015 | Echo and reverb tails after the channel fader closes | rb6 | Y | Y | Y | Built | `standardeffectchain.cpp:13` (post-fader) | DDJ-800 manual p66-69; DJM p15 states which effects keep a tail. Tail length to be compared with captures. | A:T-019 |
| FX-016 | Channel effect not heard on that channel's CUE | rb6 | Y | Y | Y | Built | `channelmixer.cpp:41`; `engineeffectsmanager.cpp:113-123` | PFL taps before the post-fader effect stage; same as DDJ-800 p74 and DJM p16-17. | A:T-018, D-129 |
| FX-017 | Beat FX quantize (ON/OFF and X-PAD land on the beat) | DJM-900NXS2 | N | N | N | Build | DJM p12 | Not in rb6. Improve candidate from the mixer standard; open question 2. | D-127 |
| FX-018 | FX FREQUENCY: HI / MID / LOW band select for Beat FX | DJM-900NXS2 | N | N | N | Build | DJM p13 | Not in rb6. Needs a 3-band split before the effect. Improve candidate; open question 2. | D-124 |
| FX-019 | X-PAD (touch strip: effect on by touch, beat by position) | DJM-900NXS2 | N | N | N | Build | DJM p7, p14 | rb6 counterpart is Pad FX hold-to-apply (FX-074). On-screen X-PAD for mouse/touch is an Improve candidate; open question 2. | D-125 |
| FX-020 | FX1 / FX2 as SEND / RETURN (external effect loop) | Owner? | N | str 6.8.5 | str 7.2.19 | Build | PAR-D9 | rb6 offers it only with a DJM mixer; the DJM-900NXS2 adds an AUX return on its own channel. No DJM to test with; open question 2. DJM AUX return channel: MIX-038. | C-177, PAR-D9, RM6 P3 Effects |
| FX-021 | Microphone FX: on/off, select, level | rb6 | Y p142 | Y p152 | Y p169 | Partial | effect unit on `[Microphone]` | The DDJ-800 processes the mic in hardware unless its Effect Mode sends it over USB (manual p77-78). The mic path itself is bucket 06 (see the X-005 row there). | C-179 |

## Beat FX roster: the DDJ-800's 14 (in selector order)

DDJ-800 order is the same in rekordbox 5.8.7, 6.8.5 and 7.2.19 (CSV `9420`-`942D`). "DDJ" = DDJ-800 manual page; "DJM" = DJM-900NXS2 manual. All need FX-002 captures before an implementation.

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| FX-022 | Beat FX roster: 34 in the rb6/rb7 selector (30 in rb5) | rb6 | ≠ 30 | Y 34 | Y 34 | Build | `src/effects/backends/builtin/` stand-ins do not count | rb6 adds PING PONG, HELIX, VINYL BRAKE, STRETCH to the selector. Single-mode parameter names come from a shared vocabulary in the exe (FEEDBACK, PITCH, ROOM SIZE, DUTY, ...); per-effect mapping unknown. SND says 34 but lists 35 (PITCH ECHO): TRU. | C-162, PAR-D2, RM6 P1 Sound |
| FX-023 | LOW CUT ECHO | rb6 | Y | Y | Y | Build | none (stand-in: Echo + filter) | Echo with the lows removed, decaying per beat. BEAT 1/16-16; DEPTH = dry/echo balance (DDJ p66). | C-162.24, A:V-001 |
| FX-024 | ECHO | rb6 | Y | Y | Y | Build | stand-in `echoeffect.cpp` | Decaying echo. BEAT 1/16-16; DEPTH = balance (DDJ p67). DJM spec: TIME 1-4000 ms, repeats decay, fade after fader-down (DJM p15). | C-162.02, A:V-002, D-111 |
| FX-025 | MT DELAY (multi-tap) | rb6 | Y | Y | Y | Build | none | Up to 7 taps at 1/8-beat steps. BEAT 1/16-16; DEPTH = odd taps (MIN to centre), then even taps (centre to MAX) (DDJ p68). | C-162.15, A:V-003 |
| FX-026 | SPIRAL | rb6 | Y | Y | Y | Build | none | Reverb-like; pitch moves when the time changes. BEAT 1/16-16; DEPTH = feedback + balance (DDJ p69). DJM: TIME 10-4000 ms (DJM p15). | C-162.04, A:V-004, D-113 |
| FX-027 | REVERB | rb6 | Y | Y | Y | Build | stand-in `reverbeffect.cpp` | BEAT sets the amount 1-100 %; DEPTH = balance (DDJ p69). DJM: TIME 1-100 %, X-PAD = filter cutoff (DJM p16). | C-162.05, A:V-005, D-114 |
| FX-028 | TRANS | rb6 | Y | Y | Y | Build | stand-in `tremoloeffect.cpp` (square) | Gates the sound per beat. BEAT 1/16-16; DEPTH = duty + balance (DDJ p70). DJM: TIME 10-16000 ms (DJM p16). | C-162.06, A:V-006, D-115 |
| FX-029 | ENIGMA JET | rb6 | Y | Y | Y | Build | none | Flanger with endlessly rising or falling pitch. BEAT +/-1/16-64 (sign = direction); DEPTH = amount, fully left = dry (DDJ p70). | C-162.21, A:V-007 |
| FX-030 | FLANGER | rb6 | Y | Y | Y | Build | stand-in `flangereffect.cpp` | One cycle per beat setting. BEAT 1/16-64; DEPTH = amount (DDJ p71). DJM: TIME 10-32000 ms, fully left = dry (DJM p16). | C-162.08, A:V-008, D-117 |
| FX-031 | PHASER | rb6 | Y | Y | Y | Build | stand-in `phasereffect.cpp` | Cycle per beat setting. BEAT 1/16-64; DEPTH = amount (DDJ p71). DJM: TIME 10-32000 ms (DJM p16). | C-162.09, A:V-009, D-118 |
| FX-032 | PITCH | rb6 | Y | Y | Y | Build | stand-in `pitchshifteffect.cpp` | BEAT sets -50 to +100 %; DEPTH = pitch amount, fully left = original (DDJ p72, DJM p16). | C-162.10, A:V-010, D-119 |
| FX-033 | SLIP ROLL | rb6 | Y | Y | Y | Build | none (Mixxx loop rolls are deck-level) | Records the input at ON and repeats it per beat; a new beat value **re-records**. BEAT 1/16-16; DEPTH = balance (DDJ p72, DJM p16-17). Must work on any channel and the master. | C-162.11, A:V-011, D-120 |
| FX-034 | ROLL | rb6 | Y | Y | Y | Build | stand-in `glitcheffect.cpp` (rough) | Records at ON and repeats the same capture. BEAT 1/16-16; DEPTH = balance (DDJ p73, DJM p17). | C-162.12, A:V-012, D-121 |
| FX-035 | MOBIUS SAW | rb6 | Y | Y | Y | Build | none | Adds an endlessly rising or falling sawtooth tone; sounds even without input. BEAT +/-1/16-64; DEPTH = tone level (DDJ p73). | C-162.22, A:V-013 |
| FX-036 | MOBIUS TRI | rb6 | Y | Y | Y | Build | none | As MOBIUS SAW with a triangle tone (DDJ p74). | C-162.23, A:V-014 |

## Beat FX roster: the other 20 (rb6.8.5 selector)

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| FX-037 | DELAY | rb6 | Y | Y | Y | Build | none | DJM spec: 1/16-16 beats, 1-4000 ms, dry/wet, a single repeat, tail survives fader-down (DJM p15). | C-162.01, D-110 |
| FX-038 | PING PONG | rb6 | N | Y | Y | Build | none | Not in rb5: measure in rb6 only. DJM: 1/16-16, repeats alternate left/right with different timing (DJM p15). | C-162.03, D-112 |
| FX-039 | FILTER | rb6 | Y | Y | Y | Build | none | DJM: LFO cycle 1/16-64 beats, 10-32000 ms, depth; X-PAD fine-tunes the cycle (DJM p16). | C-162.07, D-116 |
| FX-040 | VINYL BRAKE | rb6 | N (string only) | Y | Y | Build | none | DJM: 1/16-16, 10-4000 ms, depth sets the slow-down to a stop, fully left = normal (DJM p17). Not the deck's vinyl brake ramp (bucket 03). | C-162.13, D-122 |
| FX-041 | HELIX | rb6 | N | Y | Y | Build | none | DJM: 1/16-16, 10-4000 ms, depth = overlay ratio (fully right freezes); X-PAD changes time and pitch, snaps back (DJM p17). | C-162.14, D-123 |
| FX-042 | UP ECHO | rb6 | Y | Y | Y | Build | none | No written description; capture only. | C-162.16 |
| FX-043 | DOWN ECHO | rb6 | Y | Y | Y | Build | none | Capture only. | C-162.17 |
| FX-044 | REV DELAY | rb6 | Y | Y | Y | Build | none | Capture only. | C-162.18 |
| FX-045 | ROBOT | rb6 | Y | Y | Y | Build | none | Capture only. | C-162.19 |
| FX-046 | REV ROLL | rb6 | Y | Y | Y | Build | none | Capture only. Also an Active Censor effect (PERF). | C-162.20 |
| FX-047 | BPF ECHO | rb6 | Y | Y | Y | Build | none | Capture only. | C-162.25 |
| FX-048 | NOISE | rb6 | Y | Y | Y | Build | stand-in `whitenoiseeffect.cpp` does not count | Capture only. Distinct from the CFX NOISE (FX-065). | C-162.26 |
| FX-049 | SPIRAL UP | rb6 | Y | Y | Y | Build | none | Capture only. | C-162.27 |
| FX-050 | REVERB UP | rb6 | Y | Y | Y | Build | none | Capture only. | C-162.28 |
| FX-051 | HPF ECHO | rb6 | Y | Y | Y | Build | none | Capture only. | C-162.29 |
| FX-052 | LPF ECHO | rb6 | Y | Y | Y | Build | none | Capture only. | C-162.30 |
| FX-053 | CRUSH ECHO | rb6 | Y | Y | Y | Build | none | Capture only. | C-162.31 |
| FX-054 | SPIRAL DOWN | rb6 | Y | Y | Y | Build | none | Capture only. | C-162.32 |
| FX-055 | REVERB DOWN | rb6 | Y | Y | Y | Build | none | Capture only. | C-162.33 |
| FX-056 | STRETCH | rb6 | N | Y | Y | Build | none | Not in rb5: measure in rb6 only. Capture only. | C-162.34 |
| FX-057 | PITCH ECHO (DSP table only, not in the selector) | rb6 | DSP table | DSP table | DSP table | Build | rekordbox.exe name table | Probably a Pad FX or internal effect. Check in rb6.8.5 whether a DJ can select it; build only where rb6 uses it (with Pad FX, FX-074). Unverified. | C-162.35 |
| FX-058 | PAN (DSP table; inside the 7.2.19 menu list) | rb6 | DSP table | DSP table | str 7.2.19 | Build | rekordbox.exe name table | Not in the rb6 selector, so not an rb6 Beat FX. If rb7.2 really added it, it is an rb7 addition candidate. Unverified. | C-162.36 |

## Sound Color FX

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| FX-059 | CFX single mode with the controller's [DEFAULT] set | rb6 | Y p154 | Y p164 | Y p180 | Partial | QuickEffect + per-button chain preset (`cfx1Preset`-`cfx4Preset`); shared state `[SoundColorFX],cfx1..cfx4` (`EffectsManager`, 2026-10-30) with on-screen buttons under HEADPHONES | DDJ-800 default set is **DUB ECHO, SWEEP, NOISE, FILTER** (DEC-§1: rb6 CSV wins over the manual's PITCH). No matching presets ship yet. Fixed shipped presets vs a user setting: open question 3. Button wiring and LEDs: CTL. | C-169, PAR-D6, RM6 P1 Sound |
| FX-060 | CFX [USER] mode: pick from the 9 types per button | rb6 | Y p155 | Y p165 | Y p181 | Partial | `cfxNPreset` chain index (XML:53-66) | Uses Mixxx chain presets until the Pioneer CFX exist. | C-170, A:W-009 |
| FX-061 | CFX multi mode: a different CFX per channel | rb6 | Y p155 | Y p165 | Y p181 | Partial | Mixxx quick effect per deck | | C-171 |
| FX-062 | CFX roster: FILTER, JET, CRUSH, NOISE, PITCH, SPACE, DUB ECHO, SWEEP, GATE COMP | rb6 | str (9) | str (9) | str (9) | Build | SND lists 8 (no GATE COMP); PAR-D6 lists 8 | Same 9 in all installs. DJM-900NXS2 shares 6 names with written descriptions (rows below). Mixxx filter, bitcrusher, white noise, echo, pitch shift do not count. | C-172 |
| FX-063 | DUB ECHO (button 1) | rb6 | Y | Y | Y | Build | XML:54-56 (`cfx1Preset`, -1 = keep current) | Echo on mids only (left) or highs only (right); repeats fade (DDJ p76, DJM p15). DJM PARAMETER = feedback (DDJ-800 has none). | A:W-005, D-101 |
| FX-064 | SWEEP (button 2) | rb6 | Y | Y | Y | Build | XML:57-59 "SWEEP quick effect preset"; CSV `CFX2On` "CFX SWEEP" | **SWEEP, not PITCH** (DEC-§1). DJM spec: left = gate that tightens the sound and lowers its body; right = band-pass whose bandwidth narrows (DJM p15). DESIGN.md "Later" still says PITCH: TRU. | A:W-006, D-102 |
| FX-065 | NOISE (button 3) | rb6 | Y | Y | Y | Build | `whitenoiseeffect.cpp` exists, no preset wired, not a match | Filtered white noise mixed in; knob sweeps the noise filter (DDJ p76). DJM: noise enters **before** the channel EQ; PARAMETER = noise level (DJM p15). | A:W-007, D-103 |
| FX-066 | FILTER (button 4) | rb6 | Y | Y | Y | Partial | Mixxx quick effect default (Filter) | Left = low-pass, right = high-pass (DDJ p76). DJM PARAMETER = resonance; rb6 / DDJ-800 FILTER has no resonance knob. Curve and resonance not measured. | A:W-008, D-105 |
| FX-067 | SPACE | rb6 | Y | Y | Y | Build | none | Reverb on mid + low (left) or mid + high (right); PARAMETER = more feedback (DJM p15). | D-100 |
| FX-068 | CRUSH | rb6 | Y | Y | Y | Build | bitcrusher is not a match | Left = more distortion; right = crushed sound through a rising high-pass (DJM p15). | D-104 |
| FX-069 | PITCH, JET, GATE COMP (CFX) | rb6 | str | str | str | Build | none | No written description; capture only. These are the rb6 CFX, not the Beat FX PITCH. | SND Inventory (CFX list) |
| FX-070 | Apply CFX to the sampler with the MIC COLOR knob | Skip | Y p183 | Y p202 | N | Skip | PAR-D7 | DDJ-RZ only; not a DDJ-800 feature; rb7 removed it. | C-173, PAR-D7, RM6 P3 Effects |
| FX-071 | SMART CFX | Skip | N | str 6.8.5 | str 7.2.19 | Skip | — | Device-specific (not on the DDJ-800), not in the manuals. | C-175 |

## Release FX and Pad FX

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| FX-072 | Release FX: ECHO, VINYL BRAKE, BACKSPIN; hold to apply, beat length, cancels Beat FX | rb6 | Y p153 | Y p163 | Y p179 | Build | script:485-488 (SHIFT + ON only switches Beat FX off) | Script comment calls it Release FX but it is not (TRU). SND also names VINYL BRAKE ECHO: check the rb6.8.5 list (installs show `RELEASEECHO`, `VINYLBRAKE`, `BACKSPIN`). | C-164, A:T-016, PAR-D5, RM6 P2 Effects |
| FX-073 | Release FX prefs: number of units; "Apply RELEASE FX on CFX" | rb6 | Y p183 | Y p202 | Y p229 | Build | — | | C-165 |
| FX-074 | PAD FX: 2 banks x 16 presets, hold to apply, several at once, temporary beat change | rb6 | Y p156-157 | Y p166-168 | Y p182-184 | Partial | loop rolls + momentary units (script:983-988, 1021-1024) | Mix Two uses loop rolls on PAD FX1 and user effect units on PAD FX2, not rb6's assignable Pad FX. Engine needs real Pad FX presets; page/bank wiring is CTL. | C-166, PAR-D8, A:I-001, RM6 P2 Effects, A:I-002, A:I-004 |
| FX-075 | Pad FX assignment (effect + beat per pad, set in the app) | rb6 | Y | Y | Y | Build | — | | A:I-007 |
| FX-076 | Pad FX edit parameters grouped BEAT FX / SOUND COLOR FX / SCENE FX / RELEASE FX | rb6 | ≠ p106 (five raw parameters) | Y p111 | Y p128 | Build | — | rb6 grouping (rb5 exposed five raw parameters). | C-167 |
| FX-077 | Pad FX preset vocabulary (FILTER LFO, NOISE LFO, CHORUS, SLIP LOOP, HPF, LPF, NOISE (RMX), V.BRAKE, BACKSPIN, ...) | rb6 | str | str | str | Build | — | Same strings in all three exes; capture each preset. | C-168 |
| FX-078 | Release FX pad: stops the held pad effect and adds the release effect | rb6 | Y | Y | Y | Build | not in JS | Pad LED illumination `9F2A`/`9F2B` is CTL. | A:I-006 |

## Other effect systems

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| FX-079 | MERGE FX (BUILD FX / BUILD SAMPLE / RELEASE FX / DROP SAMPLE, 4 presets) | Owner? | N | Y p169-171 | Y p185-187 | Build | PAR-D10 (Skip) | An rb6 feature, so the rb6 default would include it; PARITY skipped it before DECISIONS. Open question 1. | C-174, PAR-D10 |
| FX-080 | TOUCH FX (touch-display controllers) | Skip | str | str | str | Skip | — | Device-specific; the DDJ-800 has no touch display. | C-176 |
| FX-081 | RMX EFFECTS (SCENE FX) | Skip | Plus Pack | plan | plan | Skip | PAR-D11 | Separate paid pack in rb5, plan-gated in rb6; not core. Revisit with MERGE FX (open question 1). | C-178, PAR-D11 |

## Open questions for the owner

1. **MERGE FX and RMX SCENE FX.** MERGE FX is an rb6 feature (rb6 p169-171), so the rb6 default says build it; PARITY skipped it before your decisions. Options: (a) build after the 34 Beat FX and the CFX are matched; (b) skip for good (it is a build-up tool with samples; the DDJ-800 has no MERGE FX control). *Recommendation:* (a), low priority, with RMX left skipped.
2. **DJM-900NXS2 extras on the Beat FX** (the mixer standard, but not in rb6): FX FREQUENCY band select (FX-018), Beat FX quantize (FX-017), an on-screen X-PAD (FX-019), MIC and crossfader A/B targets (FX-008), SEND/RETURN with an AUX return (FX-020). Options: adopt each as an Improve; adopt some; none. *Recommendation:* FX FREQUENCY and Beat FX quantize as Improve rows after the DDJ-800's 14 are matched (screen first); X-PAD later; skip targets and SEND/RETURN until a DJM is on hand.
3. **CFX buttons: fixed presets or a setting?** FUNCTIONS.md says the owner's spec is the default with no opt-in settings. Options: (a) the four buttons always load Mix Two's DUB ECHO / SWEEP / NOISE / FILTER, and only USER mode lets the DJ choose; (b) keep the per-button mapping setting for all modes. *Recommendation:* (a).
4. **PITCH ECHO and PAN.** If rb6.8.5 shows neither in a selector, drop them from the roster (keep 34). If rb7.2 added PAN to its Beat FX list, is it an rb7 addition to adopt? *Recommendation:* drop both unless rb6 uses PITCH ECHO in Pad FX; ask again about PAN only if rb7 really lists it.
5. **Release FX list.** SOUND.md names VINYL BRAKE ECHO; the installs show ECHO / VINYL BRAKE / BACKSPIN. *Recommendation:* follow what rb6.8.5's Release FX selector shows (a check, not a choice); answer only if you know rb6 has more.
6. **Improve effects beyond Pioneer (FX-006).** The tools cannot reach Reddit. Options: you supply a short list; skip the idea list until parity is done. *Recommendation:* skip until parity, then a list from you.

<!-- End of AI-generated text. -->
