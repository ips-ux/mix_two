<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 11: Hardware link

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** Medium  ·  **Scope:** how Mix Two talks to the DDJ-800 beyond the mapping: detection, auto-enable, MIDI hotplug, the HID interface (jog displays, HID input), the effect display, the DDJ-800 hardware settings page (settings sent to the deck), the rekordbox handshake rule, runtime captures and the own audio driver (Windows-only now, DEC-§8).

Reading notes:
- Src prefixes as in bucket 01 (`A:` = inventory A, `C-` / `D-` = inventories C / D, `PAR-`, `RM§6`, `AGL`, `DEC-§n`, `F<n>` = ROADMAP finding, `FW` = FIRMWARE.md, `SE` = SCRATCH-ENGINE.md).
- The deck treats Mix Two as "rekordbox not running" (it never passes the rekordbox verification, which Mix Two must not imitate: F23, FW §2.7). Everything the manual says for that case applies.
- Settings page (DEC-§9): rekordbox 6 has a DDJ-800 preference page, so Mix Two matches it. rekordbox sends each setting as a Note/CC listed in its CSV; the firmware accepts host Note/CC (FW §2.7). Value encodings still need a capture (U3).
- Jog display (18 elements): no HID output exists in Mix Two. The deck feeds the displays from per-deck HID OUT reports whose content is not decoded (F22, U2). Troubleshooting (manual p99) says the displays stay blank in forced general-MIDI mode, so the deck may refuse display data from a host it treats as "not rekordbox" (HWL-033).

## Connection and detection

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| HWL-001 | Mapping comes up on connect (auto-enable by device name) | rb6 | Y p122 | Y p128 | Y p145 | Built | `<product device_name="DDJ-800">` (XML:13-15); ControllerManager; `src/test/midiproductmatch_test.cpp` | Owner reported OK on deck 2026-10-26 ("verified on hardware", AGL): confirm. Setting `[Controller],auto_enable_matching_mappings` (default on). | A:Z-010, C-241, PAR-M1 |
| HWL-002 | USB connection, MIDI hotplug, open retry for 30 s | rb6 | - | Y | Y | Built | `src/controllers/midi/midiportwatcher.*`; `midiportwatcher_test.cpp`; ROADMAP F15 | Owner reported OK on deck 2026-10-26 (hotplug enabled the deck on replug): confirm. Linux source `/proc/asound/seq/clients` exists. Trade-offs: any MIDI change reopens all MIDI controllers; HID not hotplugged. No USB hub (manual p33). | A:Z-005, F15 |
| HWL-003 | General MIDI mode; rekordbox verification not imitated; no keep-alive | DDJ-800 | - | - | - | Built | FW §2.7; ROADMAP F23 | Utilities setting SLIP REVERSE lit = always general MIDI; either setting works for Mix Two, but forced MIDI may blank the jog displays (p99). Script header JS:48 still lists keep-alive as open; fixed in 1d3e9f8a05. | A:AC-003, A:AA-002, F23 |
| HWL-004 | Utilities mode on the unit (enter, save, leave) | DDJ-800 | - | - | - | Skip | manual p83-84 | Deck hardware handles it. | A:AA-001, A:AA-003 |
| HWL-005 | Startup guard when rekordbox runs; Pioneer ASIO crash | Improve | - | - | - | Built | `src/coreservices.cpp` (`isRekordboxRunning`, Retry / Start Anyway / Quit); ROADMAP F16 | rekordbox holds the deck, and `ddj-800_asio64.dll` crashes Mix Two at launch while rekordbox runs. Windows-only, which is fine: Linux has no Pioneer driver (DEC-§8). | F16, AGL 2026-10-27 rekordbox guard |
| HWL-006 | Control Device Information (connected MIDI / HID devices) | rb6 | Y p179 | Y p197 | Y p223 | Built | Preferences > Controllers | | C-250 |
| HWL-007 | Driver / firmware version display, driver install | DDJ-800 | - | - | - | Skip | manual p93, p105-109 | Pioneer's own tools. | A:AC-002, A:AC-007 |

