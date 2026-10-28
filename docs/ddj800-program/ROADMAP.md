<!-- AI-generated (Claude Code, 2026-10-26). Review before relying on it. -->

# Mix Two × DDJ-800: Reconnaissance and Roadmap

> **AI-generated:** Claude wrote this document from static inspection of the
> local rekordbox 5.8.7, 6.8.5 and 7.2.19 installs and the Mix Two source tree.
> No device traffic was captured and nothing was executed. Each claim is marked
> **Observed**, **Inferred** or **Unknown**.

## 0. Program goal (set 2026-10-26)
The owner's direction after the first hardware test: Mix Two should recreate
the **rekordbox 6 experience completely and accurately, top to bottom**.
Mixxx is the starting point and rekordbox 6 is the reference finish point.
Where rekordbox's design is poor, limiting or user-hostile, Mix Two fixes it
rather than copying it. In practice this means a full rework of the app, not
only a controller mapping:
- **Behavioural fidelity first:** every control, workflow and default should do
  what a rekordbox 6 user expects (e.g. SYNC is press-to-toggle, not Mixxx's
  tap/hold). Deviations must be deliberate and recorded in the parity matrix
  (WS-E) with a reason.
- **Improvements are explicit:** each "fix the rekordbox limitation" item is
  recorded as an *Improve* entry, so fidelity and improvement stay separable.
- **The sound must match Pioneer above all else** (owner, 2026-10-26):
  Beat FX, the other rekordbox effects and the deck sound (scratch, Master
  Tempo) are matched to rekordbox by measurement, and Mixxx's own versions
  (e.g. its turntable/scratch sound) are replaced. Plan and boundary:
  [SOUND.md](SOUND.md) (WS-F).
- **The UI design must match rekordbox 6 exactly** (owner, 2026-10-26): not
  only the features but the layout, wording, menus and defaults. Use
  rekordbox's own UI strings (`locale/*.lang`). Any remaining visual or
  interaction difference is listed as a gap in PARITY.md. Visuals are
  recreated; Pioneer image assets are never copied.

## 1. Finding ledger

