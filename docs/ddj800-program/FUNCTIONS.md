# Mix Two: functions (technical design fundamentals)

> AI-generated (Claude Code) from the owner's direction, 2026-10-27.
> End of AI-generated notice.

How Mix Two should behave and be built. Looks are in [DESIGN.md](DESIGN.md);
the feature-by-feature status is in [PARITY.md](PARITY.md).

## Priorities

1. **The sound matches Pioneer.** Scratching, FX, sync: match rekordbox by
   measurement and by behavioural specs from reverse engineering
   (e.g. [SCRATCH-ENGINE.md](SCRATCH-ENGINE.md)).
2. **The behaviour matches rekordbox 6 and the DDJ-800**, then improves where
   rekordbox is hostile to the DJ (rows marked Improve in PARITY.md).
3. Only then additions beyond Pioneer.

## Clean room

- Never copy decompiled code, tables or binaries into Mix Two. Write specs in
  our own words, then implement independently (example: the scratch filter is
  generated from its textbook formula).
- Never bundle Pioneer binaries, models or images. Leave `clatc.dll` (DRM) and
  the deck firmware alone. RE output stays in `build/re/` and never ships.

## Behaviour rules

- **What the owner specifies is the default.** No opt-in settings for
  specified behaviour. Settings exist only where Pioneer offers a real choice
  (Sync mode) or where several algorithms are wanted side by side (Scratch
  algorithm). Example: sync with half / double BPM is always on.
- **The engine owns behaviour; mappings stay thin.** A feature lives in a
  control (`[Master],main_cue`, `[ChannelN],rate_range_next`,
  `keysync_enabled`, ...) so the screen and the controller do exactly the same
  thing. No mapping-side workarounds (the old MASTER CUE trick was replaced).
- **A press toggles.** Buttons that are on / off on Pioneer gear toggle on
  click and on the controller; no press-and-hold (SYNC, KEY SYNC, MT).
- **Controller lights show the engine's state**, also when the engine refuses
  a press (e.g. KEY SYNC with no other analysed key): the mapping sets the LED
  from the engine's answer.
- **KEY SYNC** matches the MASTER's key, else the deck sync would follow,
  else any other loaded deck with an analysed key (a playing one first).
- **Sync, as on Pioneer:**
  - MASTER is sticky: a playing master keeps it until it stops; a chosen
    master works while paused.
  - Only the master's tempo slider sets the tempo; followers ignore theirs,
    and controller sliders catch up (soft takeover).
  - The tempo range widens on its own to fit the master's tempo.
  - Half / double is chosen from the track's own BPM when a deck joins.
  - Sync mode: **Grid Lock** (Pioneer BEAT SYNC: tempo and beats, snapping to
    the beat grid 1/8, 1/4, 1/2 or whole when playback starts) or **BPM Sync**
    (tempo only). This is independent of quantize, which only concerns cues
    and loops.
- **Imported rekordbox data keeps rekordbox's meaning** (beat grids by stored
  tempo; grid from the .DAT, cues from the .EXT).

## Layout is customisable

The "no settings" rule above is about **behaviour**. The **layout** is the
opposite: the DJ may hide and rearrange sections, so:

- Build each new section as its own template / singleton with a visibility
  control (a `[Skin]` / `[LateNight]` show control persisted like the
  existing ones), never as part of a neighbour's markup.
- Choose a section's position in one place (per-deck slot variables in
  `skin.xml` today), so a future arrangement setting can change it.
- Keep LateNight's existing show / hide options working when moving things
  (e.g. the rate +/- buttons still follow `show_rate_control_buttons`).
- Plan towards saved view arrangements (presets) in the skin settings; the
  Pioneer layout is the default.

## Code rules

- Mark Mix Two changes in upstream files with a short `Mix Two:` comment that
  says what and why.
- New app-wide controls go in `[App]`, follow Mixxx's current naming ("main",
  not "master", in control names), and are created by the engine with their
  saved value. Enums stored in `mixxx.cfg` are append-only, and 0 keeps
  Mixxx's original behaviour where one exists.
- Every behaviour change gets a test. Upstream tests change only where they
  encode behaviour Mix Two replaces, with a `Mix Two` comment.
- Build and run with `tools\mixtwo_windows.bat` (`run` builds first;
  `--nobuild` skips it). Never point a dev run at the real Mixxx profile.
