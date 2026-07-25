#!/usr/bin/env python3
"""Generates Cairn's launcher menu icon: four stacked pebbles on a shoreline,
matching draw_cairn() in src/c/art.c — light/dark grey alternating stones, an
Icterine top stone (the app's accent) and a Brass shore line, per the "night
tide" palette in src/c/config.h.

Output:
  resources/images/menu_icon.png   25x25   cairn on transparent

Rendered supersampled and downsampled for clean edges. Needs Pillow:
    .art-venv/bin/python tools/gen_icon.py
"""
import os
from PIL import Image, ImageDraw

SS = 8
W = 25

LIGHT = (0xAA, 0xAA, 0xAA, 255)   # GColorLightGray
DARK = (0x55, 0x55, 0x55, 255)    # GColorDarkGray
ACCENT = (0xFF, 0xFF, 0x55, 255)  # GColorIcterine
GROUND = (0xAA, 0xAA, 0x55, 255)  # GColorBrass (the limestone shore)

# (width, height) per stone, bottom first — the same taper as draw_cairn().
STONES = [(21, 5), (17, 5), (13, 4), (9, 4)]


def draw_cairn(size):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    s = size / W          # scale from the 25x25 design grid

    cx = 12.5 * s
    ground_y = 23 * s
    d.line([(2.5 * s, ground_y), (22.5 * s, ground_y)], fill=GROUND, width=int(1.5 * s))

    y = ground_y - 1.0 * s
    for i, (w, h) in enumerate(STONES):
        top = i == len(STONES) - 1
        fill = ACCENT if top else (LIGHT if i % 2 == 0 else DARK)
        d.rounded_rectangle(
            [cx - w * s / 2, y - h * s, cx + w * s / 2, y],
            radius=1.6 * s, fill=fill,
        )
        y -= (h + 0.6) * s
    return img


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = os.path.join(root, "resources", "images")
    os.makedirs(out_dir, exist_ok=True)
    big = draw_cairn(W * SS)
    icon = big.resize((W, W), Image.LANCZOS)
    path = os.path.join(out_dir, "menu_icon.png")
    icon.save(path)
    print("wrote", path)


if __name__ == "__main__":
    main()
