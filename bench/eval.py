# Scores ways of searching the frames against the labels.
#
#   python eval.py CONFIG [CONFIG ...] [--videos=0,1,2] [--bank=FILE] [--perquery] [--check K] [--dist]
#
# Only frames that have a line in labels.tsv are used, and with --videos only those videos' (a query
# that then has no matching frame is left out).
#
# A CONFIG is model|views|agg|template|norm|smooth, each part a comma-separated list (every
# combination is run), e.g.   clip-b32,siglip-b16|center,squash|max|plain,photo|raw|0
#   views:    a name from models.VIEW_SETS
#   agg:      max (a frame scores as its best view), mean (of its views' scores), vec (its views'
#             vectors averaged into one)
#   template: plain, photo, ens (see TEMPLATES)
#   norm:     raw, or a way of making one score mean the same for every query and frame (see
#             normalize()): against the library (qhalf, qmean, qz), against a bank of words that
#             could be about anything (fb, fbs, fbq, fbz, sm, sm0, smk5, cen, cenv, cent; --bank=FILE for
#             another bank than bank.txt), against 1000 photographs of other things (rb, rbh, rbz,
#             rbt, rbtz, fbrb, fbrbz; flickr.py makes their vectors)
#   smooth:   0, or 1 (a frame's score mixed with its neighbours' a second each side)
#
# Queries are bench/queries.txt: words <tab> expression over the labels' tags (python: and, or, not;
# cap("words") for words in the frame's caption), or NEAR / NONE for something that is in no video.
# A tag a frame is not sure of leaves that frame out of the query's sums.
#
# For each config, over the queries that have matching frames:
#   mAP   mean average precision of the frames' ranking
#   S@1   how often the best hit is right
#   P@5   how many of the best five different moments (3 s apart or another video) are right
# and over every query at once, to see whether one score means the same for all of them:
#   uAP   average precision of every (query, frame) pair in one list
#   F1    the best F1 a single cutoff gives on that list
#   AUCn  how well a query's best score tells queries with a match from the NEAR ones
#   AUCf  and from the NONE ones
# and with one cutoff for every query, set so that 95% of the NEAR and NONE queries show nothing:
#   Qc    how many of the queries with a match still show a right frame
#   mPc   of what a query shows, how much is right (mean over the queries that show anything)
#   mRc   of a query's right frames, how many it shows (mean over the queries)
import itertools
import json
import os
import sys

import numpy as np

from common import BENCH, HERE, load_labels, matches, read_lines
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
_refs = {}


def reference(name):
    if name not in _refs:
        path = os.path.join(BENCH, "emb", name, "flickr_views.npy")
        if not os.path.exists(path):
            raise SystemExit("no " + path + ": python flickr.py " + name)
        _refs[name] = np.load(path).astype(np.float32)
    return _refs[name]


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


