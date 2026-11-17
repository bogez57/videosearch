# The sound model against sound whose content is known.
#
#   python sound.py embed MODEL [--cpu]     the vectors, kept in BENCH/emb/<MODEL>/sound_*.npy
#   python sound.py esc MODEL               ESC-50: 2000 five-second clips of 50 kinds of sound
#   python sound.py mix MODEL               ESC-50 sounds mixed into the test videos' own sound
#
# MODEL is a folder in BENCH/models with onnx/audio_model.onnx, onnx/text_model.onnx and CLAP's
# tokenizer files. The sound is got ready as the program does it (embed_sound.c, ported below).
#
# mix: the test videos' sound in the five-second stretches the program indexes is the library. A
# sound from ESC-50 of a kind the videos don't have is mixed into a stretch, as loud as the stretch
# (0 dB), louder (+10) or quieter (-10), and searched for by words. Reported: how well the stretch
# with the sound in it ranks (mAP), and where scores fall for stretches with the sound, stretches
# without it, and words that are not about sound at all - which is what a cutoff has to separate.
import csv
import json
import os
import sys
import wave

import numpy as np

from common import BENCH, HERE, read_lines
from models import _session, _unit

RATE = 48000
TEN = 480000

# ---- embed_sound.c, in numpy


def _h2m(f):
    f = np.asarray(f, np.float64)
    return np.where(f >= 1000.0, 15.0 + np.log(np.maximum(f, 1e-9) / 1000.0) * (27.0 / np.log(6.4)), 3.0 * f / 200.0)


def _m2h(m):
    m = np.asarray(m, np.float64)
    return np.where(m >= 15.0, 1000.0 * np.exp((np.log(6.4) / 27.0) * (m - 15.0)), 200.0 * m / 3.0)


_ff = _m2h(np.linspace(_h2m(50.0), _h2m(14000.0), 66))
_fft = np.linspace(0, 24000, 513)
_sl = _ff[None, :] - _fft[:, None]
_d = np.diff(_ff)
_FB = np.maximum(0, np.minimum(-_sl[:, :-2] / _d[:-1], _sl[:, 2:] / _d[1:])) * (2.0 / (_ff[2:] - _ff[:-2]))[None, :]
_WIN = 0.5 - 0.5 * np.cos(2 * np.pi * np.arange(1024) / 1024)