| # | Finding | Type | Source / Evidence |
|---|---------|------|-------------------|
| F1 | DDJ-800 is USB `VID_2B73&PID_0029`, composite. **MI_00** = audio (Pioneer driver), **MI_01** = "DDJ-800" (MIDI, inbox driver), **MI_03** = HID vendor-defined. | Observed | Windows PnP history on this PC; INF `USB\VID_2B73&PID_0029&MI_00` |
| F2 | The audio driver `DDJ-800Audio64.sys` (50 KB, v1.000, 2018-12-17) is a **custom WDM PortCls miniport talking raw USBD**: imports only `ntoskrnl`, `portcls.sys`, `USBD.SYS`. It does not layer on Windows `usbaudio.sys`. | Observed | REA `ev_aac1daf57a57362d1da9e6472397d423218d06d7550dcf34f951615b93a459a4`, sha256 `36672a7c…f773b` |
| F3 | ASIO (`DDJ-800_ASIO64.dll`) reaches the kernel driver through `SetupDiEnumDeviceInterfaces` + `CreateFile` + `DeviceIoControl` (private IOCTL channel). | Observed (imports/strings); IOCTL codes Unknown | strings in the extracted driver package |
| F4 | The MI_00 audio interface is probably a vendor-specific class (Pioneer would otherwise use the inbox UAC2 driver). | Inferred | F2; the USB descriptor has not been dumped yet |
| F5 | rekordbox has **native, device-specific DDJ-800 code** beyond the CSV: `djplay::MidiMapDDJ800`, `DDJ800PreferencePage`, `JogDisplay` (`JogDispTimeMode`, `JogDispBrightness`, `OnJogIndicator`…), plus `HidD_*` imports. | Observed | strings in rekordbox 6.8.5 `rekordbox.exe` |
| F6 | `MidiMappings/DDJ-800.midi.csv` (532 lines) is identical in 6.8.5 and 7.2.19. The jog rows (`B022`, `B023`, `B01F`, `9036`…) are commented out, so jog handling is native (F5). Control counts: 138 Button, 256 Pad, 16 HiRes knob/slider, 24 Indicator, 10 Value. | Observed | CSV in both installs |
| F7 | rekordbox 7 **Library Plus** = `exportLibrary.db`, SQLCipher-encrypted (`sqlcipher_export` string), with a converter (`ConvertDeviceLibraryPlusDlgMain`, `DeviceLibraryPlusDlgManager::toSqliteDbConversion`). Legacy export = `PIONEER/USBANLZ/…/ANLZ0000.DAT/.EXT`. | Observed | strings in rekordbox 7.2.19 `rekordbox.exe` |
| F8 | rekordbox 7 analysis is partly ML: `models/detect_beat/model7` and `model8` (Keras SavedModel, TF via `tensorflow_cc.dll`), `detect_cue_model/*.onnx` (auto-cue), `embed_model/*.onnx` (genre, energy, UMAP embedding → likely "Related Tracks"). An `AnalyzeVersion` field exists. A string warns that grids and cues analysed before 7.2.9 may be misaligned. | Observed (files and strings); model→feature mapping Inferred | rb7 `models/`, `rekordbox.exe` strings |
| F9 | rekordbox 6.8.5 ships only `spleeter_model` (stems). rekordbox 5.8.7 ships no models, so its beat, key and waveform analysis is classic DSP inside `rekordbox.exe`. | Observed / Inferred | model directories |
| F10 | `clatc.dll` (rb7) exports `OEMCrypto_*`, so it is streaming DRM. **Out of scope; do not touch.** | Observed | REA `ev_dd028e3382d09dd87faf42a926e8109f0cf84c61743d9a71a1ab02566c373574` |
| F11 | Mix Two already **reads** legacy USB exports (`src/library/rekordbox/rekordboxfeature.cpp` with the Kaitai `rekordbox_pdb.ksy` and `rekordbox_anlz.ksy`, including PCO2, PWV5, PWV7 and PSSI). It has **no** writer for PDB, ANLZ or Library Plus. The only exporter is Engine Prime. | Observed | `lib/rekordbox-metadata/`, `src/library/export/` |
| F12 | Mix Two has Pioneer mappings for DDJ-200/400/FLX4/SB/SB2/SB3/SX but **not the DDJ-800**. | Observed | `res/controllers/` |
| F13 | Mix Two was not a git repository until 2026-10-27. A repo now exists in `mix_two/` only (no parent folder is a repo); `/build*`, `/buildenv` and `/.resource` (Pioneer firmware, images) are ignored, files are stored as-is (`core.autocrlf false`). Remotes: `origin` = `github.com/ips-ux/mix_two` (a fork of Mixxx), `upstream` = `github.com/mixxxdj/mixxx`. First commit `59ace09228` on 2026-10-27; agents commit and push to `origin` (see `AGENTS.md`). | Observed | `git status`, `git remote -v` |
| F14 | rekordbox 6's own DDJ-800 mapping semantics differ from Mixxx defaults: SYNC `9058` is "Sync On/Off" (press toggles; option `Blink=600`); MASTER LEVEL `B608` is "No function assigned" (the deck applies it in hardware to its own master output); BROWSE press `9641` is "Library Forward (Folder Open)" and never loads; loading is only via LOAD `9646`–`9649` (double press = Instant Double). | Observed | `MidiMappings/DDJ-800.midi.csv` in 6.8.5 |
| F15 | On this PC, Windows sometimes does not list the DDJ-800 MIDI ports at Mix Two start-up, or lists them but the first open fails. A later replug or port change makes them usable. | Observed | `mixxx.log` from the 2026-10-26 hardware tests |
| F16 | Pioneer's ASIO driver `ddj-800_asio64.dll` crashes (access violation in its own code, called from PortAudio's ASIO enumeration) after PortAudio has started and stopped it repeatedly in one process with the deck connected. Mix Two restarts PortAudio when sound devices are re-queried, so the app is exposed too. Tests now set `MIXXX_DISABLE_PORTAUDIO`. It also crashes Mix Two **at launch while rekordbox is running** (Windows error report: fault in `ddj-800_asio64.dll`, 2026-10-27); Mix Two now checks for rekordbox before loading audio drivers and offers Retry / Start Anyway / Quit. Another reason for WS-B. | Observed | Stack trace from `mixxx-test`, 2026-10-26 |
| F17 | rekordbox 6 scratching is speed-based with position conservation and band-limited rendering: platter ticks become a speed (1.0 at 33⅓/45 RPM) with a touch dead zone of 9/3240 rev, a ±8× clamp whose excess carries over, and position feedback from the player; audio is rendered (`rbxfrm::f2dsp::TimeStretchScratch`) by 2× oversampling with a 43-tap half-band filter (Hann-47 windowed sinc) and then 4-point cubic Lagrange interpolation, not linear interpolation. Full notes: [SCRATCH-ENGINE.md](SCRATCH-ENGINE.md). | Static RE | Ghidra 12.1.4 on rekordbox 6.8.5 `rekordbox.exe`, `DjController::ScratchProcessor` and `TimeStretchScratch` vftables; polynomial constants read from the running process; 2026-10-27 |
| F18 | DDJ-800 firmware v1.02 (`DDJ-800_v102.exe` → CAB → `DDJ-800_UP.upd`, unencrypted) holds 7 parts: SH-2A boot, update and **main** firmware, a second SH-2A program (jog LCDs, audio-side tasks), a jog-display asset pack, and a Renesas **M16C** panel MCU (app + boot). Main MCU: Renesas SH-2A (SH726x family, μITRON-style RTOS). Details: [FIRMWARE.md](FIRMWARE.md). | Static RE (Observed; chip family Inferred) | Container parsed by our splitter; Ghidra 12.1.4 on the SH-2A and M16C images, 2026-10-27 |
| F19 | DDJ-800 jogs: quadrature encoders on SH-2A timer channels in ×4 counting, **no scaling** (6480 per turn = 1620 lines, Inferred). One message per jog per 1 ms USB frame, value 0x40 + steps since the last message, **clamped to ±63** (extra steps lost, ≈17.5×); **nothing is sent when the count did not change** (no "stopped" message). CC 0x21 untouched / 0x22 touched + vinyl / 0x23 touched, vinyl off is chosen by the firmware. | Static RE (Observed; 1 kHz rate Inferred) | `USB` image: jog init 0x1C192C1C, message builder 0x1C18ABAA, USB interrupt 0xFFF86F70 |
| F20 | DDJ-800 jog touch is a digital line with an edge interrupt, copied straight into the key state (no debounce, no dead zone in the deck); sent as a note (CSV 0x36). **JOG ADJUST is purely mechanical**: no analogue input, MIDI or HID value for it (only a factory "JOG LOAD" coast-down test). | Static RE (touch Observed; JOG ADJUST Inferred) | handlers 0x1C192D24/0x1C192D4A; factory test strings; CSV |
| F21 | DDJ-800 USB descriptor: IF0 **vendor-specific audio** (alt 1: iso EP 0x01 OUT / 0x82 IN, 1024 B, every 0.5 ms), IF1/IF2 class MIDI (bulk EP 0x85 IN / 0x04 OUT, 512 B), IF3 **HID** (interrupt EP 0x06 OUT / 0x87 IN, 64 B, 1 ms; vendor pages 0xFFA0/0xFFA1, 64-byte reports, no report IDs). Self-powered, strings "AlphaTheta Corporation" / "DDJ-800", no serial. Confirms F4. | Static RE (Observed) | descriptor bytes in the `USB` image |
| F22 | DDJ-800 HID: OUT report byte 0 high nibble = deck 1–4; per-deck rings forwarded to the display processor (the **jog display channel**, U2/U3; contents not decoded). IN report (only in an internal mode) carries touch/move flags and the **absolute 16-bit jog counters**. | Static RE (layout Observed; meaning Inferred) | tasks `usbHidHo` 0x1C1A7778, 0x1C1A044C; IN builder 0xFFF87630 |
| F23 | From the host the deck accepts only Note/CC and the SysEx family `F0 00 40 05 00 00 02 09 …`, which is the **rekordbox verification handshake** (deck replies with "PioneerDJ"/"DDJ800" and request-derived values). No keep-alive is needed for MIDI. Mix Two must not implement the handshake. | Static RE (Observed; "no keep-alive" Inferred) | 0x1C1827DC, 0x1C181DF0, tasks `rekordboxVerification` / `rekordbox_Active` |
| F24 | `CDJ3Kv322.UPD` is a **LUKS1 volume** (aes-xts-plain64, sha256, one key slot), trailer `XDJ-RR` `3.22`. The key is not in the file; decrypting was **not attempted** (access control). CDJ-3000 scratch, Master Tempo and library questions stay open; rekordbox 6.8.5's `HidMapCDJ3000` is the reachable alternative. | Static (Observed) | LUKS header; rbx685 0x1403B1CC0 / 0x1403A88F0 |
| F25 | rekordbox 6.8.5's ScratchProcessor periodic tick has **no spin timeout** (only bend 56 ms, disable exit, touch-on time), and jog messages are timed with a 1 ms clock. With the DDJ-800 never sending a zero update, rekordbox's documented "zero movement ends the spin" cannot fire; the backspin run-on and the 1.8× hand-back need another explanation (hypothesis: bursty MIDI arrival read by a 1 ms clock). **Hypothesis withdrawn 2026-10-28: see F26–F28.** | Static RE (tick Observed; hypothesis Inferred) | existing rbx685 dump, ScratchProcessor slot 22; FIRMWARE.md §3 |
| F26 | rekordbox 6.8.5 runs the DDJ-800 through the **DDJ-1000 HID map** (`djplay::HidMapDDJ1000`, special-cased by name: 6480 steps, processor type 1) and reads its jogs and touch from the **HID input report** (touch/move/forward flags, absolute 16-bit jog counters; the layout in F22). The jog chain is `AudioCallbackSumTimerProcessor` → `ScratchProcessor`; message time stamps are ignored. How rekordbox enables the report is unknown. | Static RE (Observed; "rekordbox enables the report" Inferred) | rbx685: map 0x1403D9A10, HID parse 0x1403DAC40, chain factory 0x1414DC580; REA `ev_a50335a0d496c5ba5f953d5798f8fd7b2ca00917686e2db178d8041dd87b69de` (binary identity, WINMM imports) |
| F27 | rekordbox's DDJ-800 jog input stage: steps are summed per **1 ms multimedia-timer tick** (all decks), queued with the tick's duration (1 ms wall clock, clamped to [1 ms, 2 × buffer ms rounded up]; 0.5 ms on a direction change, zero = forward; the first entry after inactivity lasts 2 buffers), and drained per **audio buffer**; a dead band holds back fewer than **2 steps (touched) / 3 (untouched)** per buffer; touch-on is applied before and touch-off after the buffer; **56 ms** without passed steps resets the jog (ends disable). The ScratchProcessor runs once per buffer with (steps, buffer length). | Static RE (Observed) | rbx685: tick 0x1404249C0, audio side 0x140424CB0, timer start 0x141759DA0 (period 1), tick fan-out 0x141363510; SCRATCH-ENGINE §2.2 |
| F28 | rekordbox's spin rules are per buffer: an empty buffer repeats the last speed once, a second empty buffer ends the spin; the hand-back compares the buffer speed with **1 + tempo change** for 3 buffers (no check when platter and deck turn opposite ways); the release decision uses the buffer before the touch-off. With the ~2-buffer input delay this explains the backspin run-on (~37 ms), the immediate play after a slow release, and the forward spin's 21 ms offset (no early hand-back at ~1.8×). | Static RE (rules Observed; the three explanations Inferred; rekordbox's buffer size during the capture unknown) | rbx685: ScratchProcessor 0x140426910, 0x1404276A0, 0x140427B80, 0x140427060; StretchBehavior tempo field; SCRATCH-ENGINE §2.1–2.2 |
| F29 | rekordbox's `HidMapCDJ3000` jog: HID 16-bit absolute counter, **9720 steps/rev**, `AudioCallbackProcessor` with a fixed 3 ms per report and no dead band, same ScratchProcessor rules (spin count 3, or 5 with a count smoother under a per-player flag). rekordbox's view only; the player firmware stays unknown (F24). | Static RE (Observed) | rbx685: jog set-up 0x1403A96C0, factory 0x1414DC980, HID parse in `HidMapCDJ3000` |
| F30 | rekordbox vinyl speed adjust (touch/brake, release/start) is a linear speed ramp per audio block in "speed per ms" (`rbxfrm::VinylSpeedAdjustor`, default 1.0/ms, table chosen by preference; table values unknown). Master Tempo is rekordbox's own `f2dsp::TimeStretch` code (no third-party library name found); algorithm family not identified. | Static RE (ramp Observed; table and algorithm Unknown) | rbx685: 0x1419B3D50, `StretchBehavior::SpeedAdjustTask`; strings |

## 2. Workstreams

### WS-A: DDJ-800 controller mapping (plug-and-play MIDI)
- Translate F6 into `res/controllers/Pioneer-DDJ-800.midi.xml` and `-script.js`, using the existing DDJ-400/FLX4 scripts as structural templates.
- **Gap:** jog wheels, the on-jog display/indicator, the DDJ800 preference page and HID MI_03 traffic (F5). These need **runtime capture with the deck connected**: rekordbox running, plus a MIDI monitor and USBPcap/Wireshark on the DDJ-800. REA `inspect_packet_capture` can then decode the captures.
- Done when: every CSV function maps, jog scratch/bend/search work, LEDs and the jog display respond, and a human tests it on the real deck.
- **Status (2026-10-26):** the static pass is written: `res/controllers/Pioneer-DDJ-800.midi.xml` and `Pioneer-DDJ-800-script.js`. Every input is registered in the script with `midi.makeInputHandler`. A mock-engine harness confirms that every CSV input is bound, apart from the intentional gaps (mic knobs, which the deck handles in hardware; the controller-settings values; Related Tracks; FX tap). Firing every handler raised no errors. The mapping is **not tested on hardware**.
- **First hardware test (2026-10-26):** most controls work, and hotplug enabled the deck on replug. Fixed after the test: SYNC is now press-to-toggle; MASTER LEVEL drives the main gain when the mix plays through another interface (mapping setting `masterLevelSoftware`, default on; F14); BROWSE press no longer loads into deck 1 from the track list (F14); a controller that fails to open is retried for 30 s (F15). Open: the owner expects a rekordbox-style "which deck?" choice when pressing BROWSE on a track (U7).
- **Plug-and-play (implemented, compiled, unit-tested, works on hardware):** MIDI mappings can now declare devices with `<product protocol="midi" device_name="…"/>` (or `device_name_regex`). Port names are normalized per platform by `MidiUtils::canonicalDeviceName`. `MidiController::matchMapping` uses this declaration. At startup, `ControllerManager` enables any never-configured controller with the first matching mapping, preferring user mappings over system ones. The `[Controller] auto_enable_matching_mappings` setting (default on) turns this off. Tests are in `src/test/midiproductmatch_test.cpp`.
- **MIDI hotplug (implemented, compiled, unit-tested, works on hardware):** once a second, `ControllerManager` compares the operating system's live MIDI port list with the list PortMidi started with (`src/controllers/midi/midiportwatcher.cpp`). The sources are WinMM on Windows, CoreMIDI on macOS and `/proc/asound/seq/clients` on Linux. When a change has held for two checks, it restarts PortMidi, replaces every PortMidi controller, and sets up the new ones, so auto-enable applies to a newly connected DDJ-800. Replaced controllers are deleted only after the main thread has rebuilt the preferences pages and QML proxies, and stale pointers are rejected by `slotApplyMapping`/`openController`. The setting `[Controller] midi_hotplug_enabled` (default on) turns this off. Trade-offs:
  - Any MIDI device change briefly reopens all open MIDI controllers, so their scripts re-initialise.
  - HID and bulk controllers are not hotplugged: their enumerators were never written to rescan.
  - The DDJ-800 **audio** interface still has to be selected in the sound hardware preferences.
  - Rescans wait while MIDI learning is active.

### WS-B: Audio, using the existing driver first and then a unified user-mode driver
1. **Now:** Mix Two already works through the Pioneer ASIO driver (PortAudio ASIO), so no work is needed to get started.
2. **Investigate:** dump the MI_00 descriptors (USBView / `usbtreeview`) to settle F4. If MI_00 is vendor class, a user-mode replacement means **WinUSB + isochronous streaming inside Mix Two**: bind WinUSB to MI_00 through an INF, then reimplement sample format, packet sizing, clock feedback and control requests learned from USBPcap captures and from static RE of `DDJ-800Audio64.sys` (it is small, about 50 KB).
3. The "unified driver" is a **Mix Two-owned WinUSB audio + MIDI + HID backend** for VID 2B73. Pioneer's installer, ASIO DLL and config tool are then unnecessary. The deck firmware is untouched, per the stated scope.
- Risks: isochronous timing in user mode (latency), and WinUSB driver signing for distribution. The inbox WinUSB INF route avoids writing a kernel driver.

### WS-C: USB library export (Legacy and/or Library Plus)
- **Legacy:** write `export.pdb` and `ANLZ0000.DAT/.EXT/.2EX`. The Kaitai specs in `lib/rekordbox-metadata` are read-only, so a serializer is needed. Validate by round-tripping through Mix Two's own reader, then the deck or rekordbox.
- **Library Plus:** write `exportLibrary.db` with SQLCipher. Schema and key derivation still have to be extracted. Mixxx bundles plain SQLite, so adding SQLCipher is a build-system task.
- UI: an export dialog with targets {Legacy, Library Plus, Both}, modelled on `dlglibraryexport` and `engineprimeexportjob`.

### WS-D: Analysis engine choice (Mixxx / Pioneer vN)
Proposed analyzer selector in Preferences → Beat/Key analysis:

| Option | How | Fidelity | Legal / practical |
|---|---|---|---|
| **Mixxx** | existing `src/analyzer` | n/a | ✅ |
| **Pioneer (rekordbox-as-oracle)** | Run the user's installed rekordbox 5, 6 or 7 to analyse, then import the ANLZ results through the existing reader | Exact | ✅ Cleanest; nothing redistributed. Needs the rekordbox install and is slower. |
| **Pioneer v7 native** | Load the user's own rb7 `detect_beat/model7` or `model8` and ONNX models at runtime (ONNX Runtime; convert the SavedModel locally). RE the pre- and post-processing (features, peak picking, grid fit) from `rekordbox.exe`. | Near-exact once verified | ⚠️ Models stay in the user's install and are never bundled |
| **Pioneer v5/v6 native** | Clean-room reimplementation from observed input→output behaviour (oracle comparisons), **not** transcribed decompiled code | Approximate | ⚠️ Copying decompiled code would be copyright infringement; use behavioural spec + clean-room |

"v5/v6/v7" means the rekordbox major version that produced the analysis. The `AnalyzeVersion` field (F8) may map onto this; still to be confirmed.

### WS-E: rekordbox 6 parity (the core of the program goal)
This is now the main workstream (§0). Build a parity matrix covering the whole
app: layout and screens, browser and library workflows, deck and mixer
behaviour, preferences and defaults, controller behaviour, export. Each row is
**Match** (copy rekordbox 6 behaviour), **Improve** (deliberately better, with
the reason) or **Skip** (with the reason), plus its state in Mix Two
(Have / Map / Build). Sources: rekordbox 6.8.5 UI resources and `locale`
strings, its preferences, the DDJ-800 CSV (F14), and the owner's hardware
testing.
- **Status (2026-10-26):** first full pass written in [PARITY.md](PARITY.md):
  215 rows across 14 areas, built from all 4,548 UI strings of rekordbox
  6.8.5. 162 Match, 23 Improve, 30 Skip; Mix Two already has 70, partly has 57,
  and lacks 54. 40 rows are P1 for performing with the DDJ-800. Awaiting the
  owner's review of the P1 rows.

Feature areas the matrix started from:
- **Prepare:** analysis, hot cues, memory cues, My Tag, tag list, related tracks (F8 embed models), auto-cue (F8 cue model), phrase/PSSI, intelligent playlists, grid editing.
- **Perform:** recording (Mix Two has `src/library/recording`), FX/beat FX, sampler, pad modes (Hot Cue, Pad FX, Beat Jump, Sampler, Keyboard, Beat Loop…), Smart CFX and similar.
- Output: a parity matrix built from the rekordbox `locale` strings, the DDJ-800 CSV functions and Mixxx controls.

## 3. Sequencing

1. **WS-A static pass** (mapping from the CSV). Needs no hardware. **Run in parallel:** WS-E parity matrix and the WS-C legacy PDB/ANLZ writer spec.
2. **Hardware session** with the DDJ-800 connected: dump descriptors, then capture MIDI, HID and USB traffic during defined rekordbox actions. This unblocks the WS-A gaps and WS-B.
3. WS-D oracle mode, then rb7 native models.
4. WS-C Library Plus, after the SQLCipher schema and key work.
5. WS-B WinUSB backend (largest risk; last).

## 4. Open unknowns
- U1: MI_00 descriptor class (vendor vs. UAC2). Needs a connected device.
- U2: What MI_03 HID carries (jog display? VU? keep-alive?).
- U3: The DDJ800 preference-page settings and how they are sent (SysEx vs. HID).
- U4: Exportlibrary.db schema and key derivation.
- U5: How `AnalyzeVersion` values map to rekordbox releases.
- U6: Pre- and post-processing for the rb7 beat model (`model7` vs `model8` selection).
- U7: The "which deck?" choice the owner describes for BROWSE press on a track. The 6.8.5 CSV maps the press to "Library Forward" only (F14). rekordbox 6 preferences have **Controller › Deck › Load Method: "Load Button" / "Rotary Selector (Push)"**, so knob-push loading is a preference. rekordbox stores it per controller as `isLoadMethodButton`. **Resolved 2026-10-26** by the owner's choice: a "Load to the Deck" prompt; the deck is picked with its LOAD button, by clicking it, or by turning and pushing the knob (see PARITY.md row M).
- U8: rekordbox 6 `master.db` (the user's library) is SQLCipher-encrypted. Importing it (cues, grids, playlists, history) is the migration path for rekordbox users; needs the schema and key handling, as for U4.

## 5. Ground rules
- Interoperability RE only. Do not touch the DRM (`clatc.dll`). Firmware: static
  analysis of the firmware files in `.resource/` is allowed (owner, 2026-10-27:
  `DDJ-800_v102.exe`, `CDJ3Kv322.UPD`); never run the updater, flash, modify or
  redistribute firmware, and never send it to a device. Findings in our own
  words ([FIRMWARE.md](FIRMWARE.md)); extracted files stay in `build/re/firmware/`.
- Never bundle Pioneer binaries, models or driver files. Load them from the user's install at runtime.
- `AGENTS.md`: agents commit and push to this fork (`origin`); pull requests,
  above all to upstream Mixxx, stay human actions; AI-written text carries a
  disclaimer.

## 6. Remaining work by priority (handoff 2026-10-27)
What is left after the 2026-10-27 build pass, grouped by the PARITY.md
priorities. PARITY.md holds the per-row detail; this list is the work queue.

### P1: performing on the DDJ-800
**Sound (top priority, WS-F):**
- Beat FX: the DDJ-800's 14 effects, rebuilt by measuring rekordbox:
  LOW CUT ECHO, ECHO, MT DELAY, SPIRAL, REVERB, TRANS, ENIGMA JET, FLANGER,
  PHASER, PITCH, SLIP ROLL, ROLL, MOBIUS SAW, MOBIUS TRI. Then the rest of
  rekordbox's Beat FX (34 in total).
- Beat FX modes: single mode (one effect, 5 parameters) and multi mode
  (3 effects).
- FX beat and BPM: finish the beat selection and BPM Auto/Tap.
- Sound Color FX presets that sound like rekordbox's: DUB ECHO, SWEEP, NOISE,
  FILTER, plus SPACE, PITCH, CRUSH and JET.
- Scratch ([SCRATCH-ENGINE.md](SCRATCH-ENGINE.md)): ~~spin and backspin when
  a moving platter is let go; rekordbox's pitch-bend rule~~ (built
  2026-10-27); Mix Two renders of the scripted jog movements exist
  (`tools/soundparity/render_scratch.py`, real track `.resource/jvckpot.mp3`);
  compared with rekordbox 2026-10-27 by USB capture (jog resolution fixed:
  6480 ticks per turn, not 720); the per-move gap is down to -1 / +41 /
  +21 ms (scaler read-log fix, spin lag fix, 5.6 ms quiet time); still to
  do: rekordbox's spin-stop and forward hand-back rules (captures of single
  moves), and a feel test on the deck.
