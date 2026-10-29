<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 01: Controller (DDJ-800 mapping)

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** Critical  ·  **Scope:** every DDJ-800 control and its SHIFT layer, the pad mode mapping, LEDs and controller-state messages, soft takeover, deck 3/4 switching, the browse knob and the jog *mapping* (jog feel and scratch sound are in bucket 03; mixer sound and fader start in 06; jog display, effect display and the settings sent to the deck in 11).

Reading notes:
- Src `A:x-nnn` = inventory A row (its IDs restart per manual section, so they carry the `A:` prefix); `C-`, `D-`, `B-` = inventories C, D, B; `PAR-xn` = PARITY.md section x, row n; `RM§6` = ROADMAP.md §6; `AGL` / `USL` = AGENT_ / USER_changelog.md; `DEC-§n` = DECISIONS.md.
- `JS:n` / `XML:n` = `res/controllers/Pioneer-DDJ-800-script.js` / `.midi.xml` lines at commit a5377896ab (inventory A). Commit 1d3e9f8a05 added about 10 lines after JS:246 (end of `init`).
- rb columns: `CSV` = function present in rekordbox's `DDJ-800.midi.csv` (identical in 6.8.5 and 7.2.19, ROADMAP F6); `native` = rekordbox handles it in code, not in the CSV; rb5 was not compared row by row (`-`).
- Owner's hardware test 2026-10-26: "most controls work" (AGL). That report names no single rows, so it is not repeated per row; bucket 12 should list it for confirmation.
- Rule from DEC-§1: where rekordbox 6 and the DDJ-800 manual disagree, rekordbox 6 wins.

## Browser section

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-001 | LOAD loads the browser track | DDJ-800 | - | Y CSV 9646-9649 | = rb6 | Built | JS:320-330, 349-350 | While the "Load to the Deck" prompt is open, LOAD picks the deck (Mix Two addition, CTL-009). | A:A-001, A:Z-009 |
| CTL-002 | Instant Doubles (LOAD twice) | rb6 | str | str (adds L/R vs up/down pairing) | Y p163 (+ drag title) | Partial | JS:339-351; basetrackplayer.cpp:212 | Pairs fixed 1-2 and 3-4 (JS:344); the manual means "the deck the other side controls now", so deck 3 cannot double deck 2. The first press loads the browser track before the clone. Check rb6.8.5's pairing setting and double-press window. rb7 drag-title copy is an rb7-add candidate (bucket 09). | A:A-002, C-072, PAR-M3 |
| CTL-003 | SHIFT + LOAD ejects the deck | Owner? | - | N (CSV "No function assigned") | = rb6 | Built | JS:321, 331-333 | Mix Two extra, not recorded as an Improve anywhere. Open question 2. | A:A-003 |
| CTL-004 | BACK moves focus / closes a folder | rb6 | - | Y CSV 9665 | = rb6 | Partial | JS:305-307 (`MoveFocusBackward`) | Mixxx focus cycling, may land on search; no "close folder". DEC-§6 drill-in sidebar: BACK goes up one level, as on a CDJ-3000; map BACK to that once bucket 10a builds it. | A:A-004 |
| CTL-005 | BACK visits the playlist palette | rb6 | - | Y | = rb6 | Build | - | Needs a palette; DEC-§6 pinned lists are the closest planned item (bucket 08 / 10a). | A:A-005 |
| CTL-006 | SHIFT + BACK shows / hides the playlist palette | rb6 | - | Y CSV 9666 | = rb6 | Partial | JS:308-310 (`MoveFocusForward`) | Mapped to reverse focus; no palette. | A:A-006 |
| CTL-007 | Rotary selector turn | DDJ-800 | - | Y CSV B640 | = rb6 | Built | JS:275-282 | Also picks the deck while the load prompt is open. | A:A-007 |
| CTL-008 | SHIFT + rotary zooms the waveform | DDJ-800 | - | Y CSV B664 | = rb6 | Partial | JS:284-289; controlpickermenu.cpp:1564 | **Defect: direction inverted** (right sends `waveform_zoom_up` = zoom out). All decks zoom together, as the CSV says. | A:A-008 |
| CTL-009 | Rotary press: open folder; on a track, "Load to the Deck" prompt | Improve | Y p182 (DDJ-RZ only) | Y p200 (DDJ-RZ only) | Y p227 | Built | JS:290-304; src/library/loadprompt.*; PAR-M2 | Owner decision 2026-10-26 (U7): both LOAD and knob-push always work, no preference. Not rb6 behaviour for the DDJ-800 (rb6 loads only by LOAD; knob push = "Library Forward"). Prompt look waits for bucket 10a. | A:A-009, C-071, PAR-M2 |
| CTL-010 | Rotary hold: Alphabet Jump / Page Jump | Owner? | - | N | N | Build | CDJ-3000 p37 | CDJ-3000 browse feature. Unknown whether the DDJ-800 sends a hold message. Open question 3. | D-074 |
| CTL-011 | VIEW enlarges the browser | DDJ-800 | - | Y CSV 967A | = rb6 | Built | JS:311-313 (`[Skin],show_maximized_library`) | The Browse layout itself belongs to bucket 10b (PAR-A3, C-007). | A:A-010 |
| CTL-012 | SHIFT + VIEW jumps to Related Tracks | rb6 | - | Y CSV 9668 | = rb6 | Build | JS:318 (comment, unbound) | Needs Related Tracks (bucket 08). | A:A-011 |
| CTL-013 | VIEW hold adds / removes the track in the Tag List | rb6 | - | Y CSV 9667 | = rb6 | Partial | JS:314-317 (`AutoDjAddBottom`) | Adds to Auto DJ, cannot remove; real Tag List is bucket 08. | A:A-012 |

