#!/usr/bin/env python3
"""Compares where in the track two recordings of the same jog movement are,
every 50 ms: rekordbox's REC recording against Mix Two's render.

Both recordings must play render_scratch.py's music.wav (chirp, two silent
bars, music). Each is aligned on its chirp, then every window of the
recording is matched against music.wav to find the track position it plays.
A null test cannot work for scratching (any tiny timing difference wrecks
it); the position trace shows whether the motion (scratch, spin, backspin,
bend, hand-back) behaves the same.

Usage:
  scratch_trace.py MUSIC.wav REKORDBOX.wav MIXTWO.wav [--csv OUT.csv]

Needs numpy and scipy.
"""

import argparse

import numpy as np
from scipy.io import wavfile
from scipy.signal import fftconvolve

from make_signals import CHIRP_SECONDS, chirp

WINDOW = 0.08
STEP = 0.05


def mono(path):
    rate, raw = wavfile.read(path)
    data = raw.astype(np.float64)
    if data.ndim > 1:
        data = data.mean(axis=1)
    if np.issubdtype(raw.dtype, np.integer):
        data /= float(np.iinfo(raw.dtype).max) + 1.0
    return rate, data


def chirp_start(signal, rate):
    marker = chirp(rate)
    corr = fftconvolve(signal[:int(30 * rate)], marker[::-1], mode="valid")
    return int(np.argmax(np.abs(corr)))


def trace(music, recording, rate):
    """(time after the chirp, track position after the chirp, match score)."""
    start = chirp_start(recording, rate)
    music_start = chirp_start(music, rate)
    rec = recording[start:]
    ref = music[music_start:]
    n = int(WINDOW * rate)
    ref_energy = np.sqrt(np.maximum(fftconvolve(ref * ref, np.ones(n), mode="valid"), 0)) + 1e-6
    points = []
    t = CHIRP_SECONDS + 0.5
    while (t + WINDOW) * rate < len(rec):
        w = rec[int(t * rate):int(t * rate) + n]
        w = w - w.mean()
        norm = np.linalg.norm(w)
        if norm > 1e-3:
            score = fftconvolve(ref, w[::-1], mode="valid") / (ref_energy * norm)
            k = int(np.argmax(score))
            points.append((t, k / rate, float(score[k])))
        else:
            points.append((t, float("nan"), 0.0))
        t += STEP
    return points


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("music")
    parser.add_argument("rekordbox")
    parser.add_argument("mixtwo")
    parser.add_argument("--csv")
    args = parser.parse_args()

    rate, music = mono(args.music)
    traces = []
    for path in (args.rekordbox, args.mixtwo):
        r, rec = mono(path)
        if r != rate:
            raise SystemExit("%s: %d Hz, music.wav is %d Hz" % (path, r, rate))
        traces.append(trace(music, rec, rate))

    rows = []
    for (t, a, sa), (_, b, sb) in zip(*traces):
        good = sa > 0.8 and sb > 0.8  # both play recognisable track audio
        rows.append((t, a, b, b - a if good else float("nan"), sa, sb))
    diffs = np.array([r[3] for r in rows if not np.isnan(r[3])])
    if args.csv:
        with open(args.csv, "w") as f:
            f.write("seconds,rekordbox_position,mixtwo_position,difference,"
                    "rekordbox_match,mixtwo_match\n")
            for r in rows:
                f.write("%.3f,%.4f,%.4f,%.4f,%.3f,%.3f\n" % r)
    print("%d windows, %d where both play recognisable audio" % (len(rows), len(diffs)))
    if len(diffs):
        a = np.abs(diffs)
        print("position difference ms: median %.1f, 95%% %.1f, max %.1f"
              % (np.median(a) * 1000, np.percentile(a, 95) * 1000, a.max() * 1000))
        worst = max((r for r in rows if not np.isnan(r[3])), key=lambda r: abs(r[3]))
        print("largest at %.2f s: rekordbox %.3f s, Mix Two %.3f s" % worst[:3])


if __name__ == "__main__":
    main()