- Other deck sound: Master Tempo (key lock), key shift and vinyl brake/start,
  matched to rekordbox where they differ.

**Layout (the rekordbox-layout skin):**
- Prepare and Perform as two layouts you switch freely.
- 2-deck and 4-deck layouts, each horizontal or vertical.
- Browse view as a proper layout.
- Show/hide for the FX, CFX, Sampler, Mixer and Rec panels.
- Per-deck panel switch under the waveform: Pad / Memory Cue / Grid / Info,
  including the Memory Cue panel and memory cue comments on the deck.

**Decks and mixer:**
- Load setting "start at the nearest memory/hot cue".
- Fader start (SHIFT + fader).
- Sampler gate mode, alongside one-shot and loop.

**Library:**
- A real Tag List, instead of sending tracks to Auto DJ.
- Playlist folder UI: own order with drag-to-reorder and "Sort Items"; drag
  playlists onto folders; rename inside the tree; plain tree labels (no track
  count or duration); rekordbox-style icons, redrawn.
- Rekordbox USB devices: finish the remaining work (Mix Two already reads
  them).
- Import from rekordbox 6 `master.db` (encrypted), with cues, grids,
  playlists and history (U8).
- Grid editing: adjust from a point onwards, plus rekordbox's GRID panel.

### P2: Prepare and library
- **Layout:** Simple Player and 1/2-player Prepare views; split screen / a
  second browser window; My Tag, Related Tracks and Information windows; list
  font size, line spacing and artwork size; Blue and 3-Band waveform colours.