## Transport, search, quantize, memory cues

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-014 | PLAY/PAUSE (and SHIFT + PLAY/PAUSE) | DDJ-800 | - | Y CSV 900B, 9047 | = rb6 | Built | JS:539-544 | LED from `play_indicator`. | A:B-001, A:B-002 |
| CTL-015 | CUE (set, Back Cue, Cue Sampler) | rb6 | - | Y CSV 900C | = rb6 | Partial | JS:541 (`cue_default`); dlgprefdeck.cpp:59-62 | Mapping is right; the gap is the deck default cue mode (Mixxx, not Pioneer/CDJ). Behaviour row is in bucket 09 (PAR-B1, C-078, D-020). | A:B-003 |
| CTL-016 | SHIFT + CUE jumps to track start | DDJ-800 | - | Y CSV 9048 | = rb6 | Built | JS:542 | | A:B-004 |
| CTL-017 | TRACK SEARCH previous | rb6 | - | Y CSV 905E | = rb6 | Built | JS:643 (`start`) | Check in rb6 whether a second press goes to the previous track. | A:B-005 |
| CTL-018 | TRACK SEARCH next | rb6 | - | Y CSV 905F | = rb6 | Partial | JS:644-647 | Loads the next row of the browser selection, not the next track of the list the deck's track came from. | A:B-006 |
| CTL-019 | TRACK SEARCH hold: fast forward / rewind | DDJ-800 | - | Y CSV 9070-9071 | = rb6 | Built | JS:648-649 | Search speed vs rb6 unverified. | A:B-007 |
| CTL-020 | Super Fast Search (TRACK SEARCH + jog) | DDJ-800 | - | native | = rb6 | Build | FW §2.2 (CC 0x29 while a key is held); no handler | Probably small once CC 0x29 is confirmed on the deck. CDJ-3000 has the same idea (SEARCH + jog). | A:B-008, D-015 |
| CTL-021 | SHIFT function list on screen (SHIFT held) | rb6 | - | Y CSV 967B; pref default on | = rb6 | Build | not bound | rb6 has it (DDJ-800 manual p90). PAR-M7 plan: generate the list from the mapping. DESIGN "Later" prefers hover tooltips over printed SHIFT labels on the skin; the overlay is a separate thing. | A:B-010, A:AB-025, PAR-M7, RM§6 P2 SHIFT list |
| CTL-022 | QUANTIZE button | rb6 | - | Y CSV 9035 | = rb6 | Partial | JS:639-640 | Per deck in Mix Two; rb6 has an "all decks" option and a beat value (bucket 09, C-116, C-118). | A:B-011 |
| CTL-023 | QUANTIZE (right) wakes the unit from standby | DDJ-800 | - | - | - | Skip | manual p15, p91 | Deck hardware handles it. | A:B-012 |
| CTL-024 | MEMORY stores a memory cue / loop | DDJ-800 | - | Y CSV 903D | = rb6 | Built | JS:617, 732-736; cuecontrol.cpp:306 | | A:B-013 |
| CTL-025 | DELETE deletes the called cue / loop | DDJ-800 | - | Y CSV 903E | = rb6 | Built | JS:618, 738-741; cuecontrol.cpp:310 | | A:B-014 |
| CTL-026 | CUE/LOOP CALL left / right | DDJ-800 | - | Y CSV 9051, 9053 | = rb6 | Built | JS:615-616, 726-730 | "CALL while playing" still to check in rb6 (RM§6, bucket 09). | A:B-015, A:B-016 |
| CTL-027 | SHIFT + LOOP IN / OUT / QUANTIZE / CALL / DELETE / MEMORY / TEMPO RANGE / MASTER TEMPO do nothing | rb6 | - | Y CSV "No function assigned" | = rb6 | Built | none bound in JS | Correctly unassigned. | A:B-017 |

