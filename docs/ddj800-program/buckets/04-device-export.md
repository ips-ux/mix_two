<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 04: Device export

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** Critical  ·  **Scope:** library and playlist export to Pioneer gear on USB / SD in the legacy format (PDB / ANLZ) and in Device Library Plus (= OneLibrary), as rekordbox 6.8.5 writes them; My Settings on the stick; Sync Manager and device library maintenance, including legacy and Device Library Plus conversion. PRO DJ LINK rows are listed but deferred (DEC-§4).

**Export status rule (DEC-§4):** an export row reaches **Checked** only when the stick passes Mix Two's own reader, the independent parsers (crate-digger, rekordcrate) and rekordbox 6 / 7 as judge. A byte diff against a stick that rekordbox 6.8.5 made is the debugging tool. **CDJ-verified** comes only from the owner's tests on real decks.

## Export formats and writers

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| EXP-001 | Legacy Device Library export (`export.pdb` + `ANLZ0000.DAT/.EXT/.2EX`) | rb6 | Diff p.61 (4 players) | Diff p.67 (6 with CDJ-3000) | Y p.83 | Build | ROADMAP F11, WS-C; only an Engine DJ exporter in `src/library/export/` | Target what rb6.8.5 writes. The Kaitai specs in `lib/rekordbox-metadata` are read-only, so a serializer is needed. Fundamental (DEC-§4). | B-191, PAR-K1, RM-F11 |
| EXP-002 | Device Library Plus export (`exportLibrary.db`, SQLCipher); rekordbox 7.2 calls it OneLibrary | rb6 | N | str (6.8.5 "Device Library Plus (OPUS-QUAD ONLY)") | str (7.2.19 "OneLibrary") | Build | ROADMAP F7, U4 | DEC-§4: one format, called Device Library Plus here; target the rb6.8.5 writer. Needs the schema and key derivation (U4) and SQLCipher in the build. CDJ-3000 reads it only with later firmware (D conflict X10). | B-219, PAR-K2, RM-F7, RM-U4 |
| EXP-003 | Convert legacy to / from Device Library Plus; keep both in step | rb6 | N | str (6.8.5 "Convert to Device Library Plus") | str (7.2.19 "make the same") | Build | ROADMAP F7 (`ConvertDeviceLibraryPlusDlgMain`) | In scope (DEC-§4). rb7's "keep both in step" is an rb7 addition; adopt only if rb6.8.5 does not already do it on export. | PAR-K3, DEC-§4 |
| EXP-004 | Export verification harness: own reader, crate-digger, rekordcrate, rekordbox 6 / 7 as judge, byte diff | Improve | - | - | - | Build | DEC-§4 | Owner's rule for Checked. One script that runs all judges on a written stick and on a rekordbox-made reference stick. | DEC-§4 |
| EXP-005 | Export dialog with targets Legacy, Device Library Plus, Both | rb6 | - | str | str | Build | ROADMAP WS-C (model: `dlglibraryexport`, `engineprimeexportjob`) | Default target is an open question (Q2). | DEC-§4 |
| EXP-006 | Analysis data converted on export: grid (PQTZ), cues (PCOB / PCO2), waveforms (PWAV, PWV2-PWV7), phrases (PSSI), key, BPM | rb6 | Y | Y | Y | Build | DEC-§5 | Mixxx analysis is converted to Pioneer formats on export. Imported rekordbox analysis is written back unchanged (ANA-001). Analysis work rises to High only if grids block Device Library Plus. | DEC-§5 |
| EXP-007 | Players use the exported grids for quantize, beat sync and hot cues | rb6 | Y p.82-83 | Y p.90-91 | Y p.106-107 | Build | depends on EXP-001 / EXP-006 | Grid quality on the CDJ is the test of EXP-006. | B-199 |
| EXP-008 | Writable file systems: FAT32 and exFAT (HFS+ read) | rb7-add | Diff p.198 (FAT16 / FAT32 / HFS+) | Diff p.88, 218 (warning icon on unsupported) | Diff p.104, 246 (+ exFAT) | Build | reader accepts FAT / exFAT / HFS+ paths `src/library/rekordbox/rekordboxfeature.cpp:51-52` | exFAT is an rb7 addition (only some players read it); keep rb6's warning icon for unsupported drives. | B-213 |
| EXP-009 | Hot cues mapped to A-H on export (Mix Two has 36) | CDJ-3000 | - | Diff (8 in EXPORT, 16 in PERFORMANCE) | Y | Build | CDJ-3000 p58-59 | Write A-H; warn when a track has more hot cues than the player can show. | D-040 |
| EXP-010 | Memory cue / loop limit per track on the device (10) | CDJ-3000 | Y | Y | Y | Build | CDJ-3000 p84 | Mix Two keeps unlimited memory cues (PERF owns that); the exporter writes the first 10 and warns. | D-034 |
| EXP-011 | Per-track "Allow to auto load HotCue on CDJ / XDJ" flag + preference to set it on import | rb6 | Y p.181 | Y p.198 | Diff p.227 (moved to DJ System > Others) | Build | CDJ-3000 p59-60 (HOT CUE AUTO LOAD = rekordbox SETTING reads this flag) | Was Skip in PARITY; export is now in scope (DEC-§4). | B-084, D-041, PAR-G19 |
| EXP-012 | Waveform Divisions shown on CDJ / XDJ (time scale or phrase) | rb7-add | N | N | str (7.2.19) | Build | My Settings item WAVEFORM DIVISIONS (EXP-069) | rb7 addition; also a MYSETTING2 value in rb6.8.5 strings. Candidate, adopt when the writer exists. | B-220 |
| EXP-013 | Special library for MEP-4000 / XDJ-R1 (+ 320x320 artwork) | Skip | Y p.79, 81, 181 | Y p.87, 89, 199 | N (removed) | Skip | - | Dropped in rb7; owner's gear does not need it. | B-212, PAR-K9 |
| EXP-014 | Auto Export (files imported from an external device are exported automatically) | rb6 | Y p.186 | Y p.204 | Y p.233 | Build | - | Meaning unclear; unverified. Check in rekordbox 6.8.5 before building. | B-074 |

