<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 08: Library

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** High  ·  **Scope:** Collection, playlists, intelligent playlists, Tag List, My Tag, search and filters, Related Tracks, Track Suggestion, histories, track menu items, and the managed library's file handling: import in place, consolidate, backup and restore, the delete flow, Fix tags and tag writing (DEC-§6). Sidebar navigation, pane sizes and browser columns are PREP; rekordbox import paths are IMP.

## Collection and adding tracks

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LIB-001 | [Collection] = all tracks | rb6 | Y p.13 | Y p.7 | Y p.7 | Built | Mixxx Tracks view | - | B-013, PAR-F1 |
| LIB-002 | Library location on first run: Local library (internal drive) or Portable library (removable drive, clearly not the USB performance stick); the other kind offered as the backup drive | Improve | - | - | - | Build | DEC-§6 | Owner's improvement: one managed library folder the DJ chooses on purpose. | DEC-§6 |
| LIB-003 | Import in place: import a rekordbox library and leave every file where it is | Improve | - | - | - | Build | DEC-§6 | Consolidate and Back up are separate, optional offers (LIB-013, LIB-017). The import itself is IMP-001 / IMP-005. | DEC-§6 |
| LIB-004 | Watched music directories with automatic rescan (Mixxx model) | Improve | N | N | N | Partial | `src/preferences/dialog/dlgpreflibrarydlg.ui` (Music Directories, rescan on start-up) | DEC-§6: **no watched folders**. Exists in Mixxx and must be replaced by explicit import (LIB-006) and the move-in question (LIB-009). Answers inventory B Q2. | B-044 |
| LIB-005 | Drag files / folders onto the track list to add | rb6 | Y p.17 | Y p.13 | Y p.13 | Built | Mixxx drop-to-library | Tracks from outside the library folder get the move-in question (LIB-009). | B-042 |
| LIB-006 | File > Import > Track / Folder (one-shot import) | rb6 | Y p.18, 189 | Y p.15, 208 | Y p.15, 237 | Partial | Mixxx has only Add Music Directory (watched) + rescan | Add the one-shot import menu. | B-043 |
| LIB-007 | Tag formats read (ID3 v1-2.4, MP4, RIFF INFO, Vorbis) | rb6 | Y p.17 | Y p.13 | Y p.13 | Built | TagLib | - | B-045 |
| LIB-008 | Playable audio formats (MP3, AAC, WAV, AIFF, ALAC, FLAC; 16 / 24-bit; to 192 kHz) | rb6 | Y p.196-197 | Y p.216-217 | Y p.244-245 | Built | Mixxx decoders (also OGG, Opus, WavPack) | Mix Two superset is fine. | B-046 |
| LIB-009 | Adding a track from outside the library folder asks to move it in (one question per batch, "do this for all tracks") | Improve | - | - | - | Build | DEC-§6 | Replaces watched folders. | DEC-§6 |
| LIB-010 | Explorer: browse computer / USB, add to Collection | rb6 | Y p.19 | Y p.18 | Diff p.18 (via sub browser) | Built | `src/library/browse/browsefeature.cpp` | - | B-056, PAR-F22 |
| LIB-011 | Drop an Explorer folder on [Playlists] to make a playlist named after it | rb6 | Y p.33 | Y p.36 | Y p.45 | Build | not found in `src/library/trackset/baseplaylistfeature.cpp` drop handling | Unverified (inventory said Unknown). | B-057 |
| LIB-012 | Recordings tree item | rb6 | Y p.57 | Y p.64 | Y p.80 | Built | `src/library/recording/recordingfeature.cpp` | Recording itself is PERF. | B-040, PAR-F26 |

