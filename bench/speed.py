# How fast a model's picture half (or CLAP's sound half) runs through ONNX Runtime on this PC, which
# is what the indexer waits for most: pictures a second for each size of batch, with the model's
# sizes left open as the file has them and with them fixed (DirectML is told what is coming and can
# plan for it), for each file given. With more than one file, how near each one's vectors are to the
# first's for the same pictures (a half-precision file against the full one).
#
#   python speed.py FILE.onnx [FILE.onnx ...] [--cpu] [--batches 4,16,32] [--seconds 3] [--card WHICH]
#
# WHICH is the card DirectML is to use on a PC with two: fast (the default, and what the program
# asks for), any (DirectML's own first choice), or its number, 0 or 1.
#
# The pictures are noise: only the time is of interest, and whether two files agree.
import sys
import time

import numpy as np
import onnxruntime as ort

# What the sizes that are not numbers in a file are, after the first (which is the batch).
SIZES = {"pixel_values": [3, 224, 224], "input_features": [1, 1001, 64]}


CARD = "fast"


def session(path, cpu, fixed):
    """fixed: {name of a size: number} or None."""
    so = ort.SessionOptions()
    for name, n in (fixed or {}).items():
        so.add_free_dimension_override_by_name(name, n)
    providers = ["CPUExecutionProvider"]
    if not cpu and "DmlExecutionProvider" in ort.get_available_providers():
        so.enable_mem_pattern = False
        so.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
        how = {"performance_preference": "high_performance"} if CARD == "fast" else {} if CARD == "any" else {"device_id": CARD}
        providers = [("DmlExecutionProvider", how), "CPUExecutionProvider"]
    return ort.InferenceSession(path, so, providers=providers)


def shape_of(inp, batch):
    """The input's shape with the batch put in, and the names of its sizes that are not numbers."""
    rest = SIZES.get(inp.name, [])
    shape, names = [], {}
    for i, d in enumerate(inp.shape):
        n = batch if i == 0 else (d if isinstance(d, int) else rest[i - 1])
        if not isinstance(d, int):
            names[d] = n
        shape.append(n)
    return shape, names


def main():
    global CARD
    args = sys.argv[1:]
    cpu = "--cpu" in args
    batches = [4, 16, 32, 64]
    seconds = 3.0
    files = []
    i = 0
    while i < len(args):
        if args[i] == "--batches":
            batches = [int(b) for b in args[i + 1].split(",")]
            i += 1
        elif args[i] == "--seconds":
            seconds = float(args[i + 1])
            i += 1
        elif args[i] == "--card":
            CARD = args[i + 1]
            i += 1
        elif args[i] != "--cpu":
            files.append(args[i])
        i += 1

    print("onnxruntime %s, providers: %s, card: %s" % (ort.__version__, ", ".join(ort.get_available_providers()), CARD))
    rng = np.random.default_rng(0)
    first = None
    for path in files:
        s = session(path, cpu, None)
        inp = s.get_inputs()[0]
        out = s.get_outputs()
        name = next((o.name for o in out if o.name in ("image_embeds", "audio_embeds")), out[0].name)
        kind = np.float16 if "float16" in inp.type else np.float32
        print("\n%s\n  input %s %s %s, output %s, runs on %s" % (path, inp.name, inp.type, inp.shape, name, s.get_providers()[0]))

        # The same pictures for every file, so their vectors can be compared.
        shape, names = shape_of(inp, 8)
        x = np.random.default_rng(1).random(shape, dtype=np.float32)
        v = s.run([name], {inp.name: x.astype(kind)})[0].astype(np.float32)
        v /= np.linalg.norm(v, axis=-1, keepdims=True)
        if first is None:
            first = v
        else:
            cos = (first * v).sum(-1)
            print("  against the first file's vectors: cosine %.6f at least, %.6f in the middle" % (cos.min(), np.median(cos)))

        for fixed in (False, True):
            for b in batches:
                shape, names = shape_of(inp, b)
                try:
                    t0 = time.perf_counter()
                    ss = session(path, cpu, names if fixed else None)
                    x = rng.random(shape, dtype=np.float32).astype(kind)
                    ss.run([name], {inp.name: x})
                    first_s = time.perf_counter() - t0
                    for _ in range(2):
                        ss.run([name], {inp.name: x})
                    n = 0
                    t0 = time.perf_counter()
                    while time.perf_counter() - t0 < seconds:
                        ss.run([name], {inp.name: x})
                        n += 1
                    took = time.perf_counter() - t0
                    print("  %-5s batch %3d: %7.1f a second, %6.2f ms each (%5.1f ms a run; the first run, loading and all, %.1f s)"
                          % ("fixed" if fixed else "open", b, n * b / took, 1000 * took / (n * b), 1000 * took / n, first_s))
                    del ss
                except Exception as e:
                    print("  %-5s batch %3d: failed, %s" % ("fixed" if fixed else "open", b, str(e).splitlines()[0][:150]))


if __name__ == "__main__":
    main()
