<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 07: Analysis

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** Medium (rises only if grids block Device Library Plus)  ·  **Scope:** beat grid, key, phrase, vocal and waveform analysis, grid editing, cue creation at analysis time, the analysis engine, auto-analysis, and the rule that existing rekordbox analysis takes precedence (DEC-§5). Mixxx's analysers are the default and are converted to Pioneer formats on export (EXP-006); if analysis must improve, the target is rb6's analysis, not rb7's. On-screen display of grids, phrases and waveforms in the performance layout is LEG; the DDJ-800 SHIFT + jog grid slide is CTL.

## Analysis policy and management

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| ANA-001 | Existing rekordbox analysis takes precedence: never overwritten or re-analysed automatically; only an explicit user action re-analyses | Improve | - | - | - | Partial | DEC-§5; imported grids kept (`src/library/rekordbox/rekordboxbeats.cpp`) | Unverified: check that loading or batch-analysing an imported track never replaces its rekordbox grid, key or cues (USB, XML and `master.db` paths alike). | DEC-§5 |
| ANA-002 | Re-analysis keeps user-edited and locked data (rekordbox warns that it overwrites) | Improve | Y p.69 | Y p.75 | Y p.91 | Built | Mixxx keeps locked / user-edited beats | Keep Mix Two's protection; rekordbox overwrites after a warning. | B-143, PAR-I13 |
| ANA-003 | Unanalysed new tracks are auto-analysed by default, progress beside the title | rb6 | Y p.17, 180 | Y p.14, 197 | Y p.14, 224 | Partial | nothing queued on add: `src/library/library.cpp:82-85`; manual Analyze view `src/library/analysis/dlganalysis.ui` | DEC-§5: on by default. PARITY I.1 said Have (B conflict C3); tracks are analysed only when loaded to a deck. | B-047, PAR-I1 |
| ANA-004 | rekordbox's analysis settings in Preferences and in the right-click analysis dialog | rb6 | Y p.180 | Y p.197 | Y p.224 | Partial | `src/preferences/dialog/dlgprefbeatsdlg.ui`; reanalyse menu `src/widget/wtrackmenu.cpp:585,591` | DEC-§5. Mixxx has prefs and a menu, but not rb6's dialog with the same choices. | DEC-§5 |
| ANA-005 | Choice of analysis engine (Mixxx / rekordbox v5 / v6 / v7) | Owner? | - | - | - | Build | ROADMAP WS-D, U5, U6 | DEC-§5 fixes Mixxx as the default and rb6 as the target if analysis becomes a priority. Whether a user-facing selector is still wanted is Q1. | B-139, PAR-I10 |
| ANA-006 | `?` mark and "Add New Analysis Data" for tracks analysed by older versions | rb6 | Diff p.17 | Y p.14, 74 | Y p.14, 90 | Partial | needs wiring | Tie to analyser versions (ROADMAP U5). Explicit action only (ANA-001). | B-048, PAR-G3 |
| ANA-007 | Re-fix grids / cues of tracks analysed by old versions | Improve | str | str | str (+ 7.2.9 notice) | Build | - | rekordbox fixes silently; Mix Two never moves user data silently. Offer an explicit re-fix with a diff shown first (ROADMAP §6 P3). | B-144, PAR-I12 |
| ANA-008 | Cloud Analysis / "retrieve analysis information from the Internet" | Skip | N | str (6.8.5, Beta) | Y p.219, 224 | Skip | - | Cloud and account service. | B-137 |
| ANA-009 | Analysis concurrency (Performance / Power saving) | rb6 | Diff p.180 | Y p.197 | Y p.225 | Partial | worker threads, no user setting `src/analyzer/trackanalysisscheduler.cpp:36-64` | - | B-135, PAR-I6 |
| ANA-010 | Multi-thread analysis and larger analysis memory | rb6 | N | str (6.8.5) | Y p.236 | Partial | multi-threaded, no toggle | - | B-136 |
| ANA-011 | Analysis data location ("PIONEER" folder) | rb6 | str | str | str | Partial | database + `analysis/` folder | Portable library (LIB-002) should keep analysis next to the library. | B-168, PAR-I15 |
| ANA-012 | Analysis Lock: skips grid, key, phrase and vocal analysis and grid edits | rb6 | Diff p.69 (grid, key, phrase) | Diff p.76 (+ vocal) | Y p.92 | Partial | BPM lock only `src/engine/controls/bpmcontrol.cpp:282`, `src/widget/wtrackmenu.cpp:508` | PARITY G.4 and inventory C said Have (B conflict C11); key is not locked. | B-142, PAR-G4, C-136 |
| ANA-013 | Analyze video file | Skip | - | Y | Y | Skip | - | Video extension not in scope. | PAR-G22 |