## Loops

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-028 | LOOP IN | DDJ-800 | - | Y CSV 9010 | = rb6 | Built | JS:591-593, 703-709 | | A:C-001 |
| CTL-029 | Loop in / out adjust with the jog; 1/2X and 2X | rb6 | Y | Y | Y | Built | JS:714-723, 831-835, 600-603; 50 samples per tick (JS:124) | Adjust is a toggle; LOOP IN LED shows adjust mode; feel unverified. CDJ-3000 leaves adjust after 10 s without input: adopt only if rb6 does the same (check rb6). | A:C-002, A:C-005, C-112, D-030 |
| CTL-030 | LOOP IN held 1 s: 4-beat auto loop | DDJ-800 | - | Y CSV 9014 | = rb6 | Built | JS:597 | The deck sends the long-press note itself. | A:C-003 |
| CTL-031 | LOOP OUT | DDJ-800 | - | Y CSV 9011 | = rb6 | Built | JS:594-596 | | A:C-004 |
| CTL-032 | RELOOP/EXIT | DDJ-800 | - | Y CSV 904D | = rb6 | Built | JS:598, 602 | | A:C-006 |
| CTL-033 | SHIFT + RELOOP/EXIT: Active Loop | rb6 | - | Y CSV 9050 | = rb6 | Build | JS:599 (`reloop_andstop`) | **Wrong function mapped** (jumps to the loop and stops). Rule: follow rb6. Unbind it until Active Loop exists in the engine (bucket 09, PAR-B23, C-109). | A:C-007 |
| CTL-034 | CALL = LOOP 1/2X and 2X during a loop | DDJ-800 | - | Y CSV 9012, 9013 | = rb6 | Built | JS:600-612 (sends `9F 04+deck` loop state) | | A:C-008, A:C-009 |
| CTL-035 | 1/2X / 2X while paused at the loop in point keeps the length | DDJ-800 | - | - | - | Build | Mixxx `loop_halve` / `loop_double` | Unverified; Mixxx probably changes the length. Minor. | A:C-010 |
| CTL-036 | Controller and screen both control loops | Improve | Y p132 | Y p140 | Y p157 | Built | FUNCTIONS "engine owns behaviour" | rekordbox hands loop control to a connected controller; Mix Two keeps both working (one engine control for both). | C-243 |

