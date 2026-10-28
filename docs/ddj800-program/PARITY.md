<!-- AI-generated (Claude Code, 2026-10-26). Review before relying on it. -->

# rekordbox 6 Parity Matrix (WS-E)

> **AI-generated:** Claude wrote this matrix from the strings rekordbox 6.8.5
> ships (`locale/*.lang`, the DDJ-800 MIDI CSV) and from the Mix Two source
> tree. A string proves a feature or setting **exists** in rekordbox 6; it does
> not prove exactly how it behaves. Rows whose rekordbox behaviour matters in
> detail are flagged **Verify** and need checking in rekordbox 6 itself.

## How to read this

**Decision** (what Mix Two should do):
- **Match**: copy rekordbox 6's behaviour and defaults.
- **Improve**: deliberately better than rekordbox. The reason is in Notes.
- **Skip**: leave out. The reason is in Notes.

**Mix Two** (where Mix Two is today):
- **Have**: works the rekordbox way, or close enough.
- **Partial**: exists, but behaves or looks differently.
- **Map**: exists in Mix Two, but needs wiring, a setting or a mapping change.
- **Build**: does not exist yet.

**Priority**: **P1** = needed for performing with the DDJ-800 now. **P2** =
Prepare and library work. **P3** = later or nice to have.

**Evidence** names the `.lang` group the string comes from (`GUI`, `Menu`,
`Ctx` = context menu, `Prefs`, `Tip` = tooltip, `Juce` = newer rekordbox 6
dialogs, `Info` = track information) or the DDJ-800 CSV.

### Method
- `locale/german.lang` holds all 4,548 English UI strings of rekordbox 6.8.5
  as translation keys, grouped as Main GUI, Track Information, Context Menu,
  Property, General, Tooltips, Menu, Preferences, Messages, Start rekordbox and
  Juce. The extraction script is `extract_lang.py` (kept in the session
  scratchpad; it takes a `.lang` file and writes group/string pairs).
- Mix Two was checked by searching `src/`, the preference pages
  (`src/preferences/dialog/*.ui`), the library modules and the schema.

## Summary

| Area | Rows | Match | Improve | Skip | Mix Two Have | Partial / Map | Build | n/a |
|---|---|---|---|---|---|---|---|---|
| A. App structure and layout | 14 | 12 | 2 | 0 | 3 | 6 | 5 | 0 |
| B. Decks (Perform) | 32 | 27 | 5 | 0 | 18 | 6 | 8 | 0 |
| C. Mixer | 13 | 11 | 2 | 0 | 8 | 4 | 1 | 0 |
| D. Effects | 11 | 9 | 0 | 2 | 1 | 5 | 4 | 1 |
| E. Sampler | 11 | 8 | 0 | 3 | 2 | 4 | 2 | 3 |
| F. Browser and library | 29 | 25 | 3 | 1 | 11 | 8 | 9 | 1 |
| G. Track context menu | 22 | 17 | 1 | 4 | 8 | 6 | 4 | 4 |
| H. Track information | 9 | 8 | 1 | 0 | 7 | 2 | 0 | 0 |
| I. Analysis | 15 | 11 | 4 | 0 | 6 | 2 | 5 | 2 |
| J. Library management | 10 | 7 | 2 | 1 | 1 | 3 | 4 | 2 |
| K. Export and devices | 9 | 6 | 1 | 2 | 0 | 1 | 6 | 2 |
| L. Recording, Automix and sharing | 15 | 10 | 1 | 4 | 5 | 3 | 3 | 4 |
| M. Controller and MIDI | 13 | 9 | 4 | 0 | 5 | 6 | 2 | 0 |
| N. Account, cloud and extensions | 15 | 0 | 2 | 13 | 0 | 0 | 0 | 15 |
| **Total** | **218** | **160** | **28** | **30** | **75** | **56** | **53** | **34** |

Counts are rows in the tables below; some rows cover several related features.
"n/a" rows have nothing to build because they are skipped or not applicable.

## A. App structure and layout

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| EXPORT / PERFORMANCE / LIGHTING modes | GUI "Change EXPORT / PERFORMANCE / LIGHTING mode." | Partial | **Improve** | P1 | One app, no modes gated by licences. *Prepare* and *Perform* become two layouts you switch freely (one key or button), sharing one library. |
| 2 Deck Horizontal / Vertical, 4 Deck Horizontal / Vertical layouts | Menu | Partial | Match | P1 | LateNight has 2/4 decks but not the four rekordbox arrangements. Build a rekordbox-layout skin. Interim design pass 2026-10-27 (LateNight): decks mirrored from the centre out (mixer module, rate column, deck hub, track rows); a deck hub around the circular deck with the BPM large on top, the slider BPM, tempo % and a clickable tempo range. |
| Browse view (full browser) and BROWSE VIEW button | Menu "Full Browser"; CSV `967A` | Map | Match | P1 | `[Skin] show_maximized_library` exists; the new skin must make it a proper layout state. |
| Show / hide panels: FX, CFX, Sampler, Mixer, Rec | Tip "Show/Hide FX panel" … | Partial | Match | P1 | LateNight has some toggles; make the set and positions match. |
| Deck panel selector: Pad / Memory Cue / Grid / Info | Tip "Deck number: … select the Pad/Memory Cue/Grid panel" | Build | Match | P1 | Per-deck panel switch below the waveform. |
| Simple Player, 1 / 2 Players (Prepare) | Menu "Simple Player", "1 Player", "2 Players" | Build | Match | P2 | Prepare-layout players for cue and grid editing. |
| Split screen / sub browser window | Menu "Show Split Screen", "Sub Browser Window" | Build | Match | P2 | Second, independent browser window. |
| My Tag, Related Tracks, Information windows | Menu "My Tag Configuration Window" … | Build | Match | P2 | Floating tool windows. |
| Show / hide pad and platter (on-screen jog) | Menu "Show / Hide Pad and Platter" | Build | Match | P3 | |
| Full screen | Menu | Have | Match | — | |
| Font size, line space, artwork size in list | Prefs "Font size", "Line Space", "Artwork size on the track list" | Partial | Match | P2 | Mix Two has some interface scaling; add list density and artwork size. |
| Language selection (18 languages) | Prefs "Language" | Have | Match | — | Mix Two has its own translations. |
| Tooltips on/off | Prefs "Show Tooltips" | Have | Match | — | |
| Waveform colour schemes (Blue / RGB / 3-Band), half / full waveform | Prefs "Waveform color", "Full Waveform", "Half Waveform" | Partial | **Improve** | P2 | Mix Two has RGB, filtered and other renderers. Add rekordbox's Blue and 3-Band looks; keep Mix Two's extra renderers. |