## Beat grid analysis

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| ANA-014 | Track Analysis Mode Normal (steady tempo) / Dynamic | rb6 | Y p.17, 180 | Y p.14, 197 | Y p.14, 224 | Built | "Assume constant tempo" `dlgprefbeatsdlg.ui`; reanalyse const / var `wtrackmenu.cpp:585,591` | - | B-132, PAR-G1, PAR-I2 |
| ANA-015 | BPM Range for Normal analysis | rb6 | Y p.17, 180 | Y p.14, 197 | Y p.14, 224 | Build | no range in `src/analyzer/plugins/analyzerqueenmarybeats.cpp` | Avoids half / double errors. | B-133, PAR-I3 |
| ANA-016 | What to analyse: BPM / Grid, KEY, Phrase, Vocal | rb6 | Diff p.17 (no vocal) | Diff p.14 (+ Vocal) | Diff p.14, 42 (+ 7.2 high precision) | Partial | beats, key, waveform, ReplayGain, silence in `src/analyzer/`; no phrase or vocal | rb6 set. | B-134 |
| ANA-017 | High-precision BeatGrid analysis (rb7.2 ML models, "auto" per track) | Skip | N | N | str (7.2.19) | Skip | ROADMAP F8 (`models/detect_beat/model7`, `model8`) | DEC-§5: if analysis must improve, target rb6's analysis, not rb7's. | B-138, RM-F8 |
| ANA-018 | Bars and downbeats in the grid model (red bar every 4 beats; edited lines blue) | rb6 | Y p.68 | Y p.74 | Y p.90 | Partial | beat lines only; no bar / downbeat model in `src/track/beats.h` | Needed by ANA-019, ICC and phrase. Line colours on screen are LEG. | B-148 |

