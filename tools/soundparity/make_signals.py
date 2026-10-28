#!/usr/bin/env python3
"""Writes the test-signal pack for measuring rekordbox's effects (WS-F).

Every file is 16-bit PCM WAV (the most widely accepted format) and starts with
the same alignment chirp, so that a rekordbox capture and a Mix Two capture of
the same signal can be lined up sample-accurately by compare.py. Then follow
two bars of silence, the test content, and a long silent tail so that delay
and reverb tails are fully recorded.

All rhythmic content is at a fixed tempo (default 128 BPM). Set the track's
BPM to exactly that value in rekordbox (GRID panel) before capturing, because
rekordbox may not detect a tempo in signals like sweeps or impulses.

Usage: make_signals.py OUTPUT_DIR [--rate 44100] [--bpm 128]
"""

import argparse
import json
import os

import numpy as np
from scipy.io import wavfile

CHIRP_SECONDS = 0.5
PEAK = 0.5  # -6 dBFS, headroom for effects that add gain


def chirp(rate):
    """Linear 1-8 kHz chirp with a raised-cosine fade; the alignment marker."""
    t = np.arange(int(CHIRP_SECONDS * rate)) / rate
    f0, f1 = 1000.0, 8000.0
    phase = 2 * np.pi * (f0 * t + (f1 - f0) * t * t / (2 * CHIRP_SECONDS))
    window = 0.5 - 0.5 * np.cos(2 * np.pi * t / CHIRP_SECONDS)
    return np.sin(phase) * window


def silence(seconds, rate):
    return np.zeros(int(seconds * rate))


def impulses(rate, beat, bars, every_bars):
    """Single-sample impulses every `every_bars` bars (delay taps, reverbs)."""
    out = silence(bars * 4 * beat, rate)
    step = int(every_bars * 4 * beat * rate)
    out[::step] = 1.0
    return out


def clicks(rate, beat, bars):
    """A 2 ms 1 kHz click on every beat, accented on the downbeat."""
    out = silence(bars * 4 * beat, rate)
    t = np.arange(int(0.002 * rate)) / rate
    click = np.sin(2 * np.pi * 1000 * t) * np.hanning(len(t))
    for i in range(bars * 4):
        start = int(round(i * beat * rate))
        gain = 1.0 if i % 4 == 0 else 0.6
        out[start:start + len(click)] += gain * click
    return out


def sweep(rate, seconds=10.0, f0=20.0, f1=20000.0):
    """Exponential sine sweep (filter and EQ responses)."""
    t = np.arange(int(seconds * rate)) / rate
    k = np.log(f1 / f0)
    out = np.sin(2 * np.pi * f0 * seconds / k * (np.exp(t * k / seconds) - 1))
    fade = int(0.01 * rate)
    out[:fade] *= np.linspace(0, 1, fade)
    out[-fade:] *= np.linspace(1, 0, fade)
    return out


def pink_noise(rate, seconds, seed=1):
    """Pink noise (modulated filters: flanger, phaser, Mobius, filter sweeps)."""
    rng = np.random.default_rng(seed)
    n = int(seconds * rate)
    spectrum = np.fft.rfft(rng.standard_normal(n))
    freqs = np.fft.rfftfreq(n, 1 / rate)
    freqs[0] = freqs[1]
    out = np.fft.irfft(spectrum / np.sqrt(freqs), n)
    return out / np.max(np.abs(out))


def sine(rate, seconds, freq=440.0):
    """Steady sine (pitch effects, vinyl brake, Master Tempo)."""
    t = np.arange(int(seconds * rate)) / rate
    out = np.sin(2 * np.pi * freq * t)
    fade = int(0.01 * rate)
    out[:fade] *= np.linspace(0, 1, fade)
    out[-fade:] *= np.linspace(1, 0, fade)
    return out


def drum_loop(rate, beat, bars, seed=2):
    """Synthetic kick, snare and hi-hat loop for listening tests."""
    rng = np.random.default_rng(seed)
    out = silence(bars * 4 * beat, rate)
    kick_t = np.arange(int(0.25 * rate)) / rate
    kick = np.sin(2 * np.pi * (50 + 100 * np.exp(-kick_t * 30)) * kick_t) * np.exp(-kick_t * 12)
    snare_t = np.arange(int(0.2 * rate)) / rate
    snare = rng.standard_normal(len(snare_t)) * np.exp(-snare_t * 25) * 0.6
    hat_t = np.arange(int(0.05 * rate)) / rate
    hat = np.diff(rng.standard_normal(len(hat_t) + 1)) * np.exp(-hat_t * 80) * 0.25
    for i in range(bars * 4):
        start = int(round(i * beat * rate))
        out[start:start + len(kick)] += kick
        if i % 2 == 1:
            out[start:start + len(snare)] += snare
        for eighth in (0, 0.5):
            hs = start + int(eighth * beat * rate)
            out[hs:hs + len(hat)] += hat
    return out / np.max(np.abs(out))


def build(content, rate, beat, tail_bars):
    """Marker chirp, two bars of silence, content, then a silent tail."""
    parts = [chirp(rate), silence(8 * beat, rate), content, silence(tail_bars * 4 * beat, rate)]
    return np.concatenate(parts)


def write(path, signal, rate):
    data = np.clip(signal * PEAK, -1.0, 1.0)
    stereo = np.stack([data, data], axis=1)
    wavfile.write(path, rate, (stereo * 32767).astype(np.int16))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("output_dir")
    parser.add_argument("--rate", type=int, default=44100)
    parser.add_argument("--bpm", type=float, default=128.0)
    args = parser.parse_args()

    rate, bpm = args.rate, args.bpm
    beat = 60.0 / bpm
    os.makedirs(args.output_dir, exist_ok=True)

    signals = {
        "01_impulses": (impulses(rate, beat, bars=16, every_bars=4), 8,
                "Impulse every 4 bars: delay taps, feedback, reverb tails"),
        "02_clicks": (clicks(rate, beat, bars=16), 8,
                "Click on every beat: beat-synced effects (trans, roll, gate)"),
        "03_sweep": (sweep(rate), 4,
                "Exponential sine sweep 20 Hz-20 kHz: filters and EQ"),
        "04_pink_noise": (pink_noise(rate, 16 * 4 * beat), 8,
                "Pink noise, 16 bars: flanger, phaser, Mobius, filter sweeps"),
        "05_sine_440": (sine(rate, 16 * 4 * beat), 8,
                "440 Hz sine, 16 bars: pitch, spiral, vinyl brake, Master Tempo"),
        "06_drum_loop": (drum_loop(rate, beat, bars=16), 8,
                "Synthetic drum loop, 16 bars: listening and A/B tests"),
    }
    manifest = {"rate": rate, "bpm": bpm, "chirp_seconds": CHIRP_SECONDS,
            "content_offset_seconds": CHIRP_SECONDS + 8 * beat, "files": {}}
    for name, (content, tail_bars, description) in signals.items():
        path = os.path.join(args.output_dir, f"{name}_{int(bpm)}bpm.wav")
        write(path, build(content, rate, beat, tail_bars), rate)
        manifest["files"][os.path.basename(path)] = description
        print(f"wrote {path}")
    with open(os.path.join(args.output_dir, "signals.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)


if __name__ == "__main__":
    main()
