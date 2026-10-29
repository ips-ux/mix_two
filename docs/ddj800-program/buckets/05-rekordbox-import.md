<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 05: rekordbox import

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** High  ·  **Scope:** bringing a DJ's rekordbox world into Mix Two and back: rekordbox 6 / 7 `master.db` import, rekordbox XML both ways, finishing the USB / SD reader (phrases, waveforms, history, edits made on players), other-app imports (iTunes / Music.app), and repairing rekordbox's own library after Mix Two consolidates files (DEC-§6). File handling after import (import in place, consolidate, backup) is LIB; analysis precedence is ANA-001.

## rekordbox library import

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| IMP-001 | Import the rekordbox 6 / 7 library (`master.db`, SQLCipher): tracks, cues, grids, playlists, folders, history, My Tag, colours, ratings | Improve | - | Y p.7, 209 (rb6 converts rb5) | Y p.7, 238 | Build | ROADMAP U8; Mix Two reads only USB `export.pdb` (`src/library/rekordbox/rekordboxfeature.cpp:51-52`) | Migration path for rekordbox users (ROADMAP §6 P1). Needs schema and key handling (U8, like U4). Files stay where they are (LIB-003). PARITY J.5 "Partial" is true only via a USB export (B conflict C6). | B-065, PAR-J5, RM-U8 |
| IMP-002 | One-way library conversion (rb5 to rb6 style) | Improve | - | Y (Juce "Converting your library") | Y | Skip | - | Mix Two keeps a readable database and can always export back (XML, device export). No conversion step. | PAR-J6 |
| IMP-003 | Import analysis with rekordbox's meaning (grid by stored tempo, grid from .DAT, cues from .EXT) | rb6 | - | Y | Y | Built | FUNCTIONS.md "Behaviour rules"; `src/library/rekordbox/rekordboxbeats.cpp` | Common rule for all import paths (master.db, XML, USB). Never re-analysed automatically (ANA-001). | DEC-§5 |
| IMP-004 | rekordbox repair after Consolidate: offer to update rekordbox's `master.db` with the new paths (rekordbox closed, automatic backup first); else show a short Auto Relocate guide | Improve | - | - | - | Build | DEC-§6 | Only when the track is also in rekordbox. Needs the IMP-001 writer side; file names are kept, only folders change (LIB-014). | DEC-§6 |

## rekordbox XML

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| IMP-005 | rekordbox xml library: set path, browse, drag tracks / playlists into Collection, refresh | rb6 | Y p.20, 33 | Y p.19, 36 | Y p.19, 45 | Build | no XML reader in `src/` | Documented and stable in all three versions; the clean interop path. | B-058, PAR-F24 |
| IMP-006 | Import a rekordbox xml playlist | rb6 | Y p.33 | Y p.36 | Y p.45 | Build | see IMP-005 | - | B-114 |
| IMP-007 | Advanced > Database: rekordbox xml Imported Library path | rb6 | Y p.186 | Y p.204 | Y p.232 | Build | see IMP-005 | Preferences entry. | B-237 |
| IMP-008 | Export Collection in xml (+ "Export BeatGrid information" option) | rb6 | Y p.186, 190 | Y p.204, 209 | Diff p.232, 238 | Build | - | ROADMAP §6 P2 "rekordbox XML import and export". rb7 difference concerns cloud tracks only. | B-075, PAR-J7 |
| IMP-009 | XML auto-export to Dropbox on exit | rb7-add | N | N | str (7.2.19) | Skip | - | Cloud service. A local auto-export on exit could be a later Improve. | B-076 |

