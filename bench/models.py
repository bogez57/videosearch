# A picture model in BENCH/models/<name>: an ONNX export from Hugging Face with onnx/vision_model.onnx,
# onnx/text_model.onnx, preprocessor_config.json and its tokenizer's files. Pictures and words into
# vectors of length one, with the model's own way of getting a picture ready, or another way.
import json
import os
import time

import numpy as np
import onnxruntime as ort
from PIL import Image

from common import BENCH

# The ways a frame is shown to the model, in the order their vectors are stored:
#   squash: the whole frame pressed into the square
#   pad:    the whole frame, shrunk until it fits, the rest of the square grey
#   left, center, right: the shorter side scaled to the square's, three squares cut from it
VIEWS = ["squash", "pad", "left", "center", "right"]
VIEW_SETS = {
    "center": [3],
    "squash": [0],
    "pad": [1],
    "crop3": [2, 3, 4],
    "sq+center": [0, 3],
    "sq+crop3": [0, 2, 3, 4],
    "pad+crop3": [1, 2, 3, 4],
}


def _side(v):
    if isinstance(v, dict):
        return int(v.get("height") or v.get("shortest_edge") or v.get("width"))
    return int(v)


def _session(path, cpu):
    so = ort.SessionOptions()
    providers = ["CPUExecutionProvider"]
    if not cpu and "DmlExecutionProvider" in ort.get_available_providers():
        so.enable_mem_pattern = False
        so.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
        providers = ["DmlExecutionProvider", "CPUExecutionProvider"]
    return ort.InferenceSession(path, so, providers=providers)


def _unit(x):
    return x / np.maximum(np.linalg.norm(x, axis=-1, keepdims=True), 1e-12)


class Model:
    def __init__(self, name, cpu=False):
        self.name = name
        self.dir = os.path.join(BENCH, "models", name)
        self.cpu = cpu
        self.siglip = name.startswith("siglip")
        pc = json.load(open(os.path.join(self.dir, "preprocessor_config.json")))
        self.side = _side(pc.get("crop_size") or pc["size"])
        norm = pc.get("do_normalize", True)
        self.mean = np.array(pc.get("image_mean", [0, 0, 0]) if norm else [0, 0, 0], np.float32)
        self.std = np.array(pc.get("image_std", [1, 1, 1]) if norm else [1, 1, 1], np.float32)
        self.resample = {2: Image.BILINEAR, 3: Image.BICUBIC, 1: Image.LANCZOS}.get(pc.get("resample", 3), Image.BICUBIC)
        self.own_view = "center" if pc.get("do_center_crop", False) else "squash"
        self.vision = None
        self.text = None
        self.tok = None

    # ---- pictures

    def views(self, img):
        """img (PIL, RGB) as the five squares of VIEWS, each a float array as the model reads it."""
        S = self.side
        w, h = img.size
        out = [img.resize((S, S), self.resample)]

        scale = S / max(w, h)
        small = img.resize((max(1, round(w * scale)), max(1, round(h * scale))), self.resample)
        grey = tuple(int(round(m * 255)) for m in (self.mean if self.mean.any() else [0.5, 0.5, 0.5]))
        canvas = Image.new("RGB", (S, S), grey)
        canvas.paste(small, ((S - small.width) // 2, (S - small.height) // 2))
        out.append(canvas)

        if w >= h:
            big = img.resize((max(S, int(S * w / h)), S), self.resample)
            spare = big.width - S
            for x in (0, spare // 2, spare):
                out.append(big.crop((x, 0, x + S, S)))
        else:
            big = img.resize((S, max(S, int(S * h / w))), self.resample)
            spare = big.height - S
            for y in (0, spare // 2, spare):
                out.append(big.crop((0, y, S, y + S)))

        arrs = []
        for im in out:
            a = np.asarray(im, np.float32) / 255.0
            arrs.append(((a - self.mean) / self.std).transpose(2, 0, 1))
        return np.stack(arrs)

    def embed_pixels(self, pixels, batch=16):
        """pixels [n, 3, S, S] -> vectors [n, D], and the seconds the model itself took."""
        if self.vision is None:
            self.vision = _session(os.path.join(self.dir, "onnx", "vision_model.onnx"), self.cpu)
            names = [o.name for o in self.vision.get_outputs()]
            self.vision_out = "image_embeds" if "image_embeds" in names else "pooler_output"
            self.vision_in = self.vision.get_inputs()[0].name
        out = []
        took = 0.0
        for i in range(0, len(pixels), batch):
            t = time.perf_counter()
            out.append(self.vision.run([self.vision_out], {self.vision_in: pixels[i : i + batch]})[0])
            took += time.perf_counter() - t
        return _unit(np.concatenate(out).astype(np.float32)), took

    # ---- words

    def embed_texts(self, texts, batch=32):
        if self.text is None:
            from transformers import AutoTokenizer

            self.tok = AutoTokenizer.from_pretrained(self.dir)
            self.text = _session(os.path.join(self.dir, "onnx", "text_model.onnx"), True)
            names = [o.name for o in self.text.get_outputs()]
            self.text_out = "text_embeds" if "text_embeds" in names else "pooler_output"
        out = []
        for i in range(0, len(texts), batch):
            enc = self.tok(
                list(texts[i : i + batch]),
                padding="max_length",
                max_length=64 if self.siglip else 77,
                truncation=True,
                return_tensors="np",
            )
            feeds = {}
            for inp in self.text.get_inputs():
                if inp.name == "input_ids":
                    feeds[inp.name] = enc["input_ids"].astype(np.int64)
                elif inp.name == "attention_mask":
                    mask = enc["attention_mask"] if "attention_mask" in enc else np.ones_like(enc["input_ids"])
                    feeds[inp.name] = mask.astype(np.int64)
                else:
                    raise RuntimeError("text input not known: " + inp.name)
            out.append(self.text.run([self.text_out], feeds)[0])
        return _unit(np.concatenate(out).astype(np.float32))

    def texts(self, texts):
        """As embed_texts, kept in BENCH/emb/<name>/texts.* between runs."""
        d = os.path.join(BENCH, "emb", self.name)
        os.makedirs(d, exist_ok=True)
        keys_path, vec_path = os.path.join(d, "texts.json"), os.path.join(d, "texts.npy")
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
