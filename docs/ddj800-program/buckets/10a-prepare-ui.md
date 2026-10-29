<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 10a: Prepare UI

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** High  ·  **Scope:** the Prepare / Export face, a 1:1 replica of rekordbox 6's EXPORT mode (DEC-§7) with one improvement, the drill-in sidebar (DEC-§6): app shell and modes, sidebar navigation, fixed pane presets, browser columns and views, the EXPORT-mode players and panels, the Preferences window layout and menus. What the library does is LIB; the performance layout is LEG.

## App shell and modes

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PREP-001 | Two faces, one engine: Prepare (EXPORT) and Perform, switched freely with one key or button, no licence or plan gating | Improve | Diff p.2-3 (licence per mode) | Diff p.6 (plans) | Diff p.6, 60 (+ EDIT mode) | Partial | PARITY A.1; `[Skin] show_maximized_library` only | DEC-§7: Prepare is a 1:1 rb6 replica; Perform moves towards hardware skeuomorphism. | B-001, C-001, PAR-A1 |
| PREP-002 | EDIT mode (timeline editor, render) | Skip | N | N | Y (str) | Skip | - | rb7 production feature; not DJ preparation. | PAR-N12 |
| PREP-003 | MY PAGE / INFO window (account, plan, library sync, KUVO) | Skip | N | Y p.6, 191 | Diff p.6, 216 | Skip | - | No accounts; nothing local to copy. | B-002, C-015, PAR-N15 |
| PREP-004 | Notifications tab and rekordbox.com notifications | Skip | N | N | Y p.216 | Skip | - | Account service. | B-003, PAR-L14, PAR-N14 |
| PREP-005 | Licence / plan activation and "depending on your plan" gating | Improve | Diff p.188 | Diff p.191 | Diff p.68, 217 | Skip | - | Improve: everything is available, no account (PARITY N.1). Nothing to build. | B-004, B-005, C-264, PAR-N1 |
| PREP-006 | Help menu: manual, online support, about; tutorials and onboarding | rb6 | Diff p.192 | Diff p.212 (+ Tutorial Videos) | Diff p.241 | Partial | `src/widget/wmainmenubar.cpp` | rb6 set minus update manager, pointing to Mix Two docs (STE). Tutorials / questionnaire skipped. | B-007, PAR-L15 |
| PREP-007 | UI language | rb6 | Y p.176 | Y p.192 | Y p.219 | Built | Mixxx translations | - | B-008, PAR-A12 |
| PREP-008 | Display type: font size (+ bold), line spacing, artwork size in the track list; View > Display Type group | rb6 | Y p.176-178 | Y p.192-194 | Diff p.219-220, 233 | Partial | `src/preferences/dialog/dlgpreflibrarydlg.ui:230,257` (row height, library font) | Add bold and artwork size. Played-track colours (LIB-058), key format (ANA-033) and Traffic Light (LIB-059) sit on the same page. | B-009, PAR-A11, C-261 |
| PREP-009 | Skin Dark / Light | rb6 | N | Y p.195, 209 | Y p.222, 238 | Partial | LateNight Classic / PaleMoon schemes | rb6 two tones. Whether Mix Two's own palettes stay is Q3. | B-010, C-021 |
| PREP-010 | Tooltips on / off | rb6 | Y p.177 | Y p.194 | Y p.219 | Built | Mixxx pref | Cross-reference: see STE-007 (owns the tooltip rows). | - |
| PREP-011 | Full screen | rb6 | Y p.190 | Y p.209 | Y p.238 | Built | Mixxx View menu | - | B-012, C-022, PAR-A10 |

## Sidebar navigation (DEC-§6)

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PREP-012 | Drill-in sidebar: one level at a time, breadcrumb, BACK goes up, as on a CDJ-3000 browse screen; no disclosure-triangle tree | Improve | Diff p.13 (one tree) | Y p.7 (one tree) | Diff p.7-8, 80 (Media Browser column) | Partial | Mixxx sidebar tree `src/widget/wlibrarysidebar.cpp` | DEC-§6 settles inventory B Q4: neither rb6's tree nor rb7's Media Browser. The rekordbox breadcrumb above the list becomes the drill-in breadcrumb. Concept art is visual inspiration only. DDJ-800 BACK / rotary behaviour is CTL. | B-014, B-027, DEC-§6 |
| PREP-013 | Jump box: find any list at any depth | Improve | N | N | str (7.2.19 "Playlist search filter") | Build | - | DEC-§6; covers rb7.2's playlist search. | B-035 |
| PREP-014 | Pinned lists (replaces the rb6 shortcuts bar) | Improve | Y p.15-16 (shortcuts) | Y p.10-11 (shortcuts) | Diff p.10-11 (+ str 7.2.19 "Pin playlist") | Partial | Quick Links for directories only `src/library/browse/browsefeature.cpp` | DEC-§6. Drop a track on a pinned playlist to add it, as rb6 shortcuts do. | B-036, B-017, PAR-F17 |
| PREP-015 | Selecting a folder lists every track in it and below it | rb7-add | N | N | Y p.53 | Build | folders are containers only `src/library/trackset/playlistfeature.cpp` | Adopted (DEC-§6). | B-034 |
| PREP-016 | Track count in sidebar labels (option, default off) | rb7-add | N | N | str (7.2.19 option) | Partial | Mix Two always shows count + duration | PARITY F.2 wants plain labels; rb7.2 option, default off. | B-037 |
| PREP-017 | Playlist column view (multi-select playlists shows their union) | Owner? | N | N | Y p.39, 53 | Build | - | Not covered by DEC-§6; see Q1. | B-033 |