## HID interface and runtime captures

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| HWL-008 | Runtime capture of rekordbox with the deck: HID OUT (jog displays), HID IN (EP 0x87), settings page traffic | rb6 | - | - | - | Build | ROADMAP U2, U3, WS-A; SE next steps 5(3) | One session should answer the jog display, effect display, HID jog report and settings questions. Open question 1. Windows capture tools (USBPcap) only: Linux capture path is bucket 14. | RM§6 HW DDJ-800 runtime capture |
| HWL-009 | HID input: absolute 16-bit jog counters, touch flags, CH faders and crossfader | rb6 | - | native (HidMapDDJ1000, F26) | = rb6 | Build | ROADMAP F22, F26; FW §2.7 | rekordbox reads jogs, CH faders and crossfader from the HID report; how it enables the report is unknown. Mix Two uses MIDI (jog clamp +-63 steps per ms). | F26, F22 |
| HWL-010 | rekordbox-internal jog parameters (JogIndicatorInterval 12, MidiOutInterval 2) | rb6 | - | Y CSV FFF1 / FFF3 | = rb6 | Build | CSV | Unverified meaning; possibly the jog indicator refresh. Settle with HWL-008. | A:AD-016 |
| HWL-011 | Using CDJ / XDJ players over USB HID | Skip | Y p124 | Y p130 | Y p147 | Skip | ROADMAP F24, F29 | No CDJ available; the CDJ-3000 skin waits for gear (DEC-§7). | C-242 |
| HWL-012 | "Audio Output OFF/ON" message (9F2E) | rb6 | - | Y CSV 9F2E | = rb6 | Build | not sent | Unverified meaning; audio already works without it. | A:AD-005 |

## Jog display: INFO and ARTWORK screens

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| HWL-013 | INFO: deck number | DDJ-800 | - | native | = rb6 | Build | FW §2.6 | | A:R-001 |
| HWL-014 | INFO: artwork (can be hidden) | DDJ-800 | - | native | = rb6 | Build | | | A:R-002 |
| HWL-015 | INFO: key variation | DDJ-800 | - | native | = rb6 | Build | | | A:R-003 |
| HWL-016 | INFO: key | DDJ-800 | - | native | = rb6 | Build | | | A:R-004 |
| HWL-017 | INFO: CUE SCOPE (4 bars before to 16 after) | DDJ-800 | - | native | = rb6 | Build | | | A:R-005 |
| HWL-018 | INFO: deck status background (on air, deck 3/4, slip) | DDJ-800 | - | native | = rb6 | Build | | | A:R-006 |
| HWL-019 | INFO: cue point | DDJ-800 | - | native | = rb6 | Build | | | A:R-007 |
| HWL-020 | INFO: BPM | DDJ-800 | - | native | = rb6 | Build | | | A:R-008 |
| HWL-021 | INFO: playing speed (tempo %) | DDJ-800 | - | native | = rb6 | Build | | | A:R-009 |
| HWL-022 | INFO: tempo range | DDJ-800 | - | native | = rb6 | Build | | | A:R-010 |
| HWL-023 | INFO: waveform (per rekordbox waveform prefs) | DDJ-800 | - | native | = rb6 | Build | | | A:R-011 |
| HWL-024 | INFO: cue / loop / hot cue marks | DDJ-800 | - | native | = rb6 | Build | | | A:R-012 |
| HWL-025 | INFO: playback position | DDJ-800 | - | native | = rb6 | Build | | | A:R-013 |
| HWL-026 | INFO: time (elapsed / remaining; loop length in beats) | DDJ-800 | - | native | = rb6 | Build | | | A:R-014 |
| HWL-027 | INFO: MASTER indicator | DDJ-800 | - | native | = rb6 | Build | | | A:R-015 |
| HWL-028 | INFO: playback status (rotates while playing) | DDJ-800 | - | native | = rb6 | Build | | | A:R-016 |
| HWL-029 | INFO: SYNC indicator | DDJ-800 | - | native | = rb6 | Build | | | A:R-017 |
| HWL-030 | ARTWORK screen (deck number, status, artwork, SYNC, cue, MASTER) | DDJ-800 | - | native | = rb6 | Build | | | A:R-018 |
| HWL-031 | SHIFT pressed twice toggles INFO / ARTWORK | DDJ-800 | - | native | = rb6 | Build | JS:689-693 (SHIFT only tracked) | | A:B-009 |
| HWL-032 | Jog display mode / Switch JOG Display preference | rb6 | Y p178 | Y p194 | Y p220 | Build | PAR-M5 | Jog rotation speed (also in PAR-M5) is DECK-009. | C-246, PAR-M5, RM§6 P2 jog display mode |
| HWL-033 | Risk: displays blank while the deck is in general MIDI mode | DDJ-800 | - | - | - | Build | manual p99 | Unverified; HWL-008 should show whether the deck draws host data outside rekordbox mode. | A:AC-004 |
| HWL-034 | End-of-track warning on the jog display / ring | CDJ-3000 | - | My Settings (JOG RING INDICATOR) | - | Build | Mixxx `end_of_track` | CDJ-3000 blinks the jog ring as time runs out. Drive the DDJ-800 jog display from Mix Two's warning once HWL-013 to 030 work. | D-073 |