## B. Decks (Perform)

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| Play / Pause, Cue (CDJ cue behaviour) | Tip "Set a Cue point if you click during pause…" | Have | Match | P1 | Mixxx "CDJ" cue mode. Make it the default. |
| SYNC press toggles; SHIFT+SYNC = master | CSV `9058`, `905C` | Have | Match | P1 | SYNC fixed 2026-10-26. MASTER 2026-10-27: the leader button now requests an **explicit** leader (upstream Mixxx downgraded it to a soft leader, issue #11788), so a stopped deck can be MASTER; pressing MASTER on the master hands it back to automatic choice (fixed an EngineSync bug that kept re-picking the same deck). Sticky MASTER: a playing master keeps MASTER until it stops; a fader at zero no longer moves it, and the old master does not take it back when it plays again. Only the MASTER's tempo slider sets the tempo; a follower's slider is ignored. **Differences / Verify:** in Mix Two the MASTER deck also has sync on; on-screen buttons still use Mixxx's tap/hold. |
| Sync type BEAT SYNC / BPM SYNC | Prefs "Sync Type", "BEAT SYNC", "BPM SYNC" | Have | **Improve** | P1 | Built 2026-10-27: Preferences > Decks > Deck options > **Sync options**: Sync mode **Grid Lock** (BEAT SYNC; default) or **BPM Sync**, plus **Beat grid** 1/8, 1/4, 1/2, Whole for Grid Lock (Improve: rekordbox only snaps to whole beats). Independent of quantize (Mixxx only phase-locked with quantize on). The old Mixxx "Sync mode (Dynamic tempo tracks)" is now "Dynamic tempo handling" in the same group. On screen: a round SYNC button labelled GRID LOCK or BPM SYNC, tech blue while synced, red-orange on the MASTER (playing or chosen); click toggles, right-click makes MASTER; the crown is gone. KEY SYNC is a matching round button under it. |
| Sync with double / half BPM | Prefs "Allow BEAT/BPM SYNC with double/half BPM." | Have | **Improve** | — | Mixxx handles half/double. 2026-10-27: a deck turning sync on picks half / double from the **track's own BPM**, not from where its tempo slider has it (a 128 BPM track slowed to 60 syncs to a 128 master at 128, not 64); decks already in sync keep their relation. Owner decision 2026-10-27: always on, no setting (rekordbox has a checkbox). **Verify in rekordbox** the threshold (Mix Two uses Mixxx's √2). |
| Automatic BEAT/BPM SYNC setting | Juce "Automatic BEAT/BPM SYNC Setting" | Build | Match | P2 | **Verify** what it automates. |
| Scratch sound (how the track is rendered under the platter) | rekordbox `TimeStretchScratch` (F17) | Partial | **Improve** | P1 | Built 2026-10-27: Preferences > Decks > **Platter options > Scratch algorithm** with "Mixxx (2.7)" (linear interpolation) and "Rekordbox (6.8.5)" (2x half-band + cubic Lagrange, `EngineBufferScaleHalfbandLagrange`, generated from the formula; default). Improve: rekordbox has one algorithm, Mix Two lets the DJ choose and leaves room for more. It renders all playback with key lock off, as in rekordbox. Platter motion built 2026-10-27 for "Rekordbox": position-locked scratching (`[ChannelN],platter_touch` / `platter_turn`), 60 ms touch dead zone, 25x glitch filter, ±8x limit with carry-over, fixed 33⅓ (1.8 s per turn); the DDJ-800 jogs use it. Built 2026-10-27: release, spin / backspin, disable and bend as rekordbox does them (`[ChannelN],platter_state`, SCRATCH-ENGINE.md section 2.1): letting go of a moving platter spins on; a forward spin hands back to play once the platter is slower than the deck; the coasting platter is ignored until it stops or is pushed back 0.375 rev; the untouched platter bends 3 % per 1x platter speed, up to +42 %. Render harness with a real track: `tools/soundparity/render_scratch.py`. **Still to do:** render rekordbox with the same movement and compare; **verify on hardware** that one platter turn moves 1.8 s (mapping setting `jogResolution`) and how spins feel. |
| Master Tempo (key lock) | CSV `901A` | Have | Match | P1 | On screen: round MT button with a centre light above the tempo slider (2026-10-27); the lock button next to the waveform is gone. |
| Tempo range ±6 / ±10 / ±16 / WIDE | Prefs "Tempo range", "Wide" | Have | Match | P1 | `[ChannelN],rate_range_next` steps ±6, ±10, ±16 %, WIDE (±100 %), from the DDJ button or the range shown in the deck hub. While synced, the range widens to the smallest one that holds the master's tempo. **Verify in rekordbox:** whether it ever narrows back on its own (Mix Two only widens). |
| Tempo slider catch-up and slider BPM | rekordbox deck display | Have | Match | P1 | Built 2026-10-27: soft takeover on the DDJ tempo slider, so it has to reach the playing tempo before it takes over. The deck hub shows the slider's BPM under the BPM (`tempo_slider_bpm`): red-orange while it differs, white when it matches (`tempo_slider_matched`). |
| Tempo reset; double-click % to reset | Tip "Double click here … to reset the playing speed" | Partial | Match | P2 | |
| Enter BPM directly in deck | Tip "Click here to enter a BPM value" | Build | Match | P2 | |
| Key sync, semitone up / down, key reset | CSV `9065`, `9064`; Juce "Semitone Up"; Msg "Enable/Disable Key Sync." | Have | Match | P1 | KEY SYNC built 2026-10-27 as an on/off mode (`[ChannelN],keysync_enabled`, LED on the DDJ-800): follows the tempo master's key and its key changes; a manual key change or KEY RESET turns it off; stays off without an analysed key. rekordbox has a matching `KeySyncManager` (master / slaves). **Verify in rekordbox:** same key vs harmonically compatible key (Mix Two uses Mixxx's nearest compatible key); whether turning it off restores the original key (Mix Two keeps the synced key); which deck becomes key master. On screen: a round KEY SYNC button under SYNC (right-click: key reset). 2026-10-27: works with decks that are only loaded (falls back to any other deck with a key), and the DDJ LED follows the engine's answer (it stuck on when the engine refused). |
| Quantize on / off | Tip "Quantize ON/OFF" | Have | Match | P1 | |
| Quantize beat value (1/8, 1/4, 1/2, 1 beat) | Prefs "Quantize Beat Value" | Build | Match | P2 | Mixxx quantizes to whole beats only. |
| Jump before reaching the next beat | Prefs "Jump before reaching the next beat." | Partial | Match | P2 | |
| Hot cues A–H, 2 banks / 16 on DDJ-800 | Tip "Set/Play Hot Cue Bank A" …; Juce "Hot Cue Loop A … P" | Have | **Improve** | P1 | Mix Two has 36 hot cues per track; keep rekordbox's A–H display and colours, allow more. |
| Hot cue loops (saved loops on pads) | Juce "Hot Cue Loop A Active On/Off" | Have | Match | P1 | Mix Two `CueType::Loop`. |
| GATE playback of hot cues while paused | Prefs "During Pause, GATE playback is applied." | Have | Match | P1 | Mixxx previews a hot cue while held when stopped. |
| Hot cue colours and comments | Tip "Add comments to the Hot Cue" | Partial | Match | P2 | Colours yes; per-cue comments shown on the deck: build. |
| Memory cues and memory loops (call prev / next, delete) | Tip "Call Memory Cue(Loop)"; Prefs "Memory Cue 1 … 10" | Have | **Improve** | P1 | Built 2026-10-26: `CueType::MemoryCue` (point or loop, no count limit); deck controls `memory_cue_set` / `_next` / `_prev` / `_delete` / `_count`; red "M" marks on every waveform and overview that shows hotcues; rekordbox USB imports keep memory cues as memory cues; the DDJ-800 MEMORY, DELETE and CALL buttons use them. Still to do: the Memory Cue deck panel (A), memory cue comments on the deck. **Verify** CALL while playing. |
| Set Memory Cue at first beat when analysing | Prefs "Set Memory Cue at the first beat of track when analyzing" | Build | Match | P2 | |
| Beat loop 1/64 … 512 beats; loop in / out / reloop; halve / double | Tip "1/64 Beat Loop" … "512 Beat Loop" | Have | Match | P1 | |
| Active loop | Ctx "Active Loop Playback"; Tip "Set Active Loop" | Build | Match | P2 | A memory loop that auto-engages on load. **Verify.** |
| Slip mode | Tip "Toggle Slip On/Off." | Have | Match | P1 | |
| Beat jump with selectable length | Tip "Select the beat/bar length jumping…" | Have | Match | P1 | |
| Needle search on waveform / click to jump | Tip "Click the waveform and the current playback position moves" | Have | Match | — | |
| Time elapsed / remaining toggle | Tip "Switch Time/Remain" | Have | Match | — | |
| Beat count display (current bar, count to next memory cue) | Ctx "Beat Count Display", "Count to the next MEMORY CUE (Bars)" | Build | Match | P2 | Needs memory cues and phrase data. |
| Hot cue countdown on the platter display | Tip "Hot cue Countdown shows the distance to the next Hot Cue" | Build | Match | P3 | |
| Load lock / Eject-load lock / Memory cue call lock | Prefs "Eject/Load Lock", "Memory Cue Call Lock" | Build | Match | P2 | Prevents loading onto a playing deck. |
| Load setting: start at the nearest memory / hot cue | Prefs "Playback starts at Memory/Hot Cue nearest to the beginning of the track" | Partial | Match | P1 | Mixxx has "cue recall" options; map to this. |
| Undo track load; locate loaded track in the browser | Juce "Undo track load", "Locate track loaded on deck" | Partial | Match | P2 | Mixxx re-loads the last ejected track on a second eject; add an explicit undo and "locate". |

