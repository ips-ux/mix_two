<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 09: Performance features

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** Medium  ·  **Scope:** how a deck *behaves* in PERFORMANCE mode (cue modes, hot and memory cues, loops, beat jump, slip, quantize, sync, key sync, load behaviour) plus the sampler, sequencer, slicer, CAPTURE, ACTIVE CENSOR, stems, MIX POINT LINK, Automix and recording. Controller wiring is bucket 01 (CTL); how the deck sounds (scratch, Master Tempo, key shift quality, tempo ranges, pitch bend, vinyl brake) is bucket 03 (DECK); the on-screen panels are bucket 10b (LEG).

Citation notes: inventory A rows outside its section 1 are cited with an `A:` prefix (`A:M-002`); inventory B rows are `B-nnn`, inventory C `C-nnn`, inventory D `D-nnn`. `RM6` = ROADMAP.md §6; `DEC-§n` = DECISIONS.md section n. The DECISIONS rule-settled items (16 hot cues on two pages, beat loop pad sizes, sampler 4 x 16, recording defaults) mean: check rb6.8.5 and copy it.

## Cue, play and hot cues

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PERF-001 | Play / Pause | rb6 | Y p126 | Y p133 | Y p150 | Built | script:539 | | C-077 |
| PERF-002 | CDJ cue: set at pause, Back Cue, Cue Point Sampler (hold), Real-Time Cue via IN | rb6 | Y p130-131 | Y p138-139 | Y p155-156 | Partial | `dlgprefdeck.cpp:59-62` (`CueDefault` 0 = Mixxx mode) | Pioneer cue mode exists but is **not the default**; PARITY said "make it the default" and that was not done (TRU). CDJ-3000 confirms the same behaviour (p51-52). Same behaviour in EXPORT-mode players. | C-078, C-042, PAR-B1, D-020, B-181 |
| PERF-003 | JOG pref: after CUE + jog turn, return to cue and pause | rb6 | Y p182 | Y p200 | Y p228 | Build | — | Interaction of Pioneer cue mode with the rekordbox platter model not tested; unverified. Cross-reference: owned by DECK-010. | see DECK-010 |
| PERF-004 | Hot cues: 16 per track A-P in PERFORMANCE, 8 A-H in EXPORT | rb6 | Y p134 | Y p142 | Y p159 | Built | `src/util/defs.h:25` (`kMaxNumberOfHotcues = 36`) | Improve: Mix Two keeps 36 per track, shows rb6's A-P. Rule-settled (DEC-§9): 16 on two pages; the DDJ-800 page wiring is broken today (CTL, A:H-004). Only A-H reach a CDJ: export maps or warns (EXP). Export to A-H: EXP-009. | C-100, PAR-B16, B-186, DEC-§9 |
| PERF-005 | Hot cue loop (pad pressed during a loop stores the loop) | rb6 | Y p134 | Y p143 | Y p160 | Built | `CueType::Loop`; `cuecontrol.cpp:1051-1057` | | C-101, PAR-B17 |
| PERF-006 | Hot cue comment and colour | rb6 | Y p105 | Y p110 | Y p127 | Partial | Mixxx colours + labels | Comments are not shown on the deck pad (display: LEG). | C-102, PAR-B19 |
| PERF-007 | Occupied pad cannot be overwritten; quantize snaps a new hot cue | rb6 | Y p134 | Y p143 | Y p160 | Built | Mixxx hot cue behaviour | | C-103 |
| PERF-008 | GATE playback of a hot cue while paused | rb6 | Y p183 | Y p201 | Y p228 | Built | Mixxx hot cue preview | 7.2.19 strings rename it "Gate Cue" (naming only). | C-104, PAR-B18 |
| PERF-009 | Smart Cue (setting or playing a hot cue makes it the CUE point) | Owner? | N | N | str 7.2.19 | Build | — | rb7.2 addition: a candidate (DEC-§1). Open question 1. | C-105, B-131 |
| PERF-010 | Convert Memory Cues to Hot Cues (fills empty pads only) | rb7-add | N | str 6.8.5 | Y p161 | Build | — | Present in 6.8.5 strings, documented only in rb7. Useful with unlimited memory cues. Open question 1. | C-107 |
| PERF-011 | Jump to the last played HOT CUE / CUE point | rb6 | N | str 6.8.5 | str 7.2.19 | Build | — | Controller function, not in any manual; optional. | C-121 |
| PERF-012 | Memory cues and memory loops: save, call prev / next, delete; 10 + 10 per track in rekordbox | rb6 | Y p112, p131, p133 | Y p117, p139, p141 | Y p134, p156, p158 | Built | `CueType::MemoryCue`, `memory_cue_*` (`cuecontrol.cpp`); `src/test/cuecontrol_test.cpp` | Improve: no count limit (PARITY). CDJ counts cues and loops together, max 10 (D-034); export must respect it (EXP). CALL while playing still to check against rb6 (RM6 "To check"). Memory Cue panel is LEG. Device limit: EXP-010. | C-108, PAR-B20, B-182 |
| PERF-013 | Active loop (a saved loop marked active, auto-engages on a player) | rb6 | Y p133 | Y p142 | Y p159 | Build | — | rekordbox only marks the loop; the behaviour is the DJ player's (rb7 p159). SHIFT + RELOOP/EXIT maps the wrong function today (CTL, A:C-007). | C-109, PAR-B23, B-185 |

