<!-- AI-generated (Claude Code, 2026-10-27). Review before relying on it. -->

# USER_changelog: Mix Two

> **AI-generated:** Claude Code wrote this text from the change record. A
> person must examine it before you use it.

This file tells you about changes in Mix Two that you can see and use. The
writing style is ASD-STE100 Simplified Technical English.

<!--
Writing rules for this file (ASD-STE100):
- One topic for each paragraph. One instruction for each sentence.
- Instructions: 20 words or fewer, in the imperative ("Push SYNC.").
- Descriptions: 25 words or fewer, active voice, simple present or past.
- Use approved words only: "push" a button (not "press"), "show" (not
  "display" as a verb), "make sure" (not "ensure"), "start" (not "launch").
- Use technical names as they show on the screen or on the DDJ-800.
- Put a CAUTION before the step that it is about.
-->

## Unreleased (2026-10-27)

### Before you start

CAUTION: Make a copy of your Mixxx library before you start Mix Two for the
first time. Mix Two changes the library database to schema version 41.

To start Mix Two, double-click `tools\mixtwo_windows.bat`. Mix Two builds,
then starts. If an error occurs, the window stays open. Push a key to close
it.

Quit rekordbox before you start Mix Two. rekordbox holds the DDJ-800 when it
operates. The Pioneer audio driver can also stop Mix Two.

If rekordbox operates when Mix Two starts, a message shows. Quit rekordbox,
then click **Retry**. Click **Start Anyway** only if you accept the risk.

### Pioneer DDJ-800

Mix Two now has a mapping for the Pioneer DDJ-800.

Connect the DDJ-800. Mix Two finds it and starts the mapping. You do not
have to set the mapping.

You can connect the DDJ-800 while Mix Two operates. Mix Two finds the
controller in approximately two seconds.

If the controller does not open at the first attempt, Mix Two tries again
for 30 seconds.

NOTE: You must select the DDJ-800 audio interface yourself. Go to
Preferences > Sound Hardware.

To change the mapping settings, go to Preferences > Controllers, then select the DDJ-800:
- **Start in vinyl mode** (on).
- **Jog resolution** (6480 ticks for each turn).
- **Input selector controls passthrough** (off).

The jog display on the DDJ-800 does not operate yet.

In HOT CUE mode, you have 16 hot cues on two pages. Push PAGE ▶ to use hot
cues I to P. Push PAGE ◀ to go back to hot cues A to H.

In BEAT LOOP mode, push a pad to start a loop of that length. Push the same
pad again to stop the loop.

Turn the rotary selector to the right while you hold SHIFT. The waveform
zooms in. Turn it to the left to zoom out.

In KEY SHIFT mode, the top pads are +4 to +7 semitones. The bottom pads are
0 to +3 semitones. Push PAGE ◀ or PAGE ▶ to move all pads 8 semitones down
or up.

To use KEYBOARD mode, push HOT CUE while you hold SHIFT. Then push a pad to
select the hot cue to play. If that hot cue is empty, Mix Two sets it at the
current position. The pads then play the hot cue at different pitches. To
select a different hot cue, push HOT CUE with SHIFT again.

SLIP REVERSE stops after 8 beats, also if you hold the button.

Hold TRACK SEARCH and turn the top of the jog dial to move quickly through
the track (Super Fast Search). Mix Two has not tested this on the DDJ-800
yet.

SHIFT + RELOOP/EXIT does not operate yet. In rekordbox, it is Active Loop.

### Load a track: "Load to the Deck"

Push the BROWSE knob on a track. The "Load to the Deck" window opens.

Select the deck in one of these ways:
- Push the LOAD button of the deck.
- Click **Deck1** or **Deck2**.
- Turn the BROWSE knob, then push it again.

The window closes after 5 seconds if you do not select a deck.

The LOAD buttons load the track directly when the window is closed.

### Sync

Push SYNC to start sync. Push SYNC again to stop sync.

To make a deck the MASTER, push SHIFT + SYNC. On the screen, right-click
SYNC.

On the screen, the SYNC button shows GRID (Grid Lock) or BPM (BPM Sync).
The KEY SYNC button shows KEY.

The MASTER deck stays the MASTER while it plays. It stays the MASTER when you
close its channel fader.

You can make a stopped deck the MASTER.

