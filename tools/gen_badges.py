#!/usr/bin/env python3
"""Turns generated badge art into the six 48x48 PNGs the result screen uses.

Takes either shape, whichever the image generator produced:
  tools/art/badge_0.png .. badge_5.png   one medal per file (preferred)
  tools/art/badges_sheet.png             3 columns x 2 rows of medals on white

See tools/CHATGPT_PROMPTS.md, prompt 2, for the art brief.

Output:
  resources/images/badge_0.png .. badge_5.png    72x72 RGBA, PT2 64-colour palette

72 px is the diameter the result screen already drew its primitive medal at, so
the bitmap lands in exactly the same footprint. The 22 px badges on the progress
screen stay primitives — a downscale that small turns to mush.

Each cell is cropped to the medal, squared off, resized, and quantized to the
colours the PT2 can actually display; the white sheet background becomes
transparent so the app's dark blue shows through around the medal.

Run: .art-venv/bin/python tools/gen_badges.py
"""
import os
import sys

from PIL import Image

SIZE = 72
COLS, ROWS = 3, 2
# Order must match the Badge enum in src/c/config.h.
NAMES = ["first", "steady", "zenweek", "deep", "century", "tide"]
WHITE_CUTOFF = 232   # pixels at least this bright in all channels count as background

# The PT2 shows 64 colours: every channel is one of these four levels.
LEVELS = (0x00, 0x55, 0xAA, 0xFF)


def snap(v):
    return min(LEVELS, key=lambda level: abs(level - v))


def quantize(img):
    """Snaps every pixel to the PT2 palette, keeping the alpha channel intact."""
    out = img.copy()
    px = out.load()
    w, h = out.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            px[x, y] = (snap(r), snap(g), snap(b), 0 if a < 128 else 255)
    return out


def drop_background(cell):
    """Turns the near-white sheet background transparent."""
    cell = cell.convert("RGBA")
    px = cell.load()
    w, h = cell.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if r >= WHITE_CUTOFF and g >= WHITE_CUTOFF and b >= WHITE_CUTOFF:
                px[x, y] = (r, g, b, 0)
    return cell


def square_crop(cell):
    """Crops to the medal, then pads back to a square so it is not distorted."""
    box = cell.getbbox()
    if box:
        cell = cell.crop(box)
    w, h = cell.size
    side = max(w, h)
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(cell, ((side - w) // 2, (side - h) // 2))
    return square


def prepare(cell):
    """Background out, cropped square, scaled down, snapped to the PT2 palette."""
    cell = square_crop(drop_background(cell))
    return quantize(cell.resize((SIZE, SIZE), Image.LANCZOS))


def cells_from_sheet(path):
    sheet = Image.open(path).convert("RGBA")
    cw, ch = sheet.width // COLS, sheet.height // ROWS
    for i in range(len(NAMES)):
        col, row = i % COLS, i // COLS
        yield i, sheet.crop((col * cw, row * ch, (col + 1) * cw, (row + 1) * ch))


def cells_from_files(art_dir):
    for i in range(len(NAMES)):
        yield i, Image.open(os.path.join(art_dir, "badge_%d.png" % i)).convert("RGBA")


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    art_dir = os.path.join(root, "tools", "art")
    singles = [os.path.join(art_dir, "badge_%d.png" % i) for i in range(len(NAMES))]
    sheet_path = os.path.join(art_dir, "badges_sheet.png")

    if all(os.path.exists(p) for p in singles):
        source = cells_from_files(art_dir)
    elif os.path.exists(sheet_path):
        source = cells_from_sheet(sheet_path)
    else:
        sys.exit("no badge art in %s — expected badge_0.png..badge_5.png or "
                 "badges_sheet.png; see tools/CHATGPT_PROMPTS.md, prompt 2" % art_dir)

    out_dir = os.path.join(root, "resources", "images")
    os.makedirs(out_dir, exist_ok=True)
    for i, cell in source:
        path = os.path.join(out_dir, "badge_%d.png" % i)
        prepare(cell).save(path)
        print("wrote %s (%s)" % (path, NAMES[i]))


if __name__ == "__main__":
    main()
