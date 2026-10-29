<!-- AI-generated (Claude Code, 2026-10-28). Start of AI-generated text. -->

# Bucket 14: Portability

> **AI-generated:** built from the 2026-10-28 inventories and the owner's
> decisions in [DECISIONS.md](../DECISIONS.md). Status ladder and rules: DECISIONS.md §3.

**Criticality:** High, ongoing  ·  **Scope:** Linux as a first-class target (the goal: Linux runs better than Windows), packaging, permissions, CI, latency, the audit of Windows-only code, and browser groundwork far later (DEC-§8). No Windows-only code without a Linux path, except where noted.

## Linux target and build
| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PORT-001 | Linux reference: Linux Mint / Ubuntu 24.04 family | Improve | — | N | N | Build | DEC-§8 | rekordbox has no Linux build; Linux is the owner's improvement (his laptop: Mint XFCE, kernel 7.0). Every PORT row is tested on this family first. | DEC-§8 |
| PORT-002 | Linux dev script (configure / build / test / run) | Improve | — | — | — | Build | tools/mixtwo_windows.bat only; upstream tools/debian_buildenv.sh | A tools/mixtwo_linux.sh with the same commands and the separate dev profile (`MIXTWO_PROFILE`), so agents and the owner build the same way on both systems. | DEC-§8 |
| PORT-003 | Kernel 6.4+ check for the DDJ-800 audio quirk | Improve | — | — | — | Build | DEC-§8 (snd-usb-audio quirk, kernel 6.4+) | On start, if the kernel is older than 6.4 and a DDJ-800 is connected, show one STE message. The .deb can also state the kernel need. | DEC-§8 |
| PORT-004 | DDJ-800 audio channels on Linux | DDJ-800 | — | — | — | Build | FIRMWARE.md §2.7 (rekordbox: MASTER OUT 1/2, PHONES 3/4, BOOTH 5/6) | Not tested on Linux. Check that snd-usb-audio exposes the same channels and that Sound Hardware maps them like rekordbox. Auto-selecting the device is a HWL / MIX row. | DEC-§8 |
| PORT-005 | Linux sound API choice (ALSA, JACK, PipeWire) | Owner? | — | — | — | Build | upstream PortAudio host APIs | See question 1. Measure each with PORT-012 before picking a default. | DEC-§8 |
| PORT-006 | Real-time audio permissions | Improve | — | — | — | Build | DEC-§8 | The .deb installs a limits file (rtprio, memlock) for an audio group, or relies on PipeWire's RT module; first run checks and explains in STE. | DEC-§8 |
| PORT-007 | udev rule for the DDJ-800 | DDJ-800 | — | — | — | Partial | res/linux/mixxx-usb-uaccess.rules:44-47 (Pioneer 08e4 and 2b73 hidraw, uaccess) | Upstream already covers Pioneer hidraw (DDJ-800 is VID 2B73). Check that it covers MI_03 HID for the jog display (HWL) and that the Mix Two .deb installs it. | DEC-§8 |
| PORT-008 | .deb package | Improve | — | — | — | Partial | packaging/debian/; packaging/CPackDebInstall.cmake | Upstream packaging exists, still named Mixxx. Rename to Mix Two, install the udev rule and RT permissions, keep the user's profile separate from Mixxx's. | DEC-§8 |
| PORT-009 | AppImage for other distributions | Improve | — | — | — | Partial | packaging/appimage/; .github/workflows/appimage.yml | Upstream AppImage exists. An AppImage cannot install udev or RT files: first run shows the commands in STE. | DEC-§8 |
| PORT-010 | Flatpak | Skip | — | — | — | Skip | packaging/flatpak/; .github/workflows/flatpak.yml | DEC-§8: no Flatpak; its sandbox blocks USB / HID and real-time audio. Disable the upstream Flatpak workflow in the fork. | DEC-§8 |
| PORT-011 | Linux CI build | Improve | — | — | — | Partial | .github/workflows/build.yml:57-76 (Ubuntu 24.04 DEB), 206-228 (AppImage) | Upstream jobs exist; not checked that they run on the fork, or that they run the Mix Two tests (platter, scratch, sync, midiportwatcher). | DEC-§8 |
| PORT-012 | Latency benchmarks: Windows vs Linux | Improve | — | — | — | Build | ROADMAP F16 context; USER_changelog (6 ms buffer advice) | Same test on both systems: smallest stable buffer, xruns per hour, jog-to-sound delay with the DDJ-800. The goal is Linux better than Windows; this row proves it. .github/workflows/benchmark.yml exists upstream (CPU only). | DEC-§8 |

