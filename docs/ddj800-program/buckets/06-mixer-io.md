<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 06: Mixer & I/O

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** Critical (back to High when the doubled-audio rows are fixed)  ·  **Scope:** the mixer's sound and routing: EQ, channel and crossfader curves, trim, headroom and limiter, master and booth, headphones, fader start, mixer mode, microphone, external and AUX inputs, sound devices, and the rows where the same audio may be applied twice. Mixer *standard*: DJM-900NXS2 (DEC-§1). The knob mapping (soft takeover, COLOR, deck switching) is bucket 01; settings sent to the deck are bucket 11.

Reading notes:
- Src prefixes as in bucket 01 (`A:` = inventory A, `B-` / `C-` / `D-` = inventories B / C / D, `PAR-`, `RM§6`, `AGL`, `DEC-§n`, `FW` = FIRMWARE.md).
- **Owner's other session:** Q8 (MASTER / BOOTH LEVEL doubling) and the other DDJ-800 mixer-knob items are handled in the owner's other session (DEC-§9). This bucket tracks them as rows; do not change them from here. That session so far (commits 1d3e9f8a05, a18afdb1e4): the mapping asks the deck for its knob positions at the end of `init` (SysEx `F0 00 40 05 00 00 02 09 00 03 01 F7`); `masterLevelSoftware` now defaults to **off** (the deck applies MASTER and BOOTH LEVEL in hardware when it is the sound card); the audio routing was traced (rekordbox sends MASTER on OUT 1/2, PHONES 3/4, BOOTH 5/6 and mixes in software); the mixer-knob problem was set aside as a necessary to-do (ROADMAP §6 P1), waiting on a `--controller-debug` log from the owner.
- **Doubled-audio rows** (the reason for Critical): MIX-003, MIX-004, MIX-006, MIX-031, MIX-035.
- Not via PC: the deck sees Mix Two as "rekordbox not running", so its hardware mixer handles mic and external sources by itself (manual p78-80, Inferred for Mix Two).

