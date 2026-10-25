#!/usr/bin/env python3
# Recovers the three world-map banners (the sprite art, without the world name the game prints
# over it) from widescreen probe screenshots of the stage maps taken while the banner was drawn
# by class_9.mirrorExtend: the centre slot holds columns 0..127 with the name on top, and the
# mirrored copies beside it hold the same columns without the name.
#   python3 tools/extract-banners.py angkor.ppm bavaria.ppm tibet.ppm  -> port/art/banner_phone_<n>.png
import sys
from PIL import Image

W, OX, ROWS = 282, 77, 24
for n, path in enumerate(sys.argv[1:4]):
    shot = Image.open(path).convert("RGB")
    assert shot.size == (W, 160), shot.size
    out = Image.new("RGB", (128, ROWS))
    px, sp = out.load(), shot.load()
    for c in range(128):
        # left copy: column c mirrored to screen OX - 1 - c (visible for c < OX);
        # right copy: column c mirrored to screen OX + 255 - c (visible for c > 255 - W + OX)
        if c < OX:
            sx = OX - 1 - c
        else:
            sx = OX + 255 - c
        for y in range(ROWS):
            px[c, y] = sp[sx, y]
    out.save(f"port/art/banner_phone_{n}.png")
    print(f"port/art/banner_phone_{n}.png")
