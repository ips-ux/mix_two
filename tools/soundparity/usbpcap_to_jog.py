#!/usr/bin/env python3
"""Turns a USBPcap capture of the DDJ-800 into a .jog script (render_scratch.py).

Record with USBPcap (USBPcapCMD or Wireshark) while playing the deck in
rekordbox; this picks out the deck's USB-MIDI messages, keeps deck 1's jog,
touch and PLAY messages, and writes them with times relative to the first
PLAY press, which is also where the rendered track starts. The real movement
can then be rendered in Mix Two and compared with rekordbox's REC recording.

Usage:
  usbpcap_to_jog.py CAPTURE.pcap OUT.jog
  usbpcap_to_jog.py --selftest     (round trip through a synthetic capture)

Needs only Python 3 (no Wireshark libraries).
"""

import argparse
import os
import struct
import sys
import tempfile

LINKTYPE_USBPCAP = 249
TRANSFER_INTERRUPT, TRANSFER_BULK = 1, 3
# USB-MIDI 1.0 code index numbers of 3-byte channel messages.
CIN_LENGTH = {0x8: 3, 0x9: 3, 0xA: 3, 0xB: 3, 0xE: 3, 0xC: 2, 0xD: 2}


def read_pcap(path):
    """Yields (seconds, header fields, payload) per USBPcap record."""
    with open(path, "rb") as f:
        data = f.read()
    magic = struct.unpack_from("<I", data, 0)[0]
    if magic in (0xA1B2C3D4, 0xA1B23C4D):
        endian = "<"
    elif magic in (0xD4C3B2A1, 0x4D3CB2A1):
        endian = ">"
    else:
        raise SystemExit("%s is not a pcap file (pcapng: save as .pcap)" % path)
    nanos = magic in (0xA1B23C4D, 0x4D3CB2A1)
    linktype = struct.unpack_from(endian + "I", data, 20)[0]
    if linktype != LINKTYPE_USBPCAP:
        raise SystemExit("link type %d is not USBPcap (249)" % linktype)
    offset = 24
    while offset + 16 <= len(data):
        sec, frac, incl, _ = struct.unpack_from(endian + "IIII", data, offset)
        offset += 16
        record = data[offset:offset + incl]
        offset += incl
        if len(record) < 27:
            continue
        header_len, _irp, _status, _function, info, bus, device, endpoint, transfer, length = \
            struct.unpack_from("<HQIHBHHBBI", record, 0)
        yield (sec + frac / (1e9 if nanos else 1e6),
               {"info": info, "bus": bus, "device": device, "endpoint": endpoint,
                "transfer": transfer},
               record[header_len:header_len + length])


def midi_messages(path):
    """USB-MIDI messages sent by devices, grouped by (bus, device)."""
    by_device = {}
    for seconds, header, payload in read_pcap(path):
        if header["transfer"] not in (TRANSFER_BULK, TRANSFER_INTERRUPT):
            continue
        if not header["endpoint"] & 0x80 or len(payload) == 0 or len(payload) % 4:
            continue  # only device-to-host data in 4-byte USB-MIDI events
        events = []
        for i in range(0, len(payload), 4):
            cin = payload[i] & 0x0F
            if cin == 0 and payload[i:i + 4] == b"\0\0\0\0":
                continue  # padding
            if cin not in CIN_LENGTH:
                events = None
                break
            events.append(bytes(payload[i + 1:i + 1 + CIN_LENGTH[cin]]).ljust(3, b"\0"))
        if events:
            by_device.setdefault((header["bus"], header["device"]), []).extend(
                (seconds, event) for event in events)
    return by_device


def is_deck1_movement(event):
    status, data1 = event[0], event[1]
    return ((status == 0x90 and data1 in (0x36, 0x0B)) or
            (status == 0xB0 and data1 in (0x21, 0x22, 0x23)))


def convert(capture, out):
    devices = midi_messages(capture)
    if not devices:
        raise SystemExit("no USB-MIDI messages in %s" % capture)
    # The DDJ-800 is the device with the most deck 1 movement messages.
    key, messages = max(devices.items(),
                        key=lambda kv: sum(1 for _, e in kv[1] if is_deck1_movement(e)))
    start = next((t for t, e in messages if e[0] == 0x90 and e[1] == 0x0B and e[2] > 0), None)
    if start is None:
        raise SystemExit("no PLAY press on deck 1 in the capture: press PLAY to start")
    lines = ["# from %s (USB bus %d, device %d); 0 ms = first PLAY press"
             % (os.path.basename(capture), key[0], key[1])]
    count = 0
    for t, e in messages:
        if t >= start and is_deck1_movement(e):
            lines.append("%.3f %02X %02X %02X" % ((t - start) * 1000.0, e[0], e[1], e[2]))
            count += 1
    with open(out, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    return count, key


def write_test_pcap(path, events):
    """A minimal USBPcap capture: one interrupt-IN record per (seconds, msg)."""
    with open(path, "wb") as f:
        f.write(struct.pack("<IHHiIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, LINKTYPE_USBPCAP))
        for seconds, msg in events:
            payload = bytes([0x09 if msg[0] & 0xF0 == 0x90 else 0x0B]) + msg
            header = struct.pack("<HQIHBHHBBI", 27, 1, 0, 9, 1, 1, 5, 0x81,
                                 TRANSFER_INTERRUPT, len(payload))
            record = header + payload
            sec = int(seconds)
            f.write(struct.pack("<IIII", sec, int(round((seconds - sec) * 1e6)),
                                len(record), len(record)))
            f.write(record)


def selftest():
    events = [(100.0, bytes([0xB0, 0x22, 0x41])),  # before PLAY: dropped
              (100.5, bytes([0x90, 0x0B, 0x7F])),
              (100.55, bytes([0x90, 0x0B, 0x00])),
              (101.0, bytes([0x90, 0x36, 0x7F])),
              (101.0125, bytes([0xB0, 0x22, 0x43])),
              (101.5, bytes([0xB0, 0x02, 0x10])),  # another control: dropped
              (102.0, bytes([0x90, 0x36, 0x00]))]
    with tempfile.TemporaryDirectory() as tmp:
        pcap, jog = os.path.join(tmp, "t.pcap"), os.path.join(tmp, "t.jog")
        write_test_pcap(pcap, events)
        count, _ = convert(pcap, jog)
        lines = [l for l in open(jog).read().splitlines() if not l.startswith("#")]
    expected = ["0.000 90 0B 7F", "50.000 90 0B 00", "500.000 90 36 7F",
                "512.500 B0 22 43", "1500.000 90 36 00"]
    ok = lines == expected and count == 5
    print("selftest", "passed" if ok else "FAILED", lines)
    return 0 if ok else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("capture", nargs="?")
    parser.add_argument("out", nargs="?")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        sys.exit(selftest())
    if not (args.capture and args.out):
        parser.error("CAPTURE and OUT are required")
    count, key = convert(args.capture, args.out)
    print("%d deck 1 messages from USB bus %d device %d -> %s" % (count, key[0], key[1], args.out))


if __name__ == "__main__":
    main()