def normalize(norm, S, SB, T, TB, V, agg, tau, R):
    """S: the queries' scores [Q, N]. SB: the bank's [K, N] (words that could be about anything).
    R: a function giving the reference photographs' vectors [M, v, D], as V is for the frames."""
    if norm == "raw":
        return S
    if norm in ("rb", "rbh", "rbz", "rbt", "rbtz", "fbrb", "fbrbz"):
        Rv = R()
        SR = scores(T, Rv, agg)  # [Q, M]: what photographs of other things score for each query
        if norm.startswith("fb"):  # with what each picture scores for the bank taken out first
            S = S - SB.mean(0, keepdims=True)
            SR = SR - scores(TB, Rv, agg).mean(0, keepdims=True)
        mu, sd = SR.mean(1, keepdims=True), SR.std(1, keepdims=True)
        if norm in ("rb", "fbrb"):
            return S - mu
        if norm == "rbh":
            return S - 0.5 * mu
        if norm in ("rbz", "fbrbz"):
            return (S - mu) / sd
        top = np.percentile(SR, 99, axis=1, keepdims=True)  # all but 1% of them score under this
        return S - top if norm == "rbt" else (S - top) / sd
    if norm == "qhalf":  # less half each query's mean over the library, as search.c does now
        return S - 0.5 * S.mean(1, keepdims=True)
    if norm == "qmean":  # less each query's mean over the library
        return S - S.mean(1, keepdims=True)
    if norm == "qz":  # and in units of its spread
        return (S - S.mean(1, keepdims=True)) / S.std(1, keepdims=True)
    if norm == "fb":  # less what each frame scores for the bank on average
        return S - SB.mean(0, keepdims=True)
    if norm == "fbs":  # and more for a query the further its words are from the bank's middle (half of cent)
        far = np.linalg.norm(T - TB.mean(0, keepdims=True), axis=1, keepdims=True)
        return (S - SB.mean(0, keepdims=True)) / np.sqrt(far)
    if norm == "fbq":
        X = S - SB.mean(0, keepdims=True)
        return X - X.mean(1, keepdims=True)
    if norm == "fbz":
        X = S - SB.mean(0, keepdims=True)
        return (X - X.mean(1, keepdims=True)) / X.std(1, keepdims=True)
    if norm in ("sm", "sm0", "smk5"):  # the query against the bank, as the model's own softmax would put it
        m = (tau * SB).max(0, keepdims=True)
        E = np.exp(tau * SB - m)  # [K, N]
        # Not against the bank's words that say nearly what the query does: sm leaves out the ones
        # close to it, smk5 its five nearest, sm0 none.
        TT = T @ TB.T
        W = np.ones_like(TT)  # [Q, K]
        if norm == "sm":
            W = (TT < 0.85).astype(np.float32)
        if norm == "smk5":
            W[np.arange(len(TT))[:, None], np.argsort(-TT, axis=1)[:, :5]] = 0
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


def load_queries(frame_keys, labels, partial=False):
    """[(words, relevant [N] bool, keep [N] bool, kind)] ; kind is "", "NEAR" or "NONE", and relevant
    is all False for the last two."""
    out = []
    for line in read_lines(os.path.join(HERE, "queries.txt")):
        words, expr = [p.strip() for p in line.split("\t")[:2]]
        rel = np.zeros(len(frame_keys), bool)
        keep = np.ones(len(frame_keys), bool)
        kind = expr if expr in ("NEAR", "NONE") else ""
        if not kind:
            code = compile(expr, "<query>", "eval")
            for i, key in enumerate(frame_keys):
                rel[i], keep[i] = matches(code, *labels.get(key, (set(), set(), "")))
            if not rel.any():
                if partial:
                    continue
                raise SystemExit("queries.txt: no frame matches '%s' (%s)" % (words, expr))
        out.append((words, rel, keep, kind))
    return out


def frame_scores(config, queries, bank, frame_keys, use):
    """Every query's score for every frame [Q, N], the way the config says."""
    name, views, agg, template, norm, sm = config
    m, allV = model(name)
    V = allV[use][:, VIEW_SETS[views]]
    words = [q[0] for q in queries]
    T = text_vectors(m, words, template)
    TB = text_vectors(m, bank, "plain")
    S = scores(T, V, agg)
    SB = scores(TB, V, agg)
    tau = 100.0
    S = normalize(norm, S, SB, T, TB, V, agg, tau, lambda: reference(name)[:, VIEW_SETS[views]])
    if sm == "1":
        S = smooth(S, np.array([k[0] for k in frame_keys]))
    return S


def run(config, queries, bank, frame_keys, use, want_rows=False):
    S = frame_scores(config, queries, bank, frame_keys, use)
    videos = np.array([k[0] for k in frame_keys])
    seconds = np.array([k[1] for k in frame_keys])

    aps, s1, p5, top_pos, rows = [], [], [], [], []
    top_neg = {"NEAR": [], "NONE": []}
    pool_s, pool_r = [], []
    for qi, (w, rel, keep, kind) in enumerate(queries):
        s = S[qi]
        pool_s.append(s[keep])
        pool_r.append(rel[keep])
        best = float(s[keep].max())
        if kind:
            top_neg[kind].append(best)
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
    # One cutoff for every query: over what 95% of the queries with nothing to find score at best.
    cut = float(np.percentile(top_neg["NEAR"] + top_neg["NONE"], 95))
    found, shown_right, right_shown = [], [], []
    for qi, (w, rel, keep, kind) in enumerate(queries):
        if kind:
            continue
        over = (S[qi] > cut) & keep
        found.append(bool((over & rel).any()))
        if over.any():
            shown_right.append((over & rel).sum() / over.sum())
        right_shown.append((over & rel).sum() / rel.sum())
    res = dict(
        mAP=np.mean(aps), S1=np.mean(s1), P5=np.mean(p5), uAP=uap, F1=f1,
        AUCn=auc(top_pos, top_neg["NEAR"]), AUCf=auc(top_pos, top_neg["NONE"]),
        Qc=np.mean(found), mPc=np.mean(shown_right) if shown_right else float("nan"), mRc=np.mean(right_shown), cut=cut,
    )
    return (res, rows, S) if want_rows else res