- **Decks:** the "Automatic BEAT/BPM SYNC" setting (verify what it does
  first); double-click the tempo % to reset it; type a BPM straight into the
  deck; quantize to 1/8, 1/4, 1/2 or a whole beat; "jump before reaching the
  next beat"; hot cue comments on the deck; memory cue at the first beat when
  analysing; active loop; beat count display; load lock; undo track load and
  "locate loaded track".
- **Mixer:** headroom presets (0 to −12 dB) and a limiter on the master
  output; internal/external mixer mode.
- **Effects:** Release FX and Pad FX.
- **Sampler:** slot gain, start-point adjust and the edit window; sampler
  quantize; capture a section to a slot; track type (normal, one-shot sample,
  loop sample).
- **Library:** intelligent playlists, with nested AND/OR as an improvement;
  Related Tracks criteria sets; local Track Suggestion that says why it
  suggests a track; My Tag; filter bar with "use the master deck's BPM and
  key"; status icons; rekordbox XML import and export.
- **Track menu:** analyse key only; add new analysis data; export a track or
  playlist to a device; relocate track; first-class mix name, original artist
  and lyricist fields.
- **Analysis (WS-D):** BPM range for detection; phrase analysis and editing;
  choice of analysis engine (Mixxx, or rekordbox v5/v6/v7).