## USB / SD reader (Devices)

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| IMP-010 | Devices tree item: read rekordbox USB / SD exports (`export.pdb`, ANLZ) | rb6 | Y p.57 | Y p.64 | Y p.80 | Partial | `src/library/rekordbox/rekordboxfeature.cpp:51-52,194`; Kaitai `rekordbox_pdb.ksy`, `rekordbox_anlz.ksy` | ROADMAP §6 P1: finish the reader. The Kaitai spec covers PCO2, PWV5, PWV7, PSSI, but the importer uses only grid and cues (B conflict C2). Device Library Plus reading is not there yet. | B-041, PAR-F25, RM-F11 |
| IMP-011 | Reader: beat grids keep rekordbox's BPM (one tempo segment per run of beats); .DAT-only tracks keep their grid | rb6 | - | - | - | Built | `src/library/rekordbox/rekordboxbeats.cpp`; tests `src/test/rekordboxbeats_test.cpp`, `rekordboxanlz_test.cpp` | Fixed 2026-10-27; unit-tested. | B-167, PAR-I11 |
| IMP-012 | Reader: memory cues stay memory cues, hot cues stay hot cues | rb6 | Y | Y | Y | Built | AGENT_changelog "Engine: memory cues"; `src/test/cuecontrol_test.cpp` | Unit-tested 2026-10-26. | DEC-§5 |
| IMP-013 | Reader: phrases (PSSI) | rb6 | Y | Y | Y | Build | `lib/rekordbox-metadata/rekordbox_anlz.ksy:85`; importer ignores it | Cheapest first step to phrase data (ANA-035). | RM-F11 |
| IMP-014 | Reader: waveforms (PWAV, PWV3, PWV5, PWV7) | rb6 | Y | Y | Y | Build | `rekordbox_anlz.ksy:73-85`; importer ignores them | Show rekordbox's own waveforms for imported tracks instead of re-analysing (ANA-001). | RM-F11 |
| IMP-015 | Import play history from a device (automatic pref, manual "Import History", own tracks only; rb7.2 keep-on-device option) | rb7-add | Y p.83, 181 | Y p.91, 199 | Y p.107, 226 (+ 7.2 str) | Build | HISTORY pages listed but unused `rekordboxfeature.cpp:521-534` | rb6 removes the history from the device; that writes to the stick. DEC-§5 says sticks are never written to when a DJ only plays from them, so take rb7.2's keep-on-device as the default. | B-217 |
| IMP-016 | Update Collection from device (cues, loops, hot cues, grids, colours, ratings, comments changed on a player) | rb6 | Y p.36, 84 | Y p.40, 92 | Y p.52, 108 | Build | importer reads cues / grids for playback only `rekordboxfeature.cpp:949,981,1041` | Explicit user action only; it must not overwrite rekordbox analysis without asking (ANA-001). | B-216 |
| IMP-017 | Import playlists / folders from a device exported on another computer | rb6 | Y p.79 | Y p.87 | Y p.103 | Built | Devices view + "Import as Playlist / Crate" `src/library/baseexternallibraryfeature.cpp` | - | B-215 |
| IMP-018 | Never write to a USB stick that is only played from | Improve | - | - | - | Built | reader opens the stick read-only (`rekordboxfeature.cpp`) | DEC-§5. Keep it so when the writer (EXP-001) lands: only an explicit export writes. Unverified: confirm no Mixxx cache or analysis file is written to the stick. | DEC-§5 |

## Other-app imports

| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| IMP-019 | iTunes / Music.app library (shared XML, refresh, tick for tracks in Collection; Music.app exports XML) | rb6 | Y p.18-19 | Diff p.15-16 (+ Apple Music XML) | Diff p.15-16 | Built | `src/library/itunes/itunesxmlimporter.cpp`, `itunesmacosimporter.mm`; test `src/test/itunesxmlimportertest.cpp` | - | B-049, B-052, PAR-F23 |
| IMP-020 | "Get info from iTunes" (refresh; overwrites edits) | rb6 | Y p.19 | Y p.16 | Y p.16 | Build | - | Asks before overwriting edits (tag undo log, LIB-037). | B-050 |
| IMP-021 | Convert iTunes Grouping to rekordbox Label on import | rb6 | Y p.186 | Y p.204 | Y p.232 | Build | no Label column `src/library/columncache.h` | Needs the Label field (LIB-031). | B-051 |
| IMP-022 | SYNC MANAGER for iTunes: keep chosen iTunes playlists in sync (auto at start, "All Playlist") | rb6 | Y p.33-34 | Y p.37-38 | Y p.49-50 (+ 7.2 str: overwrite track info) | Build | - | Same window as the device Sync Manager (EXP-023). | B-053 |
| IMP-023 | Import an iTunes playlist (tracks join Collection) | rb6 | Y p.32 | Y p.36 | Y p.45 | Built | "Import as Playlist" `src/library/baseexternallibraryfeature.cpp` | - | B-113 |
| IMP-024 | Import To Collection (Track menu) for tracks in iTunes / xml / Explorer | rb6 | Y p.191 | Y p.210 | Y p.240 | Partial | "Import as Playlist / Crate" `src/library/baseexternallibraryfeature.cpp` | Tracks from outside the library folder get the move-in question (LIB-009). | B-060 |
| IMP-025 | Advanced > Database: iTunes Library File path | rb6 | Y p.186 | Y p.204 | Y p.232 | Built | iTunes feature chooses the XML | - | B-236 |
| IMP-026 | Inflyte promo locker; Inflyte folder setting | Skip | N | Y p.17, 206 | Y p.17, 234-235 | Skip | - | Account service. | B-054, B-238 |

## Open questions for the owner

1. **`master.db` key handling.** The key and schema are documented by community projects (e.g. pyrekordbox). Options: (a) implement from those public notes; (b) ask the DJ to export XML instead and skip `master.db`; (c) reverse-engineer it ourselves. *Recommend (a)*, written clean-room from the public description, with XML (IMP-005) as the fallback.
2. **Order of the two import paths.** Options: `master.db` first (ROADMAP §6 P1); XML first (documented, stable, also gives export back). *Recommend `master.db` first*: it is the owner's own migration and carries history and My Tag, which XML lacks.
3. **rekordbox 5 libraries.** rb6 / rb7 convert rb5 libraries one way. Options: support rb5 directly; ask rb5 users to export XML. *Recommend XML only* (IMP-005); rb5 is used only by deliberate pick (DEC-§1).

<!-- End of AI-generated text. -->
