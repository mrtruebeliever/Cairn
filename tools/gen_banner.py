#!/usr/bin/env python3
"""Generates Cairn's appstore art as pixel art, in the style of the app's own
stones and water (src/c/art.c) and of the PebbleDiver / PBL-7 art: drawn on a
small canvas and scaled up with NEAREST so it stays crisp.

Output:
  store/banner-720x320.png   180x80 canvas, x4
  store/icon-80.png          40x40 canvas, x2
  store/icon-144.png         36x36 canvas, x4

Run from the project root (needs Pillow):  python3 tools/gen_banner.py
"""
import math
import os

from PIL import Image, ImageDraw, ImageFont

W, H = 180, 80
SCALE = 4
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

# Pebble palette colours, as the app uses them (config.h).
C = {
    "K": (0x00, 0x00, 0x00), "W": (0xFF, 0xFF, 0xFF),
    "L": (0xAA, 0xAA, 0xAA), "D": (0x55, 0x55, 0x55),
    "o": (0x00, 0x00, 0x55),                            # OxfordBlue: dark stone shadow
    "i": (0xFF, 0xFF, 0x55), "A": (0xAA, 0xAA, 0x55),   # Icterine, Brass
    "a": (0x55, 0x55, 0x00),                            # ArmyGreen
    "C": (0xAA, 0xFF, 0xFF),                            # Celeste: foam, glints
    "T": (0x00, 0xAA, 0xAA),                            # TiffanyBlue: sunlit water
    "g": (0x00, 0x55, 0x55),                            # MidnightGreen: deep water
    "G": (0x55, 0xAA, 0xAA),                            # CadetBlue: wet stone
}

SURFACE = 58          # mean waterline
BAND_TOP_H = 9        # the sunlit layer, scaled down from the app's 22 px


def put(img, x, y, c):
    if 0 <= x < img.width and 0 <= y < img.height:
        img.putpixel((x, y), C[c])


def row(img, x0, x1, y, c):
    for x in range(x0, x1 + 1):
        put(img, x, y, c)


def wave_y(x, phase=0.0):
    a = math.sin(phase + x * 2 * math.pi / 47)
    b = math.sin(-phase * 1.5 + x * 2 * math.pi / 21)
    return SURFACE + round(a * 2 + b * 1)


def sky(img):
    for y in range(H):
        row(img, 0, W - 1, y, "K")
    # Stars: single pixels, and two small crosses.
    for x, y, c in ((92, 4, "L"), (100, 30, "W"), (124, 3, "L"), (150, 8, "L"),
                    (171, 16, "W"), (102, 26, "D"), (160, 30, "L"), (84, 18, "D"),
                    (138, 22, "D"), (176, 4, "L")):
        put(img, x, y, c)
    for cx, cy in ((116, 20), (166, 24)):
        put(img, cx, cy, "W")
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            put(img, cx + dx, cy + dy, "D")
    # A crescent moon, outlined like the stones.
    mx, my, r = 119, 11, 6
    for y in range(my - r - 1, my + r + 2):
        for x in range(mx - r - 1, mx + r + 2):
            d = (x - mx) ** 2 + (y - my) ** 2
            bite = (x - mx - 3) ** 2 + (y - my + 2) ** 2
            if d <= r * r and bite > (r - 1) ** 2:
                put(img, x, y, "i" if d < (r - 2) ** 2 or x < mx - 2 else "A")


def shore(img):
    """Limestone shore rising out of the water on the right, cairn on top."""
    top = {}
    for x in range(110, W):
        t = 49 + max(0, (126 - x)) // 2
        top[x] = t
    for x, t in top.items():
        put(img, x, t, "K")
        for y in range(t + 1, SURFACE + 4):
            c = "a" if (x * 5 + y * 3) % 11 == 0 else "A"
            if y == t + 1 and x % 3:
                c = "W" if x % 7 == 0 else "A"
            put(img, x, y, c)