## Browser layout and panes

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PREP-018 | Fixed preset pane sizes switched with a button (no draggable divider in the Legacy skin); initial heights match rb6.8.5 | rb6 | - | Y | - | Build | DEC-§6 | Heights still to be measured in rb6.8.5. | DEC-§6 |
| PREP-019 | Sub Browser (second tree + list beside the main one) | rb6 | Diff p.16 | Y p.12 | Diff p.12 (central to importing) | Build | PARITY A.7 | Optional helper; importing must not depend on it. | B-018 |
| PREP-020 | Split Screen (browser in a separate window; click a deck to load) | rb6 | Y p.8, 120, 190 | Y p.126, 210 | Y p.143, 239 | Build | - | Shared with the Perform face. | B-019, C-016, PAR-A7 |
| PREP-021 | View > Layout: which sources show (Related Tracks, Hot Cue Bank List, iTunes, rekordbox xml, Explorer) | rb6 | Diff p.177 | Diff p.193 | Diff p.221 | Partial | "External Libraries" show / hide `src/preferences/dialog/dlgpreflibrarydlg.ui:582` | rb6 list minus services. | B-232 |
| PREP-022 | Side windows next to the track list: My Tag, Related Tracks, Information, Sub Browser (View menu) | rb6 | Y p.191 | Y p.210 | Y p.239 | Build | - | Contents are LIB-053, LIB-063, LIB-085. | B-239, C-024, PAR-A8 |
| PREP-023 | Track information window docked right (Info / Artwork tabs) | rb6 | Y p.22, 191 | Y p.25, 210 | Y p.25, 239 | Partial | modal `src/library/dlgtrackinfo.ui` | Dock it. | B-023 |
| PREP-024 | Track-list columns: show / hide / reorder; sort by clicking a column | rb6 | Y p.14, 25 | Y p.9, 28 | Y p.9, 29 | Built | Mixxx track table header | - | B-015, B-091, PAR-F13 |
| PREP-025 | Artwork column and panel; display pattern (top part vs whole image) | rb6 | Y p.56 | Y p.63 | Y p.79 | Partial | `src/library/tabledelegates/coverartdelegate.cpp`; no pattern toggle | - | B-016, PAR-F19 |
| PREP-026 | Keep the selected track centred while scrolling | rb6 | Y p.176 | Y p.192 | Y p.219 | Build | - | - | B-028 |