## File management: consolidate, backup, delete, maintenance

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LIB-013 | Consolidate: gather files into the library folder; optional offer, pre-selected and marked Recommended | Improve | - | - | - | Build | DEC-§6 | The DJ can always proceed without it. | DEC-§6 |
| LIB-014 | Consolidate safety: copy, check by content hash, then send the original to the Recycle Bin / Trash; never remove from removable media; keep the file name, change only the folder | Improve | - | - | - | Build | DEC-§6 | Q17 rules. rekordbox repair afterwards is IMP-004. | DEC-§6 |
| LIB-015 | "Rename files from tags": separate optional action | Improve | - | - | - | Build | DEC-§6 | Never part of Consolidate. | DEC-§6 |
| LIB-016 | Folder pattern setting in Preferences; default `Artist/<file>`; patterns from tags (album, year, genre ...) | Improve | - | - | - | Build | DEC-§6 | - | DEC-§6 |
| LIB-017 | Back up: mirror the library to a second drive or a Dropbox / Google Drive folder; pre-selected and Recommended; the DJ can proceed without; a quiet "No backup" status, no repeated pop-ups | Improve | - | - | - | Build | DEC-§6 | Cloud folders are only local sync folders; no account code. | DEC-§6 |
| LIB-018 | Restore from backup: list what the backup has and the library does not, copy it back | Improve | - | - | - | Build | DEC-§6 | - | DEC-§6 |
| LIB-019 | Delete flow: "Delete from library?" and "Delete from backup?" (both unchecked); with backup on every delete asks about the backup; "Are you sure?" for backup deletes (can be turned off); deleted backup files kept 30 days in hidden `_Removed` | Improve | - | - | - | Build | DEC-§6 | No "always delete from backup" setting. | DEC-§6 |
| LIB-020 | Remove from Collection (file kept) | rb6 | Y p.21 | Y p.20 | Y p.20 | Built | Hide / Purge `src/widget/wtrackmenu.cpp:343,355` | Must follow the delete flow (LIB-019). | B-061, PAR-G9 |
| LIB-021 | Delete the file itself (to the Recycle Bin / Trash) | Improve | N | Diff p.20 (Inflyte only) | Diff p.20 (+ 7.2 str) | Built | `src/widget/wtrackmenu.cpp:304` | Mix Two already better than rb6. Must follow LIB-019. | B-062 |
| LIB-022 | Backup Library (database + analysis; music optional into `rekordbox_bak`; portable Mac / Win) | rb6 | Y p.96, 175, 189 | Y p.21, 209 | Y p.21, 238 | Build | - | Database-level backup, beside the file mirror (LIB-017). | B-067, PAR-J1 |
| LIB-023 | Restore Library (.zip) | rb6 | Y p.97, 190 | Y p.22, 209 | Diff p.22, 48 | Build | - | - | B-068 |
| LIB-024 | Missing File Manager (`!` marker, list, delete from Collection) | rb6 | Y p.98, 189 | Y p.23, 208 | Y p.23, 237 | Partial | `src/library/missing_hidden/dlgmissing.cpp` | - | B-070, PAR-J2 |
| LIB-025 | Relocate manually (pick new file; multi-select relocates the rest to the same folder) | rb6 | Y p.98, 191 | Y p.24, 211 | Y p.24, 240 | Partial | only directory relink in prefs `dlgpreflibrarydlg.ui`; no per-track relocate | - | B-071, PAR-G14 |
| LIB-026 | Auto Relocate + search folders | rb6 | N | Y p.23, 204, 211 | Y p.23, 232, 240 | Partial | rescan re-finds moved files by hash `src/library/relocatedtrack.h`, inside watched dirs only | PARITY J.3 said Build (B conflict C12). Gap: search folders and a per-track action; must not depend on watched dirs (LIB-004). | B-072, PAR-J3 |
| LIB-027 | Move the master database (with analysis files) to another drive | rb6 | Y p.186 | Y p.204 | Y p.233 | Partial | `--settings-path` only | Needs a UI (ROADMAP §6 P3). Ties to Portable library (LIB-002). | B-073, PAR-J4 |
| LIB-028 | Duplicated File Manager ("Display All Duplicated Files"; 7.2 "Merge Duplicate Files") | rb7-add | str | str | str (+ merge) | Build | - | Candidate (DEC-§1); verify behaviour in 7.2.19 first (Q2). | B-063 |
| LIB-029 | Playlist / folder trash: restore or delete permanently | rb7-add | N | N | str (7.2.19) | Build | - | Candidate (DEC-§1); verify behaviour first (Q2). | B-064 |
| LIB-030 | Library Protection (block edits / deletes of track info and lists) | rb6 | Y p.187 | Y p.205 | Y p.233 | Partial | per-playlist Lock `src/library/trackset/baseplaylistfeature.cpp:98` | - | B-032, PAR-F27, PAR-J9 |