## Tempo, sync and key buttons

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-037 | BEAT SYNC press toggles; LED | rb6 | - | Y CSV 9058 (Blink=600) | = rb6 | Partial | JS:568-570 | Toggle works; the CSV's LED blink option is not reproduced (LED steady). Sync behaviour is bucket 09. | A:D-001, A:AD-008 |
| CTL-038 | SHIFT + BEAT SYNC: MASTER | rb6 | - | Y CSV 905C | = rb6 | Built | JS:571-578 | Press on the master returns to automatic choice (Mix Two rule, PAR-B2). | A:D-002 |
| CTL-039 | KEY SYNC button | rb6 | - | Y CSV 9065 | = rb6 | Partial | JS:579-587; keycontrol.cpp:38 | Button and LED work; the target-key rule is bucket 09 (C-089, D-005). | A:D-003 |
| CTL-040 | SHIFT + KEY SYNC: key reset | DDJ-800 | - | Y CSV 9064 | = rb6 | Built | JS:588 | Also used in key shift mode (manual p55). | A:D-004, A:N-003 |
| CTL-041 | TEMPO RANGE button | DDJ-800 | - | Y CSV 9060 | = rb6 | Built | JS:564-566; ratecontrol.cpp:141 | Widens itself while synced (Mix Two rule). Button LED not driven; what rb6 shows on it is unknown. Ranges and resolution: DECK-014. | A:D-005, A:AD-009 |
| CTL-042 | MASTER TEMPO button | DDJ-800 | - | Y CSV 901A | = rb6 | Built | JS:562-563 | See DECK-016 for the Master Tempo sound. | (see DECK-016) |
| CTL-043 | TEMPO slider with soft takeover; slider BPM on screen | rb6 | - | Y CSV B000 / B005 | = rb6 | Built | JS:548-561; ratecontrol.cpp:834-847 | Slider catches up after SYNC; deck hub shows `tempo_slider_bpm` (red-orange until matched). CDJ-3000 does the same (p66). | A:D-007, PAR-B9, D-050 |

## Jog mapping (feel and sound: bucket 03)

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-044 | Jog top / side: scratch, bend, paused jog | rb6 | - | native | = rb6 | Built | JS:756-848 | See DECK-001 to DECK-005. | (see DECK-001) |
| CTL-045 | SHIFT + jog top: beat grid | rb6 | - | native (CSV B01F commented; GridAdjust 9079) | = rb6 | Partial | JS:760-765 (`gridTurn(..., true)` = slide); JS:651-655 | Mix Two slides the grid, as the owner reports rekordbox does (AGL 2026-10-28); the manual (p20) says interval adjust. rb6 wins (DEC-§1). GRID ADJUST / GRID SLIDE states are tracked but the SHIFT jog CCs bypass them. Script header still mentions "GRID buttons" (JS:28-29), which the DDJ-800 does not have. Open question 1. | A:E-005, A:AD-007 |
| CTL-046 | SHIFT + jog side: slide the beat grid | rb6 | - | native (CSV B026; GridSlide 900A) | = rb6 | Built | JS:765, 805-813; bpmcontrol.cpp:133 (`beats_translate_ms`) | 0.05 ms per tick, immediate (owner: gentle but responsive). rekordbox's distance per turn is not measured. Unit-tested `BeatsTranslateTest.TranslateByMilliseconds`; not felt on the deck. | A:E-006, B-160 |
| CTL-047 | Jog touch | DDJ-800 | - | Y CSV 9036 / 9067 | = rb6 | Built | JS:767-776, 786-803 | SHIFT + touch never scratches. | A:E-007 |
| CTL-048 | VINYL mode: default on, SHIFT + SLIP toggles, deck reports its state, check via SHIFT | DDJ-800 | - | Y CSV 9017, 903A | = rb6 | Built | XML:19-24; JS:629-637, 696-699 | The deck keeps its own vinyl state; the script follows (FW §6.6). LED check assumes the deck shows the SHIFT-layer LED while SHIFT is held. CDJ-3000 JOG MODE is the same idea. | A:E-008, A:F-003, A:F-004, A:AD-006, D-010 |

## Slip and reverse

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-049 | SLIP button | DDJ-800 | - | Y CSV 9040 | = rb6 | Built | JS:620-625 | | A:F-001 |
| CTL-050 | SLIP LED flashes while slipping | DDJ-800 | - | Y CSV 903B | = rb6 | Partial | JS:622 (steady) | Mixxx has no "slipping now" control; the setting that chooses lit / blink is HWL-047. | A:F-002 |
| CTL-051 | SLIP REVERSE hold, cancels after 8 beats; LED | rb6 | Y | Y | Y | Partial | JS:626 (`reverseroll`) | No 8-beat auto cancel; LED (9015 out) not driven. CDJ-3000's lever has no time limit: rb6 wins. Check that jog direction is right in reverse. | A:F-005, A:AD-010, D-025 |
| CTL-052 | SHIFT + SLIP REVERSE: latching reverse | DDJ-800 | - | Y CSV 9038 | = rb6 | Built | JS:627-628 | | A:F-006 |
| CTL-053 | Slip hot cue: release returns to the background position | DDJ-800 | - | Y | = rb6 | Partial | JS:980-982 | Unverified: whether Mixxx's hot cue release ends the slip jump as rekordbox does needs a deck test. | A:F-008 |
| CTL-054 | Slip with auto beat loop and manual loop | DDJ-800 | - | Y | = rb6 | Built | Mixxx slip with loops | | A:F-010, A:F-011 |
| CTL-055 | Slip-only buttons flash in slip mode | DDJ-800 | - | Y CSV 9F24-9F27 | = rb6 | Partial | JS:623-625 | The script sends the slip state; the enable setting (9F40) is never sent (HWL-046). | A:F-012, A:AD-003 |