## EXPORT-mode players and panels

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PREP-027 | Layouts 1 PLAYER, 2 PLAYER, SIMPLE PLAYER, FULL BROWSER | rb6 | Y p.43-44, 190 | Y p.48-49, 209-210 | Diff p.60-62 (+ DUAL PLAYER) | Partial | Browse view `[Skin] show_maximized_library`; prep players missing (PARITY A.6) | DEC-§7 answers inventory B Q7: rb6 layouts, built on the normal decks (one engine). ROADMAP §6 P2. | B-169, PAR-A6 |
| PREP-028 | Simple Player (play / pause / volume only) | rb6 | Y p.190 | Y p.210 | Y p.239 | Build | - | - | B-190 |
| PREP-029 | DUAL PLAYER: DUAL CONTROL, 3-band kill EQ, AUTO GAIN, key shift / KEY SYNC in preparation | rb7-add | N | N | Y p.66, 68, 77, 116-117 | Partial | decks have EQ, gain, key; no DUAL CONTROL | Candidate rb7 addition (Q2). | B-170 |
| PREP-030 | Playback info panel: artwork, title, BPM, key, time, playing address; vocal and phrase strips | rb6 | Diff p.47 | Diff p.52 (+ vocal, phrase) | Diff p.67-68 | Partial | deck widgets; no phrase / vocal | Strips need ANA-035, ANA-037. | B-172 |
| PREP-031 | Setting panel: Preferences, volume + meter, audio dropout meter, clock (no account buttons) | rb6 | Diff p.48 | Diff p.53 (+ MY PAGE) | Diff p.68 | Partial | Mixxx shows latency / CPU; no clock in LateNight | rb6 panel without the account button (rb5's set). | B-173 |
| PREP-032 | MEMORY panel: list memory cues / loops (time to ms), colour, comment, active loop, delete | rb6 | Y p.49 | Y p.55 | Y p.70 | Partial | memory cues exist `src/track/cueinfo.h:22`; no list panel | Cue behaviour is PERF. | B-174 |
| PREP-033 | HOT CUE panel: A-H, time, colour, comment, loop mark, delete | rb6 | Diff p.50 | Diff p.56 | Y p.71 | Partial | hot cues + colours; no comments panel | - | B-175 |
| PREP-034 | Enlarged waveform: zoom + / - / RST, drag to move, Matching button | rb6 | Y p.52 | Y p.58 | Y p.73 | Built | waveform zoom / drag | Matching itself is LIB-064. | B-176 |
| PREP-035 | Click enlarged waveform: left = play / pause, right = set cue and play (pref) | rb6 | Y p.66, 178 | Y p.72, 194 | Diff p.88, 220 | Build | Mixxx click-drag scrubs | As an option in Prepare. rb7 also brings it to Perform (C-056, LEG). | B-177 |
| PREP-036 | Preview column: click the mini waveform to audition from there; stop button; preview volume | rb6 | Y p.65, 179 | Y p.71, 196 | Y p.87, 223 | Partial | preview deck + preview / overview columns `src/library/tabledelegates/previewbuttondelegate.cpp`, `overviewdelegate.cpp` | CDJ-3000 Touch Preview is the same idea (D-024). Preview volume: see MIX-026. | B-178, D-024, PAR-F18 |
| PREP-037 | Hover artwork to preview from the start; click skips +30 s | rb6 | Y p.65-66 | Y p.71-72 | Y p.87-88 | Build | - | - | B-179 |
| PREP-038 | Cue / hot-cue markers on the preview waveform (pref); click near one to preview from it | rb6 | Y p.66 | Y p.72 | Y p.88 | Build | overview delegate draws no cue marks `overviewdelegate.cpp` | - | B-180 |
| PREP-039 | GRID / PHRASE EDIT panel in the Prepare players | rb6 | Y p.54 | Y p.60-61 | Y p.75-76 | Build | - | Cross-reference: see ANA-025 and ANA-036 for the editing functions. | - |

## Preferences window and menus

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PREP-040 | Preferences structure: rb6 flat tabs (View, Audio, Analysis, DJ System, Controller, Effect, Sampler, Recordings, Others, Keyboard, Advanced) | rb6 | Diff p.176-188 | Diff p.192-207 | Diff p.217-236 (categories with sub-tabs) | Partial | Mixxx page tree `src/preferences/dialog/dlgpreferencesdlg.ui` | DEC-§1 / §7 settle inventory B Q9: Prepare is a 1:1 rb6 replica and rb7 redesigns are rejected, so rb6's tabs, mapped onto Mixxx's page tree. Account / plan / cloud tabs left out. | B-231, C-255 |
| PREP-041 | View prefs: waveform drawing rate | rb6 | Y p.177 | Y p.194 | Y p.220 | Built | frame-rate setting `src/preferences/dialog/dlgprefwaveformdlg.ui` | Cross-reference: View-tab item; see LEG-044 (owns the row). | - |
| PREP-042 | Controller preference tabs: Deck (locks, Load, Auto Cue, Vinyl Speed Adjust), Others (QUANTIZE, Ableton Link), Effect, Sampler, Recordings, MIX POINT LINK | rb6 | Diff p.182-185 | Diff p.200-203 | Diff p.227-232 | Partial | Mixxx Decks / Controllers pages | Page layout only, in rb6's tabs (PREP-040); the settings themselves are PERF, FX and CTL rows. | C-258, C-259, C-260 |
| PREP-043 | Menu > View: 2 / 4 Deck H / V, Browse, Show / Hide Pad and Platter, Split Screen, Automix, Full screen | rb6 | Y p.190-191 | Y p.209-210 | Y p.238-239 | Partial | Mixxx View menu `src/widget/wmainmenubar.cpp` | Menu entries only; the layouts are LEG, Split Screen PREP-020, Full screen PREP-011. | C-262 |

## Open questions for the owner

1. **Playlist column view (rb7, PREP-017).** With the drill-in sidebar and the folder union (PREP-015), is rb7's column view with multi-select still useful? Options: skip; adopt later as an alternative view. *Recommend skip*: drill-in plus folder union covers its use.
2. **DUAL PLAYER (rb7 addition, PREP-029).** Options: adopt as a Prepare layout of the normal decks; skip. *Recommend adopt later*, after the rb6 layouts (PREP-027) exist.
3. **Skin colours in Prepare (PREP-009).** Options: rb6's Dark / Light only; also keep LateNight's other palettes. *Recommend rb6 Dark / Light* for the Prepare face (1:1 replica), other palettes only in skins that are not the replica.

<!-- End of AI-generated text. -->