- **Library management:** backup and restore; missing file manager;
  auto-relocate with search folders.
- **USB export (WS-C):** Legacy format (PDB/ANLZ) and Library Plus.
- **Controller and settings:** vinyl speed adjust (touch/brake,
  release/start); jog rotation speed and jog display mode; deck light
  settings; SHIFT function list generated from the mapping; a table editor
  for MIDI mappings; a keyboard shortcut editor.
- Auto DJ renamed and set up to match Automix.

### P3: later
- **Decks:** show/hide the on-screen pad and platter; hot cue countdown on
  the platter display.
- **Mixer:** PC MASTER OUT; SMART FADER.
- **Effects:** apply CFX to the sampler with the MIC COLOR knob; FX1/FX2 as
  send/return.
- **Sampler:** sequencer.
- **Library:** Hot Cue Bank Lists; Matching; category browser; search
  target; playlist palette; tree shortcuts; library protection.
- **Track menu:** "playlists including this track"; reset play count; set
  cues from the list; export a loop as WAV; per-track waveform colour; BPM
  reload, increment and decrement; bit depth.
- **Analysis:** segment key on the waveform; vocal detection; re-fixing old
  grids, with a diff shown first; PIONEER folder location.
- **Library storage:** a UI to move the database.
- **Devices:** convert to and from Library Plus; Sync Manager; device name
  and background colour; device library backup.
