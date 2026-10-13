#!/usr/bin/env python3
"""Turns the generated store art into the files the appstore listing needs.

Input (see tools/CHATGPT_PROMPTS.md, prompt 3):
  tools/art/store_icon_src.png     square night-tide scene with the cairn

Output:
  store/icon-80.png        80x80
  store/icon-144.png       144x144

The banner is pixel art from tools/gen_banner.py since 1.3.2.

The icon source frames the cairn small in a wide scene, which turns to soup at
80 px, so it is cropped to a square around the cairn (found by locating the
yellow top stone and the waterline) before scaling. Generators also like to add
a white paper margin; that is trimmed off first.

Run: .art-venv/bin/python tools/gen_store.py
"""
import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "tools", "art")
OUT = os.path.join(ROOT, "store")

WHITE_CUTOFF = 232      # a pixel this bright in every channel is paper, not artwork
# The yellow top stone, loosely: red and green high, blue low.
def is_top_stone(px):
    r, g, b = px[:3]
    return r > 180 and g > 180 and b < 140


def trim_white(img):
    """Drops the white frame some generators paint around the artwork."""
    w, h = img.size
    px = img.convert("RGB").load()

    def row_is_paper(y):
        return all(min(px[x, y]) >= WHITE_CUTOFF for x in range(0, w, 7))

    def col_is_paper(x):
        return all(min(px[x, y]) >= WHITE_CUTOFF for y in range(0, h, 7))

    top, bottom, left, right = 0, h - 1, 0, w - 1
    while top < bottom and row_is_paper(top):
        top += 1
    while bottom > top and row_is_paper(bottom):
        bottom -= 1
    while left < right and col_is_paper(left):
        left += 1
    while right > left and col_is_paper(right):
        right -= 1
    return img.crop((left, top, right + 1, bottom + 1))


def cairn_box(img):
    """A square crop around the cairn: yellow stone at the top, water below."""
    w, h = img.size
    px = img.convert("RGB").load()

    xs, ys = [], []
    for y in range(0, h, 3):
        for x in range(0, w, 3):
            if is_top_stone(px[x, y]):
                xs.append(x)
                ys.append(y)
    if not xs:
        sys.exit("could not find the yellow top stone in the icon source")

    cx = (min(xs) + max(xs)) // 2
    top_y = min(ys)

    # Waterline: the first row below the stone that is mostly teal at the edges,
    # sampled away from the cairn so the stones themselves do not count.
    water_y = h - 1
    for y in range(max(ys), h):
        r, g, b = px[w // 12, y]
        if g > 90 and b > 90 and r < 110:
            water_y = y
            break

    # Height from the top stone to the waterline is the cairn; give it room to
    # breathe above and keep a strip of water below.
    cairn_h = max(water_y - top_y, 40)
    side = min(int(cairn_h * 1.9), h, w)
    top = top_y - int(cairn_h * 0.30)
    top = max(0, min(top, h - side))
    left = max(0, min(cx - side // 2, w - side))
    return (left, top, left + side, top + side)


def main():
    # Since 1.3.2 the store icons are pixel art from tools/gen_banner.py; running
    # this would put the old ones back.
    sys.exit("superseded: run tools/gen_banner.py (store art) and tools/gen_icon.py (launcher icon)")
    icon_src = os.path.join(ART, "store_icon_src.png")
    if not os.path.exists(icon_src):
        sys.exit("missing %s — see tools/CHATGPT_PROMPTS.md, prompt 3" % icon_src)
    os.makedirs(OUT, exist_ok=True)

    scene = trim_white(Image.open(icon_src).convert("RGB"))
    icon = scene.crop(cairn_box(scene))
    for size in (80, 144):
        path = os.path.join(OUT, "icon-%d.png" % size)
        icon.resize((size, size), Image.LANCZOS).save(path)
        print("wrote", path)


if __name__ == "__main__":
    main()
