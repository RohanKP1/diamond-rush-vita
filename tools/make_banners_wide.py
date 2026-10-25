#!/usr/bin/env python3
# Widescreen world-map banners: extends each 128x24 banner (Angkor Wat, Bavaria, Tibet) to 282
# pixels (tools/widen.py): the sides are the banner mirrored at its edges (seamless joins), and
# where that would mirror the temple / castle / monastery, trees, forest, mountains and sky are
# synthesised instead, so the sides show scenery rather than repeated buildings.
#
#   python3 tools/make_banners_wide.py
# Inputs: port/art/banner_phone_<n>.png (made by tools/extract-banners.py); outputs port/res/port_banner_<n>.png,
# which class_9 draws in place of the banner sprite on a 282-wide screen.
import os
import sys
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from widen import reflect_widen  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")

BUILDINGS = [  # columns of each banner's building (all rows excluded as a source)
    (74, 118),  # Angkor Wat temple
    (72, 106),  # Bavarian castle
    (70, 108),  # Tibetan monastery
]
EXTRA = {2: [(0, 0, 18, 22)]}  # Tibet: the big cloud and the bird at the left edge (not to mirror)

for n, (x0, x1) in enumerate(BUILDINGS):
    src = np.asarray(Image.open(os.path.join(ROOT, "port", "art", f"banner_phone_{n}.png")).convert("RGB"),
                     dtype=np.float64)
    out = reflect_widen(src, 282, [(x0, 0, x1, src.shape[0])] + EXTRA.get(n, []), half=3, ycost=0.15)
    path = os.path.join(ROOT, "port", "res", f"port_banner_{n}.png")
    Image.fromarray(out.clip(0, 255).astype(np.uint8)).save(path)
    print("->", os.path.normpath(path))