- **Recording:** start on audio input, stop after silence, split on silence,
  optimise level.
- **Automix:** manual crossfade start, Mix Point Link, auto BPM transition.
- **Controller extras:** controller demo/screen saver; Ableton Link.
- Maybe: live track separation, after the analysis-engine work.

### Hardware and driver work
- **DDJ-800 runtime capture (WS-A, U2, U3):** the jog display and on-jog
  indicator; what the HID interface carries; how the DDJ-800 settings page
  talks to the deck. The firmware (F18–F23, [FIRMWARE.md](FIRMWARE.md))
  already shows the jog display is fed by per-deck HID output reports (byte 0
  high nibble = deck) and that no keep-alive is needed; the report contents
  still need a capture of rekordbox's HID OUT traffic.
- **Audio (WS-B, U1):** USB descriptors read from the firmware (F21: vendor
  class, isochronous 1024-byte endpoints every 0.5 ms; the stream format still
  needs a capture); automatically pick the
  DDJ-800 audio interface when it is plugged in; later, Mix Two's own audio +
  MIDI + HID driver in place of Pioneer's (largest risk, last).

### Design ([DESIGN.md](DESIGN.md) "Later")
- View arrangement presets, including separate show/hide for the deck hub,
  HEADPHONES and MASTER.
- Hover tooltips that explain every control.
- Colour FX buttons above the headphone section, and the Beat FX module
  design.
- One module look for the decks, library and FX units.
- The fixed 33⅓ platter: revisit only if control vinyl (DVS) is built.

### To check in rekordbox
Done in Mix Two but still to check against rekordbox:
- the half/double sync threshold;
- whether the tempo range ever narrows again on its own;
- key sync: same key or compatible key, what turning it off does, and which
  deck leads;
- CALL while playing;
- how active loop behaves;
- the look of the load prompt.

### Waiting on the owner
- The Beat FX recordings from rekordbox 5 (input for the Beat FX work above).

<!-- End of AI-generated document. -->