Only the tempo slider of the MASTER deck changes the tempo. The tempo sliders
of the other synced decks have no effect.

A deck that starts sync uses double or half tempo when necessary. Mix Two
uses the BPM of the track to select double or half tempo.

You can select the sync mode. Go to Preferences > Decks > Deck options >
Sync options:
- **Grid Lock** (default) keeps the tempo and the beats together.
- **BPM Sync** keeps only the tempo together.

With Grid Lock, set **Beat grid** to 1/8, 1/4, 1/2 or Whole. With Whole, the
beats of the two decks are on top of each other.

Sync and quantize are now separate. Quantize changes only cues and loops.

The setting "Sync mode (Dynamic tempo tracks)" has a new name: **Dynamic
tempo handling**.

### Tempo

To change the tempo range, click the range in the deck hub. You can also use
the DDJ-800. The steps are ±6 %, ±10 %, ±16 % and WIDE (±100 %).

During sync, Mix Two makes the tempo range larger if necessary.

The DDJ-800 tempo slider has no effect until it gets to the tempo of the
track.

The deck shows the BPM of the tempo slider below the large BPM. Red-orange
shows that the values are different. White shows that they are the same.

### Key sync and Master Tempo

Push KEY SYNC to start key sync. The deck then plays in the key of the
MASTER deck.

Key sync stops when you change the key yourself. It also stops when you push
KEY RESET.

To reset the key on the screen, right-click KEY SYNC.

Key sync operates only when another loaded deck has an analysed key.

The MT (Master Tempo) button is now a small round button above the tempo
slider, half way between KEY and the tempo slider.
The lock button near the waveform is removed.

### Deck on each side

The screen shows the deck that each side of the DDJ-800 controls. Push
DECK 1/3 or DECK 2/4 to change the deck. The window for that side follows.
At start, a side shows deck 1 or deck 2 until you use a control on it.

### Memory cues

You can now set memory cues and memory loops. There is no limit to the
quantity.

On the DDJ-800, use these buttons:
- Push MEMORY to set a memory cue.
- Push CALL to go to the next or the previous memory cue.
- Push DELETE to remove a memory cue.

A red "M" shows each memory cue on the waveform.

Memory cues from a rekordbox USB device are now memory cues in Mix Two. They
are not hot cues.

### Scratch

Mix Two now uses a scratch sound like rekordbox.

To select the scratch sound, go to Preferences > Decks > Platter options >
Scratch algorithm:
- **Rekordbox (6.8.5)** is the default.
- **Mixxx (2.7)** is the old Mixxx sound.

With the rekordbox scratch sound, one turn of the jog wheel moves the track
1.8 seconds.

When a track plays, turn the side of the jog wheel to bend the tempo. The
bend is now the same as in rekordbox: 3 % for each normal platter speed, up
to 42 % faster or slower. It operates with the two scratch algorithms.
Before this change, the "Mixxx (2.7)" scratch algorithm made the track
much too fast or too slow.

When a track is paused, turn the jog wheel to move the track, as in
rekordbox. You hear the track at the speed of the jog wheel, and one turn
moves the track 1.8 seconds. This operates with the two scratch algorithms.
Before this change, the track moved much too far.

When Mix Two starts, or when you load the DDJ-800 mapping again, Mix Two
now gets the positions of the knobs and faders from the DDJ-800. The
on-screen knobs then agree with the controller. MASTER LEVEL, BOOTH LEVEL
and the headphone knobs are not included: the DDJ-800 does not send them
until you move them.

Use AUDIO: HARDWARE | PC in the top bar, next to REC, to select where the
sound goes. The selection in use is orange (HARDWARE) or blue (PC):
- **HARDWARE** (default): the sound goes through the DDJ-800 outputs.
- **PC**: the sound goes through the audio interface of the computer.

With the two selections, the MASTER LEVEL, BOOTH LEVEL and HEADPHONES knobs
on the DDJ-800 set the Mix Two levels. The DDJ-800 cannot send the positions
of these knobs when Mix Two starts. Turn each knob a small amount to send
its position.

Mix Two keeps a sound setup for each selection. If you change Preferences >
Sound Hardware, the change applies to the current selection.

If the DDJ-800 is not connected when Mix Two starts, Mix Two uses PC.