# Stones as in art.c: an inset table per row, outline, highlight upper left,
# shadow growing from the lower right, a speck of grit.
STONE_IN = [3, 1, 0, 0, 0, 0, 1, 2]
SH = len(STONE_IN)
INK = {
    "light": ("W", "L", "D", "K"),
    "dark": ("L", "D", "o", "K"),
    "glow": ("W", "i", "A", "K"),
    "wet": ("G", "G", "T", "g"),
}


def inset(i, r, right, tbl=STONE_IN):
    sh = len(tbl)
    v = sh if r < 0 or r >= sh else tbl[r]
    lean = ((i * 5 + 2) % 3 == 0) == right
    if lean and r <= 1:
        v += 2 - r
    elif lean and r >= sh - 2:
        v += r - (sh - 3)
    return v


def stone(img, x, y, w, i, ink, flip=False, every_other=False, tbl=STONE_IN):
    hi, base, shade, outline = INK[ink]
    SH = len(tbl)
    inset_ = lambda ii, r, right: inset(ii, r, right, tbl)
    rr_ = range(SH)
    for r in rr_:
        yy = y + (SH - 1 - r if flip else r)
        if every_other and r % 2:
            continue
        l, rt = x + inset_(i, r, False), x + w - 1 - inset_(i, r, True)
        row(img, l, rt, yy, outline)
        if r in (0, SH - 1):
            continue
        il = x + 1 + max(inset_(i, r - 1, False), inset_(i, r, False), inset_(i, r + 1, False))
        ir = x + w - 2 - max(inset_(i, r - 1, True), inset_(i, r, True), inset_(i, r + 1, True))
        if ir < il:
            continue
        span = ir - il + 1
        if r >= SH - 3:
            frm = il + span * (SH - 2 - r) // 3
            row(img, il, frm - 1, yy, base)
            row(img, frm, ir, yy, shade)
        elif r == 1:
            hw = span * 2 // 3
            row(img, il, il + hw - 1, yy, hi)
            row(img, il + hw, ir, yy, base)
        else:
            row(img, il, ir, yy, base)
            if r == 3 and span > 8:
                put(img, il + 3 + (i * 7) % (span - 6), yy, shade)


CAIRN_X = 148
CAIRN_BASE = 49
CAIRN = [(30, "light"), (25, "dark"), (21, "light"), (16, "dark"), (12, "glow")]
JITTER = [0, 1, -1, 1, 0]


