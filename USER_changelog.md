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
first time. Mix Two changes the library database to schema version 44.

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

With FLANGER and PHASER, the BEAT buttons now go up to 64 beats. With ENIGMA
JET and MOBIUS, they go from -64 to 64 beats. With a negative value, the pitch
goes down. This is the same as in the DDJ-800 manual.

The BEAT FX ON/OFF button flashes while the Beat FX is on.

The Beat FX can use a tapped tempo. Hold SHIFT and push BEAT ▶ three times or
more in time with the music. The Beat FX then uses that tempo (TAP). Hold
SHIFT and push BEAT ◀ to go back to the tempo of the deck (AUTO). SHIFT + BEAT
◀ also sets the beat value to 1 beat, as before.

Fader start sets the cue point of a paused deck. Hold SHIFT and move the
channel fader fully down, or the crossfader fully to its side. Mix Two sets
the cue point at the current position. When the deck plays, it goes back to
the cue point and stops, as before. This is the same as in the DDJ-800
manual. You must enable fader start on the DDJ-800.

The TEMPO slider of the DDJ-800 now changes the tempo in steps, as rekordbox
does. The step is 0.02 % at the ±6 % range and 0.04 % at ±10 % and ±16 %.
The step is 0.5 % at WIDE. In BEAT LOOP mode, SHIFT + pad does nothing.

In SAMPLER mode, the pads of deck 1 and deck 3 play sampler slots 1 to 8. The
pads of deck 2 and deck 4 play slots 9 to 16. The two sides use one bank.
Push SHIFT + PAGE ◀ or PAGE ▶ to change the bank for both sides together.

In SAMPLER mode, push a pad to play the slot from the start. A pad on an
empty slot does nothing. Push SHIFT + pad to load the selected track into an
empty slot. Push SHIFT + pad on a slot that plays to stop it.

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

Push LOAD two times quickly. The deck gets a copy of the track and position of
the deck that the other side of the DDJ-800 controls at that time. For example,
when the right side controls deck 4, LOAD of deck 1 copies deck 4.

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

The small tempo buttons on the screen now change the tempo in the steps of
rekordbox. Keys and controllers that use these buttons do the same. The step
is 0.02 % at the ±6 % range and 0.04 % at ±10 % and ±16 %. The step is 0.5 %
at WIDE. The DDJ-800 TEMPO slider already uses these steps.

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

After you change a side to its other deck, each knob and fader of that
channel has no effect at first. Move each one to the value that the deck
has on the screen. This prevents a jump in level.

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

When Mix Two starts, it now gets the positions of the knobs and faders from
the DDJ-800. This also occurs when you load the DDJ-800 mapping again. The
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

When Mix Two selects the DDJ-800 as the sound device, it sets the audio
buffer to 256 frames, as rekordbox does. A sound setup that you saved for
HARDWARE keeps its own buffer.

Preferences > Sound Hardware has a Preview volume slider. It is the same
control as the volume slider of the preview deck.

Preferences > Sound Hardware has a Mixer Mode switch. Internal sends the main
mix to the Main output. External sends each deck to its own stereo output
and turns the main mix off. External needs 2 output channels for each deck. If
the sound device has fewer channels, Mix Two shows a message and keeps the
present setup. Internal brings back the outputs that you had before you chose
External. The change starts when you push Apply.

External turns the main mix off, so Main, Headphones and Booth outputs leave
the setup. A message names them. If a deck has no output in External mode,
Mix Two tells you which decks, and Apply does not start the setup. The page
shows Custom when the outputs are neither Internal nor External. If the
device from before External is gone, Internal puts Main on channels 1 and 2
of the device in use. A message says so. Reset to Defaults changes the
main mix when you push Apply.

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

You do not hear the Beat FX of a channel on the CUE of that channel. This is
the same as on a Pioneer mixer. You hear the effect on the main mix.

The MASTER CUE button now sends the main mix to the headphones.