## Loops, beat jump and slip

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PERF-014 | Auto beat loop, length chosen with < > | rb6 | Y p132 | Y p140 | Y p157 | Built | `beatloop_*` | | C-110 |
| PERF-015 | Manual loop IN / OUT / RELOOP / EXIT; IN also sets the cue | rb6 | Y p132-133 | Y p141 | Y p158 | Built | `loop_in`, `loop_out`, `reloop_toggle` | Same in EXPORT players; rb7's AU / MA buttons are a label change (LEG). | C-111, B-183 |
| PERF-016 | Loop halve / double; loop in / out adjust with the jog | rb6 | Y | Y | Y | Built | script:600-603; `jogTurn` | CDJ-3000 leaves adjust mode after 10 s without input (p55); rb6's timeout unknown. Cross-reference: owned by CTL-029. | see CTL-029 |
| PERF-017 | Fraction loops 1/32-1/2 and the 1/64 beat loop | rb6 | Y p129, p183 | Y p137, p201 | Y p154, p228 | Partial | `loopingcontrol.cpp:27` (smallest 1/32) | Engine cannot make 1/64, which the DDJ-800 manual's beat loop pads use (p54) and PARITY lists. Rule-settled (DEC-§9): check rb6's pad sizes; add 1/64 to the engine if rb6 uses it. Pad wiring: CTL (A:M-001). Pad-size row: CTL-081. | C-113, PAR-B22 |
| PERF-018 | Beat jump: FINE (5 ms), 1/8 ... 128 beats; moves an active loop | rb6 | Y p137 | Y p146 | Y p163-164 | Partial | `beatjump_size` | No FINE 5 ms step. CDJ-3000 default 16 beats (p74) is a My Settings value (EXP). | C-115, PAR-B25, D-031, B-171 |
| PERF-019 | Slip mode: slip hot cue, scratch, pause, loops, reverse | rb6 | Y p135 | Y p144 | Y p161-162 | Built | `slip_enabled`, `reverseroll` | Slip pause unverified; on the CDJ it is a Vinyl-mode function (D-060). Slip braking ramp: DECK. Vinyl-mode slip pause: DECK-018. | C-114, PAR-B24 |
| PERF-020 | Slip reverse cancels after 8 beats; jog direction while reversed | rb6 | Y | Y | Y | Partial | `reverseroll` (script:626) | No 8-beat auto-cancel (engine). The CDJ-3000 lever has no time limit (p64); rb6 / DDJ-800 rule wins. Check that the jog direction flips in reverse (unverified). Cross-reference: owned by CTL-051 (engine 8-beat cancel included there). | see CTL-051 |