## Transfer to devices

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| EXP-015 | Transfer tracks by drag (progress bar, cancel, 2 devices at once, delete from device) | rb6 | Y p.81 | Y p.89 | Y p.105 | Build | only Engine DJ export `src/library/export/dlglibraryexport.cpp`; "Export Track Files" copies audio only | - | B-207 |
| EXP-016 | Transfer playlists with their tracks; delete; per-player name and nesting limits | rb6 | Y p.81 | Y p.89 | Y p.105 | Build | - | - | B-208 |
| EXP-017 | Track / Playlist menu: Export Track, Export Playlist | rb6 | Y p.191 | Y p.211 | Y p.240 | Build | - | Was WS-C in ROADMAP §6 P2. | B-210, PAR-G11 |
| EXP-018 | Transfer Hot Cue Bank Lists to a device | rb6 | Y p.82 | Y p.90 | Y p.106 | Build | - | Needs Hot Cue Bank Lists (LIB). | B-209 |
| EXP-019 | Delete Tracks preference: deleting a device playlist also deletes its tracks unless used elsewhere | rb6 | Y p.181 | Y p.199 | Y p.226 | Build | - | - | B-211 |
| EXP-020 | Safe-removal button; cancel a transfer | rb6 | Y p.81 | Y p.89 | Y p.105 | Build | - | - | B-214 |

