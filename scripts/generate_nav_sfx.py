#!/usr/bin/env python3
"""Generate gentle navigation WAV files for Switch audout.

scrape_complete.wav is only generated when missing — a custom file in
resources/audio/ is preserved across builds.
"""
import math
import os
import struct
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "resources", "audio")


def write_tone(path, freq, duration, volume=0.18, harmonics=2):
    sr = 48000
    n = int(sr * duration)
    attack = int(sr * 0.012)
    release = int(sr * 0.045)
    os.makedirs(OUT_DIR, exist_ok=True)
    with wave.open(path, "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        frames = bytearray()
        for i in range(n):
            t = i / sr
            env = 1.0
            if i < attack:
                env = i / attack
            elif i > n - release:
                env = max(0.0, (n - i) / release)
            sample = math.sin(2 * math.pi * freq * t)
            if harmonics >= 2:
                sample += 0.22 * math.sin(2 * math.pi * freq * 1.5 * t)
            if harmonics >= 3:
                sample += 0.08 * math.sin(2 * math.pi * freq * 2.0 * t)
            sample *= env * volume
            s = int(max(-32767, min(32767, sample * 32767)))
            frames += struct.pack("<h", s)
        w.writeframes(frames)


def write_square_melody(path, notes, volume=0.22):
    """NES-style square-wave melody (notes as (freq_hz, duration_sec))."""
    sr = 48000
    os.makedirs(OUT_DIR, exist_ok=True)
    frames = bytearray()
    for freq, duration in notes:
        n = max(1, int(sr * duration))
        period = max(1, int(sr / freq))
        attack = min(n // 4, int(sr * 0.008))
        release = min(n // 3, int(sr * 0.04))
        for i in range(n):
            env = 1.0
            if i < attack:
                env = i / attack
            elif i > n - release:
                env = max(0.0, (n - i) / release)
            phase = (i % period) / period
            sample = volume if phase < 0.5 else -volume
            sample *= env
            s = int(max(-32767, min(32767, sample * 32767)))
            frames += struct.pack("<h", s)
    with wave.open(path, "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(frames)


def scrape_complete_melody():
    # Double Dragon (NES) stage-clear jingle — staccato ascending fanfare (~3.5s).
    q = 0.085
    return [
        (392, q), (392, q), (392, q), (523, q * 1.25),
        (659, q), (784, q), (988, q * 1.35),
        (784, q), (988, q), (1047, q * 1.5),
        (988, q), (784, q), (659, q), (523, q * 1.8),
        (392, q), (523, q), (659, q), (784, q),
        (988, q), (1047, q * 1.2), (1175, q * 1.4), (1047, q * 2.2),
    ]


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    write_tone(os.path.join(OUT_DIR, "nav.wav"), freq=520, duration=0.055, volume=0.32, harmonics=3)
    write_tone(os.path.join(OUT_DIR, "confirm.wav"), freq=392, duration=0.07, volume=0.34, harmonics=2)
    write_tone(os.path.join(OUT_DIR, "toggle.wav"), freq=660, duration=0.05, volume=0.30, harmonics=2)

    scrape_path = os.path.join(OUT_DIR, "scrape_complete.wav")
    if os.path.isfile(scrape_path) and os.path.getsize(scrape_path) > 0:
        print("kept existing", scrape_path)
    else:
        write_square_melody(scrape_path, scrape_complete_melody())
        print("wrote", scrape_path)

    print("wrote nav/confirm/toggle wav to", OUT_DIR)


if __name__ == "__main__":
    main()