## Tags and track information

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LIB-031 | Edit track info: core tags plus mix name, remixer, label, lyricist, original artist, release date | rb6 | Y p.22 | Y p.25 | Y p.25 | Partial | `src/library/dlgtrackinfo.ui` lacks mix name, remixer, label, lyricist, original artist, release date | Core tags are there. ROADMAP §6 P2 "first-class mix name, original artist and lyricist fields". | B-077, PAR-H1, PAR-H2 |
| LIB-032 | Tag edits written to the file at once, only for files in the managed library | Improve | - | - | - | Partial | Mixxx metadata sync option, off by default (`dlgpreflibrarydlg.ui`) | DEC-§6. Files outside the library are never written. | DEC-§6 |
| LIB-033 | Write the detected key into the ID3 tag on analysis | rb6 | Y p.180 | Y p.197 | Y p.224 | Partial | only via "Export To File Tags" / metadata sync `src/widget/wtrackmenu.cpp:422` | PARITY H.8 said Have (B conflict C8). Managed-library files only (LIB-032). | B-085, PAR-H8 |
| LIB-034 | Fix tags work list: tracks without artist or title; automatic save and resume, progress count, "Skip for now" (stays in `Various-Uncategorized/`), "No artist needed" (SFX, samples to `Samples-SFX/`, never asked again) | Improve | - | - | - | Build | DEC-§6 | - | DEC-§6 |
| LIB-035 | Fix tags tools: fill from file name, bulk edit, keyboard flow | Improve | - | - | - | Build | DEC-§6; bulk edit exists (LIB-044) | - | DEC-§6 |
| LIB-036 | AcoustID + MusicBrainz lookup with confidence-based auto-apply, batch "accept all high-confidence", tunable confidence threshold | Improve | - | - | - | Partial | Mixxx MusicBrainz lookup (fingerprint via AcoustID) in the track menu | DEC-§6. rekordbox has none (PARITY H.9 Improve). Missing: confidence, auto-apply, batch accept, threshold. | DEC-§6, PAR-H9 |
| LIB-037 | Undo log of tag changes | Improve | - | - | - | Build | DEC-§6 | Covers manual, bulk and lookup edits. | DEC-§6 |
| LIB-038 | One-time notice before the first online lookup; every outgoing data flow listed in a privacy policy | Improve | - | - | - | Build | DEC-§6 | The policy text itself is STE. | DEC-§6 |
| LIB-039 | Add artwork (JPEG / PNG; Change Artwork) | rb6 | Y p.23 | Y p.26 | Y p.26 | Built | `src/widget/wcoverartmenu.cpp` | - | B-078, PAR-G13 |
| LIB-040 | Reload Tag (overwrites edits) | rb6 | Y p.23, 191 | Y p.26, 211 | Y p.26, 240 | Built | `src/widget/wtrackmenu.cpp:408` | Goes into the undo log (LIB-037). | B-079, PAR-G5 |
| LIB-041 | Rating (stars) | rb6 | Y p.51 | Y p.57 | Y p.72 | Built | track table column | - | B-080, PAR-H3 |
| LIB-042 | Track colour (8 colours) + custom colour names (also exported to players) | rb6 | Y p.186 | Y p.204 | Y p.233 | Partial | rekordbox palette `src/util/color/predefinedcolorpalettes.cpp:36-44`; no colour names | Red constant has a typo `0xF870900` in two files (B conflict C7, TRU). | B-081, PAR-H4 |
| LIB-043 | Append My Tag names to Comments | rb6 | Y p.27, 186 | Y p.30, 204 | Y p.31, 233 | Build | - | After My Tag (LIB-053). | B-082 |
| LIB-044 | Edit several tracks at once | rb6 | Y | Y | Y | Built | `src/library/dlgtrackinfomulti.cpp` | - | B-083, PAR-H7 |
| LIB-045 | Info panel fields (rating, colour, comment, file type, size, sample rate, bit rate) | rb6 | Y p.51 | Y p.57 | Y p.72 | Built | track table columns, `dlgtrackinfo.ui` | - | B-086 |
| LIB-046 | DJ play count, date added, date created, release date | rb6 | - | Y (Info) | Y | Built | track table columns | Release date edit is LIB-031. | PAR-H5 |
| LIB-047 | Bit depth | rb6 | - | Y (Info) | Y | Partial | - | - | PAR-H6 |
| LIB-048 | Edit list cells (track info, playlist / folder names) by double-click | rb7-add | N | N | str (7.2.19 option) | Built | `src/preferences/dialog/dlgpreflibrarydlg.ui:179` (edit after click) | Mix Two has it as an option already. | B-038 |