## Device settings and Sync Manager

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| EXP-021 | Per-device settings: Device Name, Background Color, General (waveform colour, overview type, key format), Category, Sort, Column, colour comments, My Settings; apply automatically | rb6 | Diff p.80 | Diff p.88 | Diff p.104 ("Apply on all connected devices") | Build | - | rb6 applies automatically; rb7's button is a redesign (rejected by default). Written into the export. | B-204, PAR-K5 |
| EXP-022 | DJ System defaults for devices: General, Category, Sort, Column | rb6 | Y p.180-181 ([CDJ & Device]) | Y p.198 ([DJ System]) | Y p.226 | Build | - | Preferences page in rb6's [DJ System] tab (PREP owns the page layout). | B-205 |
| EXP-023 | SYNC MANAGER for devices: ticked playlists re-exported on start or on click | rb6 | Y p.35-36 | Y p.39-40 | Y p.51-52 | Build | - | In scope (DEC-§4). The iTunes side of SYNC MANAGER is IMP-012. | B-218, PAR-K4 |
| EXP-024 | Device library backup: to the computer, automatic on removal, restore, manage | rb6 | N | str + p.206, 209 | Y p.218, 238 | Build | - | Local destination only; cloud destination skipped. Device library maintenance is in scope (DEC-§4). | B-069, PAR-K6 |
| EXP-025 | My Settings export: MYSETTING.DAT, MYSETTING2.DAT, DEVSETTING.DAT (+ DJMMYSETTING.DAT) | rb6 | Y p.181 | Y p.198 (+ str) | Y p.226 | Build | no My Settings code in `src/library/rekordbox/` (B conflict C5) | In scope (DEC-§4); was Skip in PARITY (D conflict X1). Byte layout to confirm against files rekordbox 6.8.5 writes (format notes from Deep Symmetry are Inferred). Source of the CDJ-only values: Q1. Full setting list below. | B-206, PAR-K7 |
| EXP-026 | Import a player-made My Settings file (rb6.8.5) | rb6 | N | str (6.8.5) | Y | Build | - | Lets a DJ bring settings saved on a CDJ back into Mix Two. | DEC-§4 |
| EXP-027 | Mobile devices over Wi-Fi (rekordbox iOS / Android export, edits back) | Skip | Diff p.60, 89 | Diff p.46-47, 65 (pairing) | Y p.58-59, 80 | Skip | - | Needs the rekordbox mobile app and account; nothing local to build. | B-221, PAR-N4 |

## CDJ-3000 My Settings list

