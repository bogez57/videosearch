# The sound model against sound whose content is known.
#
#   python sound.py embed MODEL [--cpu]     the vectors, kept in BENCH/emb/<MODEL>/sound_*.npy
#   python sound.py esc MODEL               ESC-50: 2000 five-second clips of 50 kinds of sound
#   python sound.py mix MODEL               ESC-50 sounds mixed into the test videos' own sound
#   python sound.py words MODEL             which words the model's text half takes to be about sound
#   python sound.py real MODEL              the test videos' own sound, searched for what the labels
#                                           say can be seen where a sound goes with it (singing, ...)
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

from common import BENCH, HERE, load_labels, matches, read_lines
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

    def texts(self, texts):
        """As embed_texts, kept in BENCH/emb/<name>/sound_texts.* between runs."""
        d = os.path.join(BENCH, "emb", self.name)
        os.makedirs(d, exist_ok=True)
        keys_path, vec_path = os.path.join(d, "sound_texts.json"), os.path.join(d, "sound_texts.npy")
        keys, vecs = [], None
        if os.path.exists(keys_path) and os.path.exists(vec_path):
            keys = json.load(open(keys_path, encoding="utf-8"))
            vecs = np.load(vec_path)
        at = {k: i for i, k in enumerate(keys)}
        new = [t for t in dict.fromkeys(texts) if t not in at]
        if new:
            add = self.embed_texts(new)
            vecs = add if vecs is None else np.concatenate([vecs, add])
            keys += new
            at = {k: i for i, k in enumerate(keys)}
            np.save(vec_path, vecs)
            json.dump(keys, open(keys_path, "w", encoding="utf-8"))
        return vecs[[at[t] for t in texts]]


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


TEMPLATES = {"plain": "{}", "sound_of": "the sound of {}", "this_is": "This is a sound of {}."}


def _halves(w):
    h = len(w) // 2
    return [w[:h], w[h:]]


def embed(name, cpu, force=False):
    m = SoundModel(name, cpu)
    d = os.path.join(BENCH, "emb", name)
    os.makedirs(d, exist_ok=True)

    def keep(fname, make):
        path = os.path.join(d, fname)
        if force or not os.path.exists(path):
            np.save(path, make())
            print(fname, flush=True)

    clips = esc_clips()

    def esc_all():
        vecs = []
        for i in range(0, len(clips), 64):
            vecs.append(m.embed_audio([esc_wave(p) for p, _ in clips[i : i + 64]]))
        return np.concatenate(vecs)

    keep("sound_esc.npy", esc_all)

    st = stretches()
    at = [[v, t] for v, t, _ in st]
    bg_json = os.path.join(d, "sound_bg.json")
    if os.path.exists(bg_json) and json.load(open(bg_json)) != at:
        force = True  # the videos have changed
    json.dump(at, open(bg_json, "w"))
    keep("sound_bg5.npy", lambda: m.embed_audio([s for _, _, s in st]))
    # Ten seconds at a time: each stretch with the one after it in the same video.
    pairs = [i for i in range(len(st) - 1) if st[i][0] == st[i + 1][0] and st[i + 1][1] == st[i][1] + 5]
    keep("sound_bg10.npy", lambda: m.embed_audio([np.concatenate([st[i][2], st[i + 1][2]]) for i in pairs]))
    # Two and a half seconds at a time: [stretches, 2, D].
    keep("sound_bg2.npy", lambda: m.embed_audio([h for _, _, s in st for h in _halves(s)]).reshape(len(st), 2, -1))

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
        keep("sound_mix5_%d.npy" % s, lambda: m.embed_audio(w5[s]))
        keep("sound_mix10_%d.npy" % s, lambda: m.embed_audio(w10[s]))
        keep("sound_mix2_%d.npy" % s, lambda: m.embed_audio([h for x in w5[s] for h in _halves(x)]).reshape(len(w5[s]), 2, -1))
    print("done", name)


def _ap(s, rel):
    order = np.argsort(-s, kind="stable")
    r = rel[order]
    hits = np.cumsum(r)
    return float((hits[r] / (np.nonzero(r)[0] + 1)).mean())