## Search, filters, My Tag

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LIB-049 | Search filter with a picker of searchable columns; clear button | rb6 | Y p.24 | Y p.27 | Diff p.28 (+ 7.2 scope) | Partial | field prefixes, OR, negation, fuzzy `src/library/searchqueryparser.cpp:75-146,314`; no column picker | rb6 picker plus Mix Two syntax (Improve). | B-088, PAR-F14 |
| LIB-050 | Category filter (Genre / Artist / Album columns above the list) | rb6 | Y p.25 | Y p.28 | Y p.29 | Build | - | - | B-089, PAR-F12 |
| LIB-051 | Collection Filter (Date Added / Genre / Artist / Album; "Attribute" column) | rb7-add | N | N | Y p.13, 27 | Build | - | Named in DEC-§1 as an rb7 addition; adopt. | B-090 |
| LIB-052 | Alphabet Search (type letters to jump in the sorted column) | rb6 | Y p.25 | Y p.28 | Y p.29 | Partial | custom `keyPressEvent` `src/widget/wtracktableview.cpp:1250` may swallow Qt type-ahead | Unverified (inventory said Unknown). | B-092 |
| LIB-053 | My Tag: categories and tags (rename, add, delete; tick several); My Tag configuration window | rb6 | Y p.26-27, 191 | Y p.29-30, 210 | Y p.30-31, 239 | Build | crates are flat, no categories | How crates relate to My Tag is Q1. The window's place in the layout is PREP-022. | B-093, B-021, PAR-F10 |
| LIB-054 | Track Filter: BPM / KEY / RATING / COLOR / MY TAG, AND / OR, RST | rb6 | Y p.27 | Y p.30 | Y p.31 | Partial | `bpm:`, `key:`, `rating:`, `crate:` filters `searchqueryparser.cpp:75-146`; no colour filter, no filter bar | PARITY Improve: filter bar on top of Mix Two syntax. | B-094, PAR-F11 |
| LIB-055 | Track Filter BPM / KEY taken from the master player / master deck | rb6 | Diff p.27 | Diff p.30 (+ MASTER DECK) | Y p.31 | Build | "Search related Tracks" uses a chosen track `src/widget/wsearchrelatedtracksmenu.cpp` | - | B-095 |
| LIB-056 | Fuzzy BPM / key search, search history and completions | Improve | N | N | N | Built | `searchqueryparser.cpp:59` (`~`), `dlgpreflibrarydlg.ui` | Keep. | B-096 |
| LIB-057 | Status column icons (analysing, `?` old analysis, `!` missing, analysis lock, in-Collection tick) | rb6 | Diff p.17, 19 | Y p.14, 18, 23 | Diff p.14, 35 (+ radar) | Partial | - | rb6 set; the radar mark is cloud. | B-025, PAR-F20 |
| LIB-058 | Colour played tracks; RESET; reset on exit; played threshold 1-60 s | rb7-add | Diff p.176 | Diff p.192 | Diff p.220, 233 (+ threshold) | Partial | grey out played `dlgpreflibrarydlg.ui:296`; reset at start-up `src/library/dao/trackdao.cpp:131` | rb7 threshold stops auditioned tracks being marked. PARITY F.21 said Have (B conflict C9). Default value is Q3. | B-029, PAR-F21 |
| LIB-059 | Traffic Light: highlight keys compatible with the selected deck; range Same / Related 1-3 | rb6 | Y p.56 | Y p.63 | Y p.79, 220 (range pref) | Build | no deck-relative highlight in `src/library/tabledelegates/keydelegate.cpp` | rb6 feature; the range setting is documented only in rb7. | B-026 |

