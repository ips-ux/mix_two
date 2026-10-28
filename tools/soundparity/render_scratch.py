#!/usr/bin/env python3
"""Scratch-sound comparison for the DDJ-800 (WS-F, SCRATCH-ENGINE.md).

Jog scripts are DDJ-800 MIDI as the deck sends it, one message per line:
"<milliseconds> <status> <data1> <data2>" in hex bytes. The same script is
rendered through Mix Two's engine here and, once a replay path into rekordbox
exists, through rekordbox (see README.md), so both get the identical movement.

Usage:
  render_scratch.py scripts OUTDIR
      Write the standard jog scripts (*.jog).
  render_scratch.py wrap MUSIC OUT.wav [--start S] [--length S]
      Decode MUSIC with ffmpeg and wrap an excerpt with the alignment chirp
      and two silent bars, so captures line up with compare.py.
  render_scratch.py render TRACK JOG OUT.wav [--algorithm rekordbox|mixxx]
                    [--buffer FRAMES]
      Render one jog script through Mix Two (mixxx-test, PlatterRenderTest).
      The motion is always the engine's platter; --algorithm only picks the
      scaler (rekordbox: half-band + Lagrange; mixxx: linear).
  render_scratch.py all MUSIC [OUTDIR]
      All of the above for every script, with both scalers
      (mixtwo/ and mixtwo-linear-scaler/).
      OUTDIR defaults to build/soundparity/scratch (outside git: the music
      and the renders are not ours to publish).

Needs Python 3 with numpy and scipy, ffmpeg for "wrap", and a built
mixxx-test (tools/mixtwo_windows.bat build mixxx-test).
"""

import argparse
import os
import subprocess
import sys
import tempfile

import numpy as np
from scipy.io import wavfile

from make_signals import CHIRP_SECONDS, PEAK, chirp, silence

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
STEPS_PER_REVOLUTION = 6480  # DDJ-800 jog steps per turn (measured in rekordbox)
SECONDS_PER_REVOLUTION = 1.8  # 33 1/3 rpm
BPM = 128.0
# The music starts after the chirp and two silent bars.
MUSIC_START = CHIRP_SECONDS + 8 * 60.0 / BPM


class Jog:
    """Builds a jog script from platter movements, at 1 ms resolution."""

    def __init__(self):
        self.lines = []
        self.revolutions = 0.0
        self.sent = 0

    def add(self, seconds, status, data1, data2, note=""):
        line = "%.1f %02X %02X %02X" % (seconds * 1000.0, status, data1, data2)
        self.lines.append(line + ("  # " + note if note else ""))

    def play(self, seconds):
        self.add(seconds, 0x90, 0x0B, 0x7F, "PLAY")
        self.add(seconds + 0.05, 0x90, 0x0B, 0x00)

    def touch(self, seconds, touched):
        self.add(seconds, 0x90, 0x36, 0x7F if touched else 0x00,
                 "touch" if touched else "release")

    def move(self, start, end, speed0, speed1, cc=0x22):
        """Platter speed ramps from speed0 to speed1 (1.0 = 33 1/3 rpm)."""
        first = int(round(start * 1000))
        last = int(round(end * 1000))
        for ms in range(first, last):
            fraction = (ms - first + 0.5) / max(1, last - first)
            speed = speed0 + (speed1 - speed0) * fraction
            self.revolutions += speed / SECONDS_PER_REVOLUTION / 1000.0
            steps = int(round(self.revolutions * STEPS_PER_REVOLUTION)) - self.sent
            while steps:
                step = max(-63, min(63, steps))
                self.add(ms / 1000.0, 0xB0, cc, 0x40 + step)
                self.sent += step
                steps -= step

    def text(self, title):
        return "# %s\n" % title + "\n".join(self.lines) + "\n"


def scripts():
    """The standard movements. The deck plays from 0 s, so the chirp passes
    unprocessed; movements start in the music (MUSIC_START + 4 s)."""
    t = MUSIC_START + 4.0
    result = {}

    jog = Jog()
    jog.play(0.0)
    jog.touch(t, True)
    # Baby scratch: a quarter revolution forward and back, 4 times in 1.2 s.
    cycles, length = 4, 1.2
    for i in range(cycles):
        a = t + 0.1 + i * length / cycles
        half = length / cycles / 2
        jog.move(a, a + half / 2, 0.0, 2.0)
        jog.move(a + half / 2, a + half, 2.0, 0.0)
        jog.move(a + half, a + 1.5 * half, 0.0, -2.0)
        jog.move(a + 1.5 * half, a + 2 * half, -2.0, 0.0)
    jog.touch(t + 0.1 + length + 0.1, False)
    result["baby_scratch"] = jog.text("Baby scratch, deck playing; release on a still platter")

    jog = Jog()
    jog.play(0.0)
    jog.touch(t, True)
    jog.move(t + 0.1, t + 0.5, 2.25, 2.25)  # half a revolution forward
    jog.move(t + 0.5, t + 0.9, -2.25, -2.25)  # and back
    jog.touch(t + 1.1, False)
    result["forward_back"] = jog.text("Half a revolution forward and back, held, released")

    jog = Jog()
    jog.play(0.0)
    jog.touch(t, True)
    jog.move(t + 0.1, t + 0.3, 0.0, 3.0)
    jog.touch(t + 0.3, False)
    jog.move(t + 0.3, t + 2.3, 3.0, 0.0)  # the platter coasts to a stop
    result["release_spin"] = jog.text(
        "Push to 3x, release: spin, hand back to play, coast ignored")

    jog = Jog()
    jog.play(0.0)
    jog.touch(t, True)
    jog.move(t + 0.1, t + 0.25, 0.0, -4.0)
    jog.touch(t + 0.25, False)
    jog.move(t + 0.25, t + 1.75, -4.0, 0.0)
    result["backspin"] = jog.text("Backspin from -4x, coasting to a stop, play resumes")

    jog = Jog()
    jog.play(0.0)
    jog.play(t - 0.5)  # pause
    jog.touch(t, True)
    jog.move(t + 0.1, t + 0.3, 0.0, 2.0)
    jog.touch(t + 0.3, False)
    jog.move(t + 0.3, t + 2.3, 2.0, 0.0)
    jog.play(t + 3.0)
    result["spin_paused"] = jog.text("Paused deck: push to 2x and release; spins until it stops")

    jog = Jog()
    jog.play(0.0)
    jog.move(t, t + 0.5, 2.0, 2.0, cc=0x21)
    jog.move(t + 1.0, t + 1.5, -2.0, -2.0, cc=0x21)
    jog.move(t + 2.0, t + 2.3, 20.0, 20.0, cc=0x21)
    result["bend"] = jog.text("Side jog bends: +2x (+6 %), -2x (-6 %), 20x (capped +42 %)")
    return result


