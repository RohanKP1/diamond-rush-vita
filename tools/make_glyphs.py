#!/usr/bin/env python3
# Vita button glyphs for the game font: port/res/port_btn.png, a strip of 9x9 cells.
# Cell i is drawn for char U+E000 + i (java-src/class_8.java drawGlyph):
#   0 cross, 1 circle, 2 triangle, 3 square, 4-7 d-pad left/right/up/down, 8 L, 9 R
#   python3 tools/make_glyphs.py
import os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "port", "res", "port_btn.png")

DISC = [
    "..ooooo..",
    ".o#####o.",
    "o#######o",
    "o#######o",
    "o#######o",
    "o#######o",
    "o#######o",
    ".o#####o.",
    "..ooooo..",
]
# face-button symbols, 5x5, placed at (2, 2) on the disc
SYMBOLS = {
    "cross": ["X...X", ".X.X.", "..X..", ".X.X.", "X...X"],
    "circle": [".XXX.", "X...X", "X...X", "X...X", ".XXX."],
    "triangle": ["..X..", ".X.X.", ".X.X.", "X...X", "XXXXX"],
    "square": ["XXXXX", "X...X", "X...X", "X...X", "XXXXX"],
}
COLORS = {  # PlayStation symbol colours on a light disc
    "cross": (40, 96, 214),
    "circle": (218, 40, 52),
    "triangle": (8, 160, 112),
    "square": (204, 64, 170),
}
DPAD = [
    "..ooooo..",
    "..o###o..",
    "ooo###ooo",
    "o#######o",
    "o#######o",
    "o#######o",
    "ooo###ooo",
    "..o###o..",
    "..ooooo..",
]
# the arm that lights up for each direction (cells inside the arm, excluding the centre)
ARMS = {
    "left": [(x, y) for x in (1, 2) for y in (3, 4, 5)],
    "right": [(x, y) for x in (6, 7) for y in (3, 4, 5)],
    "up": [(x, y) for x in (3, 4, 5) for y in (1, 2)],
    "down": [(x, y) for x in (3, 4, 5) for y in (6, 7)],
}
PILL = [
    ".ooooooo.",
    "o#######o",
    "o#######o",
    "o#######o",
    "o#######o",
    "o#######o",
    "o#######o",
    "o#######o",
    ".ooooooo.",
]
LETTERS = {  # 3x5
    "L": ["X..", "X..", "X..", "X..", "XXX"],
    "R": ["XX.", "X.X", "XX.", "X.X", "X.X"],
}

OUTLINE = (16, 16, 20, 255)
BODY = (236, 236, 240, 255)
DPAD_BODY = (196, 196, 204, 255)
LIT = (255, 196, 32, 255)  # the game's yellow direction arrows
LETTER = (24, 24, 28, 255)


def cell_from(mask, body):
    im = Image.new("RGBA", (9, 9), (0, 0, 0, 0))
    for y, row in enumerate(mask):
        for x, c in enumerate(row):
            if c == "o":
                im.putpixel((x, y), OUTLINE)
            elif c == "#":
                im.putpixel((x, y), body)
    return im


def stamp(im, pattern, ox, oy, color):
    for y, row in enumerate(pattern):
        for x, c in enumerate(row):
            if c == "X":
                im.putpixel((ox + x, oy + y), color)


cells = []
for name in ("cross", "circle", "triangle", "square"):
    im = cell_from(DISC, BODY)
    stamp(im, SYMBOLS[name], 2, 2, COLORS[name] + (255,))
    cells.append(im)
for name in ("left", "right", "up", "down"):
    im = cell_from(DPAD, DPAD_BODY)
    for p in ARMS[name]:
        im.putpixel(p, LIT)
    cells.append(im)
for name in ("L", "R"):
    im = cell_from(PILL, BODY)
    stamp(im, LETTERS[name], 3, 2, LETTER)
    cells.append(im)

strip = Image.new("RGBA", (9 * len(cells), 9), (0, 0, 0, 0))
for i, im in enumerate(cells):
    strip.paste(im, (9 * i, 0))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
strip.save(OUT)
print("->", os.path.normpath(OUT), strip.size)