## Quantize

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PERF-021 | Quantize per deck [Q], with an option to switch all decks together | rb6 | Y p129, p185 | Y p137, p203 | Y p154, p232 | Partial | per-deck `quantize` | "All decks" option not built. Same switch in EXPORT players. | C-116, PAR-B13, B-187 |
| PERF-022 | Quantize Mode: SNAP (now, phase kept) or wait for the next beat | rb6 | Y p185 | Y p203 | Y p231 | Partial | Mixxx behaves like SNAP | "Jump before reaching the next beat" (PARITY). The CDJ waits, and also snaps reverse and slip return (p61, p85): check Mix Two's reverse / slip under quantize. | C-117, PAR-B15, D-033 |
| PERF-023 | Quantize Type (HOT CUE, LOOP / SAMPLER, REVERSE, SEQUENCER) and beat value | rb6 | Y p185 | Y p203 | Y p231-232 | Build | Mixxx snaps to whole beats only | Copy rb6.8.5's installed value list; make sure 1/8, 1/4, 1/2, 1 (CDJ / My Settings) are in it (D Q8). | C-118, PAR-B14, D-032 |

## Sync, tempo and key

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PERF-024 | BEAT SYNC / BPM SYNC (sync type) and the SYNC button | rb6 | Y p102, p182 | Y p107, p201 | Y p124, p228 | Built | `[Master],sync_type`, `sync_grid` (`enginesync.h`; `dlgprefdeck.cpp:222-237`); `src/test/enginesynctest.cpp` | Improve: Grid Lock beat grid 1/8 ... whole. Partly hardware-tested 2026-10-27 (AGENT_changelog "Engine: sync"): owner to confirm. EXPORT 2 PLAYER mixing uses the same deck engine. | C-035, C-084, PAR-B3, B-188 |
| PERF-025 | MASTER button; master hand-off when its track changes or it stops | rb6 | Y p102, p136 | Y p108, p145 | Y p125, p162 | Built | explicit sticky MASTER (AGENT_changelog "Engine: sync") | Hand-off on unload unverified. CDJ-3000 rule is the same (p67). Partly hardware-tested 2026-10-27: owner to confirm. | C-040, C-085, PAR-B2, D-052 |
| PERF-026 | BEAT / BPM SYNC with double or half BPM | Improve | ≠ p182 | Y p201 | Y p228 | Built | PARITY B4 | Improve (owner 2026-10-27): always on, picked from the track's own BPM. Threshold is Mixxx's sqrt(2): check against rb6. | C-086, PAR-B4 |
| PERF-027 | SYNC RATE x1 / x2 / x1/2 per deck | rb7-add | N | str 6.8.5 | Y p124, p163 | Partial | automatic half / double (PERF-026) | rb7 addition: a manual override on top of Mix Two's automatic choice. Open question 1. | C-036 |
| PERF-028 | Fractional quantize or fractional loops turn BEAT SYNC off | Improve | Y p129 | Y p137 | Y p154 | Partial | FUNCTIONS.md "Sync" | Deliberate deviation: Mix Two's sync is independent of quantize (owner direction in FUNCTIONS.md). Record as Improve. | C-087 |
| PERF-029 | Automatic BEAT / BPM SYNC setting | rb7-add | N | str 6.8.5 | Y p231 | Build | — | Part of MIX POINT LINK (rb7 p231): unsteady-tempo tracks use BPM sync during MPL. Build only with PERF-076. PARITY's "verify what it automates" is answered. | C-092, PAR-B5 |
| PERF-030 | Tempo slider while synced and after (catch-up) | rb6 | Y | Y | Y | Built | `ratecontrol.cpp:834-847`; soft takeover | CDJ-3000 does the same (p66). Cross-reference: owned by CTL-043. | see CTL-043 |
| PERF-031 | Pitch bend on a synced follower | CDJ-3000 | — | unknown | — | Build | — | CDJ-3000: bending a synced follower drops BEAT SYNC to BPM sync (p66). rb6 behaviour unknown: check rb6 first. Unverified. Cross-reference: owned by DECK-015. | see DECK-015 |
| PERF-032 | Tempo reset: double-click the % to reset; -100 % stops | rb6 | Y p126 | Y p133 | Y p150 | Partial | one-shot `rate_set_zero` | Double-click reset not built. CDJ-3000 TEMPO RESET is a latching mode (p47): open question 3. Also: type a BPM straight into the deck. | C-080, PAR-B10, D-002, RM6 P2 Decks, PAR-B11 |
| PERF-033 | KEY SYNC (synced key stays after cancel; KEY RESET resets) | rb6 | Y p102, p137-138 | Y p107, p146-147 | Y p124, p164 | Built | `keycontrol.cpp` (`keysync_enabled`); `src/test/keysync_test.cpp` | Manuals answer PARITY's question: the synced key stays (rb6). CDJ-3000 resets on off (p69): CDJ difference, rb6 wins. CDJ candidate set (same, dominant, subdominant, relatives; p69) is the written spec Mix Two already follows. Which deck leads: check rb6. | C-034, C-089, PAR-B12, D-005, D-006 |
| PERF-034 | Key shift +/-12 semitones, semitone up / down, key reset | rb6 | Y p102, p137 | Y p107, p146 | Y p124, p164 | Built | `pitch`, `pitch_up` / `pitch_down`, `reset_key` | Key shift sound quality: DECK. Pad layout: CTL. | C-033, C-090, D-004 |
| PERF-035 | Ableton Link (deck LINK, sampler MASTER to LINK, start / stop sync) | rb6 | N | Y p106, p131-132 | Y p123, p148-149 | Build | no Ableton Link in `src/` | Not during Automix or DVS. rb7 only moves the settings (layout change, rejected). | C-011, C-091, PAR-M13, RM6 P3 |