## C. Mixer

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| Trim / Auto Gain switch per channel | Tip "Trim knob/Auto Gain Switch button" | Have | Match | P1 | Mixxx ReplayGain normalisation plus pregain. |
| 3-band EQ or ISO, kill switches | Prefs "EQ Type"; Tip "Kill Switch" | Have | Match | P1 | |
| Channel fader curve (exponential / linear / logarithmic) | Prefs "Channel Fader Curve" | Have | Match | — | |
| Crossfader curve and assign (A / THRU / B) | Prefs "CROSSFADER Curve"; Tip "CROSSFADER Assign button" | Have | Match | — | |
| Headphone cue, mixing, level, Master Cue | Tip "Headphones Mixing knob" | Have | Match | P1 | Master Cue handled in the DDJ-800 mapping. |
| Headroom (0 / −3 / −6 / −9 / −12 dB, default −9) and limiter | Prefs "Headroom" | Partial | Match | P2 | Mix Two has main gain but no headroom presets or a limiter on the main output. |
| Master volume, booth volume, audio dropout meter | Tip "Master Volume", "Audio Dropout Meter"; CSV `B608`, `B609` | Have | **Improve** | — | MASTER LEVEL and BOOTH LEVEL drive the main and booth gain over their full range (mapping setting, on by default). Owner decision 2026-10-27: BOOTH replaces Mixxx's balance knob ("BAL is a dumb feature"). Booth audio needs a Booth output in Sound Hardware. Layout 2026-10-27, as on the DDJ-800: a MASTER section right of the channels (LEVEL, CUE, BOOTH LEVEL stacked) and a HEADPHONES section left of them (MIXING with CUE / MASTER ends, LEVEL, SPLIT; space kept above for the four colour FX buttons). MASTER CUE is a real engine control now (`[Master],main_cue`: the main mix joins the cue bus), on screen and on the DDJ-800 button `96 63`, replacing the mapping's old trick of turning MIXING to MASTER. |
| Fader start (SHIFT + fader) | Prefs "Fader Start is enabled when … pressing the SHIFT button." | Map | Match | P1 | In the DDJ-800 mapping. |
| Mixer mode internal / external | Prefs "Mixer Mode", "Internal", "External" | Partial | Match | P2 | External = hardware mixer (DJM). Mixxx supports per-deck outputs; expose it as one switch. |
| Output routing per deck / sampler / preview | Prefs "Output Deck1" … "Output Preview" | Have | Match | — | Sound hardware page. |
| PC MASTER OUT (also play through computer speakers) | Tip "ON/OFF button for PC MASTER OUT" | Partial | Match | P3 | Needs a second output device. |
| Microphone: on/off, 3-band EQ, talk-over, MIC FX | Tip "Turns the microphone On/Off" … | Have | Match | P2 | MIC FX via an effect unit. |
| SMART FADER | Juce "Fader used with SMART FADER" | Build | **Improve** | P3 | Auto-BPM transition across a fade. Make it optional and transparent. |