## Related Tracks, Track Suggestion, Matching

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LIB-060 | Related Tracks conditions: create, edit, delete, folders | rb6 | Y p.28-29 | Y p.31-32 | Y p.32-33 | Partial | ad-hoc menu only `src/widget/wsearchrelatedtracksmenu.cpp` | ROADMAP §6 P2 "Related Tracks criteria sets". | B-097, PAR-F7 |
| LIB-061 | Related Tracks criteria (BPM range, key incl. related 1-3, Matching, genre, rating, colour, My Tag, artist, year, recently added) | rb6 | Y p.28 | Y p.31 | Y p.32 | Partial | harmonic key, BPM, artist, album, genre, year, composer, grouping, directory | Missing: Matching, rating, colour, My Tag. | B-098 |
| LIB-062 | Related Tracks search target (folder / playlist), rank, source LIST / MASTER / deck 1-4 | rb6 | Y p.28, 30 | Y p.31, 33 | Diff p.32, 34 (+ A / B players) | Build | - | - | B-099, PAR-F15 |
| LIB-063 | RELATED TRACKS window / subpanel beside the list | rb6 | Y p.28-30, 191 | Y p.31-33, 210 | Y p.32-34, 239 | Partial | `src/widget/wsearchrelatedtracksmenu.cpp` | Place in the layout: PREP-022. | B-022, B-100 |
| LIB-064 | Matching: mark two tracks as a good pair; icon lights when a pair is loaded | rb6 | Y p.52, 92 | Y p.58, 100 | Y p.73, 116 | Build | - | - | B-101, PAR-F9 |
| LIB-065 | Track Suggestion: rb6 local preset lists (similarity, Era, Mood, Association) inside Related Tracks, ranked | rb6 | Diff p.8, 28 | Diff p.31 | Diff p.35-38 (own window, cloud Radar) | Build | - | DEC-§9: rb6 local presets. PARITY F.8 described only rb7's cloud kind (B conflict C10). rb7's own window is a later candidate. | B-102, B-024, PAR-F8 |
| LIB-066 | Radar Analysis (upload analysis for Collection / Streaming Radar) | Skip | N | N | Y p.35, 224 | Skip | - | Cloud and account service. | B-103 |