MASTER LEVEL and BOOTH LEVEL on the DDJ-800 now change the main volume and
the booth volume.

For booth sound, set a Booth output in Preferences > Sound Hardware.

The BOOTH knob replaces the balance knob on the screen.

The HEADPHONES section is now at the top of the left mixer column.

Below HEADPHONES are four Sound Color FX buttons: DUB ECHO, SWEEP, NOISE and
FILTER. Push a button to use that effect with the COLOR knobs. The button
comes on in blue. The buttons on the screen and on the DDJ-800 obey the same
rules:
- Push an effect that is off: only that effect is on.
- Push an effect that is on: all effects stop.
- Hold SHIFT and push an effect: only that effect starts or stops. The other
  effects stay as they are. Thus, you can use more than one effect at a
  time. On the screen, use the SHIFT key of the keyboard.

On the DDJ-800, the Sound Color FX buttons stay lit. They do not show which
effects are on. The DDJ-800 shows this only when rekordbox operates it. The
screen shows which effects are on.

When more than one effect is on, the COLOR knob controls all of them.

While the DDJ-800 is connected, a click on a Sound Color FX button on the
screen does not start or stop the effect. Use the CFX buttons on the DDJ-800.

To change the effect of a Sound Color FX button, right-click the button.
Then select an effect in the list. Mix Two keeps your selection. The
right-click operates also while the DDJ-800 is connected.

Each Sound Color FX button shows the name of its effect. A long name shows on
two lines. If the name is too long, it is cut and ends with "...".

Below MASTER is the EFFECTS section for Beat FX:
- S 3 1 2 4 M shows where Beat FX is assigned: the sampler, a channel, or
  the master. The order is that of the DDJ-800 selector. Turn the BEAT FX CH
  SELECT on the DDJ-800. When no DDJ-800 is connected, click a segment.
- BEAT shows the beat fraction. Click ◀ or ▶ to change it, as the BEAT
  buttons on the DDJ-800. Click the value to select from the list.
- Select the effect in the list. The BEAT FX selector on the DDJ-800 selects
  it also. The first 14 effects in the list are in the order of the DDJ-800
  selector.
- While the DDJ-800 is connected, some controls are locked on the screen.
  They have a switch or an ON/OFF button on the DDJ-800. These are the
  S 3 1 2 4 M segments, ENABLED / DISABLED and the deck number in each deck
  window. Use the DDJ-800 to change them. The effect list is not locked.
- Mix Two now has all 14 DDJ-800 BEAT FX, made from the DDJ-800 manual.
  The first seven are LOW CUT ECHO, ECHO, MULTI TAP DELAY, SPIRAL, REVERB,
  TRANS and ENIGMA JET. The other seven are FLANGER, PHASER, PITCH,
  SLIP ROLL, ROLL, MOBIUS (SAW) and MOBIUS (TRI).
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
- Mix Two has two more Beat FX, DELAY and PING PONG. DELAY gives one
  repeat. PING PONG gives repeats that go from left to right. You select
  them on the screen. The BEAT FX SELECT control of the DDJ-800 does not
  have them. Their sound is not yet compared with rekordbox.
- Mix Two has one more Beat FX, VINYL BRAKE. It slows the sound of the
  channel to a stop, as a record does when you stop the turntable. The BEAT
  buttons set the brake time. LEVEL/DEPTH sets how much it slows. You
  select it on the screen. Its sound is not yet compared with rekordbox.
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

Each mixer channel is now one column, as on a usual DJ mixer. From top to
bottom: TRIM, HI, MID, LOW, COLOR, CUE, the channel meter and fader, and the
crossfader assign buttons. 2 decks and 4 DECKS use the same columns.

Each COLOR knob shows LOW and HI at its ends. With 4 DECKS on, the COLOR knobs
also show when no Sound Color FX is on. The SOUND COLOR FX text is removed.
The 2-deck mixer is narrower, so the deck windows are wider.