## Grid editing

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| ANA-019 | Set the downbeat: move playhead to the first beat of a bar; nearest grid line as 1.1 Bars | rb6 | Y p.54, 113 | Y p.60, 118 | Y p.75, 135 | Build | `beats_translate_curpos`, `beats_translate_match_alignment` align beats only; no bar concept | Needs ANA-018. | B-149, C-126 |
| ANA-020 | GRID: BPM at the playhead shown; type a BPM | rb6 | Y p.54, 113 | Y p.60, 118 | Y p.75, 135 | Partial | BPM edit only in track properties | Typing the deck's playing BPM (PAR-B11) is PERF. | B-150, C-130 |
| ANA-021 | GRID: TAP sets the grid BPM | rb6 | Y p.54, 113 | Y p.60, 118 | Y p.75, 135 | Built | `bpm_tap` `src/engine/controls/bpmcontrol.cpp:240`; "Tap to Beat" `src/library/dlgtrackinfo.ui` | The DDJ-800 TAP button (tempo vs BPM) is a CTL question. | B-151, C-131 |
| ANA-022 | GRID: shift the whole grid left / right by 1 ms (performance panel 1 or 3 ms) | rb6 | Y p.54, 113 | Y p.60, 118 | Y p.75, 135 | Partial | `beats_translate_earlier/later` `bpmcontrol.cpp:103,111`; `beats_translate_ms` `bpmcontrol.cpp:133` | Step sizes differ from rb6. SHIFT + jog slide on the DDJ-800 is CTL (B-160). | B-152, C-127 |
| ANA-023 | GRID: widen / narrow beat spacing by 1 ms (3 ms in performance panel) | rb6 | Y p.54, 113 | Y p.60, 118 | Y p.75, 135 | Partial | `beats_adjust_faster/slower` `bpmcontrol.cpp:87,95` step the BPM, not the spacing in ms | Inventory C called it Have; the step unit differs. | B-153, C-132 |
| ANA-024 | GRID: double / halve BPM | rb6 | Y p.54, 113 | Y p.60, 118 | Y p.75, 135 | Built | `beats_set_double/halve` + 2/3, 3/4, 4/5 `bpmcontrol.cpp:140-210` | Mix Two superset. | B-154, C-133 |
| ANA-025 | GRID: adjust the whole track vs from the playhead onwards | rb6 | Y p.54, 113, 128 | Y p.60, 118, 135 | Y p.75, 136, 152 | Build | - | ROADMAP §6 P1 "Grid editing: adjust from a point onwards, plus rekordbox's GRID panel". The GRID panel layout is PREP / LEG. | B-155, C-134, PAR-I7 |
| ANA-026 | GRID: rebuild the grid from the playhead with the chosen BPM | rb6 | Y p.54 | Y p.60 | Y p.75 | Partial | `beats_translate_curpos` only aligns `bpmcontrol.cpp:266` | - | B-156 |
| ANA-027 | GRID: undo / redo | rb6 | Y p.54, 113 | Y p.60, 118 | Y p.75, 135 | Partial | undo only `bpmcontrol.cpp:290`, `wtrackmenu.cpp:562` | Add redo. | B-157, C-128 |
| ANA-028 | GRID: metronome on / off, 3 volume levels (tone in Audio prefs) | rb6 | Y p.54, 113, 179 | Y p.60, 118, 196 | Diff p.75, 223 (+ volume pref) | Partial | metronome effect `src/effects/backends/builtin/metronomeeffect.cpp` | Not a grid-panel switch yet. | B-158, C-135 |
| ANA-029 | Drag the enlarged waveform to align the grid while paused | rb6 | Y p.128 | Y p.135 | Y p.152 | Partial | waveform drag scrubs; grid via SHIFT + jog | - | C-138 |
| ANA-030 | CDJ grid adjust extras: SNAP GRID to cue, SHIFT GRID (bake a by-ear bend into the grid), half-beat shift, RESET to the analysed grid | CDJ-3000 | - | - | - | Partial | `beats_translate_curpos`, `beats_translate_match_alignment`, `beats_translate_half` `bpmcontrol.cpp:95-119, 266, 274` | Needs wiring; RESET does not exist. Improves sync after by-ear corrections without breaking rb6 behaviour. | D-053 |
| ANA-031 | Track menu BPM: reload master BPM, increment / decrement | rb6 | - | Y (Ctx) | Y | Partial | BPM menu in `wtrackmenu.cpp` | - | PAR-G18 |

## Key

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| ANA-032 | Key detection; segment key on the waveform | rb6 | Y p.180 (segment: str) | Y p.197 (str) | Y p.224 (str) | Partial | QM / KeyFinder `src/analyzer/plugins/`; no segment key | Segment key is ROADMAP §6 P3. | B-140, PAR-I4, PAR-I5 |
| ANA-033 | Key notation Classic / Alphanumeric; "Display key information on the database" (tag key vs analysed key) | rb6 | Y p.178 | Y p.194 | Y p.220 | Partial | Traditional / Lancelot / OpenKey `src/preferences/dialog/dlgprefkeydlg.ui`; no tag-vs-analysis switch | - | B-141 |
| ANA-034 | Analyze key only (track menu) | rb6 | - | Y (Ctx "Analyze Key") | Y | Partial | - | - | PAR-G2 |

## Phrase and vocal

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| ANA-035 | Phrase analysis (phrase types, track mood) shown on enlarged / full waveform; "always show phrase types"; View > Layout switches | rb6 | Diff p.102, 177 (PERFORMANCE only) | Diff p.52, 58, 193 (EXPORT too) | Y p.68, 73, 221-222 | Build | Kaitai PSSI `lib/rekordbox-metadata/rekordbox_anlz.ksy:85`, importer ignores it | First step: read PSSI from rekordbox exports (IMP-013). Performance-mode display is LEG (C-053). | B-145, B-233, PAR-I8 |
| ANA-036 | Phrase edit (CUT, change type, CLEAR, drag boundary) in GRID / PHRASE EDIT panels | rb6 | Diff p.114 (PERFORMANCE only) | Diff p.61, 118-119 (EXPORT too) | Y p.76, 136 | Build | needs ANA-035 | - | B-146, C-137 |
| ANA-037 | Vocal position detection and display | rb6 | N | Y p.14, 52, 193 | Y p.14, 68, 222 | Build | - | ROADMAP §6 P3. | B-147, PAR-I9 |