## Loading and deck safety

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PERF-036 | Load by drag and drop to a deck | rb6 | Y p126 | Y p133 | Y p150 | Built | Mixxx | | C-070 |
| PERF-037 | Load Method: LOAD button or rotary push | Improve | Y p182 | Y p200 | Y p227 | Built | `src/library/loadprompt.*`; `src/test/loadprompt_test.cpp` | Improve (owner 2026-10-27): no preference, both always work via the "Load to the Deck" prompt. In rekordbox the choice exists only with a DDJ-RZ; with a DDJ-800 rekordbox loads only by LOAD (ROADMAP U7 overstated: TRU). Cross-reference: owned by CTL-009. | see CTL-009 |
| PERF-038 | Instant Doubles (copy track, position, loop, pad and slip state) | rb6 | str 5.8.7 | str 6.8.5 | Y p163 | Built | `CloneFromDeck` (`basetrackplayer.cpp`) | rb7 additions: drag a deck's title onto another deck; STEM Instant Doubles (7.2.19). Candidates: open question 1. Fixed 1-2 / 3-4 pairing is a mapping issue (CTL, A-002). Cross-reference: owned by CTL-002; the rb7 candidates stay in open question 1. | see CTL-002 |
| PERF-039 | Eject / Load Lock (no load onto a playing deck) | rb6 | Y p182 | Y p200 | Y p227 | Partial | `dlgprefdeck.cpp:172-175` "Reject" option | Exists as Mixxx's "Loading a track, when deck is playing: Reject"; needs wiring to an rb6-named setting. PARITY said Build (TRU). | C-073, PAR-B30 |
| PERF-040 | Needle Lock (no needle search while playing) | rb6 | Y p182 | Y p200 | Y p227 | Build | — | CDJ-3000 middle way: needle search while playing only with the jog top held (p46); open question 3. | C-074, D-022 |
| PERF-041 | Memory Cue Call Lock (no CALL while playing) | rb6 | Y p182 | Y p200 | Y p227 | Build | — | Folded into PAR-B30 in PARITY. | C-075 |
| PERF-042 | Load setting: start at the memory / hot cue nearest the start; Auto Cue and Auto Cue Level | rb6 | Y p182 | Y p200 | Y p227 | Partial | `dlgprefdeck.cpp:154-158`; `analyzersilence.cpp:11` (fixed -60 dB) | No "nearest memory cue"; no Auto Cue Level choice (-78 ... -36 dB, CDJ p53 / rb6 same list). RM6 P1 Decks. | C-076, PAR-B31, D-021, RM6 P1 Decks |
| PERF-043 | Undo track load; locate the loaded track in the browser | rb6 | — | Y (Juce strings) | — | Partial | Mixxx reloads the last ejected track | Add an explicit undo and "locate". | PAR-B32 |
| PERF-044 | Needle search by clicking the full waveform | rb6 | Y p102 | Y p107 | Y p124 | Built | PARITY B26 | | C-119, PAR-B26 |
| PERF-045 | Touch Cue (hear another point of the playing track in the headphones) | Owner? | N | N | N | Build | — | CDJ-3000 feature (p48); could use the preview deck on the cue bus. Open question 3. | D-023 |
| PERF-046 | Emergency Loop (loop instead of silence when the next track cannot start) | Owner? | N | N | N | Build | no such code in `src/engine` | CDJ-3000 feature (p57); off by default if adopted. Open question 3. | D-026 |
| PERF-047 | Track search waits for Auto Cue (10 s limit) | Skip | — | — | — | Skip | — | CDJ track search across a medium; no equivalent in a software deck. | D-027 |
| PERF-048 | Continuous Playing (load the next track of the list automatically) | rb6 | Y p182 | Y p200 | Y p228 | Partial | Auto DJ queue | No per-deck continuous play. CDJ PLAY MODE is the same setting (EXP for My Settings). | C-204 |
| PERF-049 | A supported controller takes over loop control from the screen | Improve | Y p132 | Y p140 | Y p157 | Partial | FUNCTIONS.md "engine owns behaviour" | Mix Two: screen and controller both always work (Improve). Cross-reference: owned by CTL-036. | see CTL-036 |
| PERF-050 | Preparing for a performance (analyse, playlists, cues, play) | rb6 | Y p121 | Y p127 | Y p144 | Built | — | Workflow description; nothing to build. | C-240 |
| PERF-051 | DVS (control vinyl) and the JOG panel DVS mode | Skip | Plus Pack | Y p108, p207 | Y p126, p235 | Skip | Mixxx vinyl control | Outside the DDJ-800 goal for now; Mixxx vinyl control already exists. Revisit 33 1/3 vs 45 only if DVS is built (DESIGN.md "Later"). Open question 5. | C-043, C-273, PAR-N10 |

