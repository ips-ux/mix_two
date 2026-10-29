<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 10b: Legacy Design (performance)

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** High  ·  **Scope:** the performance-mode screen in the Legacy Design: rekordbox 6's performance layout with gentle hardware touches (DEC-§7): the 2 / 4 deck layouts, the global section, the deck display elements, waveforms as displayed, the deck panels, show / hide, and the DESIGN.md look. Prepare / Export layout, pane sizes, menus and preferences are bucket 10a (PREP); the Hardware Design skins are bucket 10c (HWD); what the deck does is bucket 09 (PERF).

Citation notes: inventory A rows outside its section 1 are cited with an `A:` prefix; inventory B rows `B-nnn`, C `C-nnn`, D `D-nnn`. `DES-<part>` = a part of DESIGN.md; `FUN` = FUNCTIONS.md; `RM6` = ROADMAP.md §6; `DEC-§n` = DECISIONS.md section n. Today's base is the reworked LateNight skin (AGENT_changelog "Skin: LateNight rework"), still provisional until the rekordbox-layout work.

## Mode and direction

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LEG-001 | PERFORMANCE as its own face of one app; no licence gating | Improve | Y p99, p101 | ≠ p104, p106 | Y p121, p123 | Partial | one app, no modes | DEC-§7: two faces, one engine (Prepare = rekordbox replica, Performance = moves towards hardware). PARITY Improve: switch freely, no plan locks (rb6's "x" icons on locked functions are not copied). Prepare side: PREP. Cross-reference: owned by PREP-001. | see PREP-001, DEC-§7 |
| LEG-002 | Legacy Design: rekordbox 6.8.5 performance layout with gentle hardware touches (centred mixer, SYNC / KEY SYNC styling) | rb6 | — | Y | — | Partial | `res/skins/LateNight/` rework (AGENT_changelog) | Current target (DEC-§7). The full Hardware Design is HWD. | DEC-§7, RM6 P1 Layout |
| LEG-003 | Player panel composition (track info, FX, global, enlarged waveform, pads, jog, sampler, mixer, REC, mic) | rb6 | ≠ p100 (10 areas) | Y p105 (11) | ≠ p122 (+ MIX POINT panel) | Partial | LateNight deck / mixer / FX / sampler / mic rows | rb6 set. rb7's MIX POINT LINK panel only if MIX POINT LINK (PERF-076) is built. | C-002 |

## Layouts

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LEG-004 | 2Deck Horizontal (full-width enlarged waveforms) | rb6 | Y p145 | Y p155 | Y p172 | Partial | `skin.xml:47` (`show_4decks`); stacked waveforms | LateNight has 2 / 4 decks but not rekordbox's four arrangements. Interim 2026-10-27 design pass applied here. | C-003, PAR-A2 |
| LEG-005 | 2Deck Vertical (full-height waveforms) | rb6 | Y p145 | Y p155 | Y p172 | Build | renderers handle `Qt::Vertical` (`waveformrenderertextured.cpp:357`) | No vertical layout in LateNight. | C-004 |
| LEG-006 | 4Deck Horizontal | rb6 | Y p145 | Y p155 | Y p172 | Partial | `skin.xml:47` | Which deck each DDJ-800 side controls is not shown (CTL, A:P-003). | C-005 |
| LEG-007 | 4Deck Vertical | rb6 | Y p145 | Y p155 | Y p172 | Build | as LEG-005 | | C-006 |
| LEG-008 | Browse layout (minimised decks, large browser) | rb6 | Y p145 | Y p155 | Y p172 | Partial | `skin.xml:95-96` (`show_maximized_library`); DDJ VIEW toggles it | Needs wiring as a proper layout state. | C-007, PAR-A3 |
| LEG-009 | Split screen (second browser panel; click a deck to load) | rb6 | Y p120 | Y p126 | Y p143 | Build | — | Cross-reference: owned by PREP-020. | see PREP-020 |
| LEG-010 | Show / Hide Pad and Platter (horizontal layouts) | rb6 | Y p190 | Y p209 | Y p239 | Build | — | RM6 P3 Decks. | C-017, PAR-A9, RM6 P3 |
| LEG-011 | Full screen | rb6 | Y p190 | Y p209 | Y p238 | Built | Mixxx full screen | Cross-reference: owned by PREP-011. | see PREP-011 |
| LEG-012 | Pane sizes: fixed presets switched with a button, no draggable divider | rb6 | — | Y | — | Build | — | Cross-reference: owned by 10a (PREP) per DEC-§6; applies to the Legacy skin too. | see PREP |

## Global section and view preferences

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LEG-013 | Panel toggles FX / SAMPLER / MIXER / REC / mic (+ video, lyric, lighting) | Owner? | ≠ p101 (text checkboxes) | ≠ p106 (icons) | ≠ p123 (no lyric, + MPL) | Partial | `res/skins/LateNight/toolbar.xml:14-111` | rb6 panel set and positions. Labels: rb6 icons by default; rb5 text was recommended for Pioneer wording: open question 1. RM6 P1 Layout. | C-008, PAR-A4 |
| LEG-014 | MIDI LEARN button in the global section | rb6 | Y p101 | Y p106 | Y p123 | Built | Mixxx learning wizard | The button only; the MIDI Learn function is CTL. rb7 gates it to a plan (not copied). Cross-reference: owned by CTL-100. | see CTL-100 |
| LEG-015 | PAD EDITOR button | Skip | str 5.8.7 | Y p106 | Y p123 | Skip | — | Only for equipment that supports Pad Editor; the DDJ-800 is not known to. Cross-reference: owned by CTL-103. | see CTL-103 |
| LEG-016 | Audio dropout meter | rb6 | Y p101 | Y p106 | Y p123 | Built | `toolbar.xml:180-185` | | C-013 |
| LEG-017 | Battery and clock | rb6 | Y p101 | Y p106 | Y p123 | Built | `toolbar.xml:145`, `:154` | | C-014 |
| LEG-018 | Account / MY PAGE / plan / INFO windows | Skip | N | ≠ p106 | ≠ p123 | Skip | PARITY N | No accounts. Cross-reference: owned by PREP-003. | see PREP-003 |
| LEG-019 | Prefs Layout [Effect Panel]: which FX panel shows | rb6 | Y p177 | Y p193 | Y p222 | Partial | LateNight effect rack options | | C-018 |
| LEG-020 | Prefs Layout [Enlarged Waveform]: waveform order in 4-deck mode | rb6 | Y p177 | Y p193 | Y p222 | Build | — | | C-019 |
| LEG-021 | Prefs Layout [PAD Mode] Auto: pad display when a controller connects | rb6 | Y p177 | Y p194 | Y p222 | Build | — | | C-020 |
| LEG-022 | Skin Dark / Light | rb6 | N | Y p195 | Y p222 | Partial | LateNight Classic / PaleMoon schemes | Mix Two's own palettes may stay. Cross-reference: owned by PREP-009. | see PREP-009 |
| LEG-023 | Hide Title, Artist and Artwork on decks | rb7-add | N | N | str 7.2.19 | Build | — | rb7.2 addition (privacy mode); candidate, low priority. Open question 4. | C-025 |

## Deck display

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LEG-024 | Artwork; hover x to unload | rb6 | Y p102 | Y p107 | Y p124 | Partial | cover art + eject | | C-030 |
| LEG-025 | Title, artist, original BPM, key | rb6 | Y p102 | Y p107 | Y p124 | Built | deck rows | | C-031 |
| LEG-026 | Remaining / elapsed time toggle | rb6 | Y p102 | Y p107 | Y p124 | Built | PARITY B27 | | C-032, PAR-B27 |
| LEG-027 | Current cue position readout | rb6 | Y p102 | Y p107 | Y p125 | Build | not in LateNight deck rows | Unverified. | C-037 |
| LEG-028 | Full (overview) waveform with cue, memory cue, hot cue and phrase marks | rb6 | ≠ p102 | ≠ p107 (+ lighting) | Y p124 | Partial | `src/waveform/renderers/waveformmarkset.*` | Hot and memory cue marks exist; no phrase (data: ANA). No lighting scenes (Skip). | C-038 |
| LEG-029 | Key readout, key difference; double-click to reset | rb6 | Y p102 | Y p107 | ≠ p125 | Partial | key shown; reset via right-click KEY SYNC | rb7 documents that with MT off the shown key follows the tempo; check rb6.8.5 does the same before changing. | C-039 |
| LEG-030 | Key compatibility colour (key in green when it matches the master) | Owner? | N | N | N | Build | — | CDJ-3000 feature (p23); rb7's Traffic Light is similar (LIB). Open question 3. | D-007 |
| LEG-031 | JOG panel loop area: Auto Beat Loop / Manual Loop switch | rb6 | ≠ p103 | ≠ p108 | ≠ p125 ([AU] / [MA]) | Partial | `decks/row_5_transportLoopJump.xml` | rb6 (lower-bar click); rb7's AU / MA buttons are a redesign, rejected by default. | C-041 |
| LEG-032 | Platter display: CURRENT CUE / SLIP or HOT CUE COUNTDOWN | rb6 | Y p104, p178 | Y p109, p194 | Y p126, p220 | Partial | spinny shows position + slip ghost | No hot cue countdown. RM6 P3 Decks. | C-044, C-120, PAR-B29 |
| LEG-033 | Platter: BPM, tempo +/-, % changed, pitch bend buttons, TEMPO RANGE, MT | rb6 | Y p104 | Y p109 | Y p126 | Built | `decks/deck_hub.xml`; rate column | | C-045 |
| LEG-034 | MT button turns into KEY RESET after a key shift | rb6 | Y p104 | Y p109 | Y p126 | Partial | separate reset (right-click KEY SYNC) | | C-046 |
| LEG-035 | SLIP, Q, PLAY on the JOG panel | rb6 | Y p104 | Y p109 | Y p126 | Built | `slip_enabled`, `quantize`, `play` | | C-047 |
| LEG-036 | Enlarged waveform zoom, hide icons, [RST] | rb6 | Y p119 | Y p125 | Y p142 | Built | waveform zoom controls | | C-048 |
| LEG-037 | Memory cue and hot cue markers on the enlarged waveform | rb6 | Y p119 | Y p125 | Y p142 | Built | red "M" marks | | C-049 |
| LEG-038 | Beat count: current bars, bars / beats to the next memory cue | rb6 | Y p119, p178 | Y p125, p194 | Y p142, p220 | Partial | "Play marker hints" (`dlgprefwaveformdlg.ui`) | Mixxx hints count to the next marker, not bars. CDJ-3000 also counts to a touched point (p21): an extra. RM6 P2 Decks. | C-050, PAR-B28, B-165, D-071 |
| LEG-039 | Matching button (mark players 1 + 2 as a good combination) | rb6 | Y p119 | Y p125 | Y p142 | Build | — | The button; the Matching data is LIB. | C-051 |
| LEG-040 | Phrase display on the enlarged / full waveform | rb6 | Y p177 | Y p125, p193 | ≠ p142, p221 | Build | needs phrase data (ANA) | rb7 splits the setting in three: redesign, keep rb6's settings. View switches row: ANA-035. | C-053 |
| LEG-041 | Vocal position display | rb6 | N | Y p125, p193 | ≠ p142, p222 | Build | needs vocal data (ANA) | rb6 setting. | C-054 |
| LEG-042 | Waveform colour Blue / RGB / 3Band | Improve | ≠ p177 (Blue / RGB) | Y p194 | Y p222 | Partial | `waveformwidgetfactory.cpp:1532-1546` | See ANA-038 (on-screen renderer choice stays a LEG concern). | - |
| LEG-043 | Click on the enlarged waveform: left = play / pause, right = set cue | Owner? | ≠ p178 (EXPORT only) | ≠ p194 (EXPORT only) | ≠ p220 (PERFORMANCE too) | Build | Mixxx click = seek / scratch drag | rb7 extends it to PERFORMANCE: a candidate. Open question 2. EXPORT-mode version: PREP. | C-056 |
| LEG-044 | Waveform drawing rate | rb6 | Y p177 | Y p194 | Y p220 | Built | Waveform prefs frame rate | | C-057, B-164 |
| LEG-045 | Full / half overview and preview waveform | rb6 | Y p177 | Y p194 | Y p220 | Partial | overview type prefs | | C-058 |
| LEG-046 | Hot cue colour schemes CDJ / COLD1 / COLD2 / COLORFUL | rb6 | Y p178 | Y p195 | Y p222 | Partial | `predefinedcolorpalettes.cpp:46-51` | | C-059, B-189 |
| LEG-047 | PAD FX colour; SAMPLER colour (frequency colour, fixed, or custom) | rb6 | Y p160, p178 | Y p174, p195 | Y p190, p222 | Build | — | Low priority. | C-060, C-186 |
| LEG-048 | STEM waveform (shows the playing stems) | rb7-add | N | N | Y p209 | Partial | LateNight `stem_channel.xml` | With live stems (PERF-073, DEC-§9). | C-061 |
| LEG-049 | Slip ghost playhead (background position) | CDJ-3000 | N | N | N | Partial | `waveformrendererslipmode.cpp` | CDJ-3000 shows the background point as a white line (p65); whether Mix Two draws a line is unverified. | D-061 |
| LEG-050 | Phase meter (bar / beat offset from the sync master) | Owner? | N | N | N | Build | — | CDJ-3000 (p22, p80); also a My Settings value (EXP). Open question 3. | D-070 |

## Deck panels and pads on screen

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LEG-051 | Deck panel selector under the waveform: Pad / Memory Cue / Grid / Info | rb6 | ≠ p113 | ≠ p118 | Y p135 | Build | — | Includes switching the lower area to GRID / PHRASE EDIT (the grid editing itself is ANA). RM6 P1 Layout. | PAR-A5, C-125, RM6 P1 Layout |
| LEG-052 | On-screen pad modes: HOT CUE, PAD FX, SLICER, BEAT JUMP, BEAT LOOP, KEYBOARD, KEY SHIFT, SEQ. CALL, ACT. CENSR, MEMORY CUE | rb6 | Y p104 | Y p110 | Y p127 | Build | LateNight has hot cue buttons, no pad-mode panel | rb6 has them, so build them (rb6 default); order: MEMORY CUE first, then SLICER and ACTIVE CENSOR, SEQ. CALL with the sequencer. Pages A-H / I-P shown (DEC-§9). | C-140 |
| LEG-053 | MEMORY CUE panel (list, times, comments, colours) | rb6 | Y p112 | Y p117 | Y p134 | Build | engine controls exist, no panel | Memory cue comments on the deck included. Engine: PERF-012. | C-149 |
| LEG-054 | On-screen Pad FX area follows the pad page | rb6 | Y | Y | Y | Build | — | | A:I-008 |

## Extension panels

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LEG-055 | Video panel and visualizers | Skip | Plus Pack | Y p106, p207 | Y p123, p236 | Skip | PARITY N | Separate product area. Video lists row: LIB-091. | C-270 |
| LEG-056 | Lyric panel | Skip | Plus Pack | Y p106, p207 | N | Skip | PARITY N | Service; removed from the rb7 manual. | C-271, PAR-N9 |
| LEG-057 | LIGHTING panel and lighting scenes on the waveform | Skip | ≠ (Lighting mode) | Y p106-107, p207 | Y p123-124, p236 | Skip | PARITY N | Large separate area; revisit after core parity. | C-272, PAR-N7 |

## Look and arrangement (DESIGN.md)

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LEG-058 | Pioneer wording on screen (MASTER, BOOTH, HEADPHONES, MIXING, LEVEL, CUE, GRID LOCK / BPM SYNC, KEY SYNC, MT); short labels | DDJ-800 | — | — | — | Partial | LateNight rework | DESIGN.md "hardware comes first" predates DEC-§7: in the Legacy Design, rekordbox 6's layout leads and the hardware gives only gentle touches; a hardware-first layout belongs to the DDJ-800 skin (HWD). Open question 5. | DES-Direction |
| LEG-059 | Decks mirrored from the centre out (mixer module, rate column, deck hub, track rows) | Improve | — | — | — | Built | `skin.xml` slots `SlotLeft1/2`, `SlotRight1/2` | Owner direction 2026-10-27 ("centred mixer" touch in DEC-§7). Compiled, layout provisional. | DES-Direction, DES-Layout-as-built |
| LEG-060 | Every section hideable and movable; position chosen in one place | Improve | — | — | — | Partial | LateNight show / hide options; per-deck slots | Build each new section as its own template with a persisted visibility control (FUN "Layout is customisable"). | DES-Arrange, FUN Layout |
| LEG-061 | Saved view arrangements (presets), Pioneer layout as default; separate show / hide for deck hub, HEADPHONES, MASTER | Improve | — | — | — | Build | — | DESIGN.md "Later" item 1; RM6 Design. | DES-Later-1, RM6 Design |
| LEG-062 | Raised, titled modules; labels above knobs; domed round buttons (34 px, 8 px Silkscreen) | Improve | — | — | — | Partial | `controls/knob_label_above.xml`; `res/fonts/Silkscreen-Bold.ttf` | Applied to the mixer and rate column so far. | DES-Aesthetic |
| LEG-063 | Colour language (tech blue = on / synced; red-orange = MASTER / not matching; orange = CUE; white = matches; vivid red = MT) | Improve | — | — | — | Built | QSS in `style*.qss` | | DES-Colour |
| LEG-064 | Deck hub, rate column and mixer modules as built 2026-10-27 | Improve | — | — | — | Built | `decks/deck_hub.xml`, `decks/rate_controls.xml`, `mixer/mixer_headphones.xml`, `mixer/mixer_main.xml` | Compiled; not yet checked against rekordbox 6.8.5's layout. | DES-Layout-as-built |
| LEG-065 | Colour FX as lit rectangular buttons above the HEADPHONES module; Beat FX / master FX module design | Improve | — | — | — | Build | — | Button labels **DUB ECHO, SWEEP, NOISE, FILTER** (DEC-§1); DESIGN.md still says PITCH (TRU). | DES-Later-4, RM6 Design |
| LEG-066 | One module language for decks, library and FX units | Improve | — | — | — | Build | — | | DES-Later-5, RM6 Design |

## Open questions for the owner

1. **Panel toggle labels (LEG-013).** rb6 uses icons; rb5 used text checkboxes ([FX] [SAMPLER] [MIXER] [REC]), which fit the Pioneer-wording rule. rb5 applies only by your deliberate pick. *Recommendation:* rb5 text labels in rb6's panel set and order.
2. **Waveform click in PERFORMANCE (LEG-043, rb7 addition).** Options: keep Mixxx drag-scrub only; add rb7's left = play / pause, right = set cue as an option; make it the default. *Recommendation:* option, default off.
3. **CDJ-3000 display extras (LEG-030, LEG-050).** Key compatibility colour and a phase meter on the deck. *Recommendation:* key colour yes (cheap, helps harmonic mixing); phase meter as an option in place of the enlarged waveform, later.
4. **Hide Title / Artist / Artwork (LEG-023, rb7.2 addition).** *Recommendation:* yes, low priority.
5. **How much hardware in the Legacy Design (LEG-058).** DESIGN.md says "the hardware comes first" and the current LateNight rework follows the DDJ-800's mixer order; DEC-§7 makes the Legacy Design rekordbox's layout with gentle touches. Options: (a) keep what is built (centred mixer, HEADPHONES / MASTER modules, domed SYNC / KEY SYNC / MT) and take rekordbox 6.8.5's layout for everything else, moving further hardware-first ideas to the DDJ-800 skin; (b) rework the Legacy Design closer to rekordbox's own mixer panel. *Recommendation:* (a), and update DESIGN.md's "Direction" to say so.

<!-- End of AI-generated text. -->