## Effect display (BEAT FX section)

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| HWL-035 | Effect display: effect name | DDJ-800 | - | native | = rb6 | Build | FW §2.1 (`oled`, `oled_display` tasks) | Unverified: unknown whether the firmware draws it from the selector or the host must send it. Mix Two sends nothing. | A:T-001 |
| HWL-036 | Effect display: AUTO / TAP | DDJ-800 | - | native | = rb6 | Build | | Unverified, as HWL-035. | A:T-002 |
| HWL-037 | Effect display: BPM (flashes when not detected) | DDJ-800 | - | native | = rb6 | Build | | Unverified, as HWL-035. | A:T-003 |
| HWL-038 | Effect display: beat / parameter (blinks at limits) | DDJ-800 | - | native | = rb6 | Build | | Unverified, as HWL-035. | A:T-004 |
| HWL-039 | Effect display: selected channel | DDJ-800 | - | native | = rb6 | Build | | Unverified, as HWL-035. | A:T-005 |

## DDJ-800 settings page (settings sent to the deck)

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| HWL-040 | DDJ-800 preference page in Mix Two | rb6 | Y | Y (DDJ800PreferencePage, F5) | Y | Build | mapping options only (XML:19-63) | DEC-§9: rb6 has one: Match. Mix Two sends none of the settings, so the deck runs on its power-on defaults. Open question 2 (order). | C-223, DEC-§9 settings page |
| HWL-041 | Jog display brightness 1-5 | DDJ-800 | - | Y CSV BF48 | = rb6 | Build | not sent | | A:AB-001 |
| HWL-042 | Jog display start screen INFO / ARTWORK | DDJ-800 | - | Y CSV 9F4F | = rb6 | Build | not sent | | A:AB-002 |
| HWL-043 | Display artwork (INFO) | DDJ-800 | - | Y (9F4F, Inferred) | = rb6 | Build | | | A:AB-003 |
| HWL-044 | Display CUE SCOPE (INFO) | DDJ-800 | - | Y | = rb6 | Build | | Message unknown (not in CSV). | A:AB-004 |
| HWL-045 | Time mode on the jog display | DDJ-800 | - | Y | = rb6 | Build | | Mix Two's screen has its own time / remain toggle. | A:AB-005 |
| HWL-046 | Slip Mode Flashing enable / disable | DDJ-800 | - | Y CSV 9F40 | = rb6 | Build | not sent | The slip state itself is sent (CTL-055). | A:AB-006 |
| HWL-047 | SLIP LED lit / blink while slip is on | DDJ-800 | - | Y CSV 9F41 | = rb6 | Build | not sent | | A:AB-007 |
| HWL-048 | Fader Start on / off (default on) | DDJ-800 | - | Y CSV 9F42 | = rb6 | Build | not sent | Fader start mapping depends on it (MIX-027 to MIX-030); the deck's default under Mix Two is unknown. | A:AB-009, A:Q-008 |
| HWL-049 | Cross Fader Cut Lag 0.5-5.7 mm (default 1.0) | DDJ-800 | - | Y CSV BF41 | = rb6 | Build | not sent | The deck applies cut lag itself; Mixxx has no cut-lag setting. | A:AB-011 |
| HWL-050 | Mic output to booth on / off | DDJ-800 | - | Y CSV 9F4D | = rb6 | Build | not sent | Disabled anyway on the not-via-PC mic path (MIX-032). | A:AB-012, A:X-008 |
| HWL-051 | Effect display brightness 1-3 | DDJ-800 | - | Y CSV BF4A | = rb6 | Build | not sent | | A:AB-013 |
| HWL-052 | Microphone Input Mode: Effect (via PC) / Low Latency | DDJ-800 | - | Y CSV 9F4B | = rb6 | Build | not sent | Unknown whether the deck streams the mic over USB in MIDI mode; decides MIX-031 / MIX-034. | A:AB-014, A:X-004 |
| HWL-053 | Mic Low Cut on / off | DDJ-800 | - | Y CSV 9F4C | = rb6 | Build | not sent | | A:AB-015, A:X-009 |
| HWL-054 | Master attenuator 0 / -6 / -12 dB | DDJ-800 | - | Y CSV BF42 | = rb6 | Build | not sent | | A:AB-018 |
| HWL-055 | Master mono / stereo | DDJ-800 | - | Y CSV 9F47 | = rb6 | Build | not sent | | A:AB-019 |
| HWL-056 | Booth attenuator 0 / -6 / -12 dB | DDJ-800 | - | Y CSV BF43 | = rb6 | Build | not sent | | A:AB-020 |
| HWL-057 | Booth mono / stereo | DDJ-800 | - | Y CSV 9F48 | = rb6 | Build | not sent | | A:AB-021 |
| HWL-058 | Master Output / Booth Output peak limiter in the deck | DDJ-800 | Y | Y CSV 9F49 | Y | Build | not sent | Unit-side limiter; active with USB and rekordbox (manual p79-80); whether it is active under Mix Two is unverified. Software limiter on the main mix: MIX-017. | A:AB-022, C-224 |
| HWL-059 | Mic peak limiter on master / booth | DDJ-800 | - | Y CSV 9F4A, 9F4E | = rb6 | Build | not sent | | A:AB-023, A:AB-024, A:X-010 |
| HWL-060 | Demo mode (1 / 5 / 10 min), screen saver, off | rb6 | - | Y CSV BF44 | = rb6 | Build | not sent | Unknown whether the deck's demo starts while Mix Two runs. | A:AB-026, PAR-M8, RM§6 P3 controller demo |
| HWL-061 | Auto standby after 4 h idle (default on) | DDJ-800 | - | Y CSV 9F45 | = rb6 | Build | not sent | Deck default (on) applies; it could switch the unit off during a long quiet idle. | A:AB-027 |
| HWL-062 | Mixer Mode 2-ch / 4-ch | rb6 | - | Y CSV 9F46 | = rb6 | Build | - | See MIX-011. | (see MIX-011) |

## Own audio driver

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| HWL-063 | Mix Two's own audio + MIDI + HID driver (WinUSB, vendor-class isochronous audio) | Improve | - | - | - | Build | ROADMAP WS-B, F2, F21 | Windows-only now (DEC-§8): Linux plays the DDJ-800 through `snd-usb-audio` (kernel 6.4+). Reason: Pioneer's ASIO driver crashes (F16) and is Windows-only. Largest risk; last. | DEC-§8 own audio driver, RM§6 HW own driver |

## Open questions for the owner

1. **Capture session (HWL-008).** The jog displays, effect display, HID jog
   input and the settings encodings all need one USB capture of rekordbox 6
   with the deck. Options: (a) schedule it now; (b) after the Beat FX work.
   *Recommendation:* (a), one short session; it also shows whether the deck
   draws display data outside rekordbox mode (HWL-033).
2. **Settings page order (HWL-040).** Which settings first? *Recommendation:*
   fader start (HWL-048), cut lag (HWL-049), master limiter and attenuator
   (HWL-058, HWL-054), demo and auto standby (HWL-060, HWL-061); brightness
   and display items after the jog display works.

<!-- End of AI-generated text. -->