def features(w):
    """Up to ten seconds at 48 kHz -> [1001, 64], a shorter sound repeated whole to fill it."""
    w = np.asarray(w, np.float64)[:TEN]
    w = np.tile(w, TEN // len(w))
    w = np.pad(w, (0, TEN - len(w)))
    w = np.pad(w, (512, 512), mode="reflect")
    idx = np.arange(1001)[:, None] * 480 + np.arange(1024)[None, :]
    P = np.abs(np.fft.rfft(w[idx] * _WIN, axis=1)) ** 2
    return (10 * np.log10(np.maximum(1e-10, P @ _FB))).astype(np.float32)


# ---- the model


class SoundModel:
    def __init__(self, name, cpu=False):
        self.name = name
        self.dir = os.path.join(BENCH, "models", name)
        self.cpu = cpu
        self.audio = None
        self.text = None

    def embed_audio(self, waves, batch=8):
        if self.audio is None:
            self.audio = _session(os.path.join(self.dir, "onnx", "audio_model.onnx"), self.cpu)
        out = []
        for i in range(0, len(waves), batch):
            f = np.stack([features(w) for w in waves[i : i + batch]])[:, None]
            feeds = {}
            for inp in self.audio.get_inputs():
                if inp.name == "input_features":
                    feeds[inp.name] = f
                elif inp.name == "is_longer":
                    feeds[inp.name] = np.zeros((len(f), 1), bool)
                else:
                    raise RuntimeError("audio input not known: " + inp.name)
            out.append(self.audio.run(["audio_embeds"], feeds)[0])
        return _unit(np.concatenate(out).astype(np.float32))

    def embed_texts(self, texts, batch=32):
        if self.text is None:
            from transformers import AutoTokenizer

            self.tok = AutoTokenizer.from_pretrained(self.dir)
            self.text = _session(os.path.join(self.dir, "onnx", "text_model.onnx"), True)
        out = []
        for i in range(0, len(texts), batch):
            enc = self.tok(list(texts[i : i + batch]), padding=True, truncation=True, max_length=77, return_tensors="np")
            feeds = {inp.name: enc[inp.name].astype(np.int64) for inp in self.text.get_inputs()}
            out.append(self.text.run(["text_embeds"], feeds)[0])
        return _unit(np.concatenate(out).astype(np.float32))


# ---- the sound


def read_wav(path):
    with wave.open(path, "rb") as f:
        n, ch, width, rate = f.getnframes(), f.getnchannels(), f.getsampwidth(), f.getframerate()
        raw = f.readframes(n)
    assert width == 2, path
    x = np.frombuffer(raw, np.int16).astype(np.float32) / 32768.0
    if ch > 1:
        x = x.reshape(-1, ch).mean(1)
    return x, rate


def esc_dir():
    root = os.path.join(BENCH, "esc50")
    for d, _, files in os.walk(root):
        if "esc50.csv" in files:
            return os.path.dirname(d)
    raise RuntimeError("no esc50.csv under " + root)


def esc_clips():
    """[(file path, kind of sound)] in the csv's order."""
    d = esc_dir()
    with open(os.path.join(d, "meta", "esc50.csv"), newline="") as f:
        return [(os.path.join(d, "audio", r["filename"]), r["category"]) for r in csv.DictReader(f)]


def esc_wave(path):
    from scipy.signal import resample_poly

    x, rate = read_wav(path)
    assert rate == 44100, (path, rate)
    return resample_poly(x, 160, 147).astype(np.float32)


def stretches():
    """The test videos' sound as the program cuts it: five seconds at a time. [(video, second, samples)]"""
    out = []
    d = os.path.join(BENCH, "audio")
    for name in sorted(os.listdir(d)):
        if not name.endswith(".wav"):
            continue
        x, rate = read_wav(os.path.join(d, name))
        assert rate == RATE, (name, rate)
        for i in range(0, len(x) - 5 * RATE + 1, 5 * RATE):
            s = x[i : i + 5 * RATE]
            if np.abs(s).max() >= 0.003:
                out.append((int(name[1:-4]), i // RATE, s))
    return out


# The kinds of ESC-50 sound mixed in (ones the test videos do not have), and words a person might type.
MIX = {
    "dog": "a dog barking",
    "rooster": "a rooster crowing",
    "pig": "a pig oinking",
    "cow": "a cow mooing",
    "frog": "a frog croaking",
    "hen": "a hen clucking",
    "sheep": "a sheep bleating",
    "crow": "a crow cawing",
    "crying_baby": "a baby crying",
    "sneezing": "a person sneezing",
    "snoring": "someone snoring",
    "toilet_flush": "a toilet flushing",
    "vacuum_cleaner": "a vacuum cleaner",
    "washing_machine": "a washing machine",
    "clock_alarm": "an alarm clock ringing",
    "keyboard_typing": "typing on a keyboard",
    "door_wood_knock": "knocking on a door",
    "glass_breaking": "glass breaking",
    "chainsaw": "a chainsaw",
    "church_bells": "church bells ringing",
    "car_horn": "a car horn",
    "train": "a train passing",
    "hand_saw": "sawing wood",
    "brushing_teeth": "someone brushing their teeth",
}
PER_KIND = 8
SNRS = [10, 0, -10]

# Words about what is seen, not heard: what the sound scores for these should fall under any cutoff.
SEEN = [
    "the color red", "a city skyline", "text on a black background", "a close up of a face", "a sunset",
    "a man wearing a hat", "a woman in a blue dress", "a soldier in a trench", "a desert", "a palace",
    "a tall building", "a field of grass", "a mountain", "a table and chairs", "a beautiful painting",
    "a map of the world", "a smiling child", "an old photograph", "a bright light", "a dark room",
    "a white horse standing", "a flag", "a book on a shelf", "a blue sky with clouds", "a crowd of people",
    "a man with a beard", "a red car parked", "a staircase", "a bridge over a river", "the moon at night",
]
# Words that say nothing, as search.c's bank.
NOTHING = ["a sound", "audio", "a recording", "something", "a video", "a", "the", "noise", "a photo of something", "this is a sound"]


def _rms(x):
    return float(np.sqrt(np.mean(np.square(x)) + 1e-12))


def embed(name, cpu):
    m = SoundModel(name, cpu)
    d = os.path.join(BENCH, "emb", name)
    os.makedirs(d, exist_ok=True)

    clips = esc_clips()
    path = os.path.join(d, "sound_esc.npy")
    if not os.path.exists(path):
        vecs = []
        for i in range(0, len(clips), 64):
            vecs.append(m.embed_audio([esc_wave(p) for p, _ in clips[i : i + 64]]))
            print("esc", i, flush=True)
        np.save(path, np.concatenate(vecs))

    st = stretches()
    np.save(os.path.join(d, "sound_bg5.npy"), m.embed_audio([s for _, _, s in st]))
    json.dump([[v, t] for v, t, _ in st], open(os.path.join(d, "sound_bg.json"), "w"))
    # Ten seconds at a time: each stretch with the one after it in the same video.
    pairs = [i for i in range(len(st) - 1) if st[i][0] == st[i + 1][0] and st[i + 1][1] == st[i][1] + 5]
    np.save(os.path.join(d, "sound_bg10.npy"), m.embed_audio([np.concatenate([st[i][2], st[i + 1][2]]) for i in pairs]))

    rng = np.random.default_rng(1)
    meta = []
    w5 = {s: [] for s in SNRS}
    w10 = {s: [] for s in SNRS}
    for kind in MIX:
        files = [p for p, k in clips if k == kind][:PER_KIND]
        for p in files:
            e = esc_wave(p)[: 5 * RATE]
            e = np.pad(e, (0, 5 * RATE - len(e)))
            loud = e[np.abs(e) > 1e-3]
            j = pairs[int(rng.integers(len(pairs)))]
            b = st[j][2]
            meta.append([kind, st[j][0], st[j][1]])
            for s in SNRS:
                g = _rms(b) * 10 ** (s / 20) / _rms(loud if len(loud) else e)
                x = np.clip(b + g * e, -1, 1)
                w5[s].append(x)
                w10[s].append(np.concatenate([x, st[j + 1][2]]))
    json.dump(meta, open(os.path.join(d, "sound_mix.json"), "w"))
    for s in SNRS:
        np.save(os.path.join(d, "sound_mix5_%d.npy" % s), m.embed_audio(w5[s]))
        np.save(os.path.join(d, "sound_mix10_%d.npy" % s), m.embed_audio(w10[s]))
        print("mix", s, flush=True)

    texts = {}
    kinds = sorted(set(k for _, k in clips))
    for k in kinds:
        w = k.replace("_", " ")
        for t in (w, "the sound of " + w, "This is a sound of " + w + "."):
            texts[t] = 1
    for t in list(MIX.values()) + SEEN + NOTHING + read_lines(os.path.join(HERE, "bank_sound.txt")):
        texts[t] = 1
    keys = list(texts)
    np.save(os.path.join(d, "sound_texts.npy"), m.embed_texts(keys))
    json.dump(keys, open(os.path.join(d, "sound_texts.json"), "w"))
    print("done", name)


def _ap(s, rel):
    order = np.argsort(-s, kind="stable")
    r = rel[order]
    hits = np.cumsum(r)
    return float((hits[r] / (np.nonzero(r)[0] + 1)).mean())


def _load_texts(d):
    keys = json.load(open(os.path.join(d, "sound_texts.json")))
    vecs = np.load(os.path.join(d, "sound_texts.npy"))
    return {k: vecs[i] for i, k in enumerate(keys)}


def esc(name):
    d = os.path.join(BENCH, "emb", name)
    clips = esc_clips()
    A = np.load(os.path.join(d, "sound_esc.npy"))
    T = _load_texts(d)
    kinds = sorted(set(k for _, k in clips))
    y = np.array([kinds.index(k) for _, k in clips])
    bank = np.stack([T[t] for t in read_lines(os.path.join(HERE, "bank_sound.txt"))])
    nothing = np.stack([T[t] for t in NOTHING])
    print(name, "ESC-50,", len(clips), "clips")
    for label, fmt in (("the kind's name", "{}"), ("the sound of ...", "the sound of {}"), ("This is a sound of ... .", "This is a sound of {}.")):
        Q = np.stack([T[fmt.format(k.replace("_", " "))] for k in kinds])
        S = Q @ A.T  # [50, 2000]
        acc = float((S.argmax(0) == y).mean())
        for norm, X in (("raw", S), ("less nothing-words", S - (nothing @ A.T).mean(0)), ("less the bank", S - (bank @ A.T).mean(0))):
            aps = [_ap(X[c], y == c) for c in range(len(kinds))]
            rel = (y[None, :] == np.arange(len(kinds))[:, None]).ravel()
            pos, neg = X.ravel()[rel], X.ravel()[~rel]
            print(
                "  %-26s %-19s right kind %.3f  mAP %.3f  uAP %.3f | match p5/p50 %.2f %.2f  no match p50/p95/p99.9 %.2f %.2f %.2f"
                % (label, norm, acc, np.mean(aps), _ap(X.ravel(), rel), *np.percentile(pos, [5, 50]), *np.percentile(neg, [50, 95, 99.9]))
            )


def mix(name):
    d = os.path.join(BENCH, "emb", name)
    T = _load_texts(d)
    meta = json.load(open(os.path.join(d, "sound_mix.json")))
    kinds = list(MIX)
    y = np.array([kinds.index(m[0]) for m in meta])
    Q = np.stack([T[MIX[k]] for k in kinds])
    seen = np.stack([T[t] for t in SEEN])
    bank = np.stack([T[t] for t in read_lines(os.path.join(HERE, "bank_sound.txt"))])
    nothing = np.stack([T[t] for t in NOTHING])
    print(name, "- ESC-50 sounds mixed into the videos' sound,", len(meta), "mixes at each loudness")
    for win in ("5", "10"):
        BG = np.load(os.path.join(d, "sound_bg%s.npy" % win))
        for norm, base in (("raw", None), ("less nothing-words", nothing), ("less the bank", bank)):
            def sc(q, a):
                s = q @ a.T
                return s if base is None else s - (base @ a.T).mean(0, keepdims=True)

            bg = sc(Q, BG)  # the videos' own sound, for the sounds' words
            bg_seen = sc(seen, BG)  # and for words about what is seen
            line = "  %2s s  %-19s" % (win, norm)
            for s in SNRS:
                MX = np.load(os.path.join(d, "sound_mix%s_%d.npy" % (win, s)))
                mx = sc(Q, MX)  # [kinds, mixes]
                aps = []
                for c in range(len(kinds)):
                    scores = np.concatenate([mx[c], bg[c]])
                    rel = np.concatenate([y == c, np.zeros(bg.shape[1], bool)])
                    aps.append(_ap(scores, rel))
                pos = mx[y, np.arange(len(y))]
                line += " | %+3d dB mAP %.2f match p10/p50 %.2f %.2f" % (s, np.mean(aps), *np.percentile(pos, [10, 50]))
            line += " | no match p50/p99/max %.2f %.2f %.2f | seen-words p50/p99/max %.2f %.2f %.2f" % (
                *np.percentile(bg, [50, 99]), bg.max(), *np.percentile(bg_seen, [50, 99]), bg_seen.max())
            print(line)


def main():
    cmd, name = sys.argv[1], sys.argv[2]
    if cmd == "embed":
        embed(name, "--cpu" in sys.argv)
    elif cmd == "esc":
        esc(name)
    elif cmd == "mix":
        mix(name)


if __name__ == "__main__":
    sys.exit(main())