To select the audio driver, go to Preferences > Sound Hardware > Audio
Routing:
- **ASIO**: the lowest latency. Other apps cannot play sound through the
  device.
- **WASAPI**: other apps can play sound at the same time. The latency is
  higher.

NOTE: With WASAPI, Windows can give only two channels of the DDJ-800. Then
the headphone cue and the booth output do not operate. To get all six
channels, set the DDJ-800 "Line" device to 5.1 in the Windows sound
settings.

Push SHIFT and turn the jog wheel to move the beat grid, as in rekordbox.
This is the same as GRID SLIDE and the jog wheel. The grid moves a small
distance, so you can zoom in on the waveform and put the grid exactly on
the kick drum. A quarter turn moves the grid approximately 80 ms.

For less audible bends, set the key lock engine to Rubberband
(Preferences > Decks), then push KEY LOCK. With key lock, a bend changes
the tempo but not the pitch, as rekordbox does with Master Tempo.

The jog wheel now operates with the same timing as rekordbox. The track
follows the jog wheel after approximately two audio buffers, as in
rekordbox. For the same timing as rekordbox, set the audio buffer to
approximately 6 ms (Preferences > Sound Hardware > Audio Buffer).

After a scratch or a spin, the track now continues from the correct
position. Before this change, the track could move backward for a short
time when play started again.

The DDJ-800 jog wheel now moves the track the correct distance. Before this
change, the track moved 9 times too far. If scratching moves the track too
far, go to Preferences > Controllers > DDJ-800 and set "Jog resolution" to
6480.

If you release the jog wheel while it turns, the track continues to move
with the jog wheel. This is a spin. A backspin operates in the same way.

The spin stops when the jog wheel stops or turns in the other direction.

During play, a forward spin stops when the jog wheel is slower than the
track. The track then plays at its normal speed.

After a spin, Mix Two ignores the jog wheel until it stops. To use it
sooner, turn it backward approximately 3/8 of a turn.

During play, turn the side of the jog wheel to change the speed for a
short time. A faster turn gives a larger change, to a maximum of 42 %.

### Mixer

The MASTER CUE button now sends the main mix to the headphones.

MASTER LEVEL and BOOTH LEVEL on the DDJ-800 now change the main volume and
the booth volume.

For booth sound, set a Booth output in Preferences > Sound Hardware.

The BOOTH knob replaces the balance knob on the screen.

The HEADPHONES section is now at the top of the left mixer column.

Below HEADPHONES are four Sound Color FX buttons: DUB ECHO, SWEEP, NOISE and
FILTER. Push a button to use that effect with the COLOR knobs. The button
comes on in blue. The buttons on the screen and on the DDJ-800 operate
together:
- Push an effect that is off: only that effect is on.
- Push an effect that is on: all effects stop.
- Hold SHIFT and push an effect: only that effect starts or stops. The other
  effects stay as they are. Thus, you can use more than one effect at a
  time. On the screen, use the SHIFT key of the keyboard.

On the DDJ-800, the Sound Color FX buttons stay lit. They do not show which
effects are on. The DDJ-800 shows this only when rekordbox operates it. The
screen shows which effects are on.

When more than one effect is on, the COLOR knob controls all of them.

To change the effect of a Sound Color FX button, right-click the button.
Then select an effect in the list. Mix Two keeps your selection.

Each Sound Color FX button shows the name of its effect. A long name shows on
two lines. If the name is too long, it is cut and ends with "...".

Below MASTER is the EFFECTS section for Beat FX:
- S 3 1 2 4 M (the order of the DDJ-800 selector) shows where Beat FX is
  assigned: the sampler, a channel, or the master. Turn the BEAT FX CH
  SELECT on the DDJ-800. When no DDJ-800 is connected, click a segment.
- BEAT shows the beat fraction. Click ◀ or ▶ to change it, as the BEAT
  buttons on the DDJ-800. Click the value to select from the list.
- Select the effect in the list. The BEAT FX selector on the DDJ-800 selects
  it also. The first 14 effects in the list are in the order of the DDJ-800
  selector.
