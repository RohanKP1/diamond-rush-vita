#!/usr/bin/env python3
# Tiles a probe run's screenshots into one image, two per row, each labelled with its index.
#   python3 tools/contact-sheet.py <probe-dir> <out.png>
import glob
import sys
from PIL import Image, ImageDraw

shots = sorted(glob.glob(sys.argv[1] + "/*.ppm"))
ims = [Image.open(p).convert("RGB") for p in shots]
w = max(im.width for im in ims)
h = max(im.height for im in ims)
sheet = Image.new("RGB", (w * 2 + 4, (h + 4) * ((len(ims) + 1) // 2)), (255, 0, 255))
d = ImageDraw.Draw(sheet)
for i, im in enumerate(ims):
    x, y = (i % 2) * (w + 4), (i // 2) * (h + 4)
    sheet.paste(im, (x, y))
    d.text((x + 2, y + 2), str(i), fill=(255, 0, 255))
sheet.save(sys.argv[2])