## D. Effects

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| Beat FX single mode (one effect, 5 parameters) / multi mode (3 effects) | Tip "Beat FX single-mode", "Beat FX multi-mode" | Partial | Match | P1 | Mixxx effect units hold 3 effects; build the single/multi switch. |
| Pioneer-style Beat FX set (Echo, Delay, Reverb, Spiral, Trans, Flanger, Phaser, Pitch, Roll, Slip Roll, Vinyl Brake, Helix …) | CSV Beat FX rows; effect table in `rekordbox.exe` (34 Beat FX) | Build | Match | P1 | **Must sound like rekordbox** (owner): Mixxx's similarly named effects do not count. Measured against rekordbox and rebuilt clean-room; see [SOUND.md](SOUND.md) (WS-F). The DDJ-800's 14 come first. |
| FX beat selection and BPM Auto / Tap | Tip "FX Beat Change button", "FX BPM Auto/Tap mode" | Partial | Match | P1 | |
| FX assign to deck / sampler / master | Tip "FX assign (Deck)", "(Master)" | Have | Match | P1 | |
| Release FX | Tip "Release FX On/Off button" | Build | Match | P2 | |
| Sound Color FX: Dub Echo, Sweep, Noise, Filter (+ Space, Pitch, Crush, Jet) | Tip "CFX Default mode" | Partial | Match | P1 | Mix Two quick effects; DUB ECHO / SWEEP / NOISE presets are user-configurable in the mapping. Build matching presets. |
| Apply CFX to sampler via MIC COLOR knob | Prefs "Apply CFX to Sampler using MIC COLOR control knob." | Build | Match | P3 | |
| Pad FX (banks 1 / 2, edit window) | Tip "Displays Bank1 of Pad FX." | Partial | Match | P2 | Approximated in the DDJ-800 mapping. |
| FX1 / FX2 as send / return | Prefs "Use FX1 and FX2 as FX SEND/RETURN." | Map | Match | P3 | |
| MERGE FX | Juce "Enable MERGE FX" | Build | Skip | P3 | Preset build-up effect; later, if wanted. |
| RMX EFFECTS (Plus Pack) | Prefs "Add RMX EFFECTS to rekordbox dj." | — | Skip | — | Separate paid pack; not core. |

## E. Sampler

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| 16-slot sampler with banks | Tip "Switching of banks" | Have | Match | P1 | Mix Two supports up to 64 samplers. |
| One-shot / loop play mode, gate mode | Tip "Play mode (Oneshot)", "Gate mode" | Partial | Match | P1 | Mixxx samplers repeat or not; add gate. |
| Slot sync, sampler master, sampler BPM | Tip "Slot Sync", "Sampler Master" | Have | Match | P2 | |
| Slot gain, start-point adjust, edit window | Tip "Slot Gain", "Adjustment of playback start point" | Partial | Match | P2 | |
| Sampler quantize | Tip "Sampler Quantize" | Partial | Match | P2 | |
| Sequencer (record, play, save 8 sequences, mute / erase) | Tip "Sequence Rec", "Sequence Save" | Build | Match | P3 | |
| Capture (loop or slicer section to a slot) | Tip "Capture: You can extract a section of a track…" | Build | Match | P2 | |
| Track type normal / one-shot sample / loop sample | Ctx "Track Type", "Oneshot Sample", "Loop Sample" | Partial | Match | P2 | |
| OSC sampler | Tip "Toggle OSC sampler" | — | Skip | — | Synth toy; low value. |
| Sample packs (download / import) | Juce "Download Sample Pack" | — | Skip | — | Online content. |
| Sampler list (playlist of samples) | Ctx "Create New Sampler List" | — | Skip | — | Covered by playlists. |