Each row is one CDJ-3000 player setting (UTILITY p74-77, SHORTCUT p80, saved
states p81) or a rekordbox-only / later-firmware item. The rb6 column says
whether rekordbox 6.8.5's My Settings page has it (Observed in `rekordbox.exe`
strings); rb5 and rb7 were not checked per item. "File" in Evidence is
Inferred from community format notes. Items rb6 does not store are Skip
(unit-local; rb6 does not write them). Src `D-MS` = inventory D section 2.

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| EXP-028 | MS: LOAD LOCK (UNLOCK*, LOCK) | CDJ-3000 | - | str (EJECT/LOAD LOCK) | - | Build | CDJ p74; MYSETTING | Mix Two equivalent: load-when-playing lock (PERF, C-073). | D-MS LOAD LOCK |
| EXP-029 | MS: QUANTIZE BEAT VALUE (1/8, 1/4, 1/2, 1*) | CDJ-3000 | - | str | - | Build | CDJ p74; MYSETTING | Mix Two has whole beats only (D-032, PERF). | D-MS QUANTIZE BEAT VALUE |
| EXP-030 | MS: BEAT JUMP BEAT VALUE (1/2 ... 64, 16*) | CDJ-3000 | - | str (later firmware item) | - | Build | CDJ p74; MYSETTING2 | Write Mix Two's `beatjump_size` or 16. | D-MS BEAT JUMP BEAT VALUE |
| EXP-031 | MS: HOT CUE AUTO LOAD (OFF, rekordbox SETTING, ON*) | CDJ-3000 | - | str | - | Build | CDJ p74; MYSETTING | Pairs with the per-track flag EXP-011. | D-MS HOT CUE AUTO LOAD |
| EXP-032 | MS: HOT CUE COLOR (OFF*, ON) | CDJ-3000 | - | str | - | Build | CDJ p58, p74; MYSETTING | Player-side setting; export field only. | D-MS HOT CUE COLOR, D-042 |
| EXP-033 | MS: AUTO CUE LEVEL (-78 to -36 dB in 6 dB steps, MEMORY*) | CDJ-3000 | - | str | - | Build | CDJ p74; MYSETTING | Mix Two uses a fixed -60 dB threshold (D-021, PERF). | D-MS AUTO CUE LEVEL |
| EXP-034 | MS: VINYL SPEED ADJUST (TOUCH&RELEASE, TOUCH*, RELEASE) | CDJ-3000 | - | str (later firmware item) | - | Build | CDJ p74; MYSETTING2 | Deck ramps are DECK (D-012). | D-MS VINYL SPEED ADJUST |
| EXP-035 | MS: PLAY MODE (CONTINUE, SINGLE*) | CDJ-3000 | - | str (PLAY MODE / AUTO PLAY MODE) | - | Build | CDJ p75; MYSETTING | - | D-MS PLAY MODE |
| EXP-036 | MS: LCD BRIGHTNESS (1-5, 3*) | CDJ-3000 | - | str (Main Display) | - | Build | CDJ p75; MYSETTING | CDJ-only value. | D-MS LCD BRIGHTNESS |
| EXP-037 | MS: JOG LCD BRIGHTNESS (1-5, 3*) | CDJ-3000 | - | str (Deck Display) | - | Build | CDJ p75; MYSETTING2 | CDJ-only value. | D-MS JOG LCD BRIGHTNESS |
| EXP-038 | MS: LANGUAGE | CDJ-3000 | - | str | - | Build | CDJ p75; MYSETTING | Could follow Mix Two's UI language. | D-MS LANGUAGE |
| EXP-039 | MS: SCREEN SAVER (OFF, ON*) | CDJ-3000 | - | N | - | Skip | CDJ p75 | Unit-local; rb6 does not write it. | D-MS SCREEN SAVER |
| EXP-040 | MS: TOUCH DISPLAY CALIBRATION | CDJ-3000 | - | N | - | Skip | CDJ p75 | Unit-local action. | D-MS TOUCH DISPLAY CALIBRATION |
| EXP-041 | MS: SLIP FLASHING (OFF, ON*) | CDJ-3000 | - | str | - | Build | CDJ p76; MYSETTING | CDJ-only value. | D-MS SLIP FLASHING |
| EXP-042 | MS: ON AIR DISPLAY (OFF, ON*) | CDJ-3000 | - | str | - | Build | CDJ p76; MYSETTING | CDJ-only value. | D-MS ON AIR DISPLAY |
| EXP-043 | MS: JOG RING BRIGHTNESS (OFF, 1, 2*) | CDJ-3000 | - | str | - | Build | CDJ p76; MYSETTING | CDJ-only value. | D-MS JOG RING BRIGHTNESS |
| EXP-044 | MS: JOG RING INDICATOR (OFF, ON*) | CDJ-3000 | - | str | - | Build | CDJ p76; MYSETTING | CDJ-only value. | D-MS JOG RING INDICATOR |
| EXP-045 | MS: PLAYER No. (AUTO*, 1-6) | CDJ-3000 | - | N | - | Skip | CDJ p76 | Unit-local. | D-MS PLAYER No. |
| EXP-046 | MS: DUPLICATION (ALL*, PLAYER 1-6) | CDJ-3000 | - | N | - | Skip | CDJ p76 | Unit-local action; rb6 offers apply-to-devices instead (EXP-021). | D-MS DUPLICATION |
| EXP-047 | MS: MIDI CHANNEL (1*-16) | CDJ-3000 | - | N | - | Skip | CDJ p76 | Unit-local for players. | D-MS MIDI CHANNEL |
| EXP-048 | MS: AUTO STANDBY (OFF, ON*) | CDJ-3000 | - | N | - | Skip | CDJ p77 | Unit-local. | D-MS AUTO STANDBY |
| EXP-049 | MS: OUTPUT ATT. (-12 to 0* dB) | CDJ-3000 | - | N | - | Skip | CDJ p77 | Unit-local; output headroom is MIX (C-214). | D-MS OUTPUT ATT. |
| EXP-050 | MS: HISTORY NAME (32 chars, numbered) | CDJ-3000 | - | N | - | Skip | CDJ p77 | Unit-local; history naming in Mix Two is LIB-082. | D-MS HISTORY NAME |
| EXP-051 | MS: DEFAULT (reset marked items) | CDJ-3000 | - | str ("Reset to defaults") | - | Build | CDJ p77 | A reset button on the My Settings page. | D-MS DEFAULT |
| EXP-052 | MS: Time display mode (REMAIN / ELAPSED) | CDJ-3000 | - | str (TIME MODE) | - | Build | CDJ p15, p81; MYSETTING | Mix Two time format pref exists. | D-MS Time display mode |
| EXP-053 | MS: Auto Cue on / off | CDJ-3000 | - | str (AUTO CUE) | - | Build | CDJ p53, p81; MYSETTING | Mix Two equivalent Partial (C-076). | D-MS Auto Cue on/off |
| EXP-054 | MS: Jog mode (VINYL / CDJ) | CDJ-3000 | - | str (JOG MODE) | - | Build | CDJ p49, p81; MYSETTING | - | D-MS Jog mode |
| EXP-055 | MS: Tempo range (6, 10, 16, WIDE) | CDJ-3000 | - | str (TEMPO RANGE) | - | Build | CDJ p47, p81; MYSETTING | Mix Two default range is a deck pref. | D-MS Tempo range |
| EXP-056 | MS: Master Tempo on / off | CDJ-3000 | - | str | - | Build | CDJ p48, p81; MYSETTING | From Mix Two `keylock` default. | D-MS Master Tempo |
| EXP-057 | MS: Quantize on / off | CDJ-3000 | - | str | - | Build | CDJ p61, p81; MYSETTING | - | D-MS Quantize |
| EXP-058 | MS: Beat Sync on / off (SYNC BPM / BEAT) | CDJ-3000 | - | str | - | Build | CDJ p66, p81; MYSETTING | From Mix Two sync mode. | D-MS Beat Sync |
| EXP-059 | MS: Phase meter type (TYPE 1 / TYPE 2) | CDJ-3000 | - | str (Inferred) | - | Build | CDJ p81; MYSETTING | Mix Two has no phase meter (D-070). | D-MS Phase meter type |
| EXP-060 | MS: WAVEFORM / PHASE METER (WAVEFORM*) | CDJ-3000 | - | str | - | Build | CDJ p80; MYSETTING2 | - | D-MS WAVEFORM / PHASE METER |
| EXP-061 | MS: WAVEFORM COLOR (BLUE*, RGB, 3 BAND) | CDJ-3000 | - | str (General: Waveform color) | - | Build | CDJ p80; DEVSETTING | Device setting; ties to EXP-021 General. | D-MS WAVEFORM COLOR, D-072 |
| EXP-062 | MS: WAVEFORM CURRENT POSITION (CENTER*, LEFT) | CDJ-3000 | - | str | - | Build | CDJ p80; DEVSETTING | Mix Two playhead position pref exists. | D-MS WAVEFORM CURRENT POSITION |
| EXP-063 | MS: Overview waveform type (Half / Full) | rb6 | - | str | - | Build | DEVSETTING | Not in the CDJ-3000 manual. | D-MS Overview waveform type |
| EXP-064 | MS: Key display format (Classic / Alphanumeric) | rb6 | - | str | - | Build | DEVSETTING | From Mix Two key notation pref. | D-MS Key display format |
| EXP-065 | MS: NEEDLE LOCK | rb6 | - | str | - | Build | MYSETTING | Touch-strip players; Mix Two Needle Lock is PERF (C-074). | D-MS NEEDLE LOCK |
| EXP-066 | MS: DISC SLOT ILLUMINATION | rb6 | - | str | - | Build | MYSETTING | Not on the CDJ-3000; write rb6's default. | D-MS DISC SLOT ILLUMINATION |
| EXP-067 | MS: JOG DISPLAY MODE (AUTO / INFO / SIMPLE / ARTWORK) | rb6 | - | str | - | Build | MYSETTING2 | Later firmware item. | D-MS JOG DISPLAY MODE |
| EXP-068 | MS: PAD / BUTTON BRIGHTNESS | rb6 | - | str | - | Build | MYSETTING2 | Later firmware item. | D-MS PAD/BUTTON BRIGHTNESS |
| EXP-069 | MS: WAVEFORM DIVISIONS (TIME SCALE / PHRASE) | rb6 | - | str | str (7.2.19) | Build | MYSETTING2 | See EXP-012. | D-MS WAVEFORM DIVISIONS |
| EXP-070 | MS: Mixer items (DJMMYSETTING: fader curves, headphones pre / post EQ, mono split, Beat FX quantize, mic low cut, talk over, MIDI button type, brightness) | DJM-900NXS2 | - | str | - | Build | DJMMYSETTING | Write only when the DJ uses a DJM (Q3). Values map to MIX rows (D-131, D-132, D-134, D-127). | D-MS Mixer items |