## Pad mode buttons and PAGE

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-056 | Pad mode buttons (HOT CUE, PAD FX1, BEAT JUMP, SAMPLER and their SHIFT modes) | rb6 | str | str (+ SP.SCRATCH) | str (+ STEMS) | Built | JS:75-87, 861-865, 917-924 | Mode select and mode LEDs work for all 8 hardware modes. rb6's extra controller modes (SAMPLE SCRATCH, STEMS) have no DDJ-800 button. | A:G-001, A:G-002, A:G-003, A:G-004, A:G-005, A:G-006, A:G-007, A:G-008, C-141 |
| CTL-057 | PAGE left / right; page-2 pad notes; page permission flags; PAGE LEDs | rb6 | - | Y CSV 9024-9033, Permission_PadModeN | = rb6 | Partial | JS:866-872, 926-955; JS:881-883 (`pad = midino & 0x07`); JS:240-242 | Page-2 notes (0x08-0x0F) are folded onto pads 1-8, so no mode has a real second page. Meaning of the permission flags unknown (all on at start; rb6 may set them per mode). PAGE LEDs not driven. | A:G-009, A:AD-001, A:AD-012 |
| CTL-058 | SHIFT + PAGE: sampler bank | rb6 | Y p158 | Y p172 | Y p188 | Partial | JS:873-878, 957-966 | 8-slot banks derived from `num_samplers`. DEC-§9: sampler is 4 x 16 (check rb6, then build); the pads page through a 16-slot bank 8 at a time. Sampler engine and panel: bucket 09. | A:G-010, A:K-002, C-148 |

## Hot cue mode

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-059 | Pad sets / plays a hot cue | DDJ-800 | Y p105 | Y p110 | Y p127 | Built | JS:980-982 | Paused: Mixxx previews while held (GATE, PAR-B18). | A:H-001, A:H-002, C-142 |
| CTL-060 | SHIFT + pad clears the hot cue or hot loop | DDJ-800 | - | Y CSV +Shift | = rb6 | Built | JS:981 | | A:H-003, A:H-007 |
| CTL-061 | Hot cues I-P on page 2 (16 per track) | rb6 | Y p134 | Y p142 | Y p159 | Build | JS:881-883 (folds to A-H); JS:932-934 (PAGE calls `memory_cue_prev/next`) | DEC-§9: 16 hot cues on two pages. **Two defects:** page-2 pads trigger A-H, and PAGE in hot cue mode moves the playhead to a memory cue. Engine has 36 hot cues; needs mapping work plus an on-screen page indicator. | A:H-004 |
| CTL-062 | Pad during a loop saves a hot loop | DDJ-800 | Y | Y | Y | Built | cuecontrol.cpp:1051-1057 | | A:H-005, A:H-006 |
| CTL-063 | Pad LED colours | rb6 | - | Y | = rb6 | Partial | JS:47, 1065-1084 (0x7F / 0x00) | No colours; whether the pads take a palette is unknown. Page-2 LEDs never lit. | A:H-008, A:AD-014 |

## Pad FX modes on the pads (Pad FX content: bucket 02)

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-064 | SHIFT + pad in PAD FX1 acts as the pad | rb6 | - | Y CSV +Shift same function | = rb6 | Built | JS:983-988 | | A:I-003 |
| CTL-065 | PAGE in PAD FX2 (pads Y-AF) | rb6 | Y | Y | Y | Build | JS:931-953 (no `padFx2` case) | PAGE does nothing in PAD FX2. The Pad FX banks themselves, Release FX pad and Pad FX assignment are bucket 02. | A:I-005, C-143 |
| CTL-066 | Pad FX banks, Release FX pad, on-screen Pad FX area | rb6 | Y | Y | Y | Build | JS:983-1063 | See bucket 02 (inventory A I-001, I-002, I-004, I-006, I-007, I-008, AD-004). Release FX pad illumination (9F2A / 9F2B) is part of this row. | (see bucket 02), A:AD-004 |