## Waveforms and gain

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| ANA-038 | Waveform colour types Blue / RGB / 3Band | rb6 | Diff p.66, 177 (Blue / RGB) | Diff p.73, 194 (+ 3Band) | Y p.89, 222 | Partial | Simple / Filtered / HSV / RGB / Stacked `src/waveform/waveformwidgetfactory.cpp:1532-1546` | PARITY Improve: add Blue and 3Band, keep Mix Two's renderers. The same data is written to sticks (EXP-006). Renderer choice on screen is LEG. | B-162, PAR-A14, C-055 |
| ANA-039 | Full vs half waveform for overview and preview | rb6 | Y p.177 | Y p.194 | Y p.220 | Partial | overview stereo mode `src/preferences/dialog/dlgprefwaveformdlg.ui` | Also a device setting (EXP-063). | B-163 |
| ANA-040 | Per-track enlarged-waveform colour (CUE / LOOP menu; track menu "Change waveform color") | Improve | Y p.53 | Y p.59 | Y p.74 | Build | - | PARITY marked it Improve. | B-166, PAR-G17 |
| ANA-041 | Auto Gain value from analysis, shown and edited in GRID EDIT | rb6 | Y p.113, 183 | Y p.118, 201 | Y p.135, 228 | Built | ReplayGain `src/analyzer/analyzerebur128.cpp`, `dlgprefreplaygaindlg.ui` | Editing it in the GRID panel is missing. Auto Gain on load (C-215) is MIX. | B-161, C-129 |

## Cue creation at analysis

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| ANA-042 | Set a memory cue at the first beat when analysing | rb6 | Y p.17, 180 | Y p.14, 197 | Diff p.14, 225 (inside ICC Manual) | Partial | analyser sets the main cue at the first sound, not the first beat `src/analyzer/analyzersilence.cpp:143-160` | rb6 checkbox. ROADMAP §6 P2. | B-122, PAR-B21 |
| ANA-043 | INTELLIGENT CUE CREATION: auto-set hot and / or memory cues during analysis; Auto / Manual modes | Owner? | N | N | Y p.41, 225 | Build | - | rb7 addition; adoption is Q2. | B-123 |
| ANA-044 | ICC cue type and upper limit (16 hot, 10 memory) | rb7-add | N | N | Y p.41-42, 225 | Build | - | Depends on Q2. | B-124 |
| ANA-045 | ICC Auto: shipped cue-trend model + personal trend from the CUE Analysis playlist | rb7-add | N | N | Y p.41-43 | Build | ROADMAP F8 (`detect_cue_model/*.onnx`) | ML; models stay in the user's install and are never bundled. Depends on Q2. | B-125 |
| ANA-046 | ICC Manual: cue at 1.1 Bars, repeated every N bars | rb7-add | N | N | Y p.41, 225 | Build | needs ANA-018 | Contains rb6's first-beat checkbox as its simplest setting. Depends on Q2. | B-126 |
| ANA-047 | ICC: prohibit overwriting existing cues | rb7-add | N | N | Y p.42, 225 | Build | - | Matches Mix Two's rule: never overwrite user edits. | B-127 |
| ANA-048 | ICC: add generated comments to the cues | rb7-add | N | N | Y p.44, 225 | Build | - | Depends on Q2. | B-128 |
| ANA-049 | ICC depends on phrase analysis; short tracks may fail | rb7-add | N | N | Y p.42 | Build | needs ANA-035 | Dependency note. | B-129 |

## Open questions for the owner

1. **Analysis engine selector (ANA-005).** DEC-§5 makes Mixxx the default and rb6 the target if analysis must improve. Is a user-facing choice (Mixxx / rekordbox-style) still wanted? Options: (a) no selector, Mixxx only, improved towards rb6 when grids block Device Library Plus; (b) selector with Mixxx and a clean-room rb6-style engine; (c) also rb7 ML models loaded from the user's install. *Recommend (a)* until grids are shown to block Device Library Plus.
2. **INTELLIGENT CUE CREATION (rb7 addition, ANA-043 to ANA-049).** Options: rb6 first-beat checkbox only (ANA-042); rb7 ICC Manual; rb7 ICC Auto (ML). *Recommend rb6 checkbox now, ICC Manual later* (deterministic, local); Auto only if the analysis work is ever done.
3. **Key display source (ANA-033).** rekordbox can show the key from the tag or the analysed key. Options: analysed key always; follow rb6's switch. *Recommend rb6's switch*, default analysed.

<!-- End of AI-generated text. -->
