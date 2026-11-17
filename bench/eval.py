# Scores ways of searching the frames against the labels.
#
#   python eval.py CONFIG [CONFIG ...] [--perquery] [--check K] [--dist]
#
# A CONFIG is model|views|agg|template|norm|smooth, each part a comma-separated list (every
# combination is run), e.g.   clip-b32,siglip-b16|center,squash|max|plain,photo|raw|0
#   views:    a name from models.VIEW_SETS
#   agg:      max (a frame scores as its best view), mean (of its views' scores), vec (its views'
#             vectors averaged into one)
#   template: plain, photo, ens (see TEMPLATES)
#   norm:     raw, qmean, qz, fb, fbq, sm, cen, cenv, cent (see normalize())
#   smooth:   0, or 1 (a frame's score mixed with its neighbours' a second each side)
#
# Queries are bench/queries.txt: words <tab> expression over the labels' tags (python: and, or, not),
# or NONE for something that is in no video. A tag a frame is not sure of leaves that frame out of
# the query's sums.
#
# For each config, over the queries that have matching frames:
#   mAP   mean average precision of the frames' ranking
#   S@1   how often the best hit is right
#   P@5   how many of the best five different moments (3 s apart or another video) are right
# and over every query at once, to see whether one score means the same for all of them:
#   uAP   average precision of every (query, frame) pair in one list
#   F1    the best F1 a single cutoff gives on that list
#   AUC   how well a query's best score tells queries with a match from queries without one
import itertools
import os
import sys

import numpy as np

from common import BENCH, HERE, load_labels, read_lines
from models import VIEW_SETS, Model

TEMPLATES = {
    "plain": ["{}"],
    "photo": ["a photo of {}"],
    "frame": ["a video frame of {}"],
    "scene": ["a scene from a movie showing {}"],
    "ens": ["{}", "a photo of {}", "a video frame of {}", "a scene from a movie showing {}", "a picture of {}"],
    "siglip": ["this is a photo of {}."],
}

_models = {}
_views = {}


def model(name):
    if name not in _models:
        _models[name] = Model(name)
        d = os.path.join(BENCH, "emb", name)
        _views[name] = np.load(os.path.join(d, "views.npy")).astype(np.float32)
    return _models[name], _views[name]


def unit(x):
    return x / np.maximum(np.linalg.norm(x, axis=-1, keepdims=True), 1e-12)


def text_vectors(m, queries, template):
    acc = 0
    for t in TEMPLATES[template]:
        acc = acc + m.texts([t.format(q) for q in queries])
    return unit(acc)


def scores(T, V, agg):
    """T [Q, D] against V [N, v, D] -> [Q, N]."""
    if agg == "vec":
        return T @ unit(V.mean(1)).T
    S = np.stack([T @ V[:, j].T for j in range(V.shape[1])])
    return S.max(0) if agg == "max" else S.mean(0)


def normalize(norm, S, SB, T, TB, V, agg, tau):
    """S: the queries' scores [Q, N]. SB: the bank's [K, N] (words that could be about anything)."""
    if norm == "raw":
        return S
    if norm == "qmean":  # less each query's mean over the library
        return S - S.mean(1, keepdims=True)
    if norm == "qz":  # and in units of its spread
        return (S - S.mean(1, keepdims=True)) / S.std(1, keepdims=True)
    if norm == "fb":  # less what each frame scores for the bank on average
        return S - SB.mean(0, keepdims=True)
    if norm == "fbq":
        X = S - SB.mean(0, keepdims=True)
        return X - X.mean(1, keepdims=True)
    if norm == "fbz":
        X = S - SB.mean(0, keepdims=True)
        return (X - X.mean(1, keepdims=True)) / X.std(1, keepdims=True)
    if norm == "sm":  # the query against the bank, as the model's own softmax would put it
        m = (tau * SB).max(0, keepdims=True)
        E = np.exp(tau * SB - m)  # [K, N]
        # Not against the bank's words that say nearly what the query does.
        W = ((T @ TB.T) < 0.85).astype(np.float32)  # [Q, K]
        lse = m + np.log(np.maximum(W @ E, 1e-30) / np.maximum(W.sum(1, keepdims=True), 1))
        return tau * S - lse
    if norm in ("cen", "cenv", "cent"):  # vectors with their means taken out, made length one again
        Vc = unit(V - V.mean((0, 1), keepdims=True)) if norm in ("cen", "cenv") else V
        Tc = unit(T - TB.mean(0, keepdims=True)) if norm in ("cen", "cent") else T
        return scores(Tc, Vc, agg)
    raise ValueError(norm)


