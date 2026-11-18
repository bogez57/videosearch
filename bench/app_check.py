# The built program against the bench: does it make a search's vectors as the bench does, and does
# what it lists come out as clips.py says it should?
#
#   python app_check.py --queries FILE     writes every search of queries.txt to FILE, one a line
#   python app_check.py LOG [--rows]       judges what the program logged for them
#
# In a scratch copy of the repo (never in the one that is worked in): git apply
# bench\app_check.patch, which makes the program search for each line of
# bin\local\check_queries.txt once everything is indexed and log what it got; build; write that
# file with --queries; run the program on the test videos' folders and close it when its log
# (bin\local\logs\videosearch_log.txt) has a line "CHECK done". LOG is that log.
#
# Prints how long a search takes, how far the program's vectors are from the bench's for the same
# words (the first 16 numbers of each: 0.000 is the same), its about_sound against the bench's, and
# the list's numbers as clips.py gives them - first, none, quiet - which should come out near
# clips.py's for mobileclip-b|sq+crop3|vec|ens|fb|0 with search.h's low, high and keep. --rows
# prints every search's rows.
import os
import sys

import numpy as np

from common import BENCH, HERE, load_labels, matches, read_lines


def queries():
    out = []
    for line in read_lines(os.path.join(HERE, "queries.txt")):
        words, expr = [p.strip() for p in line.split("\t")[:2]]
        out.append((words, expr))
    return out


def read_log(path):
    """{words: (hit count, about_sound, [16], [16], [(video name, start, end, best, score, kind)], ms or None)}"""
    out = {}
    for line in open(path, encoding="utf-8", errors="replace"):
        at = line.find("CHECK ")
        if at < 0:
            continue
        parts = line[at:].rstrip("\r\n").split("\t")
        if parts[0] == "CHECK query" and len(parts) >= 6:
            out[parts[1]] = (int(parts[2]), float(parts[3]), np.array(parts[4].split(), np.float32), np.array(parts[5].split(), np.float32), [],
                             float(parts[6]) if len(parts) > 6 else None)
        elif parts[0] == "CHECK hit" and len(parts) >= 8 and parts[1] in out:
            out[parts[1]][4].append((parts[2], float(parts[3]), float(parts[4]), float(parts[5]), float(parts[6]), parts[7]))
    return out


def main():
    if sys.argv[1] == "--queries":
        with open(sys.argv[2], "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(w for w, _ in queries()) + "\n")
        return 0

    log = read_log(sys.argv[1])
    qs = [(w, e) for w, e in queries() if w in log]
    words = [w for w, _ in qs]
    print("%d searches in the log, %d of queries.txt's" % (len(log), len(qs)))
    ms = [log[w][5] for w in words if log[w][5] is not None]
    if ms:
        print("a search takes %.0f ms in the middle, %.0f at most" % (np.median(ms), max(ms)))

    # The vectors.
    from eval import model, text_vectors
    from sound import TEMPLATES, SoundModel, _about_sound

    m, _ = model("mobileclip-b")
    want = text_vectors(m, words, "ens") - text_vectors(m, read_lines(os.path.join(HERE, "bank.txt")), "plain").mean(0, keepdims=True)
    off = np.array([np.abs(want[i][:16] - log[w][2]).max() for i, w in enumerate(words)])
    print("picture vectors: off by %.4f at most, %.4f in the middle (their numbers are about %.3f)" % (off.max(), np.median(off), np.abs(want[:, :16]).mean()))
    if any(np.abs(log[w][3]).max() > 0 for w in words):
        sm = SoundModel("clap-unfused", True)
        fmt = TEMPLATES["this_is"]
        bank = sm.texts([fmt.format(t) for t in read_lines(os.path.join(HERE, "bank_sound.txt"))]).mean(0, keepdims=True)
        swant = sm.texts([fmt.format(w) for w in words]) - bank
        soff = np.array([np.abs(swant[i][:16] - log[w][3]).max() for i, w in enumerate(words)])
        about = _about_sound(sm, words)
        aoff = np.array([abs(about[i] - log[w][1]) for i, w in enumerate(words)])
        print("sound vectors:   off by %.4f at most, %.4f in the middle (their numbers are about %.3f)" % (soff.max(), np.median(soff), np.abs(swant[:, :16]).mean()))
        print("about sound:     off by %.4f at most, %.4f in the middle" % (aoff.max(), np.median(aoff)))
        worst = int(np.argmax(aoff))
        print("                 furthest: \"%s\" the program %+.3f, the bench %+.3f" % (words[worst], log[words[worst]][1], about[worst]))
    else:
        print("no sound vectors in the log: the sound model was not there")

    # The lists, judged as clips.py judges them: a row is right if the frame at its best moment, or
    # the one a second either side, matches.
    labels = load_labels()
    names = {}
    for line in open(os.path.join(BENCH, "videos.txt"), encoding="utf-8"):
        v, name = line.rstrip("\n").split("\t")
        names[os.path.basename(name.replace("\\", "/")).lower()] = int(v)

    def right(code, hit):
        v = names.get(hit[0].lower())
        if v is None:
            return None
        got = [matches(code, *labels[(v, k)]) for k in (int(round(hit[3])) + d for d in (-1, 0, 1)) if (v, k) in labels]
        if any(r for r, _ in got):
            return True
        return False if got and all(sure for _, sure in got) else None

    pos = dict(n=0, none=0, first=0, first_picture=0, rows=[], share=[])
    neg = dict(n=0, quiet=0, rows=[])
    lines = []
    for w, expr in qs:
        count, about, _, _, hits, _ = log[w]
        if expr in ("NEAR", "NONE"):
            neg["n"] += 1
            neg["quiet"] += count == 0
            neg["rows"].append(count)
            lines.append("  %-40s %s, %d rows%s" % (w, expr, count, "".join("  [%s %.0f s %s %.2f]" % (h[0][:12], h[3], h[5], h[4]) for h in hits[:3])))
            continue
        code = compile(expr, "<query>", "eval")
        marks = [right(code, h) for h in hits]
        pos["n"] += 1
        pos["none"] += count == 0
        pos["first"] += bool(marks and marks[0])
        pictures = [k for h, k in zip(hits, marks) if h[5] != "sound"]
        pos["first_picture"] += bool(pictures and pictures[0])
        pos["rows"].append(count)
        judged = [k for k in marks if k is not None]
        if judged:
            pos["share"].append(np.mean(judged))
        lines.append("  %-40s %3d rows  %s%s" % (w, count, "".join("x" if k else "?" if k is None else "." for k in marks),
                                               "".join("  [%s %.0f s %s %.2f]" % (h[0][:12], h[3], h[5], h[4]) for h in hits[:3])))
    if pos["n"]:
        print("searches with something to find (%d): rows %.1f, right %.3f (of the first ten), first %.3f (by picture alone %.3f), none %.3f"
              % (pos["n"], np.mean(pos["rows"]), np.mean(pos["share"]), pos["first"] / pos["n"], pos["first_picture"] / pos["n"], pos["none"] / pos["n"]))
    if neg["n"]:
        print("searches with nothing to find (%d):   quiet %.3f, rows %.1f" % (neg["n"], neg["quiet"] / neg["n"], np.mean(neg["rows"])))
    if "--rows" in sys.argv:
        print("\n".join(lines))


if __name__ == "__main__":
    sys.exit(main())