The HEADPHONES MIXING and LEVEL knobs and the MASTER LEVEL and BOOTH knobs
now have scales. The level knobs show -∞ and 0 at their ends. On MASTER
LEVEL and BOOTH, the last quarter of the scale is red, as on the DDJ-800.

All knob pointers are now white. The TRIM knobs are orange. The COLOR knobs
are light blue.

A CUE button that is off is now dark grey. A CUE button that is on is
orange.

For headphone sound, set a Headphones output in Preferences > Sound
Hardware.

### Missing tracks: Relocate and Auto Relocate

A track is missing when Mix Two cannot find its file. You can now tell Mix Two
where the file is. The track keeps its cues, its playlists and its other data.
Mix Two changes only the place that the library stores. It does not change or
delete a file.

To relocate a track, right-click it, then select **Relocate...**. You can also
select the track in the Missing Tracks list, then push **Relocate**. Choose the
new file. If you select more than one track, Mix Two looks for the other
tracks by file name in the folder of the first file.

To search for the files automatically, go to Preferences > Library. In **Auto
Relocate**, add the folders to search, then push **Apply**. In the Missing
Tracks list, push **Auto Relocate**. Mix Two searches these folders and the
folders below them. A file matches a missing track when it has the same name
and the same size. The library does not store a content hash of the file, so
Mix Two cannot compare one. If more than one file matches, Mix Two takes the
file with the most similar folder names. Mix Two then shows each match as the old path and the new path. Clear the mark of a match to keep that track as it is. Push **Apply** to relocate the marked tracks. Push **Cancel** to change nothing. The file dialog of **Relocate** shows the audio files that Mix Two supports.

### Playlist folders

Click a folder in the playlist tree. The track list shows the tracks of all
playlists in the folder and in the folders below it. A track that is in
more than one playlist shows one time. The list of a folder has its own order.
You cannot drop tracks on it, change the order, or remove tracks from it. You
can still edit the rating, colour, comment and BPM of a track.

Alphabet Search now works in the track list. Type a letter to select the first
track that starts with it in the sorted column. Type more letters within 1
second to narrow the search. This is the same as in rekordbox.

You can now open the track menu with a hold. Go to Preferences > Library and
turn on **Drop-down menu**. Then hold the left mouse button on a track for 1
second. The option is off by default. The right-click menu stays available.

The History list now adds a track only after it played for 60 seconds. This
is the same as in rekordbox. Each deck counts its own play time. A pause does
not add time. A deck that you do not hear adds no time. This is a deck with the
fader down, the crossfader cut, or low gain. A track that you load and eject
quickly does not go into History.

A track that is shorter than 60 seconds goes into History when it played for
90 percent of its length.

The play count and the played mark of a track now change when the track goes
into History. This is after 60 seconds of play, or after 90 percent of a short
track. Before, they changed when the track started.

When you add tracks to a playlist and the playlist holds some of them, Mix Two
asks what to do. Push **Add them** to add all tracks again. Push **Skip
duplicates** to add only the new tracks. Push **Cancel** to add nothing. This
applies to the track menu and to drag and drop.

You can now put playlists into folders. You can put folders into folders.

Right-click in the playlist tree to use these commands:
- **Create New Folder**
- **Rename Folder**
- **Delete Folder**
- **Move to Folder**

A new playlist goes into the folder that you right-clicked.

Folders stay open when the playlist tree changes.

In the track menu, **Add to Playlist** shows the folders as submenus.

Mix Two sorts the tree alphabetically, with folders first.

To move a playlist or a folder, drag it onto a folder. To move it out of all
folders, drag it onto **Playlists**. A folder cannot go into itself.

To rename a playlist or a folder in the tree, select it, then push F2. Type the
new name, then push Enter.

To make a playlist from a folder of your computer, drag the folder from the file
manager onto **Playlists**. The playlist has the name of the folder. It holds
the tracks of the folder and of the folders in it. Mix Two adds new tracks to
the library.
A folder with more than 2000 audio files makes no playlist. Mix Two tells you.
A locked playlist cannot be renamed in the tree.