- Mix Two now has all 14 DDJ-800 BEAT FX, made from the DDJ-800 manual:
  LOW CUT ECHO, ECHO, MULTI TAP DELAY, SPIRAL, REVERB, TRANS, ENIGMA JET,
  FLANGER, PHASER, PITCH, SLIP ROLL, ROLL, MOBIUS (SAW) and MOBIUS (TRI).
  FLANGER and PHASER sweep one time for each beat fraction, in step with
  the beat grid. ENIGMA JET and the two MOBIUS effects rise one octave for
  each beat fraction without end. To make them fall, enable "Descend" in
  the effect's parameters on the screen. The DDJ-800 BEAT buttons do not
  select the negative beat fractions.
  The BEAT ◀ ▶ buttons set the beat fraction from 1/16 to 16 beats.
  LEVEL/DEPTH operates as the manual says for each effect. TRANS cuts in
  step with the beat grid. ROLL and SLIP ROLL record the sound when you
  push ON/OFF. For REVERB, the BEAT buttons set the extent of the
  reverberation. For PITCH, they set the pitch from -50 % to +100 %, and
  LEVEL/DEPTH all the way counterclockwise is the original pitch.
- LEVEL/DEPTH is the effect level.
- The ENABLED / DISABLED button starts and stops the effect. It is the same
  as the ON/OFF button on the DDJ-800.

The four DDJ-800 Sound Color FX are now Mix Two effects, made from the
DJM-900NXS2 manual. The COLOR knob has no effect at the center, as on the
DDJ-800:
- DUB ECHO: left, an echo on the mid range only; right, on the high range
  only.
- SWEEP: left, a gate makes the sound tighter; right, a band-pass filter
  that becomes narrower.
- NOISE: white noise is mixed in; left, its filter goes down; right, its
  filter goes up.
- FILTER: left, a low-pass filter; right, a high-pass filter.

If a button had a Mixxx effect from before, it now has the DDJ-800 effect.
Right-click a button to select a different effect.

No Sound Color FX is on when Mix Two starts, as in rekordbox.

When you change the effect, the COLOR knob stays at its position.

The effect lists below the mixer channels are removed. The Sound Color FX
buttons replace them.

The headphone CUE buttons look the same as the MASTER CUE button.

The Sound Color FX buttons are in two rows of two, as on the DDJ-800.

The star buttons next to the COLOR knobs are removed. Use the Sound Color
FX buttons to start and stop the effect.

The TRIM knob is now at the top of each channel, above HI. It has a scale
with -∞ and +12 at its ends. +12 dB is the maximum TRIM in Mix Two. The
DDJ-800 TRIM stops at +9 dB.

The COLOR knobs are now in a SOUND COLOR FX row below the EQ knobs, as on
the DDJ-800. Each COLOR knob shows LOW and HI at its ends. The headphone CUE
buttons are below the COLOR knobs, at the center of each channel.

The HEADPHONES MIXING and LEVEL knobs and the MASTER LEVEL and BOOTH knobs
now have scales. The level knobs show -∞ and 0 at their ends. On MASTER
LEVEL and BOOTH, the last quarter of the scale is red, as on the DDJ-800.

All knob pointers are now white. The TRIM knobs are orange. The COLOR knobs
are light blue.

A CUE button that is off is now dark grey. A CUE button that is on is
orange.

For headphone sound, set a Headphones output in Preferences > Sound
Hardware.

### Playlist folders

You can now put playlists into folders. You can put folders into folders.

Right-click in the playlist tree to use these commands:
- **Create New Folder**
- **Rename Folder**
- **Delete Folder**
- **Move to Folder**

A new playlist goes into the folder that you right-clicked.

Folders stay open when the playlist tree changes.

In the track menu, **Add to Playlist** shows the folders as submenus.

You cannot drag playlists into folders yet. Mix Two sorts the tree
alphabetically, with folders first.

### rekordbox USB devices

The BPM of an imported rekordbox beat grid is now stable. The tempo of synced
decks is also stable.

Tracks that have only a .DAT analysis file now keep their beat grid.

To use these corrections, load the track again from the rekordbox view.

### Screen layout (LateNight skin)

The two decks are now mirror images from the center out.

Each deck has a round deck hub. It shows the BPM, the tempo, the platter,
CUE and PLAY.

SYNC, KEY SYNC and MT are round buttons above the tempo slider.

The mixer has a HEADPHONES section on the left side and a MASTER section on
the right side.

Small labels use a new pixel font, Silkscreen.

The layout is not final. A rekordbox layout will follow.

