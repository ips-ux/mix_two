#!/usr/bin/env python3
"""Replays .jog scripts (render_scratch.py) into a DJ app as if a DDJ-800
sent them, so rekordbox gets the same jog movement as Mix Two's renders.

The port is created with the teVirtualMIDI driver that loopMIDI installs
(https://www.tobias-erichsen.de/software/loopmidi.html), as a send-only port:
other apps see only a MIDI input named "DDJ-800". rekordbox's LED output can
therefore not loop back into its own input, as it would through a plain
loopMIDI port. Nothing else needs installing (ctypes only).

Usage:
  send_jog.py [--name DDJ-800] [JOG ...]
      Creates the port and keeps it open. Start rekordbox after the port
      exists, then press Enter to send each file in turn; when the list is
      done, type further .jog paths, or press Enter on an empty line to quit.
  send_jog.py --list
      Lists the MIDI ports Windows sees.
  send_jog.py --loopback JOG
      Self-test: sends JOG into its own port, receives it back through
      WinMM and reports the timing error per message.

Unplug the real DDJ-800 while the virtual one is in use: two inputs with the
same name confuse apps that pick ports by name.
"""

import argparse
import ctypes
import os
import sys
import time
from ctypes import wintypes

winmm = ctypes.WinDLL("winmm")

TE_VM_FLAGS_PARSE_RX = 1
TE_VM_FLAGS_INSTANTIATE_TX_ONLY = 8

VM_DATA_CB = ctypes.WINFUNCTYPE(None, ctypes.c_void_p, ctypes.POINTER(ctypes.c_ubyte),
                                wintypes.DWORD, ctypes.c_void_p)
MIDI_IN_PROC = ctypes.WINFUNCTYPE(None, ctypes.c_void_p, wintypes.UINT, ctypes.c_void_p,
                                  ctypes.c_size_t, ctypes.c_size_t)
MIM_DATA = 0x3C3
CALLBACK_FUNCTION = 0x30000
winmm.midiInOpen.argtypes = [ctypes.POINTER(ctypes.c_void_p), wintypes.UINT, MIDI_IN_PROC,
                             ctypes.c_void_p, wintypes.DWORD]


class MIDIINCAPSW(ctypes.Structure):
    _fields_ = [("wMid", wintypes.WORD), ("wPid", wintypes.WORD),
                ("vDriverVersion", wintypes.UINT), ("szPname", wintypes.WCHAR * 32),
                ("dwSupport", wintypes.DWORD)]


class MIDIOUTCAPSW(ctypes.Structure):
    _fields_ = [("wMid", wintypes.WORD), ("wPid", wintypes.WORD),
                ("vDriverVersion", wintypes.UINT), ("szPname", wintypes.WCHAR * 32),
                ("wTechnology", wintypes.WORD), ("wVoices", wintypes.WORD),
                ("wNotes", wintypes.WORD), ("wChannelMask", wintypes.WORD),
                ("dwSupport", wintypes.DWORD)]


def midi_inputs():
    names = []
    for i in range(winmm.midiInGetNumDevs()):
        caps = MIDIINCAPSW()
        winmm.midiInGetDevCapsW(i, ctypes.byref(caps), ctypes.sizeof(caps))
        names.append(caps.szPname)
    return names


def midi_outputs():
    names = []
    for i in range(winmm.midiOutGetNumDevs()):
        caps = MIDIOUTCAPSW()
        winmm.midiOutGetDevCapsW(i, ctypes.byref(caps), ctypes.sizeof(caps))
        names.append(caps.szPname)
    return names


def read_jog(path):
    messages = []
    with open(path) as f:
        for line in f:
            line = line.split("#", 1)[0].split()
            if not line:
                continue
            ms, status, data1, data2 = line
            messages.append((float(ms) / 1000.0, bytes([int(status, 16), int(data1, 16),
                                                         int(data2, 16)])))
    return messages