## Beat jump mode

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-067 | Beat jump pads (1, 2, 4, 8 beats back / forward) and LEDs | DDJ-800 | Y p108 | Y p113 | Y p130 | Built | JS:99, 989-994, 1085-1087 | | A:J-001, A:J-003, C-144 |
| CTL-068 | PAGE changes the jump sizes | DDJ-800 | - | Y CSV 9026 / 902E | = rb6 | Built | JS:938-940 (x1/16 to x16) | rb6's limits unknown. | A:J-002 |
| CTL-069 | Screen shows the pad page's jump size | rb6 | - | Y | = rb6 | Build | JS uses `beatjump` with sizes, not `beatjump_size` | | A:J-004 |

## Sampler mode

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-070 | Pad plays the slot; again restarts it | DDJ-800 | - | Y CSV 9030-9037 | = rb6 | Built | JS:1002-1003 (`cue_gotoandplay`) | | A:K-004, A:K-005 |
| CTL-071 | SHIFT + pad: load / stop | rb6 | - | Y CSV "Sample Slot n stop" | = rb6 | Partial | JS:995-1006 | Mix Two: SHIFT + pad stops; a plain pad on an empty slot loads. Manual: SHIFT + pad loads (and stops a playing slot). rb6 (CSV) wins for stop; check in rb6 how an empty slot is loaded. No "do not overwrite" preference (bucket 09, C-191). | A:K-003, A:K-006 |
| CTL-072 | Deck 2 uses slots 9-16 | DDJ-800 | - | Y (manual p52) | = rb6 | Build | JS:964-966 | All decks address slots 1-8 of the bank. | A:K-007 |
| CTL-073 | Sampler pad LEDs show loaded / playing | rb6 | - | Y | = rb6 | Partial | JS:1088-1090 | Lit when loaded only. | A:K-010 |
| CTL-074 | SAMPLER LEVEL knob | DDJ-800 | - | Y CSV B603 | = rb6 | Built | JS:376-381 | Sets every sampler's `volume` (overwrites per-slot volume). | A:K-009, A:S-021 |

## Keyboard mode

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-075 | First press selects the hot cue; SHIFT + HOT CUE re-selects; no hot cue: current position becomes one | DDJ-800 | - | Y CSV 9040-9047, 9069 | = rb6 | Build | JS:221 (`keyboardCue: 1`), JS:1009-1019 | Always hot cue A; no selection step; no auto set. | A:L-001, A:L-002 |
| CTL-076 | Pads play the hot cue at semitone offsets | DDJ-800 | Y p109 | Y p114 | Y p131 | Built | JS:103, 1014-1015 | Pitch sound = key engine (DECK-017). | A:L-003, C-146 |
| CTL-077 | PAGE changes the pitch set | rb6 | - | Y CSV 9028 / 9030 | = rb6 | Partial | JS:944-946 (+-8 semitones) | rb6's page steps unknown: check rb6. | A:L-004 |
| CTL-078 | SHIFT + pad resets the key | Owner? | - | N (CSV blank) | = rb6 | Built | JS:1011-1012 | Mix Two extra. Open question 2. | A:L-005 |
| CTL-079 | Pad LEDs show the active pitch | DDJ-800 | - | Y | = rb6 | Built | JS:1091-1093 | | A:L-006 |