## F. Browser and library

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| Collection (all tracks) | GUI "Collection" | Have | Match | — | |
| Playlists **with folders** | Ctx "Create New Folder" | Partial | Match | P1 | Built 2026-10-26: nested folders (schema 41, `PlaylistFolders` + `Playlists.folder_id`); "Create New Folder" / "Rename Folder" / "Delete Folder" with rekordbox's wording and defaults ("Untitled Folder", "Untitled Playlist"); "Move to Folder" submenu; new and duplicated playlists go into the folder you right-clicked; folders kept expanded across sidebar refreshes; the track menu's Add to Playlist shows folders as submenus. **UI gaps to close in the rekordbox-layout skin (A):** user-defined item order with drag to reorder and "Sort Items" (Mix Two sorts alphabetically, folders first); drag playlists onto folders; inline rename in the tree instead of a dialog; tree labels without track count and duration; rekordbox-style folder and playlist icons (redrawn, not copied). Crates are not foldered (rekordbox has none; see My Tag). |
| Intelligent playlists (rules: contains / starts with / range / in the last N days; all / any; auto-update) | Ctx "Create New Intelligent Playlist", "Match all of the …" | Build | **Improve** | P2 | Match rekordbox's rules; improve with nested AND/OR groups. |
| History (per session, turn into playlist) | GUI "History"; Ctx "Turn the play history into a playlist" | Have | Match | — | Mixxx history (set log). |
| Tag List (temporary queue) | GUI "Tag List"; CSV `9667` | Partial | Match | P1 | Mapping sends to Auto DJ; build a real Tag List like rekordbox's. |
| Hot Cue Bank Lists | GUI "Hot Cue Bank List" | Build | Match | P3 | |
| Related Tracks with criteria (BPM range, key, genre, colour, artist, year, recently added) | Juce "Related Tracks" criteria; Prefs "Range of BPM", "Related key" | Partial | Match | P2 | Mixxx "search related" menu covers some; build saved criteria sets. |
| Track Suggestion (similarity, play history, matching, era, mood) | GUI "Suggest Next Tracks"; Juce "Track Suggestion", "Mood" | Build | **Improve** | P2 | rekordbox uses cloud and ML; build it locally and explain why a track is suggested. |
| Matching (mark good track combinations) | Tip "Set a good match combination of tracks." | Build | Match | P3 | |
| My Tag (user tag categories, filter by tags) | GUI "My Tag"; Prefs "Add \"My Tag\" to the \"Comments\"" | Build | Match | P2 | Crates are close but not categorised tags. |
| Track filter (BPM, key, My Tag, rating, colour), set from master deck | Prefs "Display/Hide Track Filter", "Set [BPM] and [KEY] as those of the track on MASTER DECK" | Partial | **Improve** | P2 | Mix Two search syntax (`bpm:`, `key:`) can do it; build the filter bar and one-click "from master deck". |
| Category / column browser | Prefs "Category", "Active Categories" | Build | Match | P3 | |
| Sort options and column choice | Prefs "Sort", "Column" | Have | Match | — | |
| Search (incremental) | Tip "Search filter" | Have | Match | — | |
| Search target (collection / selected playlist) | GUI "Search Target" | Partial | Match | P3 | |
| Playlist palette (pinned playlists, drag targets) | GUI "Playlist Palette" | Build | Match | P3 | |
| Shortcuts in tree view | Tip "You can create shortcuts to a playlist or a folder" | Partial | Match | P3 | Mixxx quick links in Computer only. |
| Quick preview / waveform preview in list | Tip "Waveform Preview", "Quick Preview" | Have | Match | P2 | Preview deck plus overview column. |
| Artwork column and artwork panel | GUI "Artwork" | Have | Match | — | |
| Status icons (now playing, unanalysed, missing, in Tag List, has cues) | Tip "Now playing", "Track is not analyzed" … | Partial | Match | P2 | |
| Coloring of played tracks (reset on exit) | Juce "Coloring of played tracks", "Reset color when exiting rekordbox" | Have | Match | — | Mixxx played state. |
| Explorer (file browser) | GUI "Explorer" | Have | Match | — | Computer feature. |
| iTunes / Music.app library | GUI "iTunes" | Have | Match | — | |
| rekordbox xml library (import) | GUI "rekordbox xml file" | Build | Match | P2 | Interop with other tools. |
| Devices (USB with rekordbox export) | GUI "Devices" | Partial | Match | P1 | Mix Two reads rekordbox USB exports (F11). |
| Recordings folder in tree | GUI "Recordings" | Have | Match | — | |
| Library protection (lock edits) | Juce "Library Protection" | Build | Match | P3 | |
| Display speed / cache track list | Prefs "Cache track list." | — | Skip | — | Implementation detail. |
| Drop-down on long press of a track | Prefs "Display the menu when long-pressing a track for 1 second." | Have | Match | — | Context menu. |

