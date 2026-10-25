"""Generate the Vita LiveArea assets (port/sce_sys) from a screenshot of the game's title screen.

Usage: python3 -I tools/make_livearea.py <title.ppm> <out sce_sys dir>
  <title.ppm>: a port screenshot of the title screen (tools/port-probe.sh), any width x 160;
               the centred 128x160 column is used.
Vita requirements: icon0.png 128x128, bg.png 840x500, startup.png 280x158, all 8-bit palette PNGs.
Note: the shipped bg.png is made by tools/make_title_art.py (from title_screen.png); run that
again after this script.
"""
import sys
from pathlib import Path

from PIL import Image, ImageEnhance, ImageFilter


def palettize(im: Image.Image) -> Image.Image:
    return im.convert("RGB").quantize(colors=255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)


def main():
    src = Image.open(sys.argv[1]).convert("RGB")
    out = Path(sys.argv[2])
    (out / "livearea" / "contents").mkdir(parents=True, exist_ok=True)

    x0 = (src.width - 128) // 2
    title = src.crop((x0, 0, x0 + 128, 160))         # phone-sized title art
    art = title.crop((0, 0, 128, 146))                 # without the copyright line

    # icon0: logo + boulder, pixel-exact
    palettize(art.crop((0, 2, 128, 130))).save(out / "icon0.png")

    # bg: the art scaled 3x (pixel-exact), over a blurred darkened copy filling the frame
    back = art.resize((840, int(840 * art.height / art.width)), Image.BILINEAR)
    back = back.crop((0, (back.height - 500) // 2, 840, (back.height - 500) // 2 + 500))
    back = ImageEnhance.Brightness(back.filter(ImageFilter.GaussianBlur(12))).enhance(0.45)
    big = art.resize((art.width * 3, art.height * 3), Image.NEAREST)
    big = big.crop((0, 0, big.width, min(big.height, 500)))
    back.paste(big, ((840 - big.width) // 2, (500 - big.height) // 2))
    palettize(back).save(out / "livearea" / "contents" / "bg.png")

    # startup: the logo, 2x, on a dark gradient-free background
    logo = art.crop((4, 2, 124, 62)).resize((240, 120), Image.NEAREST)
    start = Image.new("RGB", (280, 158), (12, 10, 8))
    start.paste(logo, ((280 - logo.width) // 2, (158 - logo.height) // 2))
    palettize(start).save(out / "livearea" / "contents" / "startup.png")

    (out / "livearea" / "contents" / "template.xml").write_text(
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
    print(f"wrote LiveArea assets to {out}")


if __name__ == "__main__":
    main()
