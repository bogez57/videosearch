# Contact sheets of the frames, for labeling what is in them by eye:
# BENCH/sheets/v<video>_<first second>.jpg, 20 frames a sheet, each with its second written over it.
import os
import sys

from PIL import Image, ImageDraw, ImageFont

from common import BENCH, frames

COLS, ROWS, W = 4, 5, 480


def main():
    by_video = {}
    for v, k, path in frames():
        by_video.setdefault(v, []).append((k, path))
    out_dir = os.path.join(BENCH, "sheets")
    os.makedirs(out_dir, exist_ok=True)
    try:
        font = ImageFont.truetype("arialbd.ttf", 22)
    except OSError:
        font = ImageFont.load_default()
    count = 0
    for v, items in sorted(by_video.items()):
        for start in range(0, len(items), COLS * ROWS):
            chunk = items[start : start + COLS * ROWS]
            first = Image.open(chunk[0][1])
            h = round(W * first.height / first.width)
            sheet = Image.new("RGB", (COLS * W, ROWS * (h + 28)), (20, 20, 20))
            draw = ImageDraw.Draw(sheet)
            for i, (k, path) in enumerate(chunk):
                x, y = (i % COLS) * W, (i // COLS) * (h + 28)
                img = Image.open(path).convert("RGB").resize((W - 4, h), Image.BICUBIC)
                sheet.paste(img, (x + 2, y + 28))
                draw.text((x + 6, y + 2), "v%d  %d" % (v, k), fill=(255, 255, 0), font=font)
            sheet.save(os.path.join(out_dir, "v%d_%03d.jpg" % (v, chunk[0][0])), quality=88)
            count += 1
    print("wrote", count, "sheets to", out_dir)


if __name__ == "__main__":
    sys.exit(main())