## Playlists, Tag List, Hot Cue Bank Lists, histories

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LIB-067 | Create / rename / delete playlist ("Untitled Playlist"; Delete key) | rb6 | Y p.31 | Y p.34 | Diff p.39 | Built | `src/library/trackset/playlistfeature.cpp` | - | B-105 |
| LIB-068 | Add tracks by drag; remove from playlist | rb6 | Y p.31-32 | Y p.34-35 | Diff p.39 | Built | - | - | B-106 |
| LIB-069 | Playlist folders (nested; drag playlists in; delete folder with contents) | rb6 | Y p.37 | Y p.41 | Y p.53 | Partial | `playlistfeature.cpp:69-79,566,642`; `src/library/dao/playlistdao.h:139-148`; `res/schema.xml:599-610`; test `src/test/playlistfolders_test.cpp` | Built 2026-10-26 (schema 41). Gaps (ROADMAP §6 P1): drag playlists onto folders, rename inside the tree, rekordbox-style icons redrawn. Tree look and labels are PREP. | B-107, PAR-F2 |
| LIB-070 | Sort Items (sort playlists of a folder); otherwise user order with drag to reorder | rb6 | Y p.192 | Y p.211 | Y p.241 | Build | Mix Two sorts alphabetically, folders first | - | B-108 |
| LIB-071 | Renumber Track Order (adopt current sort as playlist order) | rb6 | Y p.192 | Y p.211 | Y p.241 | Built | "Adopt current order" `playlistfeature.cpp:48` | - | B-109 |
| LIB-072 | Playlist artwork (Add Artwork) | rb6 | Y p.37 | Y p.41 | Y p.53 | Build | - | - | B-110 |
| LIB-073 | Export playlist to a file (txt, m3u8) | rb6 | Diff p.32 | Diff p.35 | Diff p.40 | Built | m3u, m3u8, pls, csv, txt `baseplaylistfeature.cpp:627` | Mix Two superset. | B-111, PAR-J8 |
| LIB-074 | Import playlist file (M3U, M3U8, PLS) | rb6 | Diff p.32, 189 | Diff p.35, 208 (+ PLS) | Y p.40, 237 | Built | `src/library/parserm3u.cpp`, `parserpls.cpp`, `parsercsv.cpp` | - | B-112 |
| LIB-075 | Duplicate prompt when adding to a playlist (add or skip) | rb6 | str | str | str | Build | Mixxx adds duplicates silently `src/library/dao/playlistdao.cpp` | - | B-115 |
| LIB-076 | "Playlists including this track" | rb6 | str | str | str | Partial | crates show membership; playlists do not | - | B-121, PAR-G7 |
| LIB-077 | Add to playlist / Tag List from the track menu | rb6 | - | Y (Ctx) | Y | Built | `src/widget/wtrackmenu.cpp` | Tag List target needs LIB-080. | PAR-G6 |
| LIB-078 | Intelligent Playlists (=, not =, >, <, contains, starts / ends with, in range, in the last; match all / any; max 128) | rb6 | Y p.40-42 | Y p.44-46 | Y p.56-58 | Build | no smart playlists in `src/library/trackset/` | PARITY Improve: nested AND / OR groups. | B-104, PAR-F3 |
| LIB-079 | Playlist Palette (Tag List + 3 playlist palettes; assign by drag; sort) | rb6 | Diff p.37-40 | Y p.41-44 | Diff p.54-56 (any sort) | Build | - | rb7's richer sort is a redesign; rb6 by default. | B-020, PAR-F16 |
| LIB-080 | Tag List: temporary list (up to 100), add / reorder, palette slot; remove on load / clear on exit options | rb6 | Y p.88-89, 187 | Y p.96-97, 205 | Y p.112-113, 233 | Partial | Auto DJ queue stands in `src/library/autodj/` | ROADMAP §6 P1: a real Tag List. The DDJ-800 VIEW hold mapping is CTL. | B-116, PAR-F5 |
| LIB-081 | Hot Cue Bank Lists (8 banks A-H from different tracks; folders, artwork) | rb6 | Y p.76-78 | Y p.84-86 | Y p.100-102 | Build | - | Also exported to devices (EXP-018). | B-117, PAR-F6 |
| LIB-082 | Histories ("HISTORY yyyy-mm-dd"; only tracks played 1 min or more; copy to playlist; share) | rb6 | Y p.20, 78 | Y p.19, 86 | Y p.19, 102 | Partial | `src/library/trackset/setlogfeature.cpp`; duplicate-distance pref `dlgpreflibrarydlg.ui:475` | No 1-minute rule. Device history import is IMP-015. | B-059, PAR-F4, PAR-L13 |

