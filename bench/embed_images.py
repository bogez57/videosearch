# Every frame through a picture model, each of the five ways (models.VIEWS):
# BENCH/emb/<model>/views.npy, [frames, 5, D] as float16, and frames.json beside it.
#   python embed_images.py clip-b32 siglip-b16 ...   [--cpu] [--batch 16]
import os
import sys
import time

import numpy as np
from PIL import Image

from common import BENCH, frames, save_json
from models import Model


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    cpu = "--cpu" in sys.argv
    batch = 16
    if "--batch" in sys.argv:
        batch = int(sys.argv[sys.argv.index("--batch") + 1])
        args = [a for a in args if a != str(batch)]
    fr = frames()
    for name in args:
        m = Model(name, cpu=cpu)
        t0 = time.perf_counter()
        vecs = []
        took = 0.0
        pending = []
        for i, (_, _, path) in enumerate(fr):
            pending.append(m.views(Image.open(path).convert("RGB")))
            if len(pending) * 5 >= batch * 5 or i == len(fr) - 1:
                pix = np.concatenate(pending)
                out, t = m.embed_pixels(pix, batch=batch)
                took += t
                vecs.append(out.reshape(len(pending), 5, -1))
                pending = []
        vecs = np.concatenate(vecs)
        d = os.path.join(BENCH, "emb", name)
        os.makedirs(d, exist_ok=True)
        np.save(os.path.join(d, "views.npy"), vecs.astype(np.float16))
        save_json(os.path.join(d, "frames.json"), [[v, k] for v, k, _ in fr])
        print(
            "%s: %d frames x 5 views, dim %d, side %d, own view %s, model %.1f ms a picture, all %.0f s"
            % (name, len(fr), vecs.shape[-1], m.side, m.own_view, 1000 * took / (len(fr) * 5), time.perf_counter() - t0),
            flush=True,
        )


if __name__ == "__main__":
    sys.exit(main())
