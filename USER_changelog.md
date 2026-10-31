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

The MT (Master Tempo) button is now a round button above the tempo slider.
The lock button near the waveform is removed.

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

Mix Two now uses a scratch sound like rekordbox 6.8.5.

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

Use the audio button in the top bar, next to REC, to select where the sound
goes:
- **HARDWARE AUDIO** (default): the sound goes through the DDJ-800. The
  MASTER LEVEL, BOOTH LEVEL and HEADPHONES knobs on the DDJ-800 set the
  levels, as in rekordbox. They are always correct.
- **PC AUDIO**: the sound goes through the audio interface of the computer.
  The MASTER LEVEL, BOOTH LEVEL and HEADPHONES knobs then set the Mix Two
  levels. The DDJ-800 cannot send the positions of these knobs when Mix Two
  starts. Turn each knob a small amount to send its position.

Mix Two keeps a sound setup for each selection. If you change Preferences >
Sound Hardware, the change applies to the current selection.

If the DDJ-800 is not connected when Mix Two starts, Mix Two uses PC AUDIO.

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
comes on in blue. Push it again to stop the effect. Only one button is on at
a time. The buttons on the screen and on the DDJ-800 operate together.

To change the effect of a Sound Color FX button, right-click the button.
Then select an effect in the list. Mix Two keeps your selection.

The Pioneer effects are not available yet. Until then, the buttons use
Mixxx effects: Filter Echo, Moog Filter, White Noise and Filter. These
effects do not sound like rekordbox.

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

### Known problems

- The jog display on the DDJ-800 does not operate.
- Beat FX do not sound like rekordbox yet.
- On-screen buttons outside the deck hub can operate differently from the
  DDJ-800.

<!-- End of AI-generated document. -->