### Search in one column

The search box has a new button with a small arrow, to the left of the clear
button. Click it to see a list of the columns that you can search: for
example **Artist**, **Title**, **BPM**, **Key** and **Date Added**. Choose a
column. The search box then puts the prefix of the column before your text,
for example `artist:`. If the text has a prefix already, the new prefix
replaces it. A text of more than one word goes between quotation marks, if it
has no operator. The search then starts as when you type. Typing, the search history and the
shortcuts work as before.

### Import Track and Import Folder

The **File** menu has two new items, **Import Track...** and **Import
Folder...**. Select **Import Track...**, then choose one or more sound files.
Select **Import Folder...** to add all sound files of a folder and its
subfolders. This is the same as in rekordbox.

An import adds the tracks to the library. The files stay where they are. Mix
Two does not copy, move or change them. An import adds no watched folder. A
track that the library already has is not added again, also when the path
uses other letters or a link. A track that you hid shows again with its data.
The window stays usable while a long import runs. When the import ends, a message shows the
number of added tracks.

The track menu has a new item, **Import To Collection**. It shows in the iTunes,
rekordbox and Explorer views. Select one or more tracks, open the track menu,
then select **Import To Collection**. Mix Two adds the tracks to the library
with no playlist.

### Export to a USB stick

The message at the end of an export has a new button, **Eject the stick**. Push
it to make the stick safe to remove. Mix Two ejects only the stick of the
export that ended. If a track of the stick is loaded in a deck, Mix Two tells
you and does not eject. Unload the track, then push **Retry**. If the eject
fails, Mix Two tells you. Do not remove the stick until the eject works.

The track menu has a new item, **Export Track**. Select one or more tracks,
open the track menu, then select **Export Track**. The export dialog opens with
the mode **Selected tracks**. Select a stick, then push **Export**. Mix Two
puts only these tracks on the stick, with no playlist.

The export dialog now shows a warning icon next to the text about a stick
that players cannot read. The question before the export also has the warning
icon. You can still continue.

### Artwork for playlists

You can add a picture to a playlist. Right-click the playlist, then select
**Add Artwork...**. Select a JPEG or PNG file. The picture shows at the top of
the menu each time that you right-click the playlist.

Mix Two keeps its own copy of the picture. You can move or delete your file
after you add it. Mix Two makes a very large picture smaller. Your file does
not change.

To change the picture, select **Change Artwork...**. To remove it, select
**Remove Artwork**. When you delete the playlist, the picture goes with it.
When Library Protection is on, you cannot add or remove artwork.

### Bit depth column in the library

The track list has a new column, **Bit Depth**. It shows the bits per sample of
a track, for example 16 or 24. It works for FLAC, WAV and AIFF files. The column
is empty for MP3, AAC and other lossy files, which have no bit depth. You can
sort the list by this column.

The column is hidden at first, also if you already use Mix Two. To show it,
right-click a column header, then select **Bit Depth**. A track that is already
in your library shows no value until Mix Two reads the file for the library
again. Loading a track to a deck does not do this. To read a file again, select
the track, then select **Import From File Tags** in the track menu. A track that
you never read again shows no value.

### Hot cue auto load on CDJ and XDJ

The track info window has a new check box, **Allow to auto load HotCue on
CDJ / XDJ**. It is on for every track, also for the tracks that are already in
your library. This is the same as in rekordbox. Clear the box to stop a CDJ or
XDJ player from loading the hot cues of that track. Push **Apply** or **OK**
to save the setting.

The export to a USB stick writes this setting for each track. Before, the
export always set it to on.

### Library preferences

Library Protection stops mistakes in the library. Go to Preferences > Library,
select **Library Protection**, then push **Apply**. The option is off by
default. With the option on, you cannot edit the track information, remove or
delete tracks, rename or delete playlists, crates and folders, or relocate
tracks. The track menu items for these actions are grey. You can still load
tracks to a deck, and use the play count and the played mark. The History list
and the cues and loops that you set on a deck continue to work.