## Sampler

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PERF-052 | Sampler deck: 16 slots per bank, 4 banks (64); drag a track in; kept after quitting | rb6 | Y p158 | Y p172 | Y p188 | Partial | LateNight `skin.xml:41` (16 samplers); engine up to 64 | Rule-settled (DEC-§9): 4 x 16. No bank UI; the DDJ mapping derives 8-slot banks (CTL, A:K-002). PARITY's "Have" is overstated (TRU). Menu > File Sample Pack download / import (Pioneer content): Skip. | C-180, PAR-E1, DEC-§9, A:K-001, C-263 |
| PERF-053 | Play modes Oneshot / Loop; SHIFT-click stops; mute via the title | rb6 | Y p159 | Y p173 | Y p189 | Partial | Mixxx sampler repeat on / off | | C-181 |
| PERF-054 | Gate mode (plays only while held) | rb6 | Y p161 | Y p175 | Y p191 | Build | — | RM6 P1 Decks "Sampler gate mode". | C-182, PAR-E2, RM6 P1 Decks |
| PERF-055 | Bank switching keeps playing slots; a slot at the same position replaces | rb6 | Y p159 | Y p173 | Y p189 | Build | — | | C-183 |
| PERF-056 | Sampler quantize [Q] (Loop slots; beat from the QUANTIZE prefs) | rb6 | Y p159 | Y p173 | Y p189 | Partial | per-sampler `quantize` | Depends on PERF-023 beat values. | C-184, PAR-E5 |
| PERF-057 | Sampler GAIN, sampler volume slider, sampler CUE | rb6 | Y p160 | Y p174 | Y p190 | Partial | per-sampler pregain / volume / pfl | DDJ SAMPLER LEVEL overwrites every slot's volume (CTL). | C-185, A:K-008 |
| PERF-058 | Slot edit: play mode, Gate, Master Tempo, Slot SYNC, Slot Gain, start-point fine tune, BPM x2 / /2 | rb6 | Y p161 | Y p175 | Y p191 | Partial | sampler keylock, sync, pregain | Start adjust and BPM x2 / /2 not built. | C-187, PAR-E4 |
| PERF-059 | Sampler deck BPM, SYNC, MASTER, output in external mode | rb6 | Y p116, p162 | Y p121, p176 | Y p138, p192 | Partial | sampler sync built | No sampler-deck BPM or sampler-deck master (sync with the sampler needs a player deck as master); separate sampler output unverified. | C-188, C-088, PAR-E3 |
| PERF-060 | Track Type: normal / one-shot sample / loop sample | rb6 | Y | Y | Y | Partial | PARITY E8 | Library field used by the sampler. | PAR-E8, B-087 |
| PERF-061 | Sampler prefs: number of samplers, [SLOT] do-not-overwrite, Auto Panel Mode, Sequence Load autoplay | rb6 | Y p184 | Y p202 | Y p229 | Partial | sampler count via skin | rb7 adds "Capture the loop in an empty slot": candidate with PERF-071. | C-191 |
| PERF-062 | SAMPLE SCRATCH pad mode (scratch a sampler slot on the deck) | rb6 | N | Y p190 | Y p206 | Build | — | Low priority; mostly MIDI Learn / keyboard on rb6 hardware. | C-150 |
| PERF-063 | OSC sampler (HORN, NOISE, SINEWAVE, SIREN) | Skip | Y p158 | Y p172 | Y p188 | Skip | PARITY E9 | Synth toy; low value. | C-189, PAR-E9 |
| PERF-064 | Sample Pack download / import | Skip | ≠ p189 (Plus Pack) | Y p208 | Y p237 | Skip | PARITY E10 | Online content. | C-190, PAR-E10 |
| PERF-065 | Sampler list (playlist of samples) | Skip | — | Y | — | Skip | PARITY E11 | Covered by playlists. | PAR-E11 |