def esc(name):
    d = os.path.join(BENCH, "emb", name)
    m = SoundModel(name, True)
    clips = esc_clips()
    A = np.load(os.path.join(d, "sound_esc.npy"))
    kinds = sorted(set(k for _, k in clips))
    y = np.array([kinds.index(k) for _, k in clips])
    print(name, "ESC-50,", len(clips), "clips")
    for label, fmt in TEMPLATES.items():
        Q = m.texts([fmt.format(k.replace("_", " ")) for k in kinds])
        bank = m.texts([fmt.format(t) for t in read_lines(os.path.join(HERE, "bank_sound.txt"))])
        nothing = m.texts(NOTHING)
        S = Q @ A.T  # [50, 2000]
        acc = float((S.argmax(0) == y).mean())
        for norm, X in (("raw", S), ("less nothing-words", S - (nothing @ A.T).mean(0)), ("less the bank", S - (bank @ A.T).mean(0))):
            aps = [_ap(X[c], y == c) for c in range(len(kinds))]
            rel = (y[None, :] == np.arange(len(kinds))[:, None]).ravel()
            pos, neg = X.ravel()[rel], X.ravel()[~rel]
            print(
                "  %-9s %-19s right kind %.3f  mAP %.3f  uAP %.3f | match p5/p50 %.2f %.2f  no match p50/p95/p99.9 %.2f %.2f %.2f"
                % (label, norm, acc, np.mean(aps), _ap(X.ravel(), rel), *np.percentile(pos, [5, 50]), *np.percentile(neg, [50, 95, 99.9]))
            )


def _centered(T, TB):
    c = TB.mean(0, keepdims=True)
    return _unit(T - c), _unit(TB - c)


def _norm(norm, S, SB, W):
    """S [Q, N]: the queries' scores. SB [K, N]: the bank's. W [Q, K]: 1 where a bank entry may stand
    against the query (it does not say nearly the same)."""
    if norm == "raw":
        return S
    if norm == "fb":  # less the bank's mean
        return S - SB.mean(0, keepdims=True)
    if norm.startswith("sm"):  # the query against the bank, as a softmax at this temperature
        tau = float(norm[2:])
        mx = (tau * SB).max(0, keepdims=True)
        E = np.exp(tau * SB - mx)
        return tau * S - (mx + np.log(np.maximum(W @ E, 1e-30)))
    if norm.startswith("mk"):  # how far over the bank's k-th best
        k = int(norm[2:])
        out = np.empty_like(S)
        for q in range(len(S)):
            B = SB[W[q] > 0]
            out[q] = S[q] - np.sort(B, axis=0)[-k]
        return out
    raise ValueError(norm)


def mix(name, argv):
    d = os.path.join(BENCH, "emb", name)
    m = SoundModel(name, True)
    meta = json.load(open(os.path.join(d, "sound_mix.json")))
    kinds = list(MIX)
    y = np.array([kinds.index(x[0]) for x in meta])
    thr = 0.6
    templates = ["plain", "this_is"]
    windows = ["5", "2", "10"]
    norms = ["raw", "fb", "sm15", "sm30", "sm60", "mk1", "mk3", "mk5"]
    for a in argv:
        if a.startswith("--templates="):
            templates = a.split("=")[1].split(",")
        if a.startswith("--windows="):
            windows = a.split("=")[1].split(",")
        if a.startswith("--norms="):
            norms = a.split("=")[1].split(",")
        if a.startswith("--thr="):
            thr = float(a.split("=")[1])
    bank_words = read_lines(os.path.join(HERE, "bank_sound.txt"))
    print(name, "- ESC-50 sounds mixed into the videos' sound,", len(meta), "mixes at each loudness; bank of", len(bank_words))
    print("  columns: mAP at +10 / 0 / -10 dB | of the mixes at +10 and at 0 dB, the share scoring over 99% of: stretches without the sound (R-no), words about what is seen (R-seen)")
    for template in templates:
        fmt = TEMPLATES[template]
        Q = m.texts([fmt.format(MIX[k]) for k in kinds])
        seen = m.texts([fmt.format(t) for t in SEEN])
        bank = m.texts([fmt.format(t) for t in bank_words])
        Qc, Bc = _centered(Q, bank)
        Sc, _ = _centered(seen, bank)
        WQ = ((Qc @ Bc.T) < thr).astype(np.float32)
        WS = ((Sc @ Bc.T) < thr).astype(np.float32)
        print("  template %s: bank entries left out as saying the same, a query: %.1f (sound words), %.1f (seen words)"
              % (template, (1 - WQ).sum(1).mean(), (1 - WS).sum(1).mean()))
        for win in windows:
            def sims(q, file):
                A = np.load(os.path.join(d, file))
                if A.ndim == 3:  # halves: the better of the two
                    return np.maximum(q @ A[:, 0].T, q @ A[:, 1].T)
                return q @ A.T

            for norm in norms:
                bg = _norm(norm, sims(Q, "sound_bg%s.npy" % win), sims(bank, "sound_bg%s.npy" % win), WQ)
                bg_seen = _norm(norm, sims(seen, "sound_bg%s.npy" % win), sims(bank, "sound_bg%s.npy" % win), WS)
                cut_no, cut_seen = np.percentile(bg, 99), np.percentile(bg_seen, 99)
                maps, recalls = [], []
                for s in SNRS:
                    f = "sound_mix%s_%d.npy" % (win, s)
                    mx = _norm(norm, sims(Q, f), sims(bank, f), WQ)
                    aps = []
                    for c in range(len(kinds)):
                        aps.append(_ap(np.concatenate([mx[c], bg[c]]), np.concatenate([y == c, np.zeros(bg.shape[1], bool)])))
                    maps.append(np.mean(aps))
                    pos = mx[y, np.arange(len(y))]
                    recalls.append((float((pos > cut_no).mean()), float((pos > cut_seen).mean())))
                print("    %-8s %3s s %-5s mAP %.2f %.2f %.2f | +10: R-no %.2f R-seen %.2f | 0: R-no %.2f R-seen %.2f"
                      % (template, win, norm, *maps, *recalls[0], *recalls[1]))


