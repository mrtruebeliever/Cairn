#!/usr/bin/env python3
"""Generates Cairn's launcher menu icon: four stacked stones on a brass
shore, dark and light grey alternating under an Icterine top stone (the
app's accent), as in draw_cairn() in src/c/art.c.

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



def art():
    g = new()
    # shore
    part(g, rect(0, 22, 24, 23), lambda x, y: "A" if y == 22 else "a")
    stones = [(12, 18.5, 10.5, 3.2, "D"), (12, 13, 8.2, 2.7, "L"),
              (12, 8.2, 6.2, 2.4, "D"), (12, 3.8, 4.2, 2.2, "i")]
    for cx, cy, rx, ry, c in stones:
        cells = ell(cx, cy, rx, ry)
        hl = "W" if c in "Li" else "L"
        part(g, cells, lambda x, y, c=c, cx=cx, cy=cy, rx=rx, ry=ry, hl=hl:
             hl if (y < cy - ry * 0.35 and x < cx - rx * 0.1 and x > cx - rx * 0.75) else c)
    return g


def main():
    os.makedirs("resources/images", exist_ok=True)
    to_img(art()).save("resources/images/menu_icon.png")
    print("wrote resources/images/menu_icon.png (25x25)")


if __name__ == "__main__":
    main()
