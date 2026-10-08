"""The regeneration sounds for the boards with a speaker: main/sounds.c.

16 kHz mono 16 bit, as the I2S runs. Made here, not at run time:
  CHIME  bell tones, a rising arpeggio at the start, a falling one at the end
  GONG   a low gong at the start, a higher, lighter one at the end
  VOICE  "Regenerace zahájena / ukončena", "Regeneration started / finished",
         espeak-ng with the MBROLA voices cz2 and en1 (MBROLA voices: free
         for non-commercial use)
Needs numpy, espeak-ng, mbrola, mbrola-cz2, mbrola-en1. Run from the
project root: python3 tools/gen_sounds.py
"""
import os
import subprocess
import tempfile
import wave

import numpy as np

RATE = 16000
ROOT = os.path.join(os.path.dirname(__file__), "..")


def bell(f, dur, amp=1.0):
    t = np.arange(int(dur * RATE)) / RATE
    parts = ((1.0, 1.0, 2.2), (2.0, 0.45, 3.5), (2.76, 0.3, 5.0),
             (5.4, 0.12, 8.0), (8.93, 0.05, 12.0))
    s = sum(a * np.exp(-d * t) * np.sin(2 * np.pi * f * m * t) for m, a, d in parts)
    s *= np.minimum(1, t / 0.003)               # 3 ms attack, no click
    return amp * s


def place(total, items):
    out = np.zeros(int(total * RATE))
    for at, snd in items:
        i = int(at * RATE)
        n = min(len(snd), len(out) - i)
        out[i:i + n] += snd[:n]
    return out


def gong(f0, dur, amp=1.0):
    t = np.arange(int(dur * RATE)) / RATE
    parts = ((1.0, 1.0, 0.9), (1.47, 0.6, 1.3), (2.09, 0.5, 1.6),
             (2.56, 0.35, 2.0), (3.42, 0.25, 2.6), (4.6, 0.15, 3.4),
             (6.1, 0.08, 4.5))
    glide = 1 - 0.015 * np.exp(-t * 4)          # the pitch settles a little
    s = sum(a * np.exp(-d * t) * np.sin(2 * np.pi * f0 * m * np.cumsum(glide) / RATE)
            for m, a, d in parts)
    rng = np.random.default_rng(1)
    s += 0.3 * rng.standard_normal(len(t)) * np.exp(-t * 40)   # the strike
    s *= np.minimum(1, t / 0.004)
    return amp * s


def voice(text, v):
    with tempfile.TemporaryDirectory() as d:
        w = os.path.join(d, "v.wav")
        subprocess.run(["espeak-ng", "-v", v, "-s", "135", "-w", w, text],
                       check=True)
        with wave.open(w) as f:
            rate = f.getframerate()
            x = np.frombuffer(f.readframes(f.getnframes()), dtype=np.int16)
    x = x.astype(float) / 32768.0
    if rate != RATE:                            # linear resample
        n = int(len(x) * RATE / rate)
        x = np.interp(np.linspace(0, len(x) - 1, n), np.arange(len(x)), x)
    nz = np.nonzero(np.abs(x) > 0.006)[0]       # trim the silence
    x = x[max(0, nz[0] - 400):nz[-1] + 1600]
    return x


LOUD = 0.16          # RMS of the loudest 0.4 s, of full scale: all the same
PEAK = 0.95


def norm(x):
    """the same loudness for every sound, not the same peak: a gong or a
    bell is a short spike and a long tail, a voice is dense; matched on the
    loudest 0.4 s, and never over PEAK"""
    w = int(0.4 * RATE)
    sq = np.convolve(x * x, np.ones(w) / w, mode="valid") if len(x) > w else [np.mean(x * x)]
    rms = np.sqrt(np.max(sq))
    g = LOUD / rms
    g = min(g, PEAK / np.max(np.abs(x)))
    return np.round(np.clip(x * g, -1, 1) * 32767).astype(np.int16)


C6, E6, G6, C7 = 1046.5, 1318.5, 1568.0, 2093.0
SOUNDS = {
    "chime_start": place(1.6, ((0.0, bell(C6, 1.3)), (0.16, bell(E6, 1.3)),
                               (0.32, bell(G6, 1.3, 1.1)))),
    "chime_end": place(1.6, ((0.0, bell(G6, 1.3)), (0.16, bell(E6, 1.3)),
                             (0.32, bell(C6, 1.3, 1.1)))),
    "gong_start": gong(110, 3.2),
    "gong_end": place(2.6, ((0.0, gong(220, 1.2, 0.7)), (0.45, gong(220, 2.1)))),
    "voice_cs_start": voice("Regenerace zahájena.", "mb-cz2"),
    "voice_cs_end": voice("Regenerace ukončena.", "mb-cz2"),
    "voice_en_start": voice("Regeneration started.", "mb-en1"),
    "voice_en_end": voice("Regeneration finished.", "mb-en1"),
}

lines = ["/* made by tools/gen_sounds.py: 16 kHz mono 16 bit */",
         '#include "sounds.h"', ""]
total = 0
for name, x in SOUNDS.items():
    pcm = norm(x)
    total += len(pcm)
    lines.append("static const int16_t %s[%d] = {" % (name, len(pcm)))
    for i in range(0, len(pcm), 16):
        lines.append("    " + ",".join(str(v) for v in pcm[i:i + 16]) + ",")
    lines.append("};")
    with wave.open(os.path.join(ROOT, "preview", "sound-%s.wav" % name), "w") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(pcm.tobytes())
lines.append("""
const int16_t *rnd_pcm(int sound, int event, int lang, size_t *n)
{
    const int16_t *p = NULL;
    size_t len = 0;
#define PICK(a) do { p = a; len = sizeof a / sizeof a[0]; } while (0)
    bool start = event == RND_EV_REGEN_START;
    switch (sound) {
    case RND_SND_CHIME:
        if (start) PICK(chime_start); else PICK(chime_end);
        break;
    case RND_SND_GONG:
        if (start) PICK(gong_start); else PICK(gong_end);
        break;
    case RND_SND_VOICE:
        if (lang == RND_LANG_CS) {
            if (start) PICK(voice_cs_start); else PICK(voice_cs_end);
        } else {
            if (start) PICK(voice_en_start); else PICK(voice_en_end);
        }
        break;
    default:
        break;
    }
#undef PICK
    if (n) *n = len;
    return p;
}""")
open(os.path.join(ROOT, "main", "sounds.c"), "w").write("\n".join(lines) + "\n")
print("wrote main/sounds.c, %.1f s of sound, %d kB" % (total / RATE, total * 2 // 1024))