Click the artwork of a track in the track list to preview the track from the
start. The preview starts when the double-click time has passed. Click the
artwork again during the preview to skip ahead 30 seconds. The preview stops
when the pointer leaves the artwork cell. A double-click still loads the
track to a deck.

Click the overview column of a track to start the preview at that position
in the track. Click the overview again during the preview to move to another
position. Use the stop control of the preview deck to stop it.

The overview column can show the memory cue marks and hot cue marks. Go to
Preferences > Library, select **Display cue markers on preview**, then push
**Apply**. Click near a mark to start the preview at that cue.

The sidebar no longer shows the track count after the name of a playlist or
crate. To show it again, go to Preferences > Library, then select **Show the
track count in sidebar labels**. Restart Mix Two to see the change.

To show the track list in bold, select **Bold** next to the library font. The list changes at once.

The artwork column shows the upper part of each picture. To show the whole
picture, select **Show the whole artwork**, then push **Apply**.

To keep the selected track in the middle of the track list, select **Show the
selected track in the center**, then push **Apply**.

Mix Two clears the played mark of all tracks when you quit. To keep the marks,
clear **Reset played tracks on exit**, then push **Apply**. To clear the marks
now, push **RESET**. The track list and the loaded tracks lose the mark at
once. The play counts stay.

Preferences > Library has a **Folder pattern** for Consolidate. The pattern
says where Consolidate puts a track in the library folder. The default is
`<artist>/<file>`. You can also use `<album>`, `<year>` and `<genre>`. A track
with no artist goes to the folder `Various-Uncategorized`. Consolidate keeps
the file name. No menu starts Consolidate yet.

### Online lookup of track tags

Before the first online lookup of a track, Mix Two shows a notice. The notice
lists the data that Mix Two sends. These are the audio fingerprint, the
duration, the client key and two fixed request options for AcoustID. For
MusicBrainz, they are the ID of each recording and the program identifier. The
program identifier is the program name as it is sent (Mixxx), the version and
the web address. For Cover Art Archive, it is the ID of the release.

Mix Two sends no data until you push **Accept**. Push **Cancel** to send
nothing. Mix Two shows the notice again at the next lookup. After you push
**Accept**, the notice does not show again. To see the notice again, go to
Preferences > Library, then push **Show the online lookup notice again**.

### Click on the waveform for PLAY and CUE

To play and cue with the mouse, go to Preferences > Waveforms, then select
**Click on the waveform for PLAY and CUE**. The setting is off by default.
When it is on, a left click on the waveform plays or pauses the deck. A right
click works as the CUE button. A click is one push and release of the mouse
button with a movement of 4 pixels or less.

These mouse actions change when the setting is on. A left mouse push no longer
starts a scratch at once. The scratch starts when you drag the mouse more
than 4 pixels. A right drag no longer bends the pitch. A right click on a hot
cue marker of a stopped deck still opens the cue menu. The mouse wheel zoom
does not change. Mix Two releases a held CUE when the waveform hides or the
window loses focus.

### Export Loop As WAV

The track menu of a deck has a new item, **Export Loop As WAV**. The item is
off when the deck has no loop. Make a loop, open the track menu of the deck,
then select **Export Loop As WAV**. Type a file name. If the file exists,
Mix Two asks before it replaces the file.

Mix Two writes the part of the track from the loop in point to the loop out
point. The WAV file has 48 kHz, 16 bit and two channels. The sound has the
speed and the pitch of the track file. The tempo slider, MT and the effects
have no effect on it. Mix Two does not change the track file.

A track file at 48 kHz is copied as it is. For another rate, Mix Two changes
the rate with the FFmpeg converter. A loop of more than 6 hours is too long
for a WAV file, and Mix Two shows a message.

The deck keeps playing while Mix Two writes the file. When the file is ready,
Mix Two adds it to the library. If an error occurs, a message shows and Mix
Two leaves no half file.

