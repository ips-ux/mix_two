<!-- AI-generated (Claude Code, 2026-10-30). Start of AI-generated text. -->

# DDJ-800 deck test: fixes from commits 3137934afa and 340bb0e157

> **AI-generated:** Claude Code wrote this checklist. The owner does the
> tests and sets Deck-verified. The writing style is ASD-STE100.

## Before you start

1. Quit rekordbox.
2. Start Mix Two with `tools\mixtwo_windows.bat`.
3. Connect the DDJ-800. Make sure that the DDJ-800 mapping starts.
4. Load a track with a beat grid on deck 1 and on deck 2.
5. Do each test on deck 1, then on deck 2.

Write OK or the problem in the Result column. Write what you see and what
you hear.

## Tests

| Row | Test | Expected result | Result |
|---|---|---|---|
| CTL-008 | Hold SHIFT. Turn the rotary selector to the right, then to the left. | Right: the waveform zooms in. Left: the waveform zooms out. | |
| CTL-061 | Push HOT CUE. Push pads 1 to 8 to set hot cues A to H. | Each pad sets a hot cue. The pad comes on in the hot cue color. | |
| CTL-061 | Push PAGE ▶. Push pads 1 to 8. | The pads set hot cues I to P. The screen shows I to P. | |
| CTL-061 | Push PAGE ◀. Push a pad. | The pads show A to H again. Pad 1 plays hot cue A. PAGE does not call a memory cue. | |
| CTL-061 | On page 2, hold SHIFT and push a pad. | The hot cue on that pad (I to P) is removed. | |
| CTL-080 | Hold SHIFT and push BEAT JUMP (BEAT LOOP mode). Push pad 5. | A 4-beat loop starts. Push pad 5 again: the loop stops. | |
| CTL-080 | In BEAT LOOP mode, push PAGE ◀ and PAGE ▶. Push the pads. | The loop lengths become shorter or longer. Each pad starts a loop. | |
| CTL-083 | Hold SHIFT and push SAMPLER (KEY SHIFT mode). Push the top pads, then the bottom pads. | Top pads: +4 to +7 semitones. Bottom pads: 0 to +3 semitones. | |
| CTL-083 | In KEY SHIFT mode, push PAGE ◀. Push the pads. | All pads are 8 semitones lower (-4 to -1 on top, -8 to -5 below). PAGE ▶ goes back. | |
| CTL-075 | Hold SHIFT and push HOT CUE (KEYBOARD mode). Push a pad that has no hot cue. | Mix Two sets a hot cue at the play position. You hear nothing new. | |
| CTL-075 | In KEYBOARD mode, push the pads. | Each pad plays the selected hot cue at a different pitch. | |
| CTL-075 | Push SHIFT + HOT CUE again. Push a pad that has a hot cue. | Mix Two selects that hot cue. It does not move the hot cue. | |
| CTL-051 | Hold SLIP REVERSE for more than 8 beats while the track plays. | The track plays in reverse for 8 beats, then normal play continues at the slip position. The button light goes off. | |
| CTL-051 | Push SLIP REVERSE for a short time, then release it. | Reverse stops when you release. Normal play continues at the slip position. | |
| CTL-020 | Hold TRACK SEARCH ▶▶ and turn the top of the jog wheel. | The track moves quickly. Write how far one turn moves the track (we set 30 seconds). | |
| CTL-020 | If nothing occurs in the test above | Write "no change". The DDJ-800 possibly does not send this signal. | |
| CTL-033 | Hold SHIFT and push RELOOP/EXIT. | Nothing occurs. The track does not stop. (Active Loop is not available yet.) | |

## Regression tests

Also do these tests. The fixes can have an effect on them.

| Test | Expected result | Result |
|---|---|---|
| Push CALL ◀ and CALL ▶. | The deck goes to the previous or the next memory cue. | |
| Push MEMORY, then DELETE. | MEMORY sets a memory cue. DELETE removes it. | |
| Do a scratch and a backspin. Use the Rekordbox (6.8.5) scratch algorithm (Preferences > Decks). | The scratch and the spin are the same as before. | |
| Push the other pad mode buttons (PAD FX 1, BEAT JUMP, SAMPLER, PAD FX 2). | Each mode operates as before. | |

## After the tests

Send the Result columns to Claude Code. Claude Code then records the results
in `buckets/01-controller.md`. Only the owner sets Deck-verified.

<!-- End of AI-generated text. -->