def smooth(S, videos):
    out = S.copy()
    for v in np.unique(videos):
        idx = np.where(videos == v)[0]
        X = S[:, idx]
        P = np.pad(X, ((0, 0), (1, 1)), mode="edge")
        out[:, idx] = 0.25 * P[:, :-2] + 0.5 * X + 0.25 * P[:, 2:]
    return out


def average_precision(s, rel, keep):
    s, rel = s[keep], rel[keep]
    order = np.argsort(-s, kind="stable")
    r = rel[order]
    if r.sum() == 0:
        return None
    hits = np.cumsum(r)
    return float((hits[r] / (np.nonzero(r)[0] + 1)).mean())


def top_moments(s, videos, seconds, k):
    """The best k frames that are different moments: 3 s apart, or another video."""
    order = np.argsort(-s, kind="stable")
    taken = []
    for i in order:
        if all(videos[i] != videos[j] or abs(seconds[i] - seconds[j]) > 3 for j in taken):
            taken.append(i)
            if len(taken) == k:
                break
    return taken


def auc(pos, neg):
    if len(pos) == 0 or len(neg) == 0:
        return float("nan")
    pos, neg = np.asarray(pos), np.asarray(neg)
    return float(((pos[:, None] > neg[None, :]).sum() + 0.5 * (pos[:, None] == neg[None, :]).sum()) / (len(pos) * len(neg)))


def load_queries(frame_keys, labels):
    """[(words, relevant [N] bool, keep [N] bool)] ; relevant all False for a NONE query."""
    out = []
    for line in read_lines(os.path.join(HERE, "queries.txt")):
        words, expr = [p.strip() for p in line.split("\t")[:2]]
        rel = np.zeros(len(frame_keys), bool)
        keep = np.ones(len(frame_keys), bool)
        if expr != "NONE":
            code = compile(expr, "<query>", "eval")
            for i, key in enumerate(frame_keys):
                sure, unsure, _ = labels.get(key, (set(), set(), ""))
                lo = eval(code, {"__builtins__": {}}, _Tags(sure))
                hi = eval(code, {"__builtins__": {}}, _Tags(sure | unsure))
                rel[i] = bool(lo)
                keep[i] = bool(lo) == bool(hi)
        out.append((words, rel, keep))
    return out


class _Tags(dict):
    def __init__(self, tags):
        super().__init__()
        self.tags = tags

    def __missing__(self, key):
        return key.lower() in self.tags


