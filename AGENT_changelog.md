<!-- AI-generated (Claude Code, 2026-10-27). Review before relying on it. -->

# AGENT_changelog: Mix Two

> **AI-generated:** Claude Code wrote this file from the git history and the
> program docs in `docs/ddj800-program/`. It is the change record for AI
> agents picking up work in this fork. The end-user version, in ASD-STE100
> Simplified Technical English, is [USER_changelog.md](USER_changelog.md).

## How to use and maintain this file

- **Read first** on a new task: [Working agreements](#working-agreements),
  then the newest entry. Per-feature status lives in
  [PARITY.md](docs/ddj800-program/PARITY.md); the work queue is
  [ROADMAP.md §6](docs/ddj800-program/ROADMAP.md).
- **Add an entry** for every change you commit: newest first, with the date,
  commit, and for each item the surface (controls, config keys, UI strings),
  files, tests, and a **Status** of `verified on hardware`, `unit-tested`,
  `compiled` or `docs only`. Record what you did not verify.
- Mirror user-visible changes in USER_changelog.md (STE rules are in its
  header). Internal-only changes (tests, refactors, tooling) go here only.
- Base: upstream Mixxx `main` at `ee17a27266` (2026-10-25). Diff against it
  with `git diff ee17a27266 HEAD`.

## Working agreements

- **Git:** `origin` = `github.com/ips-ux/mix_two` (fork), `upstream` =
  `github.com/mixxxdj/mixxx`. Agents commit and push to `origin/main` as part
  of a task ([AGENTS.md](AGENTS.md)). Never open PRs or issues upstream.
  AI-written commit bodies, comments and docs carry a start and end
  disclaimer.
- **Clean room:** never copy decompiled Pioneer code, tables, binaries,
  models or images. Write a behavioural spec, then implement it. Do not touch
  `clatc.dll` (DRM) or deck firmware. RE output stays in `build/re/`.
- **Engine owns behaviour; mappings stay thin.** New behaviour is a control
  (`[Group],key`) used by both the skin and the controller script. The DDJ-800
  script sets LEDs from the engine's answer, not from the button press.
- **The owner's spec is the default.** No opt-in setting for specified
  behaviour; settings only where Pioneer offers a real choice or several
  algorithms coexist ([FUNCTIONS.md](docs/ddj800-program/FUNCTIONS.md)).
- **Engine thread:** no allocations or locks in audio callbacks.
- **Build and test (Windows):** `tools\mixtwo_windows.bat configure | build
  [target] | test [gtest args] | run [--nobuild]`. `run` uses a separate dev
  profile (`MIXTWO_PROFILE`, default `build\mixtwo-profile`). `mixxx-test`
  sets `MIXXX_DISABLE_PORTAUDIO` because Pioneer's ASIO driver crashes after
  repeated load/unload (ROADMAP F16); keep it set for any test binary.
- **Not verified in this changelog's first pass:** the build and the test
  suite were not re-run when the history was committed. Status below comes
  from the program docs written when each item was built.

---

## 2026-10-27: changelogs

- Added this file and [USER_changelog.md](USER_changelog.md). **Status:** docs
  only.

## 2026-10-27: `e78f87dfa8` roadmap handoff

- [ROADMAP.md](docs/ddj800-program/ROADMAP.md) §6: remaining work by
  priority (P1/P2/P3, hardware and driver, design, check-in-rekordbox list,
  waiting on owner). F13 and §5 updated for the remotes and commit policy.
- [SCRATCH-ENGINE.md](docs/ddj800-program/SCRATCH-ENGINE.md): removed the
  stale "motion rules still to build" line.
- **Status:** docs only.

## 2026-10-26 – 2026-10-27: `59ace09228` first fork commit

Everything below landed in one commit on top of upstream. 136 files,
+9565 / −875. Grouped by area.

### Git and agent policy
- Repo connected to `origin`/`upstream`; local `main` tracks `origin/main`.
  `core.filemode=false`, `core.symlinks=false`, `core.autocrlf=false`
  (Windows checkout: ignore executable-bit diffs; the two symlinked SVGs in
  `res/skins/LateNight/palemoon/buttons/` are plain files locally).
- [AGENTS.md](AGENTS.md): new section "Commits and Pushes in This Fork";
  PR ban kept, scoped to upstream.

### Controllers: DDJ-800 mapping
- **Files:** `res/controllers/Pioneer-DDJ-800.midi.xml`,
  `res/controllers/Pioneer-DDJ-800-script.js` (built from rekordbox 6.8.5
  `DDJ-800.midi.csv`; DDJ-400/FLX4 as structural template).
- Every CSV input bound via `midi.makeInputHandler`. Intentional gaps: mic
  knobs (hardware), controller-settings values, Related Tracks, FX tap.
- Behaviour fixed after the first hardware test: SYNC press toggles;
  SHIFT+SYNC = MASTER; BROWSE press never loads (opens the load prompt);
  MASTER LEVEL `B608` / BOOTH LEVEL `B609` drive main/booth gain; MASTER CUE
  `96 63` uses the engine control `[Master],main_cue`; MEMORY / DELETE / CALL
  use memory cues; KEY SYNC LED follows the engine.
- **Mapping options:** `vinylModeDefault` (true), `jogResolution` (720,
  100–4096), `inputSelectPassthrough` (false), `masterLevelSoftware` (true),
  `cfx1Preset`–`cfx4Preset` (−1 = default; DUB ECHO, SWEEP, NOISE, FILTER).
- **Not done:** jog display / on-jog indicator, HID (MI_03), DDJ-800
  preference page; need runtime capture (ROADMAP WS-A, U2, U3).
- **Status:** verified on hardware for most controls (2026-10-26 test);
  later fixes unit-tested / mock-harness only.

### Controllers: plug-and-play, hotplug, open retry
- **Mapping device match:** `<product protocol="midi" device_name="…"/>` or
  `device_name_regex` in `res/schema.xml`; parsed in
  `legacycontrollermappingfilehandler.cpp` / `controllermappinginfo.*`;
  port names normalised by `MidiUtils::canonicalDeviceName`;
  `MidiController::matchMapping`.
- **Auto-enable:** `ControllerManager` enables a never-configured controller
  with the first matching mapping (user mappings before system ones).
  Config `[Controller],auto_enable_matching_mappings` (default on,
  `kAutoEnableMatchingMappingsCfgKey`).
- **MIDI hotplug:** `src/controllers/midi/midiportwatcher.*` polls the OS
  port list once a second (WinMM / CoreMIDI / `/proc/asound/seq/clients`); a
  change held for two polls restarts PortMidi and replaces all PortMidi
  controllers. Config `[Controller],midi_hotplug_enabled` (default on).
  Replaced controllers are deleted only after prefs pages and QML proxies are
  rebuilt; `slotApplyMapping` / `openController` reject stale pointers.
- **Open retry:** a controller that fails to open is retried for 30 s (F15).
- **Trade-offs:** any MIDI change reopens all MIDI controllers (scripts
  re-init); HID/bulk not hotplugged; rescans pause during MIDI learn; the
  DDJ-800 audio interface is still selected by hand.
- **Tests:** `src/test/midiproductmatch_test.cpp`,
  `src/test/midiportwatcher_test.cpp`.
- **Status:** verified on hardware.

### Startup: rekordbox guard
- `src/coreservices.cpp`: `isRekordboxRunning()` (Toolhelp snapshot for
  `rekordbox.exe`, Windows only) and `CoreServices::askToCloseRekordbox()`
  before audio drivers load: Retry / Start Anyway / Quit. Skipped when
  `MIXXX_DISABLE_PORTAUDIO` is set.
- `src/soundio/portaudioenumerator.cpp`: honours `MIXXX_DISABLE_PORTAUDIO`.
- `src/sources/soundsourceproxy.cpp`: repeated provider registration is a
  no-op (second `CoreServices` in one process).
- **Status:** compiled; reason in ROADMAP F16.

### Engine: sync (Pioneer model)
- Controls/config: `[Master],sync_type` (Grid Lock = BEAT SYNC, default /
  BPM Sync), `[Master],sync_grid` (1/8, 1/4, 1/2, Whole), per-deck
  `sync_indicator`, `sync_leader_toggle`. UI: Preferences › Decks › Deck
  options › **Sync options** (`dlgprefdeck*`). Mixxx's old "Sync mode
  (Dynamic tempo tracks)" renamed **Dynamic tempo handling**.
- Phase lock no longer depends on quantize.
- MASTER is **explicit** (reverts upstream's soft-leader downgrade, Mixxx
  issue #11788), so a stopped deck can be MASTER; pressing MASTER on the
  master returns to automatic choice (EngineSync re-pick bug fixed). Sticky:
  a playing master keeps MASTER until it stops; a zero fader does not move it.
- Only the master's tempo slider sets tempo; followers ignore theirs.
- Half/double picked from the joining track's **own BPM**, always on (owner
  decision; rekordbox has a checkbox). Threshold is Mixxx's √2 (verify).
- Known differences: the MASTER deck also has sync on; on-screen buttons
  outside the deck hub still use Mixxx tap/hold.
- **Files:** `src/engine/sync/*`, `src/engine/controls/bpmcontrol.*`.
- **Tests:** `src/test/enginesynctest.cpp` (updated).
- **Status:** unit-tested; partly hardware-tested.

### Engine: tempo
- `[ChannelN],rate_range_next` steps ±6 / ±10 / ±16 / WIDE (±100 %); while
  synced the range widens to fit the master (never narrows; verify).
- Soft takeover on the DDJ tempo slider; `tempo_slider_bpm`,
  `tempo_slider_matched`, `rate_physical`.
- `src/widget/wraterange.*`: clickable range in the deck hub.
- **Files:** `src/engine/controls/ratecontrol.*`.
- **Status:** unit-tested.

### Engine: key sync
- `[ChannelN],keysync_enabled`: on/off mode, follows the tempo master's key
  and its key changes; fallback order MASTER → sync target → any loaded deck
  with an analysed key (playing first). Manual key change or KEY RESET turns
  it off; stays off without an analysed key; the synced key is kept on off.
- **Files:** `src/engine/controls/keycontrol.*`.
- **Tests:** `src/test/keysync_test.cpp`.
- **Status:** unit-tested. Verify vs rekordbox: same vs compatible key,
  restore on off, which deck leads.

### Engine: memory cues
- `CueType::MemoryCue` (point or loop, no count limit) in
  `src/track/cueinfo.*`; deck controls `memory_cue_set` / `_next` / `_prev` /
  `_delete` / `_count`; first `kMaxNumberOfMemoryCueMarks` (64, in
  `src/util/defs.h`) published as `memory_cue_N_position` for red "M" marks
  (`src/waveform/renderers/waveformmarkset.*`).
- rekordbox USB imports keep memory cues as memory cues.
- **Files:** `src/engine/controls/cuecontrol.*`.
- **Tests:** `src/test/cuecontrol_test.cpp`.
- **Status:** unit-tested. Open: Memory Cue deck panel, comments on deck,
  CALL while playing (verify).

### Engine: scratch (rekordbox model)
- Rendering: `src/engine/bufferscalers/enginebufferscalehalfbandlagrange.*`,
  2× half-band (Hann-47 windowed sinc, 43 taps, generated from the formula)
  + 4-point cubic Lagrange. Renders all playback with key lock off.
- Motion: `RateControl::calculatePlatterRate`, controls
  `[ChannelN],platter_touch` / `platter_turn`; position-locked, 60 ms touch
  dead zone, 25× glitch filter, ±8× limit with carry-over, fixed 33⅓
  (1.8 s per turn). DDJ-800 jogs use it.
- Config `[App],scratch_algorithm`; UI Preferences › Decks › **Platter
  options › Scratch algorithm**: "Rekordbox (6.8.5)" (default) / "Mixxx
  (2.7)" (linear).
- Spec: [SCRATCH-ENGINE.md](docs/ddj800-program/SCRATCH-ENGINE.md).
- **Tests:** `src/test/enginebufferscalehalfbandlagrange_test.cpp`,
  `src/test/platter_test.cpp`.
- **Status:** unit-tested. Not done: spin/backspin on release, bend rule,
  render comparison, hardware check of 1.8 s/turn (`jogResolution`).

### Mixer
- `[Master],main_cue`: main mix joins the cue bus (`src/engine/enginemixer.*`).
- Skin: BOOTH knob replaces Mixxx's balance knob (owner decision).
- **Tests:** `src/test/enginemixertest.cpp`.
- **Status:** unit-tested; hardware button mapped.

### Library: playlist folders
- Schema **40 → 41** (`src/database/mixxxdb.cpp`): `PlaylistFolders` table
  and `Playlists.folder_id`. One-way upgrade of the user's database.
- `src/library/dao/playlistdao.*`, `src/library/trackset/playlistfeature.*`,
  `baseplaylistfeature.*`, `src/widget/wlibrarysidebar.*`,
  `src/widget/wtrackmenu.cpp`: Create New Folder / Rename Folder / Delete
  Folder, Move to Folder submenu, defaults "Untitled Folder" / "Untitled
  Playlist", new/duplicated playlists go into the right-clicked folder,
  folders stay expanded, Add to Playlist shows folders as submenus.
- **Tests:** `src/test/playlistfolders_test.cpp`.
- **Status:** unit-tested. UI gaps in ROADMAP §6 P1.

### Library: load prompt
- `src/library/loadprompt.*`, `src/widget/wloadprompt.*`,
  `src/library/librarycontrol.*`. Controls `[Library],load_prompt`,
  `load_prompt_push`, `load_prompt_select`, `load_prompt_deck`,
  `load_prompt_load`, `load_prompt_cancel`. BROWSE push opens "Load to the
  Deck"; pick with LOAD, click, or turn + push; 5 s timeout. No load-method
  preference (owner decision).
- **Tests:** `src/test/loadprompt_test.cpp`.
- **Status:** unit-tested. Look is provisional.

### Library: rekordbox import fixes
- `src/library/rekordbox/rekordboxbeats.*`: one tempo segment per run of
  beats with the same stored PQTZ tempo (was one per ms-rounded beat → BPM
  flicker, sync wobble). Beats within 1 ms of rekordbox's grid.
- `src/library/rekordbox/rekordboxfeature.cpp`, `rekordboxanlz.h`: tracks
  with only `.DAT` (no `.EXT`) keep the grid (a shared flag was split).
- Takes effect when the track is loaded again from the rekordbox view.
- **Tests:** `src/test/rekordboxbeats_test.cpp`,
  `src/test/rekordboxanlz_test.cpp`.
- **Status:** unit-tested.

### Skin: LateNight rework
- Decks mirrored from the centre out; deck hub `decks/deck_hub.xml` (large
  BPM, slider BPM, tempo % and range, platter, CUE/reverse, PLAY); rate
  column with 34 px domed SYNC / KEY SYNC / MT buttons
  (`decks/rate_controls.xml`, `decks/transport_buttons.xml`); MT lock button
  by the waveform removed; mixer split into `mixer/mixer_headphones.xml` and
  `mixer/mixer_main.xml` (old `mixer_main_headphone.xml` deleted); new
  `controls/knob_label_above.xml`; `btn__mt_light*.svg` for classic and
  palemoon; QSS in `style*.qss`; slots per deck in `skin.xml`.
- Font: `res/fonts/Silkscreen-Bold.ttf` (SIL OFL 1.1, licence alongside), 8 px.
- Tooltips: `src/skin/legacy/tooltips.cpp`.
- Design rules: [DESIGN.md](docs/ddj800-program/DESIGN.md).
- **Status:** compiled; built to the owner's direction, layout still
  provisional until the rekordbox-layout skin.

### Tooling and tests
- `tools/mixtwo_windows.bat`: Windows build/test/run helper.
- `tools/soundparity/`: `make_signals.py` (test-signal pack),
  `compare.py` (chirp alignment, null depth, third-octave diff, tail length);
  Python 3 + numpy + scipy. Plan: [SOUND.md](docs/ddj800-program/SOUND.md).
- Test harness: `src/test/main.cpp`, `mixxxtest.*`, `coreservicestest.cpp`,
  QML smoke tests adjusted for the above.
- `CMakeLists.txt`: new sources and tests registered.

### Docs
- `docs/ddj800-program/`: ROADMAP, PARITY (218 rows), SCRATCH-ENGINE,
  SOUND, DESIGN, FUNCTIONS.

<!-- End of AI-generated document. -->