## Windows-only code (audit)
| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PORT-013 | Audit of `Q_OS_WIN` / `_WIN32` added since ee17a27266 | Improve | — | — | — | Checked | `git diff ee17a27266 HEAD -- src` grep, 2026-10-28 | Three files: src/coreservices.cpp, src/controllers/midi/midiportwatcher.cpp, src/test/main.cpp. Each is covered by PORT-014 to PORT-016. Repeat the audit at each merge. | DEC-§8 |
| PORT-014 | rekordbox-running check at start | Skip | — | — | — | Skip | src/coreservices.cpp:79, 110 (Toolhelp snapshot for rekordbox.exe) | Windows only and harmless: rekordbox does not run on Linux, so no Linux path is needed. | DEC-§8 |
| PORT-015 | MIDI port watcher (hotplug) on Linux | Improve | — | — | — | Built | src/controllers/midi/midiportwatcher.cpp (Q_OS_LINUX branch: /proc/asound/seq/clients); src/test/midiportwatcher_test.cpp | Linux branch exists; never run on Linux with the DDJ-800. | DEC-§8 |
| PORT-016 | Test runner Windows workarounds | Improve | — | — | — | Built | src/test/main.cpp (OleInitialize under `_WIN32`; `MIXXX_DISABLE_PORTAUDIO`) | Windows-only fix for COM and the Pioneer ASIO crash (ROADMAP F16); no effect on Linux. | DEC-§8 |
| PORT-017 | DDJ-800 mapping device match on Linux port names | DDJ-800 | — | — | — | Build | XML `device_name="DDJ-800"`; MidiUtils::canonicalDeviceName | ALSA names ports differently from WinMM (client:port names). Check that auto-enable finds the DDJ-800 on Linux. | DEC-§8 |
| PORT-018 | Own audio driver is Windows-only | Improve | — | — | — | Skip | ROADMAP WS-B, F1-F3 | DEC-§8: accepted exception; Linux uses the kernel's snd-usb-audio. The driver work itself is a HWL row. | DEC-§8 |
| PORT-019 | Windows-only RE and measurement tools | Improve | — | — | — | Skip | tools/soundparity/README.md (USBPcap capture, usbpcap_to_jog.py; loopMIDI / teVirtualMIDI for send_jog.py) | Measurement only, never shipped. Linux equivalents (usbmon, ALSA virmidi) only if a Linux measurement is needed. | DEC-§8 |
| PORT-020 | Pioneer ASIO driver workaround | Skip | — | — | — | Skip | ROADMAP F16 | Windows only; Linux has no Pioneer driver. | DEC-§8 |

## Library and files on Linux
| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PORT-021 | Cloud backup through synced folders on Linux | Improve | — | — | — | Build | DEC-§6 (Dropbox / Google Drive folder) | Dropbox has a Linux client; Google Drive has none, so use rclone (sync or mount) to a local folder. Mix Two only writes to the folder. Document in STE (STE-018). | DEC-§6, DEC-§8 |
| PORT-022 | Send originals to Trash on Linux | Improve | — | — | — | Build | DEC-§6 (Consolidate: Recycle Bin / Trash) | Use Qt's move-to-trash (freedesktop Trash on Linux). Never on removable media. Function in LIB. | DEC-§6, DEC-§8 |
| PORT-023 | Removable drive detection on Linux | Improve | — | — | — | Build | DEC-§6 (Portable library; never remove from removable media; never write to a played stick) | Linux mounts under /media or /run/media (udisks). Detection must work there as on Windows drive letters. | DEC-§6, DEC-§8 |
| PORT-024 | Case-sensitive paths and FAT32 / exFAT sticks | CDJ-3000 | — | — | — | Build | DEC-§4 | Linux file systems are case-sensitive; Pioneer sticks are FAT32 / exFAT. Export and import must handle both (EXP / IMP). | DEC-§8 |
| PORT-025 | rekordbox master.db update on Linux | Skip | — | — | — | Skip | DEC-§6 (offer to update master.db after Consolidate) | rekordbox does not run on Linux; offer the update only where rekordbox is installed. Import of a copied master.db still works (IMP). | DEC-§6, DEC-§8 |

## Other platforms
| ID | Feature | Ref | rb5 | rb6 | rb7 | Status | Evidence | Notes | Src |
|---|---|---|---|---|---|---|---|---|---|
| PORT-026 | macOS | Owner? | Y | Y | Y | Partial | upstream macOS code and CI (.github/workflows/build.yml:79-122) | DEC-§8 names only Windows, Linux and the browser. See question 2. | DEC-§8 |
| PORT-027 | Browser groundwork (far future) | Improve | — | — | — | Build | DEC-§8 | Keep behaviour in the engine as controls (AGENT_changelog working agreements), keep UI thin, so a later browser face (WebAudio, WebMIDI, WebHID) can reuse it. No work now beyond not blocking it. | DEC-§8 |

## Open questions for the owner
1. **Default Linux sound path.** Options: (a) ALSA direct to the DDJ-800 (lowest latency, the device is exclusive); (b) PipeWire's JACK interface (shares audio with other apps); (c) JACK2. Recommendation: (a) as the default for performing, (b) as an option; decide after PORT-012 measures both on your laptop.
2. **macOS.** DECISIONS names Windows and Linux only. Options: (a) keep upstream's macOS code compiling in CI, no testing; (b) remove macOS jobs to save CI time; (c) support it. Recommendation: (a): it costs little and keeps upstream merges easy.
3. **Linux test machine.** Can agents get your Mint laptop's test results (build log, `--controller-debug` log, benchmark output) through the repo or a shared folder? Recommendation: a `build/` log folder you copy into a session when needed.

<!-- End of AI-generated text. -->
