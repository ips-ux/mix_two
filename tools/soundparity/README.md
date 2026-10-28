<!-- AI-generated (Claude Code, 2026-10-27). Review before relying on it. -->

# Sound parity toolkit (WS-F)

Tools for matching Mix Two's sound to rekordbox by measurement. The plan and
the legal boundary (measure the sound, never copy the code) are in
[docs/ddj800-program/SOUND.md](../../docs/ddj800-program/SOUND.md).

- `make_signals.py OUTPUT_DIR [--rate 44100] [--bpm 128]`: writes the test
  signals (16-bit WAV) and `signals.json`.
- `compare.py REFERENCE.wav CANDIDATE.wav [--threshold -30] [--content-end S] [--json OUT]`:
  lines both recordings up on the alignment chirp and reports the null depth
  (raw and gain-matched), residual latency, third-octave differences and
  tail lengths. Exits 0 when the gain-matched null depth is at or below the
  threshold.

Both need Python 3 with numpy and scipy.

## The test signals

Every file is: alignment chirp (0.5 s), two silent bars, the content, a silent
tail. At 128 BPM two bars last 3.75 s.

| File | Content | Used for |
|---|---|---|
| `01_impulses` | impulse every 4 bars, 16 bars | delay taps, feedback, reverb tails |
| `02_clicks` | click on every beat, 16 bars | TRANS, ROLL, SLIP ROLL, gates |
| `03_sweep` | 20 Hz–20 kHz sine sweep, 10 s | filters, LOW CUT ECHO's filter, EQ |
| `04_pink_noise` | pink noise, 16 bars | FLANGER, PHASER, ENIGMA JET, MOBIUS |
| `05_sine_440` | 440 Hz sine, 16 bars | PITCH, SPIRAL, VINYL BRAKE, Master Tempo |
| `06_drum_loop` | synthetic drum loop, 16 bars | listening and A/B tests |

## Capturing rekordbox (the reference)

**Recorder (decided 2026-10-27): rekordbox 5.8.7's own REC.** On this PC
rekordbox 5 has a permanent rekordbox dj activation, so its Performance mode
and REC panel are available. Its Beat FX table matches rekordbox 6's
(including all 14 DDJ-800 effects; only HELIX is rekordbox 6-only) and its
DDJ-800 mapping is identical. REC records the master mix digitally, so no
audio interface is involved. Whether rekordbox 6.8.5's REC works with the
DDJ-800 as a Hardware Unlock device is unclear: Pioneer's current plan page
includes recording in the Free plan, but rekordbox 6-era owners report the
opposite.

**Before trusting rekordbox 5 as the reference for rekordbox 6:** once the
first effects are captured, record a few of the same runs in rekordbox 6 and
compare them with `compare.py`. If they do not null, the versions' algorithms
differ and both are kept as separate references ("Pioneer v5" / "v6").
rekordbox 6 can be recorded with its REC if available, otherwise through the
Audient EVO 4's Loopback (enable it in the EVO app, send rekordbox's master to
the EVO 4 ASIO output, and record the loopback input in Ableton, Bitwig or
Cakewalk).

Run only one rekordbox version at a time; both share the settings folder
`%APPDATA%\Pioneer\rekordbox`.