def words(name):
    """Does the text half alone tell words about sound from words about what is seen?"""
    m = SoundModel(name, True)
    bank_s = read_lines(os.path.join(HERE, "bank_sound.txt"))
    bank_v = read_lines(os.path.join(HERE, "bank.txt"))
    qs = [l.split("\t")[0].strip() for l in read_lines(os.path.join(HERE, "queries.txt"))]
    groups = {"sound (mixed-in kinds)": list(MIX.values()), "seen": SEEN, "picture queries": qs}
    TS, TV = m.texts(bank_s), m.texts(bank_v)
    c = np.concatenate([TS, TV]).mean(0, keepdims=True)
    TSc, TVc = _unit(TS - c), _unit(TV - c)
    for g, ws in groups.items():
        T = _unit(m.texts(ws) - c)
        a = np.sort(T @ TSc.T, axis=1)[:, -6:-1].mean(1)  # the five nearest sound words, not itself
        b = np.sort(T @ TVc.T, axis=1)[:, -6:-1].mean(1)
        sc = a - b
        print("%s: n=%d  sound-nearness less picture-nearness: p10 %.2f p50 %.2f p90 %.2f" % (g, len(ws), *np.percentile(sc, [10, 50, 90])))
        if g != "sound (mixed-in kinds)":
            order = np.argsort(-sc)
            print("   most sound-like: " + "; ".join("%s %.2f" % (ws[i], sc[i]) for i in order[:12]))
            print("   least:           " + "; ".join("%s %.2f" % (ws[i], sc[i]) for i in order[-12:]))


# Sounds that go with something the labels say can be seen: words, the frames' tags.
REAL = [
    ("a woman singing", "woman and singing"),
    ("a man singing", "man and singing"),
    ("singing", "singing"),
    ("an acoustic guitar", "playing_guitar"),
    ("a crowd cheering", "audience"),
    ("an explosion", "explosion"),
    ("gunshots", "shooting"),
    ("a man shouting", "man and screaming"),
]


def real(name):
    d = os.path.join(BENCH, "emb", name)
    m = SoundModel(name, True)
    labels = load_labels()
    at = json.load(open(os.path.join(d, "sound_bg.json")))
    A = np.load(os.path.join(d, "sound_bg5.npy"))
    use = [i for i, (v, t) in enumerate(at) if (v, t) in labels]
    at, A = [at[i] for i in use], A[use]
    bank_words = read_lines(os.path.join(HERE, "bank_sound.txt"))
    print(name, "- the videos' own sound,", len(at), "five-second stretches of", len(set(v for v, _ in at)), "videos")
    for template in ("plain", "this_is"):
        fmt = TEMPLATES[template]
        Q = m.texts([fmt.format(w) for w, _ in REAL])
        bank = m.texts([fmt.format(t) for t in bank_words])
        for norm, S in (("raw", Q @ A.T), ("fb", Q @ A.T - (bank @ A.T).mean(0, keepdims=True))):
            aps = []
            lines = []
            for qi, (w, expr) in enumerate(REAL):
                code = compile(expr, "<real>", "eval")
                rel = np.zeros(len(at), bool)
                keep = np.ones(len(at), bool)
                for i, (v, t) in enumerate(at):
                    got = [matches(code, *labels[(v, t + k)][:2]) for k in range(5) if (v, t + k) in labels]
                    rel[i] = any(r for r, _ in got)
                    keep[i] = rel[i] or all(sure for _, sure in got)
                if not rel.any():
                    continue
                ap = _ap(S[qi][keep], rel[keep])
                aps.append(ap)
                order = np.argsort(-S[qi], kind="stable")[:5]
                lines.append("      %-28s n=%-3d AP %.2f  best: %s" % (
                    w, rel.sum(), ap, " ".join("v%d:%d%s %.2f" % (*at[i], "+" if rel[i] else "", S[qi][i]) for i in order)))
            print("  %-8s %-4s mAP %.3f" % (template, norm, np.mean(aps)))
            print("\n".join(lines))


def main():
    cmd, name = sys.argv[1], sys.argv[2]
    if cmd == "embed":
        embed(name, "--cpu" in sys.argv, "--force" in sys.argv)
    elif cmd == "esc":
        esc(name)
    elif cmd == "mix":
        mix(name, sys.argv[3:])
    elif cmd == "words":
        words(name)
    elif cmd == "real":
        real(name)


if __name__ == "__main__":
    sys.exit(main())