def write_scripts(outdir):
    os.makedirs(outdir, exist_ok=True)
    paths = []
    for name, text in scripts().items():
        path = os.path.join(outdir, name + ".jog")
        with open(path, "w", newline="\n") as f:
            f.write(text)
        paths.append(path)
    return paths


def wrap(music, out, start=30.0, length=12.0, rate=44100):
    with tempfile.TemporaryDirectory() as tmp:
        decoded = os.path.join(tmp, "decoded.wav")
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", music, "-ac", "2", "-ar", str(rate),
                        "-c:a", "pcm_s16le", decoded], check=True)
        _, data = wavfile.read(decoded)
    excerpt = data[int(start * rate):int((start + length) * rate)].astype(np.float64) / 32768.0
    excerpt *= PEAK / max(1e-9, np.max(np.abs(excerpt)))  # peaks at -6 dBFS
    lead = np.concatenate([chirp(rate) * PEAK, silence(8 * 60.0 / BPM, rate)])
    tail = silence(2.0, rate)
    stereo = np.concatenate([np.stack([lead, lead], axis=1), excerpt,
                             np.stack([tail, tail], axis=1)])
    wavfile.write(out, rate, (np.clip(stereo, -1, 1) * 32767).astype(np.int16))


def render(track, jog, out, algorithm="rekordbox", buffer_frames=128,
           resolution=STEPS_PER_REVOLUTION):
    env = dict(os.environ)
    env["MIXTWO_RENDER_TRACK"] = os.path.abspath(track)
    env["MIXTWO_RENDER_JOG"] = os.path.abspath(jog)
    env["MIXTWO_RENDER_OUT"] = os.path.abspath(out)
    env["MIXTWO_RENDER_ALGORITHM"] = "1" if algorithm == "rekordbox" else "0"
    env["MIXTWO_RENDER_BUFFER"] = str(buffer_frames)
    env["MIXTWO_RENDER_JOG_RESOLUTION"] = str(resolution)
    bat = os.path.join(ROOT, "tools", "mixtwo_windows.bat")
    result = subprocess.run(["cmd", "/c", bat, "test", "--gtest_filter=PlatterRenderTest.*"],
                            cwd=ROOT, env=env, capture_output=True, text=True)
    if result.returncode != 0 or not os.path.exists(out):
        sys.stderr.write(result.stdout[-4000:] + result.stderr[-2000:])
        raise SystemExit("render failed: " + jog)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("scripts")
    p.add_argument("outdir")
    p = sub.add_parser("wrap")
    p.add_argument("music")
    p.add_argument("out")
    p.add_argument("--start", type=float, default=30.0)
    p.add_argument("--length", type=float, default=12.0)
    p = sub.add_parser("render")
    p.add_argument("track")
    p.add_argument("jog")
    p.add_argument("out")
    p.add_argument("--algorithm", choices=["rekordbox", "mixxx"], default="rekordbox")
    p.add_argument("--buffer", type=int, default=128)
    p.add_argument("--resolution", type=float, default=STEPS_PER_REVOLUTION,
                   help="jog steps per revolution (default %(default)s)")
    p = sub.add_parser("all")
    p.add_argument("music")
    p.add_argument("outdir", nargs="?",
                   default=os.path.join(ROOT, "build", "soundparity", "scratch"))
    args = parser.parse_args()

    if args.command == "scripts":
        for path in write_scripts(args.outdir):
            print(path)
    elif args.command == "wrap":
        wrap(args.music, args.out, args.start, args.length)
    elif args.command == "render":
        render(args.track, args.jog, args.out, args.algorithm, args.buffer, args.resolution)
    else:
        jogdir = os.path.join(args.outdir, "jog")
        track = os.path.join(args.outdir, "music.wav")
        os.makedirs(args.outdir, exist_ok=True)
        wrap(args.music, track)
        for jog in write_scripts(jogdir):
            name = os.path.splitext(os.path.basename(jog))[0]
            for algorithm, folder in (("rekordbox", "mixtwo"), ("mixxx", "mixtwo-linear-scaler")):
                outdir = os.path.join(args.outdir, folder)
                os.makedirs(outdir, exist_ok=True)
                out = os.path.join(outdir, name + ".wav")
                render(track, jog, out, algorithm)
                print(out)


if __name__ == "__main__":
    main()
