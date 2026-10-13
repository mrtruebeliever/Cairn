#!/usr/bin/env python3
"""Generates Cairn's launcher menu icon: four pixel pebbles stacked on a brass
shore, dark and light grey alternating under an Icterine top stone (the
app's accent), drawn like draw_cairn() in src/c/art.c.

The launcher draws menu icons in four greys (black, dark grey, light grey,
white) on white, so the icon is crisp pixel art in Pebble palette colours
chosen for their grey level, with every part outlined in black; pale colours
without an outline would vanish.

Output:
  resources/images/menu_icon.png   25x25 on transparent

Run from the project root (needs Pillow):  python3 tools/gen_icon.py
"""
import os

from PIL import Image

N = 25
PAL = {
    "K": (0x00, 0x00, 0x00), "W": (0xFF, 0xFF, 0xFF), "L": (0xAA, 0xAA, 0xAA),
    "D": (0x55, 0x55, 0x55), "R": (0xFF, 0x00, 0x00), "r": (0xAA, 0x00, 0x00),
    "o": (0xFF, 0x55, 0x55), "Y": (0xFF, 0xFF, 0x00), "y": (0xFF, 0xAA, 0x00),
    "i": (0xFF, 0xFF, 0x55), "A": (0xAA, 0xAA, 0x55), "a": (0x55, 0x55, 0x00),
    "S": (0xFF, 0xAA, 0x55), "s": (0xFF, 0x55, 0x00), "N": (0xAA, 0x55, 0x00),
    "n": (0x55, 0x00, 0x00), "b": (0x00, 0x00, 0x55), "B": (0x00, 0x55, 0xAA),
    "c": (0x00, 0x55, 0xFF), "C": (0xAA, 0xFF, 0xFF), "E": (0x00, 0xFF, 0xAA),
    "e": (0x00, 0xAA, 0x55), "p": (0x00, 0x55, 0x00), "M": (0x55, 0xAA, 0x00),
    "Z": (0xAA, 0xFF, 0x55), "T": (0xAA, 0xFF, 0xAA), "m": (0xAA, 0x00, 0xAA),
    "P": (0x00, 0xAA, 0x00), "G": (0x55, 0xAA, 0xAA), "g": (0x00, 0x55, 0x55),
}


def disc(cx, cy, r):
    return {(x, y) for y in range(N) for x in range(N) if (x - cx) ** 2 + (y - cy) ** 2 <= r * r}


def ell(cx, cy, rx, ry):
    return {(x, y) for y in range(N) for x in range(N)
            if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1}


def rect(x0, y0, x1, y1):
    return {(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)}


def part(g, cells, shade, diag=False):
    nb = [(1, 0), (-1, 0), (0, 1), (0, -1)] + ([(1, 1), (-1, -1), (1, -1), (-1, 1)] if diag else [])
    for (x, y) in cells:
        for dx, dy in nb:
            p = (x + dx, y + dy)
            if p not in cells and 0 <= p[0] < N and 0 <= p[1] < N:
                g[p[1]][p[0]] = "K"
    for (x, y) in cells:
        if 0 <= x < N and 0 <= y < N:
            g[y][x] = shade(x, y) if callable(shade) else shade


def new():
    return [["."] * N for _ in range(N)]


def to_img(g):
    img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    for y in range(N):
        for x in range(N):
            if g[y][x] != ".":
                img.putpixel((x, y), PAL[g[y][x]] + (255,))
    return img



# Pixel pebbles, as drawn in the app (src/c/art.c): an outline, a highlight on
# the upper left, a shadow from the lower right. Six rows each, resting on one
# another with their outlines sharing two rows.
STONE_IN = [2, 1, 0, 0, 1, 2]
INK = {            # highlight, body, shadow -- grey levels kept apart for the launcher
    "light": ("W", "L", "D"),
    "dark": ("L", "D", "K"),
    "glow": ("W", "i", "A"),
}


def stone(g, x, y, w, ink):
    hi, base, shade = INK[ink]
    sh = len(STONE_IN)
    for r in range(sh):
        l, rt = x + STONE_IN[r], x + w - 1 - STONE_IN[r]
        for xx in range(l, rt + 1):
            g[y + r][xx] = "K"
        if r in (0, sh - 1):
            continue
        il = x + 1 + max(STONE_IN[r - 1], STONE_IN[r], STONE_IN[r + 1])
        ir = x + w - 2 - max(STONE_IN[r - 1], STONE_IN[r], STONE_IN[r + 1])
        for xx in range(il, ir + 1):
            if r == 1:
                g[y + r][xx] = hi if xx < il + (ir - il) * 2 // 3 + 1 else base
            elif r >= sh - 2:
                g[y + r][xx] = shade if xx >= (il + ir) // 2 else base
            else:
                g[y + r][xx] = base


def art():
    g = new()
    # shore
    part(g, rect(0, 22, 24, 23), lambda x, y: "A" if y == 22 else "a")
    # four stones, widest at the bottom, a pixel off-centre each
    for i, (w, ink, dx) in enumerate([(23, "dark", 0), (19, "light", 1),
                                      (15, "dark", -1), (11, "glow", 0)]):
        stone(g, (N - w) // 2 + dx, 16 - 4 * i, w, ink)
    return g


def main():
    os.makedirs("resources/images", exist_ok=True)
    to_img(art()).save("resources/images/menu_icon.png")
    print("wrote resources/images/menu_icon.png (25x25)")


if __name__ == "__main__":
    main()