def cairn(img):
    for i, (w, ink) in enumerate(CAIRN):
        # Each stone rests on the one below: their outlines share a row.
        y = CAIRN_BASE - (i + 1) * (SH - 1)
        stone(img, CAIRN_X - w // 2 + JITTER[i], y, w, i, ink)


def water(img):
    flat = SURFACE + 3
    seam = flat + BAND_TOP_H
    for x in range(W):
        wy = wave_y(x)
        for y in range(wy, H):
            put(img, x, y, "T" if y < seam else "g")
    # The cairn's reflection, washed out and broken into every other row.
    for i, (w, _) in enumerate(CAIRN):
        y = SURFACE + 3 + i * (SH - 1)
        if y + SH < H:
            stone(img, CAIRN_X - w // 2 + JITTER[i], y, w, i, "wet", flip=True, every_other=True)
    # Glints: two rows, the second narrower.
    for x, dy, w in ((6, 2, 5), (40, 2, 3), (78, 2, 4), (102, 2, 3), (22, 5, 3),
                     (58, 5, 5), (92, 5, 3), (176, 3, 3), (12, 7, 3), (66, 7, 3)):
        row(img, x, x + w - 1, flat + dy, "C")
        row(img, x + 1, x + w - 2, flat + dy + 1, "C")
    # Stepped seam into the deep.
    for x in range(-10, W, 10):
        row(img, x + 3, x + 5, seam - 2, "g")
        row(img, x + 1, x + 7, seam - 1, "g")
        row(img, x + 8, x + 10, seam, "T")
    # Light streaks in the deep.
    for x, dy, w in ((8, 3, 8), (52, 3, 5), (96, 4, 9), (30, 7, 6), (74, 8, 7), (118, 6, 5),
                     (14, 10, 5), (60, 11, 8)):
        row(img, x, x + w - 1, seam + dy, "T")
    # Foam crest, unbroken.
    prev = wave_y(0)
    for x in range(W):
        wy = wave_y(x)
        for y in range(min(wy, prev) - 1, max(wy, prev)):
            put(img, x, y, "C")
        prev = wy


def text(img, xy, s, size, fill, shade=None, shadow=(0x00, 0x55, 0x55)):
    """Thresholded bold text with a black outline and a drop shadow."""
    f = ImageFont.truetype(FONT, size)
    mask = Image.new("L", img.size, 0)
    ImageDraw.Draw(mask).text(xy, s, font=f, fill=255)
    mp = mask.point(lambda v: 255 if v > 127 else 0).load()
    ink, outline = {}, set()
    for y in range(img.height):
        for x in range(img.width):
            if mp[x, y]:
                ink[(x, y)] = shade if shade and y > xy[1] + size * 0.55 else fill
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        outline.add((x + dx, y + dy))
    for (x, y) in outline:
        if 0 <= x + 1 < img.width and 0 <= y + 2 < img.height:
            img.putpixel((x + 1, y + 2), shadow)
    for (x, y) in outline:
        put(img, x, y, "K")
    for (x, y), c in ink.items():
        put(img, x, y, c)


def banner():
    img = Image.new("RGB", (W, H))
    sky(img)
    shore(img)
    cairn(img)
    water(img)
    text(img, (6, 2), "CAIRN", 28, "C", "T")
    text(img, (8, 35), "Breathe with the tide.", 7, "W", shadow=(0, 0, 0))
    text(img, (8, 44), "Every session lays a stone.", 7, "i", shadow=(0, 0, 0))
    return img


ICON_IN = [3, 1, 0, 0, 0, 0, 1, 3]  # eight rows per stone, so each reads as a pebble


def icon(n):
    """The store icon: a night sky, the cairn, a strip of tide at its foot."""
    img = Image.new("RGB", (n, n))
    for y in range(n):
        row(img, 0, n - 1, y, "K")
    k = n / 36.0
    for x, y, c in ((4, 5, "L"), (31, 4, "W"), (32, 14, "D"), (3, 17, "D")):
        put(img, round(x * k), round(y * k), c)

    surface = round(29 * k)
    sand = surface - 1
    row(img, 0, n - 1, sand, "A")
    cx = n // 2
    widths = [round(w * k) for w in (26, 22, 18, 14)]
    inks = ["light", "dark", "light", "glow"]
    jitter = [0, 1, -1, 0]
    # Each stone rests on the one below: their outlines share two rows.
    pitch = len(ICON_IN) - 2
    for i, (w, ink) in enumerate(zip(widths, inks)):
        y = sand - 1 - i * pitch - (len(ICON_IN) - 1)
        stone(img, cx - w // 2 + jitter[i], y, w, i, ink, tbl=ICON_IN)

    seam = surface + max(3, round(4 * k))
    for x in range(n):
        wy = surface + round(math.sin(x * 2 * math.pi / (12 * k)) * 0.8 * k)
        for y in range(wy, n):
            put(img, x, y, "T" if y < seam else "g")
    for x, dy, w in ((3, 2, 3), (n - 10, 3, 3), (n // 2 - 3, 1, 2)):
        row(img, round(x * k), round(x * k) + round(w * k) - 1, surface + dy, "C")
    prev = surface
    for x in range(n):
        wy = surface + round(math.sin(x * 2 * math.pi / (12 * k)) * 0.8 * k)
        for y in range(min(wy, prev) - 1, max(wy, prev)):
            put(img, x, y, "C")
        prev = wy
    return img


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    store = os.path.join(root, "store")
    out = os.path.join(store, "banner-720x320.png")
    banner().resize((W * SCALE, H * SCALE), Image.NEAREST).save(out)
    print("wrote", out)
    icon(40).resize((80, 80), Image.NEAREST).save(os.path.join(store, "icon-80.png"))
    icon(36).resize((144, 144), Image.NEAREST).save(os.path.join(store, "icon-144.png"))
    print("wrote store/icon-80.png, store/icon-144.png")


if __name__ == "__main__":
    main()
