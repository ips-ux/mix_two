<!-- AI-generated (Claude Code, 2026-10-27). Review before relying on it. -->

# Pioneer firmware: static analysis (DDJ-800 v1.02, CDJ-3000 v3.22)

> **AI-generated:** Claude wrote this document from static analysis of the two
> firmware files the owner placed in `.resource/` (ground rule in
> [ROADMAP.md §5](ROADMAP.md)). Nothing was run, flashed or sent to a device.
> Findings are behaviour and protocol facts in our own words; no decompiled
> code, tables or images are copied here. Extracted files, Ghidra projects and
> scratch scripts live in `build/re/firmware/` (git-ignored) and must never
> ship. Each claim is marked **Observed** (seen in the file) or **Inferred**
> (deduced; may be wrong).

## 1. Method

| File | What it is | How it was opened |
|---|---|---|
| `DDJ-800_v102.exe` (1,375,736 B) | 32-bit self-extracting stub (`stub32i.exe`, "Pioneer DJ DDJ-800 Updater", AlphaTheta) with an embedded CAB | 7-Zip 19.00 (already installed, `C:\Program Files\7-Zip`) lists and extracts the CAB: `DDJ-800_UP.upd` (2,316,494 B, the firmware), `DDJUpdate.exe` (updater GUI, "DDJ-800Updater" 0.9.0.0, 2021, also knows DDJ-1000), `pcupdate.dll` (Pioneer DJ "HID Com Library", 2018). |
| `DDJ-800_UP.upd` | Firmware container, **not encrypted** | Our own splitter `build/re/firmware/scripts/split_ddj.py` (format in §1.1). |
| `CDJ3Kv322.UPD` (162,484,239 B) | **LUKS1 encrypted volume** | Header parsed by hand (§4). Not opened further. |

Tools: Ghidra 12.1.4 headless (installed at `Z:\Program Files\ghidra_12.1.4_PUBLIC`,
not under `build/re/`), languages `SuperH:BE:32:SH-2A` and `M16C/60:LE:16`;
Python 3.11 run with `-I`, scripts in `build/re/firmware/scripts/`, data in
`build/re/firmware/ddj800/`. Ghidra helper scripts written for this pass:
`MakeFuncsFromPtrs.java` (functions at code pointers), `DumpAll.java`
(decompile everything to text for grepping), `DumpAt.java`, `PanelEntry.java`;
batch files `fw_ghidra2.bat`, `fw_panel.bat`, `fw_at.bat`. Function addresses
below refer to those Ghidra projects (`build/re/firmware/ghidra/ddj800`,
`.../panel`).

### 1.1 DDJ-800 `.upd` container (Observed)

- 32-byte header: model name `DDJ-800` and `Ver.102`, then a 32-bit
  little-endian size.
- Then 7 parts, each: 16-bit checksum, 16-byte name whose last character is a
  flag (`Y` = written by this update, `N` = left alone), 16-byte version, 32-bit
  little-endian length, data. A 16-bit checksum ends the file.

| Part | Version | Flag | Size | Flash offset (Inferred) | Content |
|---|---|---|---|---|---|
| `BOOT` | 1.000 | N | 8 KB | 0x000000 | SH-2A boot loader |
| `UPDATE` | 1.000 | N | 144 KB | 0x002000 | SH-2A update-mode firmware, with its own USB descriptor (same VID/PID) |
| `USB` | 1.020 | Y | 424 KB | 0x026000 | **Main SH-2A firmware** (USB, MIDI, HID, jogs, keys, test mode) |
| `DSP` | 1.020 | Y | 320 KB | 0x090000 | Second SH-2A program: jog LCD graphics, audio-side tasks (`lcd`, `dsp`, `ssi`, `dac`, `mixer` init entries) |
| `LCD` | 1.020 | Y | 1.1 MB | 0x0E0000 | Asset pack for the jog displays: offset table, raw bitmaps, five embedded JPEGs |
| `PANEL` | 1.000 | N | 128 KB | (MCU flash 0xC0000) | Panel MCU application, Renesas **M16C**-family |
| `P_BOOT` | 1.000 | N | 128 KB | (MCU flash 0xE0000) | Panel MCU boot block (fixed vector table at its end) |

