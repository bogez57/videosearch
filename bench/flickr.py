# 1000 captioned photographs (the test split of Flickr30k): a check that each model is run here as
# its makers ran it, and pictures of other things to set a search's scores against.
#
#   python flickr.py MODEL... [--cpu]
#
# Wants BENCH/flickr1k/images/<file> and BENCH/flickr1k/captions.json: {"<file>": [its captions]}.
# Makes BENCH/emb/<MODEL>/flickr_views.npy ([photographs, 5, D], the five ways of models.VIEWS) and
# flickr_texts.npy (the captions' vectors), and BENCH/flickr1k/bank_captions.txt (one caption of
# each photograph, for eval.py --bank=). Prints:
#   how often a caption finds its photograph in the best 1, 5 and 10 of the 1000, and a photograph
#   one of its captions - to set against the numbers published for the model;
#   then, for each way of scaling the scores (eval.py's norms), whether one cutoff serves every
#   caption. The photographs are split in three: 400 to search, 400 to set scores against, and 200
#   whose captions are searches with nothing to find.
#     uAP   average precision of every (caption, photograph) pair in one list
#     AUC   how well a caption's best score tells the captions with a photograph to find from those without
#     Qc    with the cutoff where 95% of the captions with nothing to find show nothing: how many
#           of the others still show their photograph
#     mPc   and how much of what they show is theirs
import json
import os
import sys

import numpy as np
from PIL import Image

from common import BENCH, HERE, read_lines
from eval import auc, normalize, scores
from models import VIEWS, Model

NORMS = ["raw", "qhalf", "qmean", "qz", "fb", "fbq", "fbz", "sm", "sm0", "smk5", "cen", "cent", "rb", "rbh", "rbz", "rbt", "rbtz", "fbrb", "fbrbz"]


def main():
    names = [a for a in sys.argv[1:] if not a.startswith("--")]
    root = os.path.join(BENCH, "flickr1k")
    caps = json.load(open(os.path.join(root, "captions.json"), encoding="utf-8"))
    files = sorted(caps)
    texts, owner = [], []
    for i, f in enumerate(files):
        for c in caps[f]:
            texts.append(" ".join(c.split()))
            owner.append(i)
    owner = np.array(owner)
    with open(os.path.join(root, "bank_captions.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("".join(" ".join(caps[x][0].split()) + "\n" for x in files))
    bank = read_lines(os.path.join(HERE, "bank.txt"))
    print("%d photographs, %d captions" % (len(files), len(texts)))

    for name in names:
        m = Model(name, cpu="--cpu" in sys.argv)
        d = os.path.join(BENCH, "emb", name)
        os.makedirs(d, exist_ok=True)
        vp, tp = os.path.join(d, "flickr_views.npy"), os.path.join(d, "flickr_texts.npy")
        if not os.path.exists(vp):
            out = []
            for i in range(0, len(files), 8):
                pix = np.concatenate([m.views(Image.open(os.path.join(root, "images", f)).convert("RGB")) for f in files[i : i + 8]])
                vecs = m.embed_pixels(pix)[0]
                out.append(vecs.reshape(-1, 5, vecs.shape[-1]))
            np.save(vp, np.concatenate(out).astype(np.float16))
        if not os.path.exists(tp):
            np.save(tp, m.embed_texts(texts).astype(np.float16))
        V = np.load(vp).astype(np.float32)
        T = np.load(tp).astype(np.float32)
        TB = m.texts(bank)

        own = VIEWS.index(m.own_view)
        line = "%-12s" % name
        for label, j in (("as the model has it (%s)" % m.own_view, own), ("squash", 0), ("center", 3)):
            S = T @ V[:, j].T  # [captions, photographs]
            rank = (S > S[np.arange(len(T)), owner][:, None]).sum(1)
            best = np.full(len(files), len(T))  # for a photograph: the best rank of a caption of its own
            St = S.T
            for i in range(len(files)):
                mine = St[i, owner == i].max()
                best[i] = (St[i, owner != i] > mine).sum()
            print(
                "%-12s %-32s caption finds photograph R@1 %.3f R@5 %.3f R@10 %.3f | photograph finds caption R@1 %.3f R@5 %.3f R@10 %.3f"
                % (name, label, *[(rank < k).mean() for k in (1, 5, 10)], *[(best < k).mean() for k in (1, 5, 10)]),
                flush=True,
            )

        # One cutoff for every caption?
        A, B, C = np.arange(0, 400), np.arange(400, 800), np.arange(800, len(files))
        qa, qc = np.isin(owner, A), np.isin(owner, C)
        Tq = np.concatenate([T[qa], T[qc]])
        na = int(qa.sum())
        rel = owner[qa][:, None] == A[None, :]  # [captions of A, photographs of A]
        Vg, Vr = V[A][:, [own]], V[B][:, [own]]
        S0 = scores(Tq, Vg, "max")
        SB = scores(TB, Vg, "max")
        for norm in NORMS:
            S = normalize(norm, S0, SB, Tq, TB, Vg, "max", 100.0, lambda: Vr)
            Sa, Sc = S[:na], S[na:]
            order = np.argsort(-Sa.ravel(), kind="stable")
            r = rel.ravel()[order]
            hits = np.cumsum(r)
            uap = float((hits[r] / (np.nonzero(r)[0] + 1)).mean())
            cut = np.percentile(Sc.max(1), 95)
            over = Sa > cut
            shown = over.sum(1)
            print(
                "    %-6s uAP %.3f  AUC %.3f  Qc %.3f  mPc %.3f  shown a caption %.1f"
                % (norm, uap, auc(Sa.max(1), Sc.max(1)), (over & rel).any(1).mean(), ((over & rel).sum(1)[shown > 0] / shown[shown > 0]).mean(), shown.mean()),
                flush=True,
            )


if __name__ == "__main__":
    sys.exit(main())
