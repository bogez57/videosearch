# The program's own index against the bench's vectors for the same frames: do the program's way of
# getting a frame ready (its decoder, embed.c, the indexer's average of the views) and the model's
# own way give the same vectors?
#   python check_index.py [index folder]      (default: the repo's bin\local\index)
# Needs the bench's vectors of the model the index was made with (embed_images.py mobileclip-b).
# Prints, for each video, how alike the two vectors of each second are (their dot product: 1 is the
# same): the program's against what the bench makes the program's way, and against each view alone.
import json
import os
import struct
import sys

import numpy as np

from common import BENCH, REPO
from models import VIEW_SETS, VIEWS

# The bench's name for the model an index file names, and the views the program averages for it.
MODELS = {"mobileclip_blt": ("mobileclip-b", "sq+crop3"), "clip-vit-base-patch32": ("clip-b32", "center")}


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
    names = {}
    for line in open(os.path.join(BENCH, "videos.txt"), encoding="utf-8"):
        v, name = line.rstrip("\n").split("\t")
        names[os.path.basename(name).lower()] = int(v)
    loaded = {}
    for f in sorted(os.listdir(folder)):
        if not f.endswith(".vsi"):
            continue
        video, model, times, vecs = read_index(os.path.join(folder, f))
        v = names.get(os.path.basename(video.replace("\\", "/")).lower())
        if v is None or len(times) < 20:
            continue
        if model not in MODELS:
            print("%s: made with %s, which the bench has no vectors for" % (f, model))
            continue
        bench_name, views = MODELS[model]
        if bench_name not in loaded:
            d = os.path.join(BENCH, "emb", bench_name)
            keys = [tuple(k) for k in json.load(open(os.path.join(d, "frames.json")))]
            loaded[bench_name] = ({k: i for i, k in enumerate(keys)}, np.load(os.path.join(d, "views.npy")).astype(np.float32))
        at, V = loaded[bench_name]
        rows, same = [], []
        for t, x in zip(times, vecs):
            k = int(round(float(t)))
            if (v, k) in at:
                rows.append(V[at[(v, k)]] @ x)
                mean = V[at[(v, k)]][VIEW_SETS[views]].mean(0)
                same.append(mean / np.linalg.norm(mean) @ x)
        rows, same = np.array(rows), np.array(same)  # [seconds, views], [seconds]
        print("v%d %s (%s, %d frames)" % (v, f, model, len(times)))
        print("    the program's way (%s): least %.3f, a tenth under %.3f, half under %.3f" % (views, same.min(), np.percentile(same, 10), np.median(same)))
        print("    each view alone: " + "  ".join("%s %.3f" % (n, rows[:, j].mean()) for j, n in enumerate(VIEWS)))


if __name__ == "__main__":
    sys.exit(main())
