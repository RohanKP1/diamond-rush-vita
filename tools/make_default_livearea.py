#!/usr/bin/env python3
"""Basic LiveArea images from the jar's own icon, for builds that have none in port/sce_sys.

build.sh runs this when port/sce_sys/icon0.png is missing. For nicer ones, see
tools/make_livearea.py (from a title screenshot) and tools/make_title_art.py.

Usage: python3 -I tools/make_default_livearea.py <jar dir> <out sce_sys dir>
Vita requirements: icon0.png 128x128, bg.png 840x500, startup.png 280x158, 8-bit palette PNGs.
"""
import sys
from pathlib import Path

from PIL import Image


def palettize(im):
    return im.convert("RGB").quantize(colors=255, method=Image.Quantize.MEDIANCUT,
                                      dither=Image.Dither.NONE)


def on(size, im, scale, colour=(12, 10, 8)):
    canvas = Image.new("RGB", size, colour)
    big = im.resize((im.width * scale, im.height * scale), Image.Resampling.NEAREST)
    canvas.paste(big, ((size[0] - big.width) // 2, (size[1] - big.height) // 2), big)
    return canvas


def main():
    icon = Image.open(Path(sys.argv[1]) / "icon.png").convert("RGBA")
    out = Path(sys.argv[2])
    contents = out / "livearea" / "contents"
    contents.mkdir(parents=True, exist_ok=True)
    palettize(on((128, 128), icon, max(1, 128 // max(icon.size)))).save(out / "icon0.png")
    palettize(on((840, 500), icon, max(1, 400 // max(icon.size)))).save(contents / "bg.png")
    palettize(on((280, 158), icon, max(1, 140 // max(icon.size)))).save(contents / "startup.png")
    (contents / "template.xml").write_text(
        """<?xml version="1.0" encoding="utf-8"?>
<livearea style="a1" format-ver="01.00" content-rev="1">
  <livearea-background>
    <image>bg.png</image>
  </livearea-background>
  <gate>
    <startup-image>startup.png</startup-image>
  </gate>
</livearea>
""", encoding="utf-8")
    print(f"wrote default LiveArea assets to {out}")


if __name__ == "__main__":
    main()