## G. Track context menu

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| Analyze track(s), Normal / Dynamic | Ctx "Analyze Track (Normal)", "(Dynamic)" | Have | Match | — | Mixxx "assume constant tempo" on / off. |
| Analyze key only | Ctx "Analyze Key" | Partial | Match | P2 | |
| Add new analysis data | Ctx "Add New Analysis Data" | Map | Match | P2 | Tie to the analysis engine choice (WS-D). |
| Analysis lock | Ctx "Analysis Lock" | Have | Match | — | BPM lock. |
| Reload tag | Ctx "Reload Tag" | Have | Match | — | Import metadata from file. |
| Add to playlist / Tag List / sampler list | Ctx "Add To Playlist" | Have | Match | — | |
| Playlists including this track | Ctx "Playlists including this track" | Partial | Match | P3 | Crates show membership; playlists do not. |
| Reset DJ play count | Ctx "Reset DJ Play Count" | Partial | Match | P3 | |
| Remove from collection / delete track | Ctx "Remove from Collection", "Delete Track" | Have | Match | — | |
| Show in Explorer | Ctx "Show in Explorer" | Have | Match | — | |
| Export track(s) / playlist to a device | Ctx "Export Track", "Export Playlist" | Build | Match | P2 | WS-C. |
| Property (track information) | Ctx "Property" | Have | Match | — | |
| Change artwork | Ctx "Change Artwork" | Have | Match | — | |
| Relocate track | Ctx "Relocate Track" | Partial | Match | P2 | |
| Set cue / hot cue A–D / memory loop from the list | Ctx "Set Hot Cue A" … | Build | Match | P3 | |
| Export loop as WAV | Ctx "Export Loop As Wav" | Build | Match | P3 | |
| Change waveform colour | Ctx "Change waveform color" | Build | **Improve** | P3 | Per-track waveform colour. |
| BPM: reload master BPM, increment / decrement | Ctx "Reload Master BPM", "Increment BPM" | Partial | Match | P3 | |
| Hot Cue Auto Load (CDJ) | Ctx "Auto Load Hot Cue" | — | Skip | — | CDJ-only setting; revisit with WS-C. |
| Make information public / private (KUVO) | Ctx "Make information public" | — | Skip | — | KUVO service. |
| Video list, photo list, slideshow | Ctx "Add to Video list" | — | Skip | — | Video extension. |
| Analyze video file | Ctx "Analyze video file" | — | Skip | — | |

## H. Track information

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| Core tags: title, artist, album, genre, comments, year, BPM, key, duration, bitrate, sample rate, file type | Info | Have | Match | — | |
| Mix name, remixer, original artist, label, lyricist, composer, grouping, album artist, disc / track number | Info | Partial | Match | P2 | Mix Two lacks mix name, original artist, lyricist as first-class columns. |
| Rating (stars) | Info "Rating" | Have | Match | — | |
| Track colour (8 colours, custom names) | Info "Color"; Property "Customize Color Comments" | Have | Match | P2 | Use rekordbox's 8-colour palette and its names (Pink, Red, Orange, Yellow, Green, Aqua, Blue, Purple). |
| DJ play count, date added, date created, release date | Info | Have | Match | — | |
| Bit depth | Info "Bitdepth" | Partial | Match | P3 | |
| Edit multiple tracks at once | (rekordbox property sheet) | Have | Match | — | Mixxx multi-track editor. |
| Write analysed value to ID3 tag | Prefs "Write the value to the ID3 tag" | Have | Match | — | Metadata export. |
| Auto-tag from online database | — | Have | **Improve** | P3 | Mixxx MusicBrainz lookup; rekordbox has none. |

## I. Analysis

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| Auto analysis after import | Prefs "Auto Analysis after Import" | Have | Match | — | |
| Track analysis mode Normal / Dynamic | Prefs "Track Analysis Mode" | Have | Match | — | |
| BPM range for detection | Prefs "BPM Range" | Build | Match | P2 | Avoids half/double errors. |
| Key analysis on import, notation (Classic / Alphanumeric / Mixed In Key style) | Prefs "Enable Key Analysis on import", "Notation for Mixed In Key" | Have | Match | — | |
| Show segment key on waveform | Prefs "Show Segment Key on waveform" | Build | Match | P3 | |
| Concurrent / multi-thread analysis | Prefs "Concurrent Process Mode"; Juce "Apply multi-thread to the analysis process" | Have | Match | — | |
| Grid editing: shift, stretch, set first beat, tap, whole / partial, undo | Tip "Shift the whole Beatgrid left" … | Partial | **Improve** | P1 | Mix Two has grid shift and BPM edit with undo; add partial (from here) adjustment and rekordbox's GRID panel layout. |
| Phrase analysis (intro / verse / chorus / outro), edit phrases | Juce "Phrase (Enlarged Waveform)", "Cut phrases." | Build | Match | P2 | Needed for beat count to phrase, lighting, Mix Point Link. WS-D. |
| Vocal position detection | Juce "Vocal (Enlarged Waveform)" | Build | Match | P3 | |
| Analysis engine choice (Mixxx / Pioneer v5 / v6 / v7) | — (owner requirement) | Build | **Improve** | P2 | WS-D. |
| Imported rekordbox beat grids keep rekordbox's BPM | ANLZ PQTZ (beat time in ms + tempo x100) | Have | Match | P1 | Fixed 2026-10-27: the import built one tempo segment per millisecond-rounded beat, so the BPM flickered (131.98 / 132.01) while playing, and sync followers wobbled with it. Now one segment per run of beats with the same stored tempo (`src/library/rekordbox/rekordboxbeats.cpp`); beats stay within 1 ms of rekordbox's grid. Takes effect when a track is loaded from the rekordbox view again. Also fixed: tracks with only an ANLZ .DAT (no .EXT) lost the rekordbox grid entirely (one flag did double duty for grid and cues). |
| Fix grids / cues of tracks analysed by old versions | Prefs "Automatically fix misalignment of BeatGrid…" | — | **Improve** | P3 | Never overwrite user edits; show a diff before re-analysis replaces anything. |
| Analysis overwrites BPM / grid / key ("Analyzed data will be overwritten") | Prefs note | Have | **Improve** | — | Mixxx preserves locked and user-edited data; keep that. |
| Allow analysis while LAN connected | Prefs | — | Match | — | Not applicable without PRO DJ LINK. |
| Analysis file location ("PIONEER" folder) | Prefs "\"PIONEER\" Folder (analysis files)" | Partial | Match | P3 | Mix Two stores analysis in its database and `analysis/` folder. |

