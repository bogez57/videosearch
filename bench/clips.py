# What the program would list for each search, built from the frames' scores as search.c builds
# it, and judged row by row against the labels.
#
#   python clips.py CONFIG... [--videos=0,1,2] [--bank=FILE] [--low=X --high=X] [--margin=0.25]
#                             [--keep=0.5] [--floor=0] [--scan] [--show "words of a query"]
#
# CONFIG as eval.py's. A frame's score is put on the scale the program shows, 0 for --low and 1 for
# --high; left out, low is what 95% of the searches with nothing to find stay under at best, and
# high the middle one of the right frames' best scores, so every CONFIG is set up the same way.
# Clips are made as in _SRCH_Clips: the best frame not yet in a clip starts one, and it takes in
# its neighbours for as long as they score within --margin of it. A clip is listed if it scores at
# least --floor, and at least --keep of what the best clip does (of 1, if that is more).
#
# For the searches with something to find:
#   rows   how many rows the list has
#   right  how many of them hold a frame that matches (rows whose only doubt is a "not sure" label
#          are counted neither way)
#   found  how many of the stretches that match (frames in a row that match, gaps of a second or two
#          closed) have a row
#   first  how often the first row is right
#   order  average precision of the rows before any is left out: are right ones over wrong ones
#   none   how many of these searches list nothing at all
# For the searches with nothing to find (NEAR, NONE):
#   quiet  how many of them list nothing
#   rows   how many rows they list on average
import itertools
import os
import sys

import numpy as np

from common import HERE, load_labels, read_lines
from eval import frame_scores, frames_and_queries


def make_clips(s, videos, margin):
    """s: one query's scores on the program's scale. -> [(score, first, last, best)], best first."""
    out = []
    for v in np.unique(videos):
        idx = np.where(videos == v)[0]
        x = s[idx]
        clip_of = np.full(len(x), -1)
        for k in np.argsort(-x, kind="stable"):
            if clip_of[k] != -1:
                continue
            first = last = k
            clip_of[k] = k
            while first > 0 and clip_of[first - 1] == -1 and x[first - 1] >= x[k] - margin:
                first -= 1
                clip_of[first] = k
            while last + 1 < len(x) and clip_of[last + 1] == -1 and x[last + 1] >= x[k] - margin:
                last += 1
                clip_of[last] = k
            out.append((float(x[k]), idx[first], idx[last], idx[k]))
    out.sort(key=lambda c: -c[0])
    return out


def stretches(rel, videos):
    """Runs of matching frames, gaps of up to two frames closed: [(first, last)]."""
    out = []
    for i in np.nonzero(rel)[0]:
        if out and videos[out[-1][1]] == videos[i] and i - out[-1][1] <= 3:
            out[-1][1] = i
        else:
            out.append([i, i])
    return out


def judge(S, queries, videos, low, high, margin, keep_share, floor):
    pos = dict(rows=[], right=[], found=[], first=[], order=[], none=[])
    neg = dict(quiet=[], rows=[])
    lists = []
    for qi, (w, rel, keep, kind) in enumerate(queries):
        clips = make_clips((S[qi] - low) / (high - low), videos, margin)
        best = clips[0][0]
        shown = [c for c in clips if c[0] >= floor and c[0] >= keep_share * min(best, 1.0)]
        lists.append(shown)
        if kind:
            neg["quiet"].append(len(shown) == 0)
            neg["rows"].append(len(shown))
            continue

        def verdict(c):
            if rel[c[1] : c[2] + 1].any():
                return 1
            return 0 if keep[c[1] : c[2] + 1].all() else -1

        v_all = np.array([verdict(c) for c in clips])
        v = v_all[: len(shown)]
        pos["rows"].append(len(shown))
        pos["none"].append(len(shown) == 0)
        if (v >= 0).any():
            pos["right"].append((v == 1).sum() / (v >= 0).sum())
        pos["first"].append(len(v) > 0 and v[0] == 1)
        st = stretches(rel, videos)
        hit = 0
        for a, b in st:
            hit += any(c[1] <= b and a <= c[2] for c, r in zip(shown, v) if r == 1)
        pos["found"].append(hit / len(st))
        r = v_all[v_all >= 0] == 1
        if r.any():
            hits = np.cumsum(r)
            pos["order"].append(float((hits[r] / (np.nonzero(r)[0] + 1)).mean()))
    return {k: float(np.mean(x)) if len(x) else float("nan") for k, x in pos.items()}, {k: float(np.mean(x)) for k, x in neg.items()}, lists


def main():
    args, opt = [], {}
    argv = sys.argv[1:]
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--show":
            opt["show"] = argv[i + 1]
            i += 1
        elif a.startswith("--"):
            k, _, val = a[2:].partition("=")
            opt[k] = val
        else:
            args.append(a)
        i += 1
    only = set(int(x) for x in opt["videos"].split(",")) if "videos" in opt else None
    margin = float(opt.get("margin", 0.25))
    keep_share = float(opt.get("keep", 0.5))
    floor = float(opt.get("floor", 0.0))
    labels = load_labels()
    bank = read_lines(opt.get("bank") or os.path.join(HERE, "bank.txt"))
    configs = []
    for a in args:
        configs += list(itertools.product(*[p.split(",") for p in a.split("|")]))
    head = "%-46s %7s %7s | rows  right found first order none | quiet rows"
    print(head % ("config", "low", "high"))
    for c in configs:
        frame_keys, use, queries = frames_and_queries(c[0], labels, only)
        S = frame_scores(c, queries, bank, frame_keys, use)
        videos = np.array([k[0] for k in frame_keys])
        neg_best = [S[qi][q[2]].max() for qi, q in enumerate(queries) if q[3]]
        pos_best = [S[qi][q[1]].max() for qi, q in enumerate(queries) if not q[3]]
        low = float(opt["low"]) if "low" in opt else float(np.percentile(neg_best, 95))
        high = float(opt["high"]) if "high" in opt else float(np.median(pos_best))
        high = max(high, low + 1e-6)

        def line(label, m, k, f):
            p, n, lists = judge(S, queries, videos, low, high, m, k, f)
            print("%-46s %7.3f %7.3f | %5.1f %.3f %.3f %.3f %.3f %.3f | %.3f %.1f" % (
                label, low, high, p["rows"], p["right"], p["found"], p["first"], p["order"], p["none"], n["quiet"], n["rows"]), flush=True)
            return lists

        lists = line("|".join(c), margin, keep_share, floor)
        if "scan" in opt:
            for m, k, f in itertools.product((0.15, 0.25, 0.4), (0.0, 0.3, 0.5, 0.7), (-0.25, 0.0, 0.15, 0.3)):
                line("    margin %.2f keep %.1f floor %5.2f" % (m, k, f), m, k, f)
        if "show" in opt:
            for qi, q in enumerate(queries):
                if q[0] == opt["show"]:
                    for score, first, last, best in lists[qi][:30]:
                        right = q[1][first : last + 1].any()
                        print("    %5.2f  v%d %d-%d (best %d) %s  %s" % (
                            score, frame_keys[first][0], frame_keys[first][1], frame_keys[last][1], frame_keys[best][1],
                            "right" if right else "     ", labels[frame_keys[best]][2][:70]))


if __name__ == "__main__":
    sys.exit(main())
