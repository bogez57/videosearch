# The program's own index against the bench's vectors for the same frames: do the program's way of
# getting a frame ready (its decoder, embed.c) and the model's own way give the same vectors?
#   python check_index.py [index folder]      (default: the repo's bin\local\index)
# Needs BENCH/emb/clip-b32 (embed_images.py clip-b32). Prints, for each video, how alike the two
# vectors of each second are (their dot product: 1 is the same).
import json
import os
import struct
import sys

import numpy as np

from common import BENCH, REPO
from models import VIEWS


def read_index(path):
    data = open(path, "rb").read()
    magic, version, dim, _size, _mod, _sample = struct.unpack_from("<8sIIQQd", data, 0)
    model = data[40:104].split(b"\0")[0].decode()
    count, path_size = struct.unpack_from("<QQ", data, 104)
    at = 120
    video = data[at : at + path_size].decode("utf-8", "replace")
    at += path_size
    times = np.frombuffer(data, np.float32, count, at)
    vecs = np.frombuffer(data, np.float16, count * dim, at + 4 * count).reshape(count, dim).astype(np.float32)
    return video, model, times, vecs


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "bin", "local", "index")
    d = os.path.join(BENCH, "emb", "clip-b32")
    keys = [tuple(k) for k in json.load(open(os.path.join(d, "frames.json")))]
    V = np.load(os.path.join(d, "views.npy")).astype(np.float32)
    at = {k: i for i, k in enumerate(keys)}
    names = {}
    for line in open(os.path.join(BENCH, "videos.txt"), encoding="utf-8"):
        v, name = line.rstrip("\n").split("\t")
        names[os.path.basename(name).lower()] = int(v)
    for f in sorted(os.listdir(folder)):
        if not f.endswith(".vsi"):
            continue
        video, model, times, vecs = read_index(os.path.join(folder, f))
        v = names.get(os.path.basename(video.replace("\\", "/")).lower())
        if v is None or len(times) < 20:
            continue
        rows = []
        for t, x in zip(times, vecs):
            k = int(round(float(t)))
            if (v, k) in at:
                rows.append(V[at[(v, k)]] @ x)
        rows = np.array(rows)  # [seconds, views]
        print("v%d %s (%s, %d frames): %s" % (v, f, model, len(times), "  ".join("%s %.3f" % (n, rows[:, j].mean()) for j, n in enumerate(VIEWS))))
        c = rows[:, VIEWS.index("center")]
        print("    center: least %.3f, a tenth under %.3f, half under %.3f" % (c.min(), np.percentile(c, 10), np.median(c)))


if __name__ == "__main__":
    sys.exit(main())
