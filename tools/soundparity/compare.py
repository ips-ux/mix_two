#!/usr/bin/env python3
"""Compares a Mix Two recording of a test signal with rekordbox's (WS-F).

Both recordings are lined up on the alignment chirp that make_signals.py puts
at the start of every test signal, then compared:

- null depth: level of the difference signal relative to the reference, in
  dB (lower is better; identical audio gives -inf), also after the best
  overall gain match;
- residual latency: the remaining offset between the two after alignment;
- band differences: level difference per third-octave band;
- tail length: how long each recording stays above -60 dB of its peak after
  the test content ends (delay and reverb tails).

Exit status 0 when the gain-matched null depth is at or below --threshold.

Usage: compare.py REFERENCE.wav CANDIDATE.wav [--threshold -30] [--json OUT]
"""

import argparse
import json
import sys

import numpy as np
from scipy import signal
from scipy.io import wavfile

CHIRP_SECONDS = 0.5
SEARCH_SECONDS = 30.0
EPS = 1e-12


def chirp(rate):
    # Must match make_signals.chirp().
    t = np.arange(int(CHIRP_SECONDS * rate)) / rate
    f0, f1 = 1000.0, 8000.0
    phase = 2 * np.pi * (f0 * t + (f1 - f0) * t * t / (2 * CHIRP_SECONDS))
    window = 0.5 - 0.5 * np.cos(2 * np.pi * t / CHIRP_SECONDS)
    return np.sin(phase) * window


def load(path, rate=None):
    """Returns (float samples [n, channels], rate), resampled to `rate`."""
    file_rate, data = wavfile.read(path)
    if data.dtype.kind == "i":
        data = data.astype(np.float64) / np.iinfo(data.dtype).max
    elif data.dtype.kind == "u":
        data = (data.astype(np.float64) - 128.0) / 128.0
    else:
        data = data.astype(np.float64)
    if data.ndim == 1:
        data = data[:, None]
    if rate is not None and file_rate != rate:
        g = np.gcd(int(rate), int(file_rate))
        data = signal.resample_poly(data, rate // g, file_rate // g, axis=0)
        file_rate = rate
    return data, file_rate


def find_chirp(mono, rate):
    """Index where the alignment chirp starts."""
    search = mono[: int(SEARCH_SECONDS * rate)]
    template = chirp(rate)
    corr = signal.fftconvolve(search, template[::-1], mode="valid")
    return int(np.argmax(np.abs(corr)))


def db(x):
    return 20 * np.log10(max(x, EPS))


def rms(x):
    return float(np.sqrt(np.mean(np.square(x)))) if x.size else 0.0


def third_octave_levels(mono, rate):
    freqs, psd = signal.welch(mono, rate, nperseg=8192)
    centers = 1000.0 * 2.0 ** (np.arange(-17, 14) / 3.0)  # 20 Hz .. 20 kHz
    levels = {}
    for fc in centers:
        band = (freqs >= fc / 2 ** (1 / 6)) & (freqs < fc * 2 ** (1 / 6))
        if np.any(band) and fc < rate / 2:
            levels[round(float(fc), 1)] = 10 * np.log10(np.sum(psd[band]) + EPS)
    return levels


def tail_seconds(mono, rate, content_end):
    """Time after content_end until the envelope stays below -60 dB of peak."""
    window = int(0.01 * rate)
    envelope = np.sqrt(np.convolve(np.square(mono), np.ones(window) / window, mode="same"))
    threshold = np.max(envelope) * 10 ** (-60 / 20)
    above = np.nonzero(envelope[content_end:] > threshold)[0]
    return float(above[-1] / rate) if above.size else 0.0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("reference", help="rekordbox recording")
    parser.add_argument("candidate", help="Mix Two recording")
    parser.add_argument("--threshold", type=float, default=-30.0,
            help="pass if the gain-matched null depth (dB) is at or below this")
    parser.add_argument("--content-end", type=float, default=None,
            help="seconds after the chirp where the test content ends (for tails)")
    parser.add_argument("--json", help="also write the results to this file")
    args = parser.parse_args()

    ref, rate = load(args.reference)
    cand, _ = load(args.candidate, rate)
    ref_mono, cand_mono = ref.mean(axis=1), cand.mean(axis=1)

    ref_start = find_chirp(ref_mono, rate)
    cand_start = find_chirp(cand_mono, rate)
    length = min(len(ref) - ref_start, len(cand) - cand_start)
    ref = ref[ref_start:ref_start + length]
    cand = cand[cand_start:cand_start + length]
    channels = min(ref.shape[1], cand.shape[1])
    ref, cand = ref[:, :channels], cand[:, :channels]

    # Compare after the chirp, which both recordings contain unprocessed.
    body = slice(int(CHIRP_SECONDS * rate), length)
    r, c = ref[body], cand[body]

    # Remaining latency (e.g. an effect's processing delay), within +-50 ms.
    max_lag = int(0.05 * rate)
    corr = signal.correlate(c.mean(axis=1), r.mean(axis=1), mode="full", method="fft")
    mid = len(r) - 1
    lag = int(np.argmax(corr[mid - max_lag:mid + max_lag + 1]) - max_lag)
    if lag > 0:
        c, r = c[lag:], r[: len(r) - lag]
    elif lag < 0:
        r, c = r[-lag:], c[: len(c) + lag]

    ref_rms = rms(r)
    null_raw = db(rms(r - c) / max(ref_rms, EPS))
    gain = float(np.sum(r * c) / max(np.sum(c * c), EPS))
    null_matched = db(rms(r - gain * c) / max(ref_rms, EPS))

    ref_bands = third_octave_levels(r.mean(axis=1), rate)
    cand_bands = third_octave_levels(c.mean(axis=1), rate)
    band_diff = {f: round(cand_bands[f] - ref_bands[f], 2) for f in ref_bands if f in cand_bands}

    results = {
        "reference": args.reference,
        "candidate": args.candidate,
        "rate": rate,
        "channels": channels,
        "residual_latency_samples": lag,
        "null_depth_db": round(null_raw, 2),
        "best_gain_db": round(db(abs(gain)), 2),
        "null_depth_gain_matched_db": round(null_matched, 2),
        "max_band_difference_db": max((abs(v) for v in band_diff.values()), default=0.0),
        "band_difference_db": band_diff,
        "threshold_db": args.threshold,
        "pass": null_matched <= args.threshold,
    }
    if args.content_end is not None:
        content_end = int(args.content_end * rate)
        results["tail_seconds"] = {
            "reference": round(tail_seconds(r.mean(axis=1), rate, content_end), 3),
            "candidate": round(tail_seconds(c.mean(axis=1), rate, content_end), 3),
        }

    print(f"null depth: {results['null_depth_db']} dB raw, "
          f"{results['null_depth_gain_matched_db']} dB gain-matched "
          f"(gain {results['best_gain_db']} dB, latency {lag} samples)")
    print(f"max third-octave difference: {results['max_band_difference_db']} dB")
    if "tail_seconds" in results:
        print(f"tail: reference {results['tail_seconds']['reference']} s, "
              f"candidate {results['tail_seconds']['candidate']} s")
    print("PASS" if results["pass"] else "FAIL")
    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(results, f, indent=2)
    return 0 if results["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