**rekordbox 5 REC settings** (Preferences › Advanced › Recording; the labels
are rekordbox's):
- "Recording starts when:" **Pressing the REC button.**
- "Recording ends when:" **not** "Detecting 20 sec. of silence." (the
  signals contain long silences).
- **Off:** "Automatically separate files when silence continues.",
  "Automatically optimize audio level of a recorded file." (it would change
  the level), "Automatically import a recorded file." and "Automatically
  display tag information window of a recorded file."
- Recording level knob at its default (unity), and note it.

1. **Set up rekordbox** in Performance mode with the DDJ-800 connected.
   Make the signal path transparent and fixed:
   - deck 1 channel fader up, trim at centre, EQ flat, CFX off, crossfader
     THRU;
   - Auto Gain off, master level fixed (note the value), headroom setting
     noted;
   - keep the test signals' peaks at −6 dBFS (as generated) so that the
     master limiter never engages.
2. **Import the signals** and set each one's BPM to exactly 128.00 in the GRID
   panel; rekordbox may not detect a tempo in sweeps or impulses.
3. **Record a dry baseline first:** each signal with every effect off. Compare
   it with the original file using `compare.py`. If it does not null, the
   master path is colouring the sound; fix that before measuring effects.
4. **For each effect run:** start recording, cue deck 1 at the start, press
   PLAY, and switch the Beat FX on **during the two silent bars after the
   chirp**. The chirp must pass unprocessed, because both recordings are
   aligned on it. Leave the effect on until the tail has died out.
5. **Name the files** `<effect>_<signal>_<beat>_d<depth>.wav`, for example
   `echo_01_impulses_1beat_d100.wav`. Store them outside the repository;
   they are measurements of Pioneer's software, kept for our comparisons only.

**Grid for the DDJ-800's 14 Beat FX** (first pass): beat values 1/2, 1 and 2
(or the effect's nearest values), depth 50 % and 100 %, with the signals
from the table above. Add the parameter knobs once the static facts list
which parameters each effect has.

## Capturing Mix Two (the candidate)

The same steps in Mix Two (`tools\mixtwo_windows.bat run`), with the same
signals, BPM, settings and file names in a separate folder. Then, per pair:

```
python tools/soundparity/compare.py rekordbox/echo_01_impulses_1beat_d100.wav mixtwo/echo_01_impulses_1beat_d100.wav --content-end 33.75
```

`--content-end` is counted from the end of the chirp: two silent bars plus
the content. For the 16-bar signals at 128 BPM that is 18 bars = 33.75 s.

The latency the tool reports counts as a difference too: rekordbox's
processing delay is part of how its effects feel.

## Scratch renders (deck sound)

`render_scratch.py` compares the scratch, spin and bend sound. The audio is
a real track, not a test signal: the owner's `.resource/jvckpot.mp3`
(ignored by git, like everything in `.resource/`). `wrap` decodes it with
ffmpeg, takes 12 s from 0:30, scales the peaks to −6 dBFS and puts the
alignment chirp and two silent bars in front, so captures line up with
`compare.py`.

```
python tools/soundparity/render_scratch.py all .resource/jvckpot.mp3
```

writes to `build/soundparity/scratch/` (ignored by git):
- `music.wav`: the wrapped track. Use the same file in rekordbox.
- `jog/*.jog`: the movements, as DDJ-800 MIDI with times in ms (`B0 22`
  top jog, `B0 21` side jog, value 0x40 ± steps at 720 steps per
  revolution; `90 36` touch; `90 0B` PLAY). The deck plays from 0 s; the
  movements start 4 s into the music.
- `mixtwo/*.wav`: Mix Two's render (scratch algorithm "Rekordbox").
- `mixtwo-linear-scaler/*.wav`: the same motion through Mixxx's linear
  scaler, to hear what the rekordbox scaler changes.

The renders run in `mixxx-test` (`PlatterRenderTest`, skipped unless its
`MIXTWO_RENDER_*` variables are set), through the real engine with
128-frame buffers. It turns the MIDI into controls as the DDJ-800 mapping
does.

**The rekordbox side is still open.** rekordbox holds the DDJ-800's MIDI
port, so the movement cannot simply be logged on Windows 11 23H2 while
rekordbox runs. Two ways:
1. Perform the movement on the deck in rekordbox with REC running, capture
   the USB MIDI with USBPcap / Wireshark at the same time, convert the
   capture to a `.jog` file, and render that in Mix Two.
2. Send the `.jog` files into rekordbox through a virtual MIDI port named
   "DDJ-800" (`send_jog.py`, being tried first, see below).

### Replaying a `.jog` into rekordbox 5 (virtual DDJ-800)

rekordbox 5 finds MIDI controllers by port name (`midiInGetDevCapsW`, the
mapping file is `DDJ-800.midi.csv`), and on this PC its rekordbox dj
activation is permanent, so it should not need the real deck for the
unlock. `send_jog.py` creates a send-only port named "DDJ-800" with the
teVirtualMIDI driver that loopMIDI installs: rekordbox sees only an input,
so its LED messages cannot loop back as button presses.

1. Install loopMIDI once (the owner; it installs a driver). No port needs to
   be created in loopMIDI itself.
2. Unplug the real DDJ-800's USB cable, and quit Mix Two and rekordbox 6/7.
3. `python tools/soundparity/send_jog.py --loopback build/soundparity/scratch/jog/release_spin.jog`
   checks the driver and the send timing.
4. `python tools/soundparity/send_jog.py build/soundparity/scratch/jog/*.jog`
   opens the port and waits; keep it running.
5. Start rekordbox 5.8.7 in Performance mode. Check that it shows the DDJ-800
   as connected. Import `build/soundparity/scratch/music.wav`, set its BPM to
   128.00, load it on deck 1, cue it at the very start, paused. Same mixer
   settings as for the effect captures above (fader up, EQ flat, Auto Gain
   off, crossfader THRU).
6. Start REC, then press Enter in `send_jog.py`: the script presses PLAY at
   0 s and then moves the jog. When the render length has passed, stop REC
   and save the file as `build/soundparity/scratch/rekordbox/<name>.wav`.
   Pause and cue deck 1 back to the start before the next script.
7. Compare, for example:
   `python tools/soundparity/compare.py build/soundparity/scratch/rekordbox/backspin.wav build/soundparity/scratch/mixtwo/backspin.wav`

Unknown until tried: whether rekordbox accepts a DDJ-800 that is only a MIDI
port (no audio interface, no HID), and whether it needs an output port too.

## Later

Replaying recorded controller MIDI into both apps, so that knob moves are
identical between runs, and batch comparison of whole capture folders.

<!-- End of AI-generated document. -->