Flash offsets follow from the source addresses in each SH-2A image's own
section table: each image starts with an entry address and a stack address,
then groups of three "copy N bytes from flash to RAM" entries plus one
"clear N bytes" entry. Our script turns that table into an ELF with one
segment per RAM region (`sh_img2elf.py`), which Ghidra imports directly.

## 2. DDJ-800 findings (question 2)

### 2.1 Processors (Inferred from memory map and register use)

- **Main MCU: Renesas SH-2A, SH726x family.** Code runs from large on-chip RAM
  at 0x1C000000 (uncached mirror 0x3C000000) and fast RAM at 0xFFF80000; the
  USB 2.0 high-speed controller is at 0xE8010000, the multi-function timer
  (MTU2) at 0xFFFE4000/0xFFFE4380, ports at 0xFFFE38xx, IRQ control at
  0xFFFE0800. These match the SH7264/SH7268/SH7269 manuals; about 2.3 MB of
  RAM in use points to an SH7269-class part. The RTOS is μITRON-style
  (named tasks, mailboxes `*_MBX`, fixed pools `*_MPF`).
- **Second SH-2A** runs the `DSP` image (same image format and RAM layout, so a
  separate chip; both images have an `sh_ini` link task). Its task list:
  idle, cpu, wdt, lcd, dsp, ssi, sh, scan, led, key, gui, mixer, dac, pic, grid,
  hid_data, midi_data, system. It draws the two jog displays from the `LCD`
  asset pack.
- **Panel MCU: Renesas M16C family** (16-bit, little-endian, 3-byte vector
  addresses with an ID byte, flash 0xC0000–0xFFFFF). The main firmware talks to
  it (`m16c_ini`). Its exact job was not traced (no strings); the pads and
  analogue controls are the likely candidates.
- Main firmware init list (`USB` image, table at 0x1C1BB570): idle, cpu, wdt,
  sf, m16c, lcd, ssi, share, sh, scan, key, adc, gui, oled, oled_display, jog,
  usb, test, setup, ctrl, illumi, pic, grid, hid_data, midi_data, system, stby.

### 2.2 Jog wheels: sensor to MIDI (Observed unless marked)

- **Sensor:** each jog is a quadrature encoder read by an MTU2 channel in
  phase-counting mode 1 (`jog_ini` 0x1C192C1C sets mode 4 on channels 1 and 2
  and starts them). That mode counts every edge of both phases (×4). The
  16-bit hardware counters are used **without scaling**, so the MIDI step is
  one encoder edge. With the measured 6480 steps per turn
  ([SCRATCH-ENGINE.md](SCRATCH-ENGINE.md)) the encoder has 1620 lines per
  turn (Inferred).
