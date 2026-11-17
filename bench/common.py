# Shared by the bench scripts: where things are, and the frames.
import json
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BENCH = os.environ.get("VS_BENCH") or os.path.join(os.path.dirname(REPO), "videosearch_bench")
HERE = os.path.dirname(os.path.abspath(__file__))


def frames():
    """Every frame as (video index, second, path), in order."""
    out = []
    root = os.path.join(BENCH, "frames")
    for v in sorted(os.listdir(root)):
        if not v.startswith("v"):
            continue
        for f in sorted(os.listdir(os.path.join(root, v))):
            if f.endswith(".jpg"):
                out.append((int(v[1:]), int(f[:-4]), os.path.join(root, v, f)))
    return out


def read_lines(path):
    with open(path, encoding="utf-8") as f:
        return [l.rstrip("\n") for l in f if l.strip() and not l.startswith("#")]


def load_labels(path=None):
    """labels.tsv: v <tab> second <tab> tags (comma separated, a trailing ? is 'not sure') <tab> caption.
    Returns {(v, second): (set of sure tags, set of unsure tags, caption)}."""
    path = path or os.path.join(BENCH, "labels.tsv")
    out = {}
    for line in read_lines(path):
        parts = line.split("\t")
        if len(parts) < 3:
            continue
        sure, unsure = set(), set()
        for t in parts[2].replace(" ", "").split(","):
            if not t:
                continue
            (unsure if t.endswith("?") else sure).add(t.rstrip("?").lower())
        out[(int(parts[0].lstrip("v")), int(parts[1]))] = (sure, unsure, parts[3] if len(parts) > 3 else "")
    return out


def save_json(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f)