## Beat loop mode

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-080 | Pad starts a loop; same pad cancels; LEDs | DDJ-800 | Y p108 | Y p113 | Y p130 | Build | JS:1025-1029 sets `beatloop_size` then `beatloop_toggle`, which does not exist (loopingcontrol.cpp:130-150, 2150) | **Broken**: pads only change the size. Fix with `beatloop_<N>_toggle` or `beatloop_activate`. Inventory C and PAR-B22 call it Have (bucket 12). | A:M-001, A:M-003, A:M-006, C-145 |
| CTL-081 | Beat loop pad sizes and PAGE | rb6 | - | Y CSV 902A / 9032 | = rb6 | Partial | JS:100 (1/4 to 32), JS:947-949 | Manual: 1/64 to 2 beats; Mixxx stops at 1/32 (loopingcontrol.cpp:27). DEC-§9: check rb6's pad sizes before building (engine change if rb6 uses 1/64). | A:M-002, A:M-004 |
| CTL-082 | SHIFT + pad does nothing | rb6 | - | Y CSV "No function assigned" | = rb6 | Partial | JS:1025 (SHIFT ignored) | Minor. | A:M-005 |

## Key shift mode

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-083 | Key shift pad layout | rb6 | Y p109 | Y p114 | Y p131 | Partial | JS:102 (+1..+4 / -1..-4), JS:1031-1038 | Manual: +4..+7 / 0..+3. DEC-§9: check rb6's layout before building. SHIFT + pad also resets the key (extra, see CTL-078 question). | A:N-001, C-147 |
| CTL-084 | PAGE re-labels the pads | rb6 | - | Y CSV 902B / 9033 | = rb6 | Partial | JS:950-952 (`pitch_up` / `pitch_down`) | PAGE shifts the key one semitone instead. Follow rb6 with CTL-083. | A:N-002 |

## Deck 3/4 switching and soft takeover

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-085 | Soft takeover of TRIM, EQ, fader, COLOR on deck switch | rb6 | - | Y (4-ch mode, manual p57) | = rb6 | Partial | JS:662-666; JS:555 (takeover on `rate` only) | Switching a side to deck 3 makes deck 3's mixer controls jump to the knob positions. Mixer mode setting: MIX-011. | A:P-002 |
| CTL-086 | Input selector DECK1/3, DECK2/4: screen follows | rb6 | - | Y CSV 9055 / 9056 | = rb6 | Partial | JS:678-687 | The deck re-channels its own messages (works). The skin does not switch to 4 decks or show which deck each side controls (`[Skin],show_4decks`). | A:P-003 |

