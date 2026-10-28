# Mix Two: design (aesthetics)

> AI-generated (Claude Code) from the owner's direction, 2026-10-27.
> End of AI-generated notice.

How Mix Two should look. Technical rules are in [FUNCTIONS.md](FUNCTIONS.md).

## Direction

- **The hardware comes first.** Lay the screen out like the DDJ-800 itself:
  its sections, its order, its wording. Where rekordbox's on-screen layout
  differs and does not help a DJ (especially a new one), follow the mixer.
  rekordbox 6 stays the reference for everything else.
- **Pioneer wording.** MASTER, BOOTH, HEADPHONES, MIXING, LEVEL, CUE,
  GRID LOCK / BPM SYNC, KEY SYNC, MT. Keep labels short on screen ("Booth",
  not "Booth Level").
- **Mirrored from the centre out.** Deck 1 is the mirror image of deck 2.
  From the mixer outwards: the mixer module (HEADPHONES / MASTER), the rate
  column (SYNC, KEY SYNC, MT, tempo slider), the circular deck hub, then the
  track rows. Deck 1: rows | hub | rate | HEADPHONES; deck 2: MASTER | rate |
  hub | rows. Slots are set per deck in `skin.xml` (`SlotLeft1/2`,
  `SlotRight1/2`).

## The DJ can still arrange it

The Pioneer-style layout is the default, not a cage. As sections are built,
keep them hideable and movable, and work towards customisable view
arrangements:

- Every section (deck hub, rate column, mixer modules, FX, sampler, ...) can
  be shown or hidden on its own, as LateNight's existing show / hide options
  already allow (spinny size, rate controls, key controls, mixer).
- Sections can be reordered: build them so their place is chosen in one spot
  (today the per-deck slots in `skin.xml`), not hard-wired into neighbours.
- Later: saved view arrangements (presets) that the DJ picks in the skin
  settings, with the Pioneer layout as the default preset.

## Aesthetic: tasteful 90s skeuomorphism meets neumorphism

A requirement to keep improving on, never gaudy.

- **Raised modules.** Each control group (a mixer section, later each deck
  area and FX unit) is a framed module: a soft vertical gradient face, a
  slightly lighter top edge and a darker bottom edge, small corner radius.
  No heavy drop shadows, no gloss, no bevel clichés.
- **Titled modules.** A module's title sits inside its frame, at the top, in a
  slightly inset bar, as section names are printed on the hardware. Titles
  separate sections; plain floating text does not.
- **Labels above knobs**, as printed on the mixer. Range labels (CUE / MASTER
  under MIXING) sit close under their knob.
- **Domed round buttons** where the hardware has round buttons (SYNC, KEY SYNC,
  MT): a slightly domed cap and a ring or centre light. Keep them small, like
  the deck's, but legible: 34 px for labelled buttons (SYNC, KEY SYNC) with
  8 px pixel lettering; 26 px tried first was too small.
- **Pixel font for tiny labels:** Silkscreen Bold (Jason Kottke, SIL OFL 1.1,
  `res/fonts/Silkscreen-Bold.ttf` with `Silkscreen.OFL.txt`; from the Google
  Fonts repository). Use it at 8 px (or 16 px), where it is crisp; smaller
  sizes blur. The MT label uses the same 8 px lettering.
- **Room to breathe.** It is fine to make modules taller to get the layout
  right.

## Colour language

| Colour | Means |
|---|---|
| Tech blue | On / synced (SYNC, KEY SYNC) |
| Red-orange | The MASTER deck; a value that does not match yet (slider BPM) |
| Orange | CUE (MASTER CUE), headphone-related actions |
| White | A value that matches (slider BPM) |
| Vivid red | MT (master tempo) centre light, as on the DDJ-800 |

## Layout as built (2026-10-27)

- Deck hub (`decks/deck_hub.xml`): BPM large and centred, slider BPM, tempo %
  and range, the platter, then CUE / reverse and PLAY, and under PLAY a row
  with the rate − / nudge / + buttons (parked there for now).
- Rate column: SYNC (GRID LOCK / BPM SYNC), KEY SYNC and MT stacked, each a
  34 px domed round button (MT with its title centred below), then the tempo
  slider; kept narrow.
- Mixer: HEADPHONES module left of the channels (MIXING, LEVEL, SPLIT; space
  above for the four colour FX), MASTER module right of them (LEVEL, CUE,
  BOOTH).

## Later

- View arrangements (owner, 2026-10-27: do it with the presets work): the
  deck hub needs its own show / hide option, and HEADPHONES and MASTER need
  separate ones (today they share LateNight's main / headphone option).
- Platter speed: the jog / platter engine uses a fixed 33⅓ (rekordbox's
  default; owner 2026-10-27: no 33⅓ / 45 setting). Revisit 33⅓ / 45 only if
  DVS (control vinyl) support is built; Mixxx's Vinyl Control preferences
  already have the turntable speed for that.
- Hover tooltips explain every control, instead of printing SHIFT functions.
- Colour FX (DUB ECHO, PITCH, NOISE, FILTER) as rectangular lit buttons above
  the headphone module; Beat FX / master FX module design.
- Bring the decks, library and FX units into the same module language.