Each deck can now show as one HUD window, as on a CDJ-3000. The HUD window
shows the deck number, the key, the track title, the artist and the cover
art. It also shows the BPM, the tempo, the tempo range, MT and a MASTER
badge.

Under the track information, the HUD window shows the zoomed waveform, the
track overview and a strip with QUANTIZE and the time. The platter, CUE and
PLAY are on the mixer side of the window.

The waveforms at the top of the screen stay.

To change the HUD window, open the skin settings (SETTINGS):

- Push HUD Deck to show the HUD window or the old deck rows.
- Push Waveform in Deck to show or hide the zoomed waveform in the deck.
- Push Platter to show or hide the platter.

Each side of the screen has one deck window. The left window shows deck 1
or deck 3. The right window shows deck 2 or deck 4.

Each window header shows the two deck numbers of its side. The left order is
3, 1. The right order is 2, 4. The number of the deck in the window is lit.

To change the deck in a window, click the number that is not lit. When the
DDJ-800 is connected, the DECK button on the DDJ-800 changes the deck, and
the numbers do not change on a click.

4 DECKS adds the channels of deck 3 and deck 4 to the mixer, in the order 3,
1, 2, 4. It does not add deck windows. The waveforms at the top follow the
deck windows.

With 4 DECKS on, the master output meter shows in the Master panel, on the
left of the LEVEL and BOOTH knobs.

### Waveform type: Color Saturated

There is a new waveform type, **Color Saturated**. It shows each part of the
track in one colour. Bass parts, such as kicks, show in the base colour.
Middle sounds, such as vocals, show nearer to white. This is the same as the
rekordbox blue waveform. USB exports use the same rule.

To use it, open Preferences > Waveforms. Set **Type** to Color Saturated.
Set the overview **Type** to Color Saturated too.

To change the base colour, click the **Color Saturated base colour** button.
The default colour is blue. The waveforms change immediately.

Color Saturated needs "Use acceleration". Without it, the waveform is empty.

### Waveform type: RGB colours

The RGB waveform in the LateNight skin has new colours. Bass sounds show in
red. Middle sounds show in cyan. High sounds show in blue. The colours are
closer to the colours of the rekordbox RGB waveform.

USB exports use the same colours for the RGB waveform. Before, an exported
track showed bass in blue and middle sounds in red on a CDJ.

The RGB overview now shows the same colours as the waveform. Before, the
overview always showed red, green and blue, also when a skin set other
colours.

### QUANTIZE beat value

Each deck has a QUANTIZE beat value: 1/8, 1/4, 1/2 or 1 beat. The default
is 1. Mix Two keeps the value of each deck when you quit.

When QUANTIZE is on, cues, hot cues and loops move to the nearest point of
that value. Example: with 1/4, a hot cue moves to the nearest quarter beat.

To set QUANTIZE on or off, click the QUANTIZE cell in the HUD window.

To change the beat value, right-click the QUANTIZE cell. The value changes
in this sequence: 1, 1/8, 1/4, 1/2, 1. The cell shows the value.

BEAT JUMP does not use the beat value. It moves by full beats.

When QUANTIZE is off, the beat value has no effect.

### NEXT CUE countdown

The NEXT CUE cell in the HUD window shows the distance to the next hot cue
or memory cue after the play position. The distance shows as bars and
beats, with 4 beats in each bar. Example: 12.3 is 12 bars and 3 beats.

The count changes while the track plays. It also changes when you edit the
beat grid or the cues.

When there is no cue after the play position, the cell shows "-". The cell
also shows "-" when the track has no beat grid.

### A.HOT CUE and AUTO CUE

The HUD window has two load settings: A.HOT CUE and AUTO CUE. A lit cell
shows that the setting is on. The settings apply to all decks.

To set A.HOT CUE or AUTO CUE on or off, right-click its cell. A left-click
has no effect.

When A.HOT CUE is on, the hot cues load with the track. This is the
default. When it is off, the hot cue pads stay empty after a track loads.
The track keeps its hot cues.

To show the hidden hot cues, push a hot cue pad. All hot cues of the track
show, then the pad operates as usual. If the pad has no hot cue, it sets a
new hot cue.

A hot cue clear (SHIFT + pad) on hidden hot cues only shows the hot cues. It
does not delete a hot cue. To delete the hot cue, do the clear again.