### Linux kernel notice for the DDJ-800

On Linux, Mix Two checks the kernel version at the start. If a DDJ-800 is
connected and the kernel is older than 6.4, a notice shows. The DDJ-800 can
have audio problems with an old kernel. Update the Linux kernel to version
6.4 or newer.

The notice shows one time for each profile. It does not stop the start of Mix
Two. Windows is not changed.

### Real-time priority on Linux

The .deb package of Mix Two has a limits file for the group `audio`. The
file is `/etc/security/limits.d/mixtwo-audio.conf`. It allows a real-time
priority of 82 and unlimited locked memory. Mix Two needs this for stable
sound.

Make sure that your user is in the group `audio`. Then log out and log in
again. If Mix Two cannot use real-time priority, a notice shows at the start.
The notice shows one time for each profile. It does not stop the start of Mix
Two. Windows is not changed.

### Track files on removable drives

Mix Two does not move a track file to the trash when the file is on a removable
drive, such as a USB stick. It does the same for a file on a network drive.
These drives have no trash that keeps the file. The file stays on the drive. The track stays in the
library. The list of files that Mix Two could not move shows each file. It also
gives the reason. Mix Two also checks the target of a link or a substitute drive.

### rekordbox USB devices

Mix Two now finds the rekordbox library of a USB stick when the folder names
`PIONEER`, `rekordbox` and `USBANLZ` have lower-case or mixed-case letters.
The analysis files of the tracks are also found.

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

The master output meter shows in the Master panel, on the left of the LEVEL
and BOOTH knobs. This is the same with 2 decks and with 4 DECKS.

The mixer has a new Mic module. The MIC/AUX panel and the MIC/AUX button are
removed. The talk-over ducking settings are now in Preferences, on the Sound
Hardware page.

In the Mic module, the level meter of the selected microphone is a horizontal
bar below the Level knob. The knob and CUE are at the center of the module.

If Mix Two cannot open a microphone input, it sets the microphone off. The
audio output stays active, and a message shows.

The INFO tab of the HUD window now shows FILE TYPE, BIT RATE and DATE ADDED
of the loaded track.

The beat count in the strip of the HUD window now has two modes. Click the
cell to change the mode. NEXT CUE shows the bars and beats to the next cue.
POSITION shows the bars and beats from the first beat of the beat grid to
the play position. Mix Two saves the mode.

The beat count now goes up to the next beat when the position is within 0.01
beat of it. Before, a count of 8 beats could show as 1.3 and not as 2.0.

In slip mode, a yellow line now shows the playback position on the overview
and on the zoomed waveform. On the overview, a white line also shows the
background position. The background position is where the track would be
without the slip.

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

<!-- AI-generated (Claude Code, 2026-11-13). Start of AI-generated text. -->
### Waveform bands and USB export

The waveform analysis now divides the sound into bands at 150 Hz and
2000 Hz. Before, the limits were 600 Hz and 4000 Hz. The bass, middle and
high bands are now nearer to the rekordbox 3-band waveform.

All waveform types that show the bands use the new limits: 3Band, RGB,
Filtered, Stacked, HSV and Color Saturated, and their overviews. Their
colours change a little. The on-screen colour rules do not change.

USB exports now write the bass, middle and high bands in the same order as
rekordbox. This applies to the 3-band waveform, its small overview, and the
band values of the small colour overview. Before, the order was different
from rekordbox.

The exported RGB waveform uses new colour values. Its colours are now nearer
to rekordbox than before. They are not the same as the LateNight skin
colours.

The exported blue waveform also uses new values. In the small blue overview,
the white parts are now nearer to rekordbox. In the large blue waveform, the
white parts are a little further from rekordbox than before.

The height of the exported blue and RGB waveforms is a little further from
rekordbox than before.

NOTE: These export changes are not examined on a CDJ yet.
<!-- End of AI-generated text. -->