## DDJ-800 mixer knobs and levels (owner's other session)

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| MIX-001 | DDJ-800 mixer knobs control Mix Two (TRIM, EQ, COLOR, channel faders, crossfader) | rb6 | - | Y CSV | = rb6 | Partial | ROADMAP §6 P1 "Necessary to-do"; FW §2.7; AGL 2026-10-28 | **Owner's other session.** Owner's report 2026-10-28: the knobs do not control Mix Two. Ruled out so far: the deck's MIDI matches the mapping; a Node run of `init` reaches pregain, EQ, volume, crossfader, CFX; no script errors. Next: the owner's `--controller-debug` log. Possibly relevant: after boot the deck copies each TRIM / EQ message onto channel + 2 until the host sends `9F 46 00` (FW §2.7; see MIX-011). | RM§6 P1 mixer knobs, DEC-§9 mixer knob items |
| MIX-002 | Knob and fader positions read at start | rb6 | - | Y (SysEx cmd 0x03 after setup) | = rb6 | Built | JS `init` (`PioneerDDJ800.SYSEX_SEND_ALL`), commit 1d3e9f8a05 | **Owner's other session.** The deck never sends positions by itself; the reply leaves out MASTER, BOOTH and the headphone knobs. Script-checked; not tried on the deck. | AGL 2026-10-28 position request |
| MIX-003 | MASTER LEVEL (Q8) and master level meter | rb6 | Y p101 | Y p106 (CSV B608 "No function assigned") | Y p123 | Checked | JS `initMixer` (`levelKnobsSoftware`, default on), 2026-10-30; mock harness | 2026-10-30: owner's mix goes through another interface (Audient), so the knob drives `[Master],gain` by default again; turn the option off with the DDJ-800 as sound card. **Owner's other session (Q8). Doubled-audio row.** The deck applies MASTER LEVEL in hardware on its own outputs; with the old default (on) the level was applied twice. Open part: drive software main gain only when the main output is not the DDJ-800 (inventory A's option c). PAR-C7's BOOTH-replaces-balance decision stays. | A:S-007, C-012, PAR-C7 |
| MIX-004 | BOOTH LEVEL and the BOOTH output | rb6 | - | Y CSV B609 "No function assigned" | = rb6 | Checked | JS `initMixer` (`levelKnobsSoftware`, default on), 2026-10-30; mock harness | 2026-10-30: drives `booth_gain` by default (same option). **Owner's other session (Q8). Doubled-audio row.** Same double-level issue as MIX-003; needs a Booth output on the deck's BOOTH channels (OUT 5/6), set by hand. | A:S-009, A:Z-002 |
| MIX-005 | HEADPHONES LEVEL knob | DDJ-800 | - | Y CSV B60D "No function assigned" | = rb6 | Checked | JS `initMixer` (`levelKnobsSoftware`, default on), 2026-10-30; mock harness | 2026-10-30: bound `B6 0D` -> `headGain` (same option); the on-screen LEVEL now follows. **Owner's other session.** Deck hardware handles it (applied in hardware, Inferred). Gap: the on-screen LEVEL knob does not follow, and the position request does not include it. | A:S-014 |
| MIX-006 | HEADPHONES MIXING knob (cue / master balance) | rb6 | - | Y CSV B60C "No function assigned" | = rb6 | Checked | JS `initMixer` (`levelKnobsSoftware`, default on), 2026-10-30; mock harness | 2026-10-30: bound `B6 0C` -> `headMix` (same option). The doubled-mix question stays for the DDJ-800-as-sound-card case. **Owner's other session. Doubled-audio row.** If the deck blends master into PHONES in hardware (FW §2.7 says it does, Inferred), Mix Two's centre `headMix` adds the main mix to the cue feed a second time. Unverified; the manual's signal flow diagram (p101, not in the text extract) would settle it. | A:S-015, A:Z-012 |

## Channel strip

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| MIX-007 | TRIM and Auto Gain | rb6 | Y p183 | Y p201 | Y p228 | Partial | JS:662 (`pregain`); ReplayGain | Gain law vs rb6's trim not matched (unknown). Knob problem: MIX-001. Takeover on deck switch: CTL-085. | A:S-004, PAR-C1, C-215 |
| MIX-008 | EQ HI / MID / LOW, EQ / ISOLATOR, kills; DJM-900NXS2 curve | rb6 | Y p140, p183 | Y p150, p201 | Y p167, p228 | Partial | JS:663-665; `dlgprefmixerdlg.ui` (Equalizer Plugin) | rb6 offers DJM-900NXS and DJM-900NXS2 EQ types; DJM-900NXS2 range is -26 / +6 dB. Mixxx's knob law (kill to +12 dB) and filters are not Pioneer's. Build rb6's DJM-900NXS2 type, measured, as the default. | A:S-019, C-211, D-130, PAR-C2 |
| MIX-009 | Channel fader and Channel Fader Curve | rb6 | Y p183 | Y p201 | Y p228 | Partial | JS:666 (`volume`); `dlgprefmixerdlg.ui` has only a crossfader curve | Fader works; no channel fader curve setting (PAR-C3 says Have: wrong, bucket 12). DJM-900NXS2 gives the three shapes rb6 names. | A:S-012, C-212, D-131, PAR-C3 |
| MIX-010 | Crossfader and CROSSFADER Curve | rb6 | Y p183 | Y p201 | Y p228 | Built | JS:360-362; `dlgprefmixerdlg.ui:25-185` | Curve matches the DJM-900NXS2's three positions in idea. Cut lag is set in the deck (HWL-049). | A:S-013, C-213, D-132, PAR-C4, A:AB-010 |
| MIX-011 | Mixer Mode 2-channel / 4-channel | rb6 | - | Y (DDJ-800 settings, CSV 9F46) | = rb6 | Build | - | rb6 has it on the DDJ-800 settings page (DEC-§9: Match). In 2-ch mode decks 1/3 and 2/4 share a strip and CH SELECT offers only 1 and 2. Firmware: `9F 46 00` / `7F` switches the copy of TRIM / EQ messages onto channel + 2 (on after boot; rekordbox sends `00`); Mix Two never sends it, so check the effect on decks 3/4 (MIX-001). | A:P-001, A:AB-008, A:T-011 |
| MIX-012 | Mixer panel on screen (meters, TRIM, EQ, CUE, faders, crossfader assign) | rb6 | Y p118 | Y p123 | Y p140 | Built | LateNight `mixer/mixer_main.xml` | rb7 hides the panel when a controller is connected; rb6 does not. | C-210 |
| MIX-013 | Hardware indicators: microphone, master level, CLIP | DDJ-800 | - | - | - | Skip | manual p26-27 | Deck hardware handles them. | A:S-002, A:S-005, A:S-006 |

## Headphones

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| MIX-014 | Headphones in software: per-channel CUE, MIXING, LEVEL, mono split | rb6 | Y p140 | Y p151 | Y p168 | Built | JS:667-668 (`pfl`); `headMix`, `headGain`, `headSplit` | The deck's own MIXING / LEVEL knobs: MIX-005, MIX-006. | A:S-010, C-218, D-136, PAR-C5 |
| MIX-015 | MASTER CUE (main mix to the headphones) | DDJ-800 | - | Y CSV 9663 / 9662 | = rb6 | Built | JS:383-390; enginemixer.cpp:126 (`[Master],main_cue`) | Engine control, on screen and on the deck. | A:S-008 |
| MIX-016 | Sampler on headphones | rb6 | Y | Y | Y | Built | Mixxx sampler PFL | | A:K-011 |

## Output level and limiter

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| MIX-017 | Output Level (headroom) and a limiter on the main output | rb6 | Y p183 | Y p201 | Y p228 | Build | no limiter or headroom in `src/engine` | Two value lists exist in rekordbox: manual Output Level (internal -21 to -3 dB / NONE; external -12 to +9 dB) and a "Headroom" string (0 to -12 dB, default -9; PAR-C6). Check which one rb6.8.5 shows with the DDJ-800, then build it with the limiter. DJM-900NXS2 and CDJ-3000 (OUTPUT ATT., ignored with DJ software) agree on the idea. Deck-side limiter: HWL-058. | C-214, D-080, D-135, PAR-C6, RM§6 P2 mixer headroom |
| MIX-018 | PC MASTER OUT (also play through the computer) | rb6 | Y p101 | Y p106 | Y p123 | Partial | PAR-C11 | Needs a second output device. | C-222, PAR-C11, A:Z-008, RM§6 P3 PC MASTER OUT |
| MIX-019 | Audio dropout meter | rb6 | Y p101 | Y p106 | Y p123 | Built | `toolbar.xml:180-185` | See LEG-016. | - |
| MIX-020 | DJM-900NXS2 DSP (96 kHz / 64-bit mix bus) | Skip | - | - | - | Skip | DJM p3 | Not audible in practice; not worth an engine change. | D-082 |

## Sound devices and routing

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| MIX-021 | DDJ-800 audio routing: Master to OUT 1/2, Headphones to 3/4, Booth to 5/6; pick the DDJ-800 automatically | rb6 | - | Y (auto) | = rb6 | Partial | Preferences > Sound Hardware; FW §2.7 | Needs wiring: set by hand today; automatic selection when the deck is plugged in is not built (USL note). Use one PHONES jack at a time (manual p33). Linux: through `snd-usb-audio` (DEC-§8). | A:Z-007, A:Z-003, A:Z-004, RM§6 HW audio auto-pick |
| MIX-022 | Audio setup: device, sample rate, buffer size | rb6 | Y p179 | Y p196 | Diff p223 (metronome volume) | Partial | `dlgprefsounddlg.ui`; PAR-M12 | Needs wiring: the DDJ-800 runs 44.1 kHz / 24-bit (6 out / 6 in, FW §2.7) and rekordbox uses 256-frame buffers (~6 ms, the jog timing reference, DECK-003); preset these when the deck is chosen. CDJ-3000 file and output specs are hardware figures only. | C-256, PAR-M12, A:AC-001, A:AC-006, D-081 |
| MIX-023 | Audio input channels (recording); Setting Utility button | rb6 | Y p179 | Y p196 | Y p223 | Partial | Sound Hardware page | Inputs exist; no Setting Utility launcher (Windows-only Pioneer tool). The DDJ-800's USB inputs carry no programme audio (FW §2.7). | C-257, B-235 |
| MIX-024 | Mixer Mode Internal / External (separate outputs per deck, sampler, preview) | rb6 | Y p179 | Y p196 | Y p223 | Partial | Sound Hardware "Main Mix" on/off and deck outputs | Expose it as one switch; sampler and preview outputs unverified. | C-216, PAR-C9, RM§6 P2 internal/external |
| MIX-025 | Output channels Master / Headphones / Booth; per deck / sampler / preview routing | rb6 | Y p179 | Y p196 | Y p224 | Built | Sound Hardware page | | C-217, PAR-C10 |
| MIX-026 | Preview volume | rb7-add | Diff p179 (PERFORMANCE only) | Diff p196 (PERFORMANCE only) | Y p223 (general) | Partial | preview deck volume | rb7 makes it a general audio setting; adopt (small, helps Prepare mode). | B-234 |

## Fader start

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| MIX-027 | SHIFT + channel fader: up plays from the cue, down returns to the cue | rb6 | Y p183 | Y | Y | Partial | JS:674-676 (`play`, `cue_gotoandstop`) | Works only if the deck's fader-start setting is on (HWL-048, never sent; default unknown). Needs a cue point (manual p100). | A:Q-001, A:Q-002, C-219, PAR-C8, A:AC-005, RM§6 P1 fader start |
| MIX-028 | SHIFT + crossfader: start / back cue for the decks on that side | rb6 | Y | Y | Y | Partial | JS:392-404 | Decks chosen by Mixxx crossfader orientation; same setting dependency. | A:Q-005, A:Q-006 |
| MIX-029 | Fader start sets the cue while paused (channel fader and crossfader) | rb6 | Y | Y | Y | Build | `cue_gotoandstop` jumps, does not set | | A:Q-003, A:Q-007 |
| MIX-030 | Fader start with no cue plays from the track start | rb6 | Y | Y | Y | Partial | Mixxx cue defaults | Depends on the load point setting (bucket 09, PAR-B31). | A:Q-004 |

## Microphone

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| MIX-031 | MIC ON/OFF switch | rb6 | Diff p142 | Diff p152 | Diff p169 (+ mic EQ) | Partial | JS:406-412 (`[Microphone],talkover`) | **Doubled-audio row.** On this deck the mic is mixed in hardware (not via PC, p78); if a Mixxx mic input is also set, the mic is heard twice; without one, MIC ON does nothing. Mic panel layout (rb7 adds EQ) is bucket 10b. Open question 1. | A:S-001, C-220, PAR-C12 |
| MIX-032 | Mic hardware path: MIC 1/2 inputs, MIC ATT, mic EQ and levels, not-via-PC limits | DDJ-800 | - | Y CSV B605-B60F (Effect Mode only) | = rb6 | Skip | manual p29-33, p78 | Deck hardware handles it. Booth output, limiters, TALK OVER and Mic FX do not work on this path. | A:X-001, A:X-002, A:X-003, A:S-017, A:S-018 |
| MIX-033 | TALK OVER: level -6 / -12 / -18 / -24 dB, Normal / Advanced (mids only), trigger at -10 dB | rb6 | Y p143 | Y p153 | Y p170 | Partial | `enginetalkoverducking.cpp:23, 39` | Mixxx ducks full-band with a continuous strength; add rb6's level steps and Advanced mode. On this deck the mic is in hardware, so Mixxx cannot hear it to duck (open question 1). DJM-900NXS2 gives the trigger level. | A:S-023, A:X-006, A:X-007, A:AB-016, A:AB-017, C-221, D-134 |
| MIX-034 | Mic FX (on/off, select, level) | rb6 | Y p142 | Y p152 | Y p169 | Build | effect unit on `[Microphone]` possible | No DDJ mic path into Mix Two; needs Microphone Input Mode "Effect" (HWL-052) and a test. Mic FX itself: FX-021. | A:X-005 |

## External inputs and AUX

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| MIX-035 | Input selector PHONO / LINE and passthrough through Mix Two | rb6 | - | Y CSV 9057, 9046 | = rb6 | Partial | XML:36-42 (`inputSelectPassthrough`, default off); JS:678-687 | **Doubled-audio row.** Under Mix Two the deck sends external audio out without the PC (p79); turning passthrough on may double it. Needs a deck test. Open question 2. | A:S-003, A:Y-003 |
| MIX-036 | External source hardware path: rear LINE/PHONO switches, SIGNAL GND, the not-via-PC feature table, level jumps between paths | DDJ-800 | - | - | - | Skip | manual p32, p79-81 | Deck hardware handles it (TRIM, EQ, CUE, faders, cut lag work; CFX, Beat FX, fader start, limiters do not). | A:Y-001, A:Y-002, A:Y-004, A:Y-006 |
| MIX-037 | AUX input, AUX gain switch, AUX TRIM; no effects on AUX | DDJ-800 | - | - | - | Skip | manual p29, p33, p65, p82 | Deck hardware handles it; AUX never reaches the PC. | A:S-016, A:Y-005, A:T-017 |
| MIX-038 | SEND / RETURN with an AUX return channel | Owner? | - | N (FX1/FX2 send/return is DJM-only, C-177) | N | Build | DJM p14 | DJM-900NXS2 extra: the return comes back on its own channel with fader and EQ. Mixxx aux inputs exist. FX1/FX2 as send/return belongs to bucket 02. Open question 3. | D-133 |
| MIX-039 | SMART FADER (automatic BPM transition across a fade) | Owner? | N | str 6.8.5 | str 7.2.19 | Build | PAR-C13 | See PERF-081 (Skip; open question there). | - |

## Rear and front panel

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| MIX-040 | Power, DC IN, cord hook, Kensington slot | DDJ-800 | - | - | - | Skip | manual p31, p33 | Deck hardware. | A:Z-001, A:Z-006 |

## Open questions for the owner

1. **Microphone (MIX-031, MIX-033, MIX-034).** The DDJ-800 mixes the mic in
   hardware under Mix Two. Options: (a) MIC ON/OFF does nothing in Mix Two;
   (b) keep it driving Mixxx's mic channel (risk: mic twice); (c) build a
   via-PC path with Mic FX and talkover (needs a test of Microphone Input
   Mode "Effect" in MIDI mode). *Recommendation:* (a) now, (c) later if you
   want Mic FX.
2. **External inputs (MIX-035).** Options: (a) leave PHONO / LINE to the
   deck's hardware mixer (no software FX); (b) route them through Mix Two
   with passthrough (only if the deck then stops mixing them itself).
   *Recommendation:* test what the deck does under Mix Two; default (a).
3. **Optional mixer extras (MIX-038, MIX-039).** DJM-900NXS2 SEND / RETURN
   with an AUX return channel, and rekordbox's SMART FADER. Options: build
   either, both, or skip. *Recommendation:* skip both for now; revisit
   SEND / RETURN with the Hardware Design skin (bucket 10c).

<!-- End of AI-generated text. -->