## FX and Sound Color FX controls (effect sound: bucket 02)

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-087 | BEAT FX SELECT (14 positions) | rb6 | Y CSV 5.8.7 | Y CSV 9420-942D | = rb6 | Partial | JS:466-472 (loads visible effect 1-14 by list position) | The user must order the visible effects list. Load by identity once bucket 02 ships the Pioneer effects (SOUND.md step 4). | A:T-009, C-163 |
| CTL-088 | BEAT FX CH SELECT (SAMPLER, CH1-4, MASTER) | rb6 | Y p114 | Y p119 | Y p137 | Built | JS:105-115, 474-478, 501-515 | Master cancels other assignments. | A:T-010, C-156, PAR-D4 |
| CTL-089 | CH SELECT LEFT / RIGHT codes | rb6 | - | Y CSV 941C / 941D | = rb6 | Partial | JS:113-114 (Ch1+3 / Ch2+4) | Unverified: when the deck sends them is unknown. | A:T-012 |
| CTL-090 | Beat FX ON/OFF and its flashing LED | DDJ-800 | - | Y CSV 9447 | = rb6 | Partial | JS:483-490 | LED steady, not flashing. | A:T-014, A:T-015 |
| CTL-091 | BEAT L/R, AUTO / TAP, LEVEL/DEPTH, SHIFT + ON/OFF (Release FX) | rb6 | - | Y CSV | = rb6 | Build | JS:480-501 | See bucket 02 (inventory A T-006, T-007, T-008, T-013, T-016). | (see bucket 02) |
| CTL-092 | SOUND COLOR FX buttons: select, switch, cancel; LED flash; start state | rb6 | - | Y CSV 9600-960B | = rb6 | Partial | JS:428-455 | LEDs steady, not flashing. Mix Two starts with FILTER selected (JS:432-435): check rb6's start state and copy it. Button 2 is **SWEEP** (DEC-§1). Effect presets: bucket 02. | A:W-001, A:W-002, A:W-003 |
| CTL-093 | COLOR knobs per channel | rb6 | - | Y CSV B617-B61A | = rb6 | Partial | JS:419-427 (quick effect `super1`) | Owner reports the mixer knobs, COLOR included, do not control Mix Two (2026-10-28; owner's other session, MIX-001). No takeover on deck switch (CTL-085). | A:S-022, A:W-004 |
| CTL-094 | SHIFT + HEADPHONES CUE: TAP | rb6 | - | Y CSV 9068 (`TapBpm`) | = rb6 | Partial | JS:669 (`bpm_tap`); bpmcontrol.cpp:253 (`tempo_tap`) | Manual: taps the deck's tempo; Mix Two re-tunes the track's BPM / grid. Check what rb6 does and follow it. LED not driven. | A:S-011, A:AD-011 |

## Meters, LEDs and controller-state messages

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-095 | Channel level indicators | DDJ-800 | - | Y CSV B002 out | = rb6 | Built | JS:670-672 (`vu_meter` x 127) | Scale and ballistics vs rb6 unverified. | A:S-020 |
| CTL-096 | Loop active state to the deck | rb6 | - | Y CSV 9F04-9F07 | = rb6 | Built | JS:603-606 | Makes CALL send 1/2X / 2X. | A:AD-002 |
| CTL-097 | LOOP IN / OUT LEDs during a loop | rb6 | - | Y CSV 9010 / 9011 out | = rb6 | Partial | JS:721-722 (lit only in adjust mode) | rb6's lighting unknown. | A:AD-013 |
| CTL-098 | All LEDs off on exit | DDJ-800 | - | Y | = rb6 | Built | JS:248-263 | | A:AD-015, A:Z-011 |
| CTL-099 | Deck indication settings (sync, slip, on-air, vinyl, countdown) | rb6 | - | Y prefs | = rb6 | Partial | PAR-M6 | Needs wiring: the LED behaviours exist in the mapping but rb6's choices are not settings yet. | PAR-M6, RM§6 P2 deck light settings |

## Mapping tools and other controllers

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| CTL-100 | MIDI LEARN | rb6 | Y p101 | Y p106 | Y p123 (Core+ plan) | Built | Mixxx learning wizard | No plan gating in Mix Two. | C-009, PAR-M9 |
| CTL-101 | MIDI setting window (table editor, import / export) | Improve | Y p101 | Y p106 | Y p123 | Partial | mappings are files | PAR-M10 Improve: a table editor like rekordbox's that keeps script support. | C-248, PAR-M10, RM§6 P2 MIDI table editor |
| CTL-102 | Keyboard shortcut editor and presets | rb6 | Y p185 | Y p204 | Y p232 | Partial | Mixxx keyboard mappings, no editor | rb6 has separate EXPORT alphabet search and PERFORMANCE shortcuts. | C-249, PAR-M11, B-241, RM§6 P2 keyboard editor |
| CTL-103 | PAD EDITOR and USER pad modes | Skip | str | Y p106 | Y p123 | Skip | - | Only for devices that support Pad Editor; the DDJ-800 is not known to. | C-010, C-152 |

## Open questions for the owner

1. **SHIFT + jog top (CTL-045).** You reported that rekordbox slides the grid
   with SHIFT + jog; the manual (p20) says the top adjusts the grid
   *interval* and only the side slides. Did your test use the top as well as
   the side? Options: (a) keep slide on both (today); (b) top = interval
   adjust, side = slide. *Recommendation:* try SHIFT + top once in rekordbox
   6; keep (a) unless rekordbox changes the interval.
2. **Mix Two extras that rekordbox 6 leaves unassigned (CTL-003, CTL-078, and
   SHIFT + pad in key shift mode, CTL-083).** SHIFT + LOAD ejects the deck;
   SHIFT + pad resets the key in keyboard and key shift modes. Options: (a)
   keep as Improve rows (give a reason, e.g. "eject without the mouse");
   (b) remove and follow rb6. *Recommendation:* (a) for eject if you use it,
   (b) for the key-reset extras (SHIFT + KEY SYNC already resets).
3. **CDJ-3000 browse jumps (CTL-010).** Holding the rotary on a CDJ-3000
   jumps by first letter or by page. Adopt it for the DDJ-800 rotary (if the
   deck sends a hold) or on screen? *Recommendation:* later, after the
   drill-in sidebar (DEC-§6) exists; low priority.

<!-- End of AI-generated text. -->
