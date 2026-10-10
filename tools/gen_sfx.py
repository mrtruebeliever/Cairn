#!/usr/bin/env python3
"""Generates Cairn's breathing-phase samples (resources/sfx/*.raw).

A watch speaker note has no envelope: it starts and stops at full level, which
is why a short phase note sounds like a click. These samples carry their own
envelope instead -- a swell that rises and falls -- and are played unshifted
through speaker_play_tracks(), so what is generated here is what you hear.

  breath_in   a warm tone gliding up over a faint breath of air, slow swell
  breath_out  the same drawing back down, with a longer tail
  breath_hold one small bell, struck softly and left to ring out
  bowl        a singing bowl, struck once: the app opening, a finished session,
              a badge and the daily reminder, at different pitches

The breaths are deliberately rounder and a little quieter than the bell: two
sines a few Hz apart beat gently against each other, the second harmonic is
only a trace, and the air is a dull low band rather than a hiss. Pitches stay
at or above A4: a watch speaker barely moves air below ~400 Hz, and that trace
of harmonic is what still makes the tone read on a tiny speaker.
They are computed, not recorded, so there is nothing to license; rerun after
changing them.

Output: mono, signed 8-bit PCM at 16 kHz (SpeakerPcmFormat_16kHz_8bit).

    python3 tools/gen_sfx.py            # writes resources/sfx/*.raw
    python3 tools/gen_sfx.py --wav DIR  # also writes .wav previews to DIR
"""
import argparse
import math
import os
import random
import struct
import wave

RATE = 16000
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'resources', 'sfx')


class Resonator:
    """Two-pole band-pass with a movable centre: colours white noise into air."""
    def __init__(self, bw):
        self.r = math.exp(-math.pi * bw / RATE)
        self.y1 = self.y2 = 0.0

    def __call__(self, x, freq):
        a1 = 2 * self.r * math.cos(2 * math.pi * freq / RATE)
        a2 = -self.r * self.r
        y = (1 - self.r) * x + a1 * self.y1 + a2 * self.y2
        self.y2, self.y1 = self.y1, y
        return y


def swell(x, peak):
    """0 -> 1 -> 0 over x in [0, 1], peaking at `peak`, smooth at both ends."""
    if x < peak:
        return math.sin(0.5 * math.pi * x / peak) ** 2
    return math.cos(0.5 * math.pi * (x - peak) / (1 - peak)) ** 2


def breath(dur, f_from, f_to, peak, air_from, air_to, seed):
    """A warm tone gliding f_from -> f_to over a dull band of air, both swelling."""
    rnd = random.Random(seed)
    n = int(RATE * dur)
    res = Resonator(350)
    out, p1, p2 = [], 0.0, 0.0
    for i in range(n):
        x = i / n
        glide = 0.5 - 0.5 * math.cos(math.pi * x)          # ease in and out
        f = f_from + (f_to - f_from) * glide
        p1 += 2 * math.pi * f / RATE
        p2 += 2 * math.pi * (f + 3.0) / RATE               # 3 Hz apart: a slow beat
        tone = math.sin(p1) + 0.8 * math.sin(p2) + 0.12 * math.sin(2 * p1)
        air = res(rnd.uniform(-1, 1), air_from + (air_to - air_from) * glide) * 9
        out.append((0.88 * tone + 0.12 * air) * swell(x, peak))
    return out


def breath_in():
    return breath(0.60, 440, 587, 0.45, 600, 900, seed=1)


def breath_out():
    return breath(0.70, 587, 440, 0.25, 900, 600, seed=2)


def breath_hold():
    """A soft bell: C5 plus an inharmonic partial, short attack, exponential ring."""
    dur = 0.30
    n = int(RATE * dur)
    out = []
    for i in range(n):
        t = i / RATE
        att = min(1.0, t / 0.006)
        ring = math.exp(-t / 0.075)
        y = (math.sin(2 * math.pi * 523 * t) +
             0.30 * math.sin(2 * math.pi * 1046 * t) * math.exp(-t / 0.04) +
             0.15 * math.sin(2 * math.pi * 1444 * t) * math.exp(-t / 0.025))
        out.append(y * att * ring)
    return out


def bowl():
    """A singing bowl: inharmonic partials (a bowl's modes are not a harmonic
    series), each a pair a hair apart so they beat slowly, the higher ones
    dying first. Struck soft, as with a padded mallet. Cut to 0.9 s with a
    fade, which keeps it inside one speaker_play_tracks() call (16 KB)."""
    dur = 0.90
    n = int(RATE * dur)
    f0 = 440.0   # played at MIDI 69 it sounds as recorded; see BOWL_BASE in sfx.c
    modes = [  # (ratio, level, decay s, beat Hz)
        (1.00, 1.00, 0.60, 1.3),
        (2.71, 0.50, 0.35, 2.1),
        (5.12, 0.22, 0.18, 2.9),
        (8.01, 0.08, 0.10, 3.7),
    ]
    out = []
    for i in range(n):
        t = i / RATE
        att = 0.5 - 0.5 * math.cos(math.pi * min(1.0, t / 0.010))
        y = 0.0
        for ratio, level, tau, beat in modes:
            f = f0 * ratio
            y += level * math.exp(-t / tau) * (
                math.sin(2 * math.pi * f * t) + math.sin(2 * math.pi * (f + beat) * t)) / 2
        tail = min(1.0, (dur - t) / 0.15)
        out.append(y * att * (0.5 - 0.5 * math.cos(math.pi * tail)))
    return out


def to_pcm8(samples, top=120):
    """Normalised so the loudest sample hits `top` (of 127)."""
    peak = max(abs(s) for s in samples) or 1.0
    return b''.join(struct.pack('b', max(-127, min(127, int(round(s / peak * top)))))
                    for s in samples)


def write_wav(path, pcm8):
    """Preview from the 8-bit data itself, so the grain you hear is the real one.
    Padded with silence: players that wake the audio device clip the first few
    hundred ms, which swallows a cue that starts by fading in."""
    pad = b'\0\0' * (RATE // 2)
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pad + b''.join(struct.pack('<h', v * 250)
                               for v in struct.unpack('%db' % len(pcm8), pcm8)) + pad)


# name: (generator, peak level). The breaths sit ~2.5 dB under the bell.
SOUNDS = {
    'breath_in': (breath_in, 90),
    'breath_hold': (breath_hold, 120),
    'breath_out': (breath_out, 90),
    'bowl': (bowl, 115),
}

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--wav', help='also write .wav previews to this directory')
    args = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    for name, (fn, top) in SOUNDS.items():
        pcm = to_pcm8(fn(), top)
        with open(os.path.join(OUT, name + '.raw'), 'wb') as f:
            f.write(pcm)
        if args.wav:
            write_wav(os.path.join(args.wav, name + '.wav'), pcm)
        print('%-11s %5d bytes' % (name, len(pcm)))