_set_up = {}


def frames_and_queries(name, labels, only):
    """The model's frames that have labels (and are of the videos in only, if given): their keys,
    their places in the model's vectors, and the queries against them."""
    keys = tuple(tuple(k) for k in json.load(open(os.path.join(BENCH, "emb", name, "frames.json"))))
    if keys not in _set_up:
        use = [i for i, k in enumerate(keys) if k in labels and (only is None or k[0] in only)]
        frame_keys = [keys[i] for i in use]
        queries = load_queries(frame_keys, labels, partial=only is not None)
        n_pos = sum(1 for q in queries if not q[3])
        print("# %d frames of %d videos, %d queries with a match, %d without" % (
            len(frame_keys), len(set(k[0] for k in frame_keys)), n_pos, len(queries) - n_pos))
        _set_up[keys] = (frame_keys, use, queries)
    return _set_up[keys]


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = [a for a in sys.argv[1:] if a.startswith("--")]
    only = None
    for a in flags:
        if a.startswith("--videos="):
            only = set(int(x) for x in a.split("=")[1].split(","))
    check = 0
    if "--check" in flags:
        check = int(sys.argv[sys.argv.index("--check") + 1])
        args.remove(str(check))
    labels = load_labels()
    bank_path = os.path.join(HERE, "bank.txt")
    for a in flags:
        if a.startswith("--bank="):
            bank_path = a.split("=", 1)[1]
    bank = read_lines(bank_path)
    configs = []
    for a in args:
        parts = [p.split(",") for p in a.split("|")]
        configs += list(itertools.product(*parts))
    cols = ["mAP", "S1", "P5", "uAP", "F1", "AUCn", "AUCf", "Qc", "mPc", "mRc"]
    print("%-52s " % "config" + " ".join("%-5s" % c for c in cols) + " cut")
    for c in configs:
        frame_keys, use, queries = frames_and_queries(c[0], labels, only)
        res, rows, S = run(c, queries, bank, frame_keys, use, want_rows=True)
        print("%-52s " % "|".join(c) + " ".join("%.3f" % res[k] for k in cols) + " %.3f" % res["cut"], flush=True)
        if "--perquery" in flags:
            for w, n, ap, right, best in rows:
                if ap is None:
                    print("    %-40s none          best %.3f" % (w, best))
                else:
                    print("    %-40s n=%-3d AP %.2f  %s  best %.3f" % (w, n, ap, "".join("x" if r else "." for r in right), best))
        if "--dist" in flags:
            qs = [1, 5, 25, 50, 75, 95, 99]
            groups = [
                ("matching frames", [S[i][q[1] & q[2]] for i, q in enumerate(queries)]),
                ("not matching, same queries", [S[i][~q[1] & q[2]] for i, q in enumerate(queries) if not q[3]]),
                ("NEAR queries' frames", [S[i] for i, q in enumerate(queries) if q[3] == "NEAR"]),
                ("NONE queries' frames", [S[i] for i, q in enumerate(queries) if q[3] == "NONE"]),
            ]
            for label, parts in groups:
                x = np.concatenate(parts)
                print("    %-27s n=%-6d pct %s: %s" % (label, len(x), qs, np.round(np.percentile(x, qs), 3)))
        if check:
            # The best-scoring frames the labels say do not match: to look at again, in case a label is wrong.
            for qi, (w, rel, keep, kind) in enumerate(queries):
                order = np.argsort(-S[qi], kind="stable")[:check]
                wrong = ["v%d:%d%s" % (*frame_keys[i], "" if keep[i] else "?") for i in order if not rel[i]]
                if wrong:
                    print("    CHECK %-36s %s" % (w, " ".join(wrong)))


if __name__ == "__main__":
    sys.exit(main())