class VirtualPort:
    def __init__(self, name):
        try:
            self.dll = ctypes.WinDLL("teVirtualMIDI64.dll" if sys.maxsize > 2**32
                                     else "teVirtualMIDI.dll", use_last_error=True)
        except OSError:
            raise SystemExit("teVirtualMIDI is not installed: install loopMIDI "
                             "(https://www.tobias-erichsen.de/software/loopmidi.html)")
        self.dll.virtualMIDICreatePortEx2.restype = ctypes.c_void_p
        self.dll.virtualMIDICreatePortEx2.argtypes = [wintypes.LPCWSTR, VM_DATA_CB,
                                                      ctypes.c_void_p, wintypes.DWORD,
                                                      wintypes.DWORD]
        self.dll.virtualMIDISendData.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
                                                 wintypes.DWORD]
        self.dll.virtualMIDISendData.restype = wintypes.BOOL
        self.dll.virtualMIDIClosePort.argtypes = [ctypes.c_void_p]
        self.callback = VM_DATA_CB(lambda *args: None)  # nothing comes back
        self.port = self.dll.virtualMIDICreatePortEx2(
            name, self.callback, None, 0, TE_VM_FLAGS_PARSE_RX | TE_VM_FLAGS_INSTANTIATE_TX_ONLY)
        if not self.port:
            raise SystemExit("Cannot create the virtual port %r (error %d); is the name "
                             "already taken?" % (name, ctypes.get_last_error()))
        # Windows lists a new port after a short delay.
        for _ in range(40):
            if name in midi_inputs():
                break
            time.sleep(0.05)

    def send(self, data):
        if not self.dll.virtualMIDISendData(self.port, data, len(data)):
            raise SystemExit("Sending failed (error %d)" % ctypes.get_last_error())

    def close(self):
        if self.port:
            self.dll.virtualMIDIClosePort(self.port)
            self.port = None


def play(port, messages, on_sent=None):
    """Sends the messages at their times, spinning for the last 2 ms."""
    winmm.timeBeginPeriod(1)
    try:
        start = time.perf_counter()
        for when, data in messages:
            while True:
                left = start + when - time.perf_counter()
                if left <= 0:
                    break
                if left > 0.002:
                    time.sleep(left - 0.002)
                else:
                    time.sleep(0)  # yield the GIL (the --loopback receiver needs it)
            port.send(data)
            if on_sent:
                on_sent(when, data, time.perf_counter() - start)
    finally:
        winmm.timeEndPeriod(1)


def loopback(name, path):
    port = VirtualPort(name)
    try:
        index = len(midi_inputs()) - 1 - midi_inputs()[::-1].index(name)
    except ValueError:
        port.close()
        raise SystemExit("The port does not show up as a MIDI input")
    received = []
    t0 = [None]

    def on_midi(handle, msg, instance, param1, param2):
        if msg == MIM_DATA and t0[0] is not None:
            received.append((time.perf_counter() - t0[0], param1 & 0xFFFFFF))

    proc = MIDI_IN_PROC(on_midi)
    handle = ctypes.c_void_p()
    if winmm.midiInOpen(ctypes.byref(handle), index, proc, None, CALLBACK_FUNCTION) != 0:
        port.close()
        raise SystemExit("midiInOpen failed")
    winmm.midiInStart(handle)
    time.sleep(0.2)
    messages = read_jog(path)
    t0[0] = time.perf_counter()
    play(port, messages)
    time.sleep(0.2)
    winmm.midiInStop(handle)
    winmm.midiInClose(handle)
    port.close()

    expected = [(when, data[0] | data[1] << 8 | data[2] << 16) for when, data in messages]
    if len(received) != len(expected):
        print("sent %d messages, received %d" % (len(expected), len(received)))
    errors = [(got - when) * 1000 for (when, a), (got, b) in zip(expected, received) if a == b]
    wrong = sum(1 for (_, a), (_, b) in zip(expected, received) if a != b)
    errors.sort()
    print("%d messages, %d out of order or changed" % (len(expected), wrong))
    if errors:
        print("timing error ms: median %.2f, 95%% %.2f, max %.2f"
              % (errors[len(errors) // 2], errors[int(len(errors) * 0.95)], errors[-1]))


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--name", default="DDJ-800")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--loopback", metavar="JOG")
    parser.add_argument("jog", nargs="*")
    args = parser.parse_args()

    if args.list:
        print("MIDI inputs: ", midi_inputs())
        print("MIDI outputs:", midi_outputs())
        return
    if args.loopback:
        loopback(args.name, args.loopback)
        return
    if args.name in midi_inputs():
        print("Warning: a MIDI input named %r already exists (the real deck?)." % args.name)
    port = VirtualPort(args.name)
    print("Port %r is open. Start rekordbox now if it is not running." % args.name)
    queue = list(args.jog)
    try:
        while True:
            if queue:
                path = queue.pop(0)
                if input("Enter: send %s  (q: quit) " % os.path.basename(path)).strip() == "q":
                    break
            else:
                path = input(".jog to send (empty: quit): ").strip().strip('"')
                if not path:
                    break
            messages = read_jog(path)
            print("Sending %d messages over %.1f s ..." % (len(messages), messages[-1][0]))
            play(port, messages)
            print("Done.")
    except (KeyboardInterrupt, EOFError):
        pass
    finally:
        port.close()


if __name__ == "__main__":
    main()