## J. Library management

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| Library backup / restore (database + analysis files) | Menu "Backup", "Restore"; GUI "Library Backup" | Build | Match | P2 | |
| Missing File Manager | Menu "Missing File Manager" | Partial | Match | P2 | Mixxx "Missing tracks" list. |
| Auto Relocate with search folders | Juce "Auto Relocate Search Folders" | Build | Match | P2 | |
| Master database on an external drive, move database | Prefs "Move Database" | Partial | Match | P3 | Mix Two `--settings-path`; build a UI to move it. |
| Import a rekordbox 5 library | Juce "Import your library from ver. 5 (or earlier)" | Partial | **Improve** | P1 | Import the user's **rekordbox 6** `master.db` (SQLCipher) as well, with cues, grids, playlists and history. This is the migration path for rekordbox users. New unknown U8. |
| Library conversion (rb5 → rb6, one way) | Juce "Converting your library" | — | **Improve** | — | Mix Two keeps a readable database and can always export back (WS-C). |
| Export / import collection as rekordbox XML | Ctx "Export Collection in xml format" | Build | Match | P2 | |
| Export playlist to text / M3U | Juce "Export a playlist to a file (*.txt)" | Have | Match | — | |
| Library protection | Juce "Library Protection" | Build | Match | P3 | Same as F. |
| Cloud Library Sync, Dropbox / Google Drive | Juce "Cloud Library Sync" | — | Skip | — | Account-bound service. A future local-network sync would be an Improve. |

## K. Export and devices

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| Device Library export (PDB / ANLZ) | GUI "Devices"; Ctx "Export Playlist" | Build | Match | P2 | WS-C. |
| Device Library Plus | Juce "Device Library Plus (OPUS-QUAD ONLY)" | Build | Match | P2 | WS-C, SQLCipher. |
| Convert Device Library ↔ Library Plus | Juce "Convert to Device Library Plus" | Build | Match | P3 | |
| Sync Manager (keep device playlists up to date) | GUI "Playlist Sync Manager" | Build | Match | P3 | |
| Device background colour, device name | Property "Device Name", "Background Color" | Build | Match | P3 | Written into the export. |
| Device library backup / restore | Juce "Device library backup" | Build | **Improve** | P3 | |
| CDJ "My Settings" (jog brightness, auto cue, quantize …) | Prefs "My Settings" | Partial | Skip | — | For CDJs, not the DDJ-800. Revisit if CDJ export users ask. |
| PRO DJ LINK (Link Export, Link Monitor) | Prefs "Remote setting using PRO DJ LINK" | — | Skip | — | Networked CDJs; outside the DDJ-800 goal. |
| MEP-4000 library | Prefs "Create library for MEP-4000…" | — | Match | — | Folded into device export if needed; no separate work. |

## L. Recording, Automix and sharing

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| Recording: source, level, start / stop | Tip "Recording source", "Start/Stop recording." | Have | Match | P2 | |
| Start recording on audio input; stop after 20 s silence | Prefs "Recording starts when:" … | Build | Match | P3 | |
| Split recording on silence | Prefs "Automatically separate files when silence continues." | Build | Match | P3 | |
| Optimise level, auto-import, tag window after recording | Prefs "Automatically optimize audio level of a recorded file." | Partial | Match | P3 | |
| Mix MIC into REC OUT | Juce "Mix the MIC audio to REC OUT" | Have | Match | — | |
| Automix: playlist, start / stop, repeat, random, delete played | Juce "Automix settings", "Repeat playback", "Random playback" | Have | Match | P2 | Mix Two Auto DJ; align names and defaults. |
| Automix manual crossfade start | Juce "Auto crossfade manual start" | Partial | Match | P3 | |
| Mix Point Link (set mix-in / mix-out points between decks) | Juce "Enable MIX POINT LINK Function" | Build | Match | P3 | |
| Auto BPM transition | Juce "The AUTO BPM TRANSITION was stopped." | Partial | **Improve** | P3 | |
| Broadcasting (live stream) | — | Have | Match | — | Mix Two extra; keep. |
| Share mix to Mixcloud / YouTube | Juce "Share Mix", "YouTube Upload Settings" | — | Skip | — | Account services. |
| KUVO live playlist | Prefs "Send Live Playlist to KUVO." | — | Skip | — | Service. |
| Share play history | Juce "Share Play History" | Have | Match | — | History export. |
| Notifications from rekordbox.com | Juce "Display rekordbox notifications" | — | Skip | — | |
| Tutorials and onboarding questionnaire | Juce "rekordbox dj tutorial", "What kinds of music do you usually play?" | — | Skip | — | |