When AUTO CUE is on, the track starts at the first sound. When you set it
off, the track starts at the cue point. AUTO CUE uses the "Track load point"
setting in Preferences > Decks. When you change that setting, AUTO CUE
changes too.

When the hot cues are hidden, NEXT CUE does not count to them.

### Phase meter

The HUD window has a phase meter between the zoomed waveform and the
overview. It shows how far the deck is from the beat of the sync leader.

The line in the center is the leader beat. The bar goes left when the deck is
behind the leader. The bar goes right when the deck is ahead.

The bar is blue when the deck is in phase. It is red-orange when the deck is
out of phase. A bar half the width of the meter is half a beat.

The meter shows only the center line when no deck or clock is the sync leader.
It also shows only the center line on the leader deck and on a deck with no
track.

### Lower panel and pads

Under each HUD window, a lower panel shows. It has four tabs: the pad mode,
GRID / PHRASE, MEMORY CUE and INFO. Push a tab to show its page.

The first tab shows the name of the pad mode. Its page has eight pads in two
rows of four.

To change the pad mode, select a mode in the pad mode list on the right.

To show the pads A-H or I-P, push PAGE A-H or I-P. BEAT JUMP and BEAT LOOP
have no PAGE buttons.

These pad modes operate:

- HOT CUE: the pads are the hot cues A-H (1-8) or I-P (9-16). A set hot cue
  shows in its color.
- BEAT JUMP: the pads jump forward by 1, 2, 4, 8, 16, 32, 64 or 128 beats.
- BEAT LOOP: the pads set a loop of 1/4 to 32 beats on or off.
- KEY SHIFT: the pads set the key from -3 to +4 semitones (A-H) or from +5 to
  +12 semitones (I-P). The pad of the current key shift comes on.

PAD FX, SLICER, KEYBOARD, SEQ. CALL and ACT. CENSR do not operate yet. The
page shows "This mode is not available yet."

### GRID / PHRASE and INFO pages

The GRID / PHRASE tab shows GRID EDIT on the left and PHRASE EDIT on the
right.

GRID EDIT has these buttons:

- **<<** and **>>**: move the beat grid 50 ms earlier or later.
- **<** and **>**: move the beat grid 10 ms earlier or later.
- **BPM+** and **BPM-**: make the BPM of the beat grid a little faster or
  slower.
- **x2** and **x1/2**: double or halve the BPM of the beat grid.
- **TAP**: tap the beat. After 4 taps, the beat grid gets the tapped BPM.
- **UNDO**: undo the last change of the beat grid.
- **LOCK**: lock the BPM of the beat grid.
- **1/2 BT**: move the beat grid half a beat.
- **SET**: set the beat grid to the playhead.

To type a BPM, click the BPM value in the middle of the first row.

REDO, CUT, CLEAR and the phrase kind buttons are dim. They do not operate
yet.

The INFO tab shows the genre, album, composer, comment, year and number of
plays of the loaded track. It also shows the star rating. To change the
rating, click a star.

Mix Two keeps the tab, the pad mode and the pad page of each deck when you
start it again.

### MEMORY CUE tab and pads

The MEMORY CUE tab shows a list of the memory cues and memory loops of the
track. The list is in time order. Each row shows the color, the time
(minutes:seconds.milliseconds), the loop length and the comment.

- Click the time or the loop length of a row to go to that memory cue. A
  click on the comment does not move the deck.
- Right-click a row to delete that memory cue. The deck does not move.
- Double-click the comment to change it. Push Enter to keep the new comment.

The buttons on the right of the list are:

- **CALL <** and **CALL >**: go to the previous or the next memory cue.
- **MEMORY**: save the play position as a memory cue. If a loop is on, save
  the loop as a memory loop.

In the MEMORY CUE pad mode, the pads go to memory cues 1-8 in time order. A
pad comes on when its memory cue is set. When the track has more than 8
memory cues, PAGE shows. Push I-P to use memory cues 9-16.

### Known problems

- The jog display on the DDJ-800 does not operate.
- The hot cue pads do not show the time of the hot cue.
- The INFO tab does not show the label, the date added, the file type and the
  bitrate.
- Beat FX do not sound like rekordbox yet.
- On-screen buttons outside the deck hub can operate differently from the
  DDJ-800.

<!-- End of AI-generated document. -->