## Track menu items

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LIB-083 | Reset DJ Play Count | rb6 | - | Y (Ctx) | Y | Partial | - | - | PAR-G8 |
| LIB-084 | Show / Hide information; Show in Explorer | rb6 | Y p.191 | Y p.211 | Y p.240 | Built | Properties; Open in File Browser `src/widget/wtrackmenu.cpp:397` | - | B-240, PAR-G10 |
| LIB-085 | Property (track information) | rb6 | - | Y (Ctx) | Y | Built | `src/library/dlgtrackinfo.ui` | Docked window is PREP-023. | PAR-G12 |
| LIB-086 | Set cue / hot cue A-D / memory loop from the list | rb6 | - | Y (Ctx) | Y | Build | - | ROADMAP §6 P3. | PAR-G15 |
| LIB-087 | Export Loop As WAV (48 kHz / 16-bit; added to Collection) | rb6 | Y p.74, 188 | Y p.81, 206 | Diff p.97, 234 | Build | - | The new file goes into the managed library folder. | B-184, PAR-G16 |
| LIB-088 | Sort hot cues by time | rb7-add | N | N | str (7.2.19) | Built | `src/widget/wtrackmenu.cpp:498` | Inventory C said Build; the menu entry exists. | B-130, C-106 |
| LIB-089 | Pop-up menu on 1 s long press of a track | rb6 | Y p.186 | Y p.205 | Y p.233 | Partial | right-click only | PARITY F.29 said Have (B conflict C4). Low value. | B-030, PAR-F29 |
| LIB-090 | Display speed (cache the iTunes track list) | Skip | Y p.186 | Y p.205 | Y p.233 | Skip | - | Implementation detail. | B-031, PAR-F28 |

## Account and cloud services (skipped)

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| LIB-091 | All Audio / All Videos split, Photo list, video lists and slideshow | Skip | N | Diff p.13, 64 | Diff p.13 | Skip | - | Video extension not in scope. | B-039, PAR-G21, PAR-N8 |
| LIB-092 | Streaming sources (SoundCloud, Beatport, Beatsource, TIDAL, Apple Music, Spotify) | Skip | N | Y p.193 | Y p.80, 221 | Skip | - | Licensing and accounts. | B-055, PAR-N5 |
| LIB-093 | Cloud Library Sync, Collection Auto Upload, cloud data transfer | Skip | N | Y p.47, 206 | Y p.59, 217-218 | Skip | - | Account service. A local backup to a Dropbox / Google Drive folder is LIB-017. | B-066, PAR-J10, PAR-N3, PAR-N13 |
| LIB-094 | Collaborative Playlist | Skip | N | N | Y p.46-49, 241 | Skip | - | Account service (rb7). | B-119 |
| LIB-095 | Cloud Auto Upload per playlist | Skip | N | Diff p.211 | Y p.240-241 | Skip | - | Cloud service. | B-120 |
| LIB-096 | KUVO (profile, publish tracks, live playlist, make information public) | Skip | Y p.186, 188, 192 | Y p.191, 205, 206 | N | Skip | - | Service closed. | B-006, PAR-G20, PAR-L12, PAR-N6 |

## Open questions for the owner

1. **Crates vs My Tag (LIB-053).** Mixxx crates are flat tag-like sets; rekordbox has categorised My Tags and no crates. Options: (a) turn crates into one My Tag category ("Crates") and hide the crate UI; (b) keep both; (c) drop crates after migrating them. *Recommend (a)*: rb6 layout, no data loss.
2. **rb7.2 library-hygiene additions (LIB-028, LIB-029).** Duplicate manager with merge, playlist trash. Options: adopt both; adopt duplicates only; wait. *Recommend adopt both* after checking their behaviour in 7.2.19; both protect the DJ's data.
3. **Fixed defaults vs settings for library behaviours** (inventory B Q12, C15): played-track threshold (LIB-058), remove-from-Tag-List-on-load, Library Protection. *Recommend*: played threshold fixed at rekordbox 7's default (verify the value); the other two as settings, as rb6 has them.

<!-- End of AI-generated text. -->