## Sequencer, slicer, CAPTURE, ACTIVE CENSOR

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PERF-066 | Sequencer: overdub record per bank, bar length, 8 saved sequences, MUTE / ERASE, metronome | rb6 | Y p117, p163-165 | Y p122, p177-180 | Y p139, p193-196 | Build | — | RM6 P3 Sampler. | C-192, PAR-E6, RM6 P3 |
| PERF-067 | SEQUENCE LOAD (drag a sequence onto a deck) | rb6 | Y p165 | Y p180 | Y p196 | Build | — | | C-193 |
| PERF-068 | SEQ. CALL pad mode (8 sequences; SHIFT pauses) | rb6 | Y p110, p166 | Y p115, p180 | Y p132, p196 | Build | — | With the sequencer. | C-194 |
| PERF-069 | Slicer: range in 8 slices, hold a pad to roll a slice while playback continues | rb6 | Y p107, p167-169 | Y p112, p181-183 | Y p129, p197-199 | Build | no slicer in Mixxx | PARITY had no row. | C-195 |
| PERF-070 | Slicer mode vs Slicer Loop mode | rb6 | Y p169 | Y p183 | Y p199 | Build | — | | C-196 |
| PERF-071 | CAPTURE: loop or slicer range to sampler slots; stored captures | rb6 | Y p170-172 | Y p184-186 | ≠ p200-202 | Build | — | rb6: captures stored under Collection; rb7 moves them to Media Browser (redesign, rejected). CAPTURE button on the enlarged waveform included. | C-197, C-052, PAR-E7 |
| PERF-072 | ACTIVE CENSOR: IN / OUT sections per track; REV ROLL, TRANS, ECHO, V.BRAKE; IN ADJ / OUT ADJ | rb6 | Y p111, p173-174 | Y p116, p187-189 | Y p133, p203-205 | Build | Mixxx has only a momentary censor (`reverseroll`) | Effects use the Beat FX sounds (FX). PARITY had no row. | C-198 |