## PRO DJ LINK (deferred, DEC-§4)

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| EXP-071 | LINK EXPORT: computer feeds players over LAN (4 / 6 players, 2 computers) | rb6 | Diff p.63-64 | Diff p.69-70 | Y p.85-86 | Build | - | Deferred (DEC-§4). | B-192, PAR-K8 |
| EXP-072 | Link status panel: LINK button, Master BPM, Sync Master, player icons | rb6 | Diff p.58 | Y p.65 | Y p.81, 110 | Build | - | Deferred with EXP-071. | B-193 |
| EXP-073 | LINK MONITOR (rekordbox audio in the DJM headphones) | rb6 | Y p.86, 179 | Y p.94, 196 | Diff p.110, 227 | Build | - | Deferred with EXP-071. | B-194 |
| EXP-074 | Drag a track onto a networked player; EJECT / LOAD LOCK respected | rb6 | Y p.87 | Y p.95 | Y p.111 | Build | - | Deferred with EXP-071. | B-195 |
| EXP-075 | Load a Hot Cue Bank List onto a networked player | rb6 | Y p.88 | Y p.96 | Y p.112 | Build | - | Deferred with EXP-071. | B-196 |
| EXP-076 | Tag List browsing from players; used tag lists saved as playlists | rb6 | Y p.88-89 | Y p.96-97 | Y p.112-113 | Build | - | Deferred with EXP-071. Local Tag List: LIB-080. | B-197 |
| EXP-077 | LINK HISTORY | rb6 | Y p.20, 85 | Y p.19, 93 | Y p.19, 109 | Build | - | Deferred with EXP-071. | B-198 |
| EXP-078 | Computer Name on players; Quantize Beat Value for players | rb6 | Y p.187 | Y p.206 | Diff p.234 | Build | - | Deferred with EXP-071. | B-200 |
| EXP-079 | Grid editing and analysis blocked while PRO DJ LINK is connected | rb6 | Y p.69 | Y p.75 | Y p.91 | Build | - | Deferred with EXP-071; applies only once LINK exists. | B-159, PAR-I14 |
| EXP-080 | Firewall program list for LINK / mobile | Skip | Diff p.195 | Diff p.215 | Diff p.243 | Skip | - | Installer / OS plumbing; revisit with EXP-071. | B-201 |
| EXP-081 | CloudDirectPlay, device authentication, cloud analysis on DJ equipment | Skip | N | N | Y p.219 | Skip | - | Cloud and account service. | B-202 |
| EXP-082 | Hide info of CDJ-played tracks from the PRO DJ LINK phone app | Skip | N | N | Y p.235 | Skip | - | rb7 only; phone-app service. | B-203 |

## Open questions for the owner

1. **Source of the CDJ-only My Settings values** (brightness, jog ring, slip flashing, on-air display, play mode, hot cue colour, hot cue auto load). Options: (a) a "Player settings" page in Mix Two Preferences like rb6's My Settings tab; (b) derive what we can from Mix Two prefs and write rekordbox defaults for the rest; (c) import a player-made file and pass it through. *Recommend (a) with (c) as import (EXP-026), as rb6.8.5 does.*
2. **Default export target.** Options: Legacy only; Device Library Plus only; Both. *Recommend Both by default* (rb6.8.5 writes both, and older players read only legacy).
3. **DJM My Settings (DJMMYSETTING).** Options: always write; write only when the DJ ticks "I use a DJM"; never. *Recommend write only on request.*
4. **Waveform Divisions (rb7 addition, EXP-012).** Adopt when the writer exists, or wait? *Recommend adopt*: rb6.8.5 already stores the value in MYSETTING2.

<!-- End of AI-generated text. -->