<!-- AI-generated (Claude Code, 2026-11-13). Start of AI-generated text. -->
### Waveform type: 3Band

There is a new waveform type, **3Band**. It shows the bass, middle and high
sounds as three layers, as the rekordbox 3-band waveform does. Bass sounds
show in blue, middle sounds in orange and high sounds in white. The high
layer is in front, the bass layer is at the back. Each layer is brightest
at the center line and has a bright edge.

3Band is now the default waveform type. When you start Mix Two for the
first time after this change, Mix Two sets the waveform type to 3Band one
time. To use a different type, open Preferences > Waveforms and set
**Type**. Mix Two keeps your selection.

3Band needs "Use acceleration".

The EQ knobs and the band gains in Preferences > Waveforms change the height
of the layers, as with the other types.

### Deeper waveform zoom

You can now zoom in on the waveform more than before. At the deepest zoom,
one pixel shows approximately half a millisecond of the track. This helps
when you put a cue or the beat grid exactly on a kick.

The waveform zoom buttons now change the zoom by the same proportion at each
step. Controller mappings that use the zoom up and zoom down controls, for
example the DDJ-800 mapping, do this too. Each step makes the waveform 1.25
times larger or smaller. Before, each step changed the zoom
value by 1, from 1 to 10. Thus, a step near the deepest zoom was large, and
a step near the widest zoom was small.

These items do not change:

- The default zoom.
- The zoom step of the mouse wheel.
- The list of default zoom values in Preferences > Waveforms (1 to 10).
- The shape of the overview waveform.

### Waveforms are analysed again

To show the deeper zoom, the waveform analysis keeps 5 times more detail
than before. Thus, Mix Two analyses each track again when you load it for
the first time after this change.

Until the new analysis is complete, the deck shows the waveform from the
earlier analysis. Then the deck shows the new waveform. On the test
computer, this took approximately 1 second for a track of 3 minutes. After
that, Mix Two does not analyse the track again.
<!-- End of AI-generated text. -->

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

### Preferences window

The Preferences window has a new design. The window is always dark. Your
settings do not change.

The page list is at the left, in groups. To hide the page names, click the
button at the top of the list. Click it again to show the names. Mix Two keeps
this selection.

To find a setting:

1. Push Ctrl+F.
2. Type a word from the setting, for example "asio".
3. Push Enter. The page opens at the setting.

Push Esc to clear the search.

The rows of the controllers show only when the Controllers page is open.

The tile below the list shows the controller that is enabled. Click the tile to
open its page.

On/off options now show as switches.

The Preferences window has no title bar and has round corners. It stays in
front of Mix Two and does not show in the taskbar. While it is open, you
cannot use the main window.

- To move the window, drag the page title or the border area.
- To change the size, drag an edge or a corner.
- To close the window, click **Close**, click the X at the top right, or push
  Esc.
- To keep the changes, click **Apply and close**. **Apply** keeps the changes
  and keeps the window open.

If you changed a setting and did not apply it, Mix Two asks before the window
closes. Click **Apply and close**, **Close without applying** or **Keep
editing**.

### Mix Two logo

The Mix Two logo replaces the Mixxx logo. You see it on the start screen, in
the toolbar of the skins and in the About window. You also see it in the
Windows taskbar and on the app icon. The icon shows the Mix Two creature on
a dark tile.

### Contributions page

Mix Two is a fork of Mixxx. The new **Contributions** page, in the About group
of Preferences, gives credit to the Mixxx team and to all its contributors.
It shows the full list of names.

To support the Mixxx team, open Preferences > Contributions, then click
**Donate to Mixxx**. The Mixxx donations page opens in your web browser.

### Known problems

- The jog display on the DDJ-800 does not operate.
- The hot cue pads do not show the time of the hot cue.
- The INFO tab does not show the label, the date added, the file type and the
  bitrate.
- Beat FX do not sound like rekordbox yet.
- On-screen buttons outside the deck hub can operate differently from the
  DDJ-800.

<!-- End of AI-generated document. -->