## Stems

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PERF-073 | Live 4-stem separation: ACTIVE STEM, STEM ISO, STEM FX (VOCAL, DRUMS, BASS, INST) | rb7-add | N | str 6.8.5 (TRACK SEPARATION, 3 parts) | ≠ p207-209; 4 stems in 7.2.19 | Partial | Mixxx plays stem files (`stem_count`, `enginedeck.cpp`) | DEC-§9 (Q24): the goal is rb7-style **live** separation with 4 stems; Medium. No real-time separation yet. Never bundle Pioneer models (load from the user's install or use an own model). | C-200, PAR-N11, DEC-§9, RM6 P3 |
| PERF-074 | STEMS pad mode | rb7-add | N | N | str 7.2.19 | Build | — | With PERF-073. | C-151 |
| PERF-075 | GROOVE CIRCUIT (DRUM SWAP, DRUM CAPTURE, DRUM RELEASE FX) | Owner? | N | N | str 7.2.19 | Build | — | Undecided (DEC-§9). Open question 2. | C-199 |

## MIX POINT LINK, Automix, other helpers

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PERF-076 | MIX POINT LINK: MIX IN / MIX OUT decks, mix points from cues, automatic start | rb6 | N | str 6.8.5 | Y p210-215 | Build | — | In rb6.8.5 (41 strings), documented only in rb7; follow rb6.8.5 behaviour with the rb7 manual as description. Panel display: LEG. | C-201, PAR-L8, RM6 P3 Automix |
| PERF-077 | MIX POINT LINK prefs (default point, auto switch, time scale, grid, zoom limit) | rb6 | N | str | Y p230-231 | Build | — | | C-202 |
| PERF-078 | Automix: playlist, repeat, random, load method; stop lets the track finish | rb6 | Y p139, p191 | Y p149, p210 | Y p166, p239 | Built | `src/library/autodj/autodjprocessor.cpp` | rb6 naming to adopt (RM6 P2 "Auto DJ renamed"). Automix blocks Ableton Link. Automix window in the View menu included. | C-203, C-023, PAR-L6, B-118, RM6 P2 |
| PERF-079 | Automix manual crossfade start | rb6 | — | Y (Juce) | — | Partial | PARITY L7 | | PAR-L7 |
| PERF-080 | AUTO BPM TRANSITION | rb6 | — | str 6.8.5 | — | Partial | PARITY L9 | PARITY marked Improve: make it optional and transparent. | PAR-L9 |
| PERF-081 | SMART FADER (auto BPM transition across a fade) | Skip | N | str 6.8.5 | str 7.2.19 | Skip | PARITY C13 | Not in the manuals; device or plan specific; not a DDJ-800 feature. Open question 4. | C-205, PAR-C13 |
| PERF-082 | JOG CUTTER | Skip | N | str 6.8.5 | str 7.2.19 | Skip | — | Device-specific, not on the DDJ-800. | C-206 |

## Recording

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PERF-083 | Recording panel: source, level, meter, start / stop, elapsed / remaining | rb6 | Y p55, p119 | Y p62, p124 | Y p78, p141 | Partial | `[Recording],toggle_recording` (`toolbar.xml:221-249`); `dlgrecording.ui` | Start / stop and time exist; no level knob or meter on the panel. KUVO button dropped (PERF-089). | C-230, PAR-L1, B-223 |
| PERF-084 | Record source: Master Out in internal mode; hardware inputs | rb6 | Y p93, p146 | Y p101, p156 | ≠ p118, p173 | Partial | Mixxx records the main mix or the Record/Broadcast input | | C-231, B-222 |
| PERF-085 | REC standby: start on audio input; stop after 20 s silence | rb6 | Y p94, p146-147 | Y p102, p156-157 | Y p118-119, p173-174 | Build | Mixxx starts at once | Rule-settled (DEC-§9): copy rb6.8.5's recording defaults. | C-232, PAR-L2, B-224 |
| PERF-086 | After recording: tag window, saved to Recordings | rb6 | Y p94-95, p147 | Y p102-103, p157 | ≠ p119-120, p174 | Partial | tags set in prefs | rb7 moves Recordings under Media Browser (redesign, rejected). | C-233, B-225 |
| PERF-087 | Recording prefs: silence threshold, split on silence, optimise level, auto import | rb6 | Y p184 | Y p202-203 | Y p230 | Build | split by size / time only (`recordingmanager`) | Split on silence, optimise level: Build; auto import only via a watched folder (DEC-§6 has no watched folders: import directly). | C-234, PAR-L3, PAR-L4, B-226, B-227, B-228 |
| PERF-088 | Recording location and format | Improve | Y p93, p184 | Y p101, p202 | Y p118, p230 | Built | `dlgprefrecorddlg.ui` | rekordbox records WAV only; Mix Two keeps WAV / AIFF / FLAC / MP3 / OGG / Opus + CUE sheet (Improve). | B-229, B-230 |
| PERF-089 | Record while publishing a KUVO Live Playlist | Skip | Y p147 | Y p157 | N | Skip | PARITY L12 | Service; removed in rb7. KUVO service row: LIB-096. | C-235 |
| PERF-090 | Share Mix to Mixcloud / YouTube | Skip | ≠ p147-149 | ≠ p157-159 | ≠ p174-175 | Skip | PARITY L11 | Account services. | C-236, PAR-L11 |
| PERF-091 | Mix MIC into REC OUT | rb6 | str | str | str | Built | PARITY L5 | | C-237, PAR-L5 |

## Open questions for the owner

1. **rb7 additions to adopt (DEC-§1 makes each a candidate).** SYNC RATE x1 / x2 / x1/2 override (PERF-027), Convert Memory Cues to Hot Cues (PERF-010), Smart Cue (PERF-009), Instant Doubles by dragging a deck title and STEM Instant Doubles (PERF-038). *Recommendation:* adopt SYNC RATE (as an override on the automatic choice), Convert Memory Cues and drag-title Instant Doubles; Smart Cue only if you use it.
2. **GROOVE CIRCUIT** (left undecided in DEC-§9). Options: skip; build after live 4-stem separation works. *Recommendation:* skip until PERF-073 works, then decide.
3. **CDJ-3000 deck behaviours as Improve rows** (rb6 has none of them): Emergency Loop (PERF-046), needle search while playing only with the jog held (PERF-040), Touch Cue (PERF-045), latching TEMPO RESET (PERF-032). (Bend on a synced follower and the loop-adjust timeout are in DECK-015 and CTL-029.) *Recommendation:* Emergency Loop opt-in (off), the needle option as a third Needle Lock choice, Touch Cue later; skip the TEMPO RESET latch (no DDJ-800 button).
4. **Device-specific rb6 helpers** not on the DDJ-800: SMART FADER (PERF-081), JOG CUTTER (PERF-082). *Recommendation:* skip both.
5. **DVS** (PERF-051). Options: skip; build on Mixxx's vinyl control later. *Recommendation:* skip for now.
6. **Memory cue limit vs export.** Keep "no limit" (Improve) and warn on export when a track has more than the CDJ's 10, or cap at 10 + 10 like rekordbox? *Recommendation:* no limit, warn on export (the export side is bucket 04).

<!-- End of AI-generated text. -->