- **Message builder** (0x1C18ABAA, called per jog from the USB interrupt
  handler 0xFFF86F70 through 0xFFF8790A on the frame-refresh interrupt,
  i.e. **once per 1 ms USB frame**; the 1 kHz rate is Inferred from the
  controller's register layout and matches the capture):
  - step count = counter now − counter at the last message (16-bit wrap);
  - **if the count did not change, no jog message is sent at all.** The
    firmware never reports "stopped" or "0 movement";
  - value = 0x40 + count, the count **clamped to ±63**; the counter at the
    last message is updated to the true position, so steps beyond ±63 in one
    millisecond are lost. ±63 steps/ms is about 17.5× normal speed
    (1× = 3.6 steps/ms);
  - no smoothing, acceleration or speed estimate in the firmware: the value is
    the raw edge count since the previous frame.
- **Which CC** (Inferred for the state names; CC numbers Observed): the firmware
  picks the controller number from a per-deck table base 0x21:
  0x21 = jog not touched (side, "Wheel Pitch Bend" in rekordbox's CSV);
  0x22 = touched with vinyl mode on ("Jog Scratch"); 0x23 = touched with vinyl
  mode off ("Jog Pitch Bend"). The vinyl flag is a per-deck state kept in the
  firmware. Other states send 0x1F and 0x26 (beat-grid adjust/slide editing,
  together with notes 0x79 and 0x0A on the deck's channel, which rekordbox's
  CSV names Grid Adjust / Grid Slide) and 0x29 (while one of two buttons per
  deck, key ids 0x18/0x19 and 0x3A/0x3B, is held; not in rekordbox's CSV).
  Channel = deck (0–3).
- A second, slower jog speed meter exists only for the factory test "JOG
  LOAD" (0x1C192DD4): a 6-sample window of step counts, timing how long a
  spun platter takes to slow from one speed threshold to another. Inferred: a
  production check of platter friction (the effect of the JOG ADJUST
  tension), not used for MIDI.

### 2.3 Jog touch (Observed unless marked)

- Touch arrives as a **digital input** on two port pins with an edge
  interrupt each (handlers 0x1C192D24 / 0x1C192D4A): the handler copies the
  pin level into the key-state array (key ids 0x14 = left, 0x36 = right) and
  clears the interrupt. No software debounce or dead zone in the main MCU.
  The capacitive sensing itself is outside this firmware (Inferred: a touch
  IC or the panel MCU).
- Key changes go through the table-driven key engine (0x1C185EE8 /
  0x1C184B1C) and come out as Note messages; rekordbox's CSV lists touch as
  note 0x36 (0x67 with SHIFT).
- Ordinary buttons use a scan debouncer (0xFFF86988): 3 equal samples to
  press, 18 to release; a slower variant (0xFFF869C0) uses 9 / 104.

### 2.4 JOG ADJUST (light–heavy) knob (Inferred, fairly strong)

**Purely mechanical.** The factory test lists every analogue input it reads
(MIC LEVEL, MIC EQ, TRIM, EQ HI/MID/LOW, COLOR, CH fader, AUX/SAMPLER,
HEADPHONES, MASTER/BOOTH, EFFECT, TEMPO, CROSSFADER, the eight pad pairs per
side); JOG ADJUST is not among them, no MIDI or HID message carries it, and
rekordbox's mapping has no row for it. The only firmware link is the JOG LOAD
test above, which measures the platter's coast-down. Mix Two has nothing to
build for it.

### 2.5 USB descriptors (Observed; `USB` image at file offset 0x47B54)

- Device: USB 2.0, class 0, EP0 64 bytes, VID 0x2B73, PID 0x0029, bcdDevice
  1.00, strings "AlphaTheta Corporation" / "DDJ-800", no serial number, one
  configuration, self-powered, 0 mA.
- **Interface 0: vendor-specific (class 0xFF)**, alt 0 empty; alt 1 has
  isochronous asynchronous **EP 0x01 OUT** and **EP 0x82 IN**, 1024 bytes,
  bInterval 3 (every 4 microframes, 0.5 ms). This confirms F4: the audio is
  not USB Audio Class, so Windows' inbox driver cannot play it; the stream
  format is still unknown. A routine accepts sample rates 44.1, 48 and
  96 kHz (Inferred meaning).
- Interface 1: Audio Control (header only, for MIDI). Interface 2: MIDI
  Streaming, one embedded + one external jack each way, **bulk EP 0x85 IN /
  EP 0x04 OUT, 512 bytes**.
- **Interface 3: HID 1.11**, interrupt **EP 0x06 OUT / EP 0x87 IN, 64 bytes,
  1 ms**. Report descriptor (52 bytes): vendor usage pages 0xFFA0/0xFFA1, one
  64-byte input and one 64-byte output report, **no report IDs**.

### 2.6 HID traffic (Observed layout; meanings Inferred)

- **Input report** (built by 0xFFF87630, only while an internal mode flag is
  2; what sets it is unknown): byte 0 = 0x00, byte 1 = 0x20, byte 2 = flags
  (touch L 0x08, touch R 0x01, jog L moved 0x20 / forward 0x10, jog R moved
  0x04 / forward 0x02), byte 4 = status bits, bytes 5–6 and 7–8 = the
  **absolute 16-bit jog counters** (little-endian), then further 16-bit values
  (tempo-fader-like, read through a calibration table). An absolute counter
  would let a host read the jog without the ±63 clamp.
- **Output reports** (task `usbHidHo` 0x1C1A7778): the high nibble of byte 0
  selects a stream 1–4 (= deck 1–4); each stream has a 16-report ring buffer.
  The per-deck reports are forwarded to the display processor (0x1C1A044C,
  0xFFF87C08); for a report with byte 1 = 0x21 and bit 7 of byte 9 set, bytes
  11–12 are kept per deck. This is the **jog display channel** (ROADMAP U2/U3):
  the left jog shows deck 1 or 3, the right deck 2 or 4. The report contents
  were not decoded; a capture of rekordbox's HID OUT traffic (or tracing the
  `DSP` image's parser) would settle them.
- The updater also uses HID (`pcupdate.dll`); update mode runs the `UPDATE`
  image.

### 2.7 MIDI from the host, SysEx, keep-alive (Observed)

- From the host the firmware accepts only Note On/Off and Control Change
  (2–4 bytes; LEDs, indicators, settings such as jog LCD brightness `BF48` and
  the artwork display `9F4F` in rekordbox's CSV) and one SysEx family:
  `F0 00 40 05 00 00 02 09 …` (00 40 05 = Pioneer DJ), 12–512 bytes.
- That SysEx family is the **rekordbox verification** handshake (tasks
  `rekordboxVerification`, `rekordbox_Active`): the deck answers with its
  maker and model strings ("PioneerDJ", "DDJ800") and values derived from the
  host's request. It proves to rekordbox that genuine hardware is attached.
  **Mix Two must not implement or imitate it.** No keep-alive or init message
  is needed for MIDI: nothing in the MIDI path waits for the host (Inferred).
- A routine (0x1C188E0A) sends the analogue controls as CC pairs, plus
  pad-mode notes, or a reset set of values (CC 0x00/0x20 = 0x40/0x00 on
  channels 1–4, pad-mode notes off) depending on an internal flag; what
  triggers it (connect, mode change, host request) was not traced.
- **DDJ-800 settings utility:** not in these files; its protocol stays
  unknown (candidates: the HID output streams or Note/CC on channel 16).

### 2.8 Factory test mode (Observed strings)

A full test menu exists: every button by name, JOG TOUCH L/R, JOG ROT L/R,
all analogue inputs, LCD colour/brightness patterns, crossfader calibration
(CRF CLB), JOG LOAD, FACTORY RST, VERSION per part. It is entered by a
button combination not traced here. Nothing in Mix Two needs it.

## 3. Question 1: CDJ-3000 scratch and spin rules

**Not answerable from this file**: the CDJ-3000 update is encrypted (§4).
What could be established instead:

| Item | CDJ-3000 | DDJ-800 firmware | rekordbox 6.8.5 (SCRATCH-ENGINE §2.1) |
|---|---|---|---|
| Jog resolution | Unknown (rekordbox's CDJ-3000 HID map uses 9720 per turn, Observed 2026-10-28) | 6480 edges/turn, raw ×4 quadrature (Observed: no scaling) | 6480 for the DDJ-800 (12960 default for others) |
| Stop report | Unknown | **None**: silent when not turning (Observed) | Not needed: rekordbox works per audio buffer; a spin ends after 2 empty buffers, a reversal, or 3 slow buffers (2026-10-28, SCRATCH-ENGINE §2.2) |
| Speed measurement | Unknown | None; one message per 1 ms frame with the raw count, clamp ±63 | **Corrected 2026-10-28:** steps are summed per 1 ms timer tick, queued, and drained per audio buffer; speed = buffer steps ÷ buffer length. Message time stamps are ignored for the DDJ-800. rekordbox reads the DDJ-800 jog from the HID report's absolute counters (§2.6), so the ±63 clamp does not apply to it |
| Touch dead zone | Unknown | None (digital touch, no debounce) | 60 ms / 9/3240 rev / 0.05× |
| Spin end timeout | Unknown | n/a | The ScratchProcessor's periodic tick has **no** spin timeout; the input stage resets the jog 56 ms after the last passed movement (ends disable) — Observed 2026-10-28 |
| Vinyl speed adjust | Unknown | Not in the deck (no firmware trace) | rekordbox setting: linear speed ramp in "speed per ms", default 1.0/ms, table values unknown (SCRATCH-ENGINE §2.3) |

Consequences for the open rekordbox questions. The 2026-10-27 hypotheses
(bursty MIDI read by a 1 ms clock) are **withdrawn**; the rekordbox trace of
2026-10-28 (SCRATCH-ENGINE §2.2; static analysis of rekordbox 6.8.5, REA
evidence `ev_a50335a0d496c5ba5f953d5798f8fd7b2ca00917686e2db178d8041dd87b69de`
for the binary identity and its `timeGetTime` / `timeSetEvent` / `midiInOpen`
imports, code paths from the existing headless Ghidra project) gives:

1. The deck's silence is no problem for rekordbox: its input stage sums the
   steps per 1 ms timer tick (also when zero), queues them and hands the
   ScratchProcessor one count per audio buffer. Two empty buffers in a row
   end a spin. (Observed)
2. The queue delays the jog by about two audio buffers (the first entry of
   each movement lasts two buffers). That delay, plus the two empty buffers,
   explains the ~37 ms backspin run-on, and the delay alone the forward
   spin's 21 ms offset that had been read as a hand-back at ~1.8×. The
   hand-back threshold is the play speed (1 + tempo change), 3 buffers in a
   row. (Mechanism Observed; buffer size during the capture unknown.)
3. The slow release does not spin because the release decision uses the
   buffer processed just before the touch-off, which shows the platter as it
   was one to two buffers earlier, still under the hand; per-buffer counts
   below 2 steps (touched) / 3 steps (untouched) are held back as 0.
   (Mechanism Observed; the specific case Inferred.)

rekordbox 6.8.5 itself contains `djplay::HidMapCDJ3000` (constructor
0x1403A88F0, created for device "CDJ3000" in 0x1403B1CC0): rekordbox can run a
CDJ-3000 over HID. Traced 2026-10-28 (its jog set-up is in 0x1403A96C0, not
the constructor): HID report with a 16-bit absolute jog counter, **9720 steps
per revolution**, each report counted as 3 ms of motion, no dead band, then
the same ScratchProcessor rules (spin count 3, or 5 with a count smoother when
a per-player flag is set). This is rekordbox's view of a CDJ-3000, not the
player's own behaviour, which stays unknown (§4).

## 4. CDJ-3000 `CDJ3Kv322.UPD` (Observed)

- The whole file is a **LUKS version 1** volume: cipher `aes`,
  mode `xts-plain64`, hash `sha256`, 64-byte master key, payload at sector
  4096 (2 MiB), UUID `9424e626-da90-4eb0-a72f-a1a32e8326e0`, one active key
  slot (slot 0, 1,347,367 PBKDF2 iterations), slots 1–7 unused. The payload has
  full entropy (≈7.95 bits/byte) throughout.
- A 15-byte trailer follows the last 512-byte sector: ASCII `XDJ-RR` then
  `3.22` then a 4-byte value (Inferred: a product code field and a checksum).
- The key is not in the file. **Decrypting it would mean circumventing an
  access control, which is outside the owner's permission and our ground
  rules, so it was not attempted** (no key search, no brute force, no
  third-party keys). Questions 1, 3 and 4 for the CDJ-3000 therefore stay
  open.

What would settle them lawfully:
- **Behaviour:** measure a real CDJ-3000 (audio out + jog by hand, or its
  Pro DJ Link / USB traffic) the way the DDJ-800 was measured in rekordbox.
- **rekordbox's view:** static analysis of rekordbox (already allowed) for
  `HidMapCDJ3000` and rekordbox's own Master Tempo, key shift and vinyl
  brake/start, which are what Mix Two must match anyway (program goal:
  rekordbox 6).
- **Library formats (WS-C):** rekordbox 6/7 already write PDB/ANLZ and Library
  Plus (F7, F11); the player image is not needed for them. Public,
  independently written format notes exist in the community (the Deep
  Symmetry project's analysis of PDB/ANLZ and Pro DJ Link); they can be read
  as documentation without downloading tools.

## 5. Questions 3 and 4 (CDJ-3000 deck sound, inventory)

Blocked by the encryption (§4): no library, binary or data file of the player
is visible. Nothing to list.

## 6. Implications for Mix Two

Engine code was not changed in this pass; these are inputs for the next one.

1. **Jog resolution 6480 confirmed at the source.** The CC value is the raw
   ×4 encoder count; `Pioneer-DDJ-800-script.js` `jogResolution` 6480 and the
   6480 in `kPlatterQuietSeconds` (`src/engine/controls/ratecontrol.cpp`) are
   right.
2. **"Quiet" is the only stop signal, confirmed.** The deck never sends a zero
   update, so keep a time-based end. The deck cannot send more often than once
   per 1 ms per jog, so `kPlatterQuietSeconds` (5.6 ms) must stay well above
   1 ms; it does. The rekordbox rule "a zero-movement update ends a spin" can
   never fire with this deck; Mix Two's quiet timer stands in for it.
3. **Value decoding:** a jog message is a signed count −63…+63 (value − 0x40);
   0x40 itself never arrives. Above 63 steps per millisecond (≈17.5×) the deck
   loses steps. `kPlatterGlitchSpeed` (25×) and `kPlatterSpinGlitchSpeed`
   (50×) can therefore never be reached by genuine DDJ-800 data within one
   message; they only guard against batching artefacts. Keep them (they are
   rekordbox's).
4. **Speed window:** the deck adds no smoothing. Mix Two's 20 ms window
   (`kPlatterSpeedWindowSeconds`) is our own; rekordbox (traced 2026-10-28)
   uses a 1 ms-tick queue drained per audio buffer, with a 2/3-step dead
   band. SCRATCH-ENGINE §2.2 lists the exact changes for `ratecontrol.cpp`.
5. **Touch:** a plain on/off line with no dead zone or debounce in the deck, so
   the 60 ms / 9/3240 rev / 0.05× dead zone (`kPlatterDeadZone*`) is the only
   one and stays.
6. **CC choice is the deck's:** the firmware already sends 0x21 (side), 0x22
   (top, vinyl) or 0x23 (top, vinyl off) from its own touch and vinyl state; the
   mapping does not need to infer side vs top. Vinyl mode (SHIFT+SLIP,
   `9017`) is kept in the deck.
7. **JOG ADJUST has no software function**; add nothing for it (PARITY row
   can say "mechanical, nothing to do").
8. **No keep-alive** is needed; never send or answer the verification SysEx.
9. **WS-B:** the audio interface is vendor class with 1024-byte isochronous
   endpoints every 0.5 ms; a Mix Two user-mode driver needs the stream format
   (capture rekordbox's audio traffic). HID is a plain 64-byte in/out pipe with
   no report IDs; MIDI is class-compliant bulk.
10. **Later option:** the HID input report carries absolute jog counters;
    rekordbox 6.8.5 reads the DDJ-800 jogs and touch from exactly this report
    (2026-10-28). If the mode that enables it is found, Mix Two could read the
    jogs without the ±63 clamp, as rekordbox does.

## 7. Unknown, and what would settle it

| Unknown | How to settle |
|---|---|
| CDJ-3000 anything | Not from this file (encrypted). Measure a player. rekordbox's `HidMapCDJ3000` view is traced (§3: 9720 steps/rev, 3 ms per report, same scratch rules). |
| ~~rekordbox backspin run-on (~37 ms), slow-release and 1.8× hand-back~~ | **Answered 2026-10-28** by the rekordbox trace (§3, SCRATCH-ENGINE §2.2): 1 ms-tick queue drained per audio buffer, two-buffer input delay, dead band, two-empty-buffer spin end. Still unknown: rekordbox's audio buffer size during the capture (sets the numbers). |
| HID output report contents (jog display) | Capture rekordbox's HID OUT reports (EP 0x06), or trace the `DSP` image's display parser. |
| What enables the HID input report | rekordbox reads its jogs from it (2026-10-28), so rekordbox must enable it. Capture EP 0x87 IN and EP 0x06 OUT while rekordbox starts (the old capture parser decoded MIDI only). |
| Audio stream format (WS-B) | Capture EP 0x01/0x82 while rekordbox plays a known signal. |
| DDJ-800 settings utility protocol | Not in these files; capture the utility. |
| Panel MCU (M16C) role | Trace `m16c_ini` and the serial link in the `USB` image. |
| Meaning of CC 0x29 and key ids 0x18/0x19, 0x3A/0x3B | Map key ids to buttons via the key table and the test-mode names. |

<!-- End of AI-generated document. -->