def run(config, queries, bank, frame_keys, want_rows=False):
    name, views, agg, template, norm, sm = config
    m, allV = model(name)
    V = allV[:, VIEW_SETS[views]]
    words = [q[0] for q in queries]
    T = text_vectors(m, words, template)
    TB = text_vectors(m, bank, "plain")
    S = scores(T, V, agg)
    SB = scores(TB, V, agg)
    tau = 100.0
    S = normalize(norm, S, SB, T, TB, V, agg, tau)
    videos = np.array([k[0] for k in frame_keys])
    seconds = np.array([k[1] for k in frame_keys])
    if sm == "1":
        S = smooth(S, videos)

    aps, s1, p5, top_pos, top_neg, rows = [], [], [], [], [], []
    pool_s, pool_r = [], []
    for qi, (w, rel, keep) in enumerate(queries):
        s = S[qi]
        pool_s.append(s[keep])
        pool_r.append(rel[keep])
        best = float(s.max())
        if not rel.any():
            top_neg.append(best)
            rows.append((w, 0, None, None, best))
            continue
        top_pos.append(best)
        ap = average_precision(s, rel, keep)
        tm = top_moments(s, videos, seconds, 5)
        # A moment is right if its frame, or the one a second either side, is.
        near = rel.copy()
        for d in (-1, 1):
            j = np.arange(len(rel)) + d
            ok = (j >= 0) & (j < len(rel))
            ok[ok] &= videos[j[ok]] == videos[ok]
            near[ok] |= rel[j[ok]]
        right = [bool(near[i]) for i in tm]
        aps.append(ap)
        s1.append(right[0])
        p5.append(np.mean(right))
        rows.append((w, int(rel.sum()), ap, right, best))
    ps, pr = np.concatenate(pool_s), np.concatenate(pool_r)
    order = np.argsort(-ps, kind="stable")
    r = pr[order]
    hits = np.cumsum(r)
    prec = hits / (np.arange(len(r)) + 1)
    uap = float(prec[r].mean()) if r.any() else float("nan")
    f1 = float((2 * hits / (np.arange(len(r)) + 1 + r.sum())).max())
    res = dict(mAP=np.mean(aps), S1=np.mean(s1), P5=np.mean(p5), uAP=uap, F1=f1, AUC=auc(top_pos, top_neg))
    return (res, rows, S) if want_rows else res


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = [a for a in sys.argv[1:] if a.startswith("--")]
    check = 0
    if "--check" in flags:
        check = int(sys.argv[sys.argv.index("--check") + 1])
        args.remove(str(check))
    labels = load_labels()
    bank = read_lines(os.path.join(HERE, "bank.txt"))
    configs = []
    for a in args:
        parts = [p.split(",") for p in a.split("|")]
        configs += list(itertools.product(*parts))
    frame_keys = None
    queries = None
    print("%-58s  mAP   S@1   P@5   uAP   F1    AUC" % "config")
    for c in configs:
        import json

        keys = [tuple(k) for k in json.load(open(os.path.join(BENCH, "emb", c[0], "frames.json")))]
        if keys != frame_keys:
            frame_keys = keys
            queries = load_queries(frame_keys, labels)
        res, rows, S = run(c, queries, bank, frame_keys, want_rows=True)
        print(
            "%-58s  %.3f %.3f %.3f %.3f %.3f %.3f"
            % ("|".join(c), res["mAP"], res["S1"], res["P5"], res["uAP"], res["F1"], res["AUC"]),
            flush=True,
        )
        if "--perquery" in flags:
            for w, n, ap, right, best in rows:
                if ap is None:
                    print("    %-40s none          best %.3f" % (w, best))
                else:
                    print("    %-40s n=%-3d AP %.2f  %s  best %.3f" % (w, n, ap, "".join("x" if r else "." for r in right), best))
        if "--dist" in flags:
            rel_s = np.concatenate([S[i][q[1] & q[2]] for i, q in enumerate(queries)])
            non_s = np.concatenate([S[i][~q[1] & q[2]] for i, q in enumerate(queries)])
            qs = [1, 5, 25, 50, 75, 95, 99]
            print("    matching frames     n=%d  pct %s: %s" % (len(rel_s), qs, np.round(np.percentile(rel_s, qs), 3)))
            print("    not matching frames n=%d  pct %s: %s" % (len(non_s), qs, np.round(np.percentile(non_s, qs), 3)))
        if check:
            # The best-scoring frames the labels say do not match: to look at again, in case a label is wrong.
            for qi, (w, rel, keep) in enumerate(queries):
                order = np.argsort(-S[qi], kind="stable")[:check]
                wrong = ["v%d:%d" % frame_keys[i] for i in order if not rel[i]]
                if wrong:
                    print("    CHECK %-36s %s" % (w, " ".join(wrong)))


if __name__ == "__main__":
    sys.exit(main())