## M. Controller and MIDI

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| Plug-and-play DDJ-800 | — | Have | Match | P1 | Auto-enable plus hotplug (WS-A). |
| Load method: LOAD button or rotary selector push | Prefs "Load Method", "Load Button", "Rotary Selector (Push)" | Have | **Improve** | P1 | rekordbox makes this a per-controller preference (`isLoadMethodButton`). Owner decision 2026-10-27: **no preference**; both ways always work. Pushing BROWSE on a track opens the "Load to the Deck" prompt; while it is open the DJ turns and pushes BROWSE again, presses a deck's LOAD button, or clicks Deck1/Deck2; it closes after 5 s. LOAD buttons always load directly when the prompt is closed. Generic controls `[Library] load_prompt*` let other mappings use it. **UI gap:** the prompt's look is provisional until the rekordbox-layout skin (A). |
| Instant Doubles | Prefs "Instant Doubles"; CSV `9646` | Have | Match | P1 | Double-press LOAD. |
| Vinyl speed adjust (touch / brake, release / start) | Prefs "Vinyl Speed Adjust", "Touch/Brake" | Partial | Match | P2 | Mixxx brake / softStart exist; expose as settings. |
| Jog rotation speed, jog display mode | Prefs "JOG Rotation Speed", "Switch JOG Display" | Partial | Match | P2 | Mapping has `jogResolution`; jog display needs the hardware capture (U2, U3). |
| Deck indication settings (sync, slip, on-air, vinyl, hot cue countdown) | Prefs "Deck selection indication", "Sync indication" … | Map | Match | P2 | LED behaviour in the mapping. |
| SHIFT function list overlay | Juce "SHIFT Function List" | Build | **Improve** | P2 | Show what SHIFT does on the connected controller. Generate it from the mapping. |
| Demo / screen saver on the controller | Juce "Demo (1 minute)" | Map | Match | P3 | LED demo after idle. |
| MIDI Learn | Prefs "Click to enter the MIDI LEARN standby mode." | Have | Match | — | Mixxx learning wizard. |
| MIDI setting window (edit, import / export mapping files) | Prefs "MIDI setting", "Import MIDI setting file" | Partial | **Improve** | P2 | Mix Two edits mappings as files; build a table editor like rekordbox's, keeping script support. |
| Keyboard shortcut editor and presets | Prefs "Keyboard"; Tip "Change this key-mapping" | Partial | **Improve** | P2 | Mix Two has keyboard mappings but no editor UI. |
| Audio setup: buffer size, driver, setting utility | Prefs "Buffer size", "Setting Utility" | Have | Match | P1 | Auto-select the DDJ-800 audio interface when it is plugged in (WS-B). |
| Ableton Link (tempo, start / stop sync) | Juce "Enable/disable Ableton Link" | Build | Match | P3 | |

## N. Account, cloud and extensions

All of these are **Skip**, except the two **Improve** rows.

| Feature | Evidence | Mix Two | Decision | Pri | Notes |
|---|---|---|---|---|---|
| Subscription plans, licence keys, feature gating | Prefs "License", Juce "Choose your plan" | — | **Improve** | — | Everything is available; no account needed. |
| Telemetry ("Sending Log Information") | Prefs "Agree to send information…" | — | **Improve** | — | No telemetry. |
| Cloud Library Sync, CloudDirectPlay, Collection Auto Upload | Juce | — | Skip | — | Account services. |
| Mobile device sync (rekordbox iOS / Android) | Juce "Search mobile device" | — | Skip | — | |
| Streaming: Beatport, Beatsource, SoundCloud, TIDAL, Inflyte | Juce "Log in to Beatport." … | — | Skip | — | Licensing and API access. |
| KUVO | Prefs "Privacy Settings for KUVO" | — | Skip | — | |
| Lighting (DMX, fixtures, macros, scenes, PRO DJ LINK Lighting) | GUI "Fixture"; Juce "Enable lighting function" | — | Skip | — | Large, separate product area. Revisit after core parity. |
| Video (video files, visualizers, Syphon, Touch FX) | Tip "Select a visualizer here" | — | Skip | — | |
| Lyric (rekordbox lyric) | Juce "Enable Lyric function" | — | Skip | — | Service. |
| DVS Plus Pack | Prefs "Add DVS function to rekordbox dj." | — | Skip | — | Mix Two already has vinyl control for free; nothing to copy. |
| Track separation (vocal / inst / drums, Active Part, Part FX) | Juce "Enable the TRACK SEPARATION Function" | — | Skip (for now) | P3 | Mix Two plays stem files. Real-time separation needs a model; revisit after WS-D. |
| EDIT mode (timeline editor, render) | Juce "Tracks created in EDIT mode." | — | Skip | — | |
| Dropbox / Google Drive storage transfer | Juce "Cloud Data Transfer" | — | Skip | — | |
| Notifications, tutorials | Juce | — | Skip | — | Same as L. |
| "My Page", account profile | Juce "Open MY PAGE" | — | Skip | — | |

## Next steps

1. **Owner check of the 40 P1 rows** (23 already Have): confirm the Match / Improve calls and
   the **Verify** rows in rekordbox 6, especially the load method (U7) and the
   sync type.
2. **P1 build order** (agreed with the owner 2026-10-26):
   1. ~~Memory cues as a real cue type, and the DDJ-800 mapping switched from
      hot-cue stand-ins to them (B).~~ Done 2026-10-26.
   2. ~~Playlist folders (F).~~ Done 2026-10-26; UI gaps listed in row F.
   3. ~~Load method preference with rotary-push loading (M, U7).~~ Done
      2026-10-26; UI gaps listed in row M.
   4. Beat FX single / multi mode and the missing Pioneer-style effects (D).
   5. rekordbox 6 library import (J, U8).
   6. rekordbox-layout skin: 2/4-deck horizontal and vertical layouts, panel
      toggles, deck panel selector (including the Memory Cue panel), Browse
      view (A). Needs the owner's layout decisions first.
3. Keep this matrix current: when a row changes state, update its **Mix Two**
   column; when the owner overrides a decision, record why in Notes.

<!-- End of AI-generated document. -->
