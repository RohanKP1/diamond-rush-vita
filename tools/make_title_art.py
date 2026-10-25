#!/usr/bin/env python3
# Title art:
#   port/res/port_title_wide.png            282x160: the title / main menu background. The middle
#       is the phone's own 128x160 title painting (port/art/title_phone.png), pixel for pixel; the
#       sides are the painted extension in title_screen.png, scaled and positioned so that its copy
#       of the painting lies exactly on the phone's (found by search). Over the painting's outer
#       FADE columns the crisp pixels ease into the softer sides, so there is no visible join.
#   port/sce_sys/livearea/contents/bg.png   840x500: the Vita LiveArea background: the menu
#       background above, enlarged in whole pixels (4x) then fitted (centre-cropped)
#   port/sce_sys/livearea/contents/startup.png  280x158: the LiveArea start gate, from
#       startup.png (centred), with rounded corners (CORNER_RADIUS)
#
#   python3 tools/make_title_art.py
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
LA = os.path.join(ROOT, "port", "sce_sys", "livearea", "contents")
W, H, PW = 282, 160, 128
OX = (W - PW) // 2
FADE = 24        # columns at each edge of the painting that blend into the sides
SIDE_BLUR = 1.2  # how soft the sides are, at this size (the eased edge goes to this softness)
LOGO_ROWS = 70   # the painting's rows holding the logo (kept crisp across the FADE columns)
CORNER_RADIUS = 14  # start gate image's rounded corners, in its own pixels
BORDER = 4          # width of startup.png's white border

phone_im = Image.open(os.path.join(ROOT, "port", "art", "title_phone.png")).convert("RGB")
phone = np.asarray(phone_im, dtype=np.float64)
wide_src = Image.open(os.path.join(ROOT, "title_screen.png")).convert("RGB")

# where does title_screen's copy of the painting match the phone's? (scale = height, offset)
best = None
for h in range(160, 200):
    w = round(wide_src.width * h / wide_src.height)
    if w < W:
        continue
    sc = np.asarray(wide_src.resize((w, h), Image.Resampling.BOX), dtype=np.float64)
    for oy in range(0, h - H + 1):
        for ox in range(max(OX, (w - PW) // 2 - 20), min(w - W + OX, (w - PW) // 2 + 20) + 1):
            # rows above the copyright line, every second pixel
            d = ((sc[oy:oy + 140:2, ox:ox + PW:2] - phone[0:140:2, ::2]) ** 2).mean()
            if best is None or d < best[0]:
                best = (d, h, ox, oy, sc)
_, h, ox, oy, sc = best
print(f"title_screen.png at height {h}: painting at ({ox}, {oy}), rms {best[0] ** 0.5:.1f}")
sides = sc[oy:oy + H, ox - OX:ox - OX + W]

soft = np.asarray(phone_im.filter(ImageFilter.GaussianBlur(SIDE_BLUR)), dtype=np.float64)
out = sides.copy()
for c in range(PW):
    e = min(c, PW - 1 - c)  # distance from the painting's nearer edge
    t = min(e / FADE, 1.0)  # 0 at the edge .. 1 from FADE columns in
    a = min(e / (FADE / 2), 1.0)  # sides -> painting over the first half
    col = (1 - t) * soft[:, c] + t * phone[:, c]  # soft -> crisp painting
    out[:, OX + c] = (1 - a) * sides[:, OX + c] + a * col

# the logo reaches into the FADE columns (its two Ds): it stays crisp, only the scenery around it
# eases into the sides. Logo pixels: colours of the top LOGO_ROWS rows that the scenery below never
# uses (the logo has its own palette), holes filled, grown 1 px to take in its dark outline.
rgb = np.asarray(phone_im, dtype=np.int64)
key = rgb[..., 0] << 16 | rgb[..., 1] << 8 | rgb[..., 2]
logo = np.zeros((H, PW), bool)
logo[:LOGO_ROWS] = np.isin(key[:LOGO_ROWS], np.setdiff1d(key[:LOGO_ROWS], key[LOGO_ROWS + 5:140]))
outside = np.zeros_like(logo)  # flood fill of the non-logo pixels from the image border
stack = [(y, x) for y in range(H) for x in (0, PW - 1)] + [(y, x) for x in range(PW) for y in (0, H - 1)]
while stack:
    y, x = stack.pop()
    if 0 <= y < H and 0 <= x < PW and not logo[y, x] and not outside[y, x]:
        outside[y, x] = True
        stack += [(y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)]
logo = ~outside
grown = logo.copy()
grown[1:] |= logo[:-1]; grown[:-1] |= logo[1:]; grown[:, 1:] |= logo[:, :-1]; grown[:, :-1] |= logo[:, 1:]
out[:, OX:OX + PW][grown] = phone[grown]
path = os.path.join(ROOT, "port", "res", "port_title_wide.png")
menu_bg = Image.fromarray(out.clip(0, 255).round().astype(np.uint8))
menu_bg.save(path)
print("->", os.path.normpath(path))


def fit(im, w, h):
    """Scale to height h (area averaging when shrinking), then crop the width to w, centred."""
    sw = round(im.width * h / im.height)
    im = im.resize((sw, h), Image.Resampling.BOX if h < im.height else Image.Resampling.LANCZOS)
    x0 = (sw - w) // 2
    return im.crop((x0, 0, x0 + w, h))


# --- LiveArea (the Vita requires 8-bit palette PNGs: error diffusion keeps colours closest)
big = menu_bg.resize((W * 4, H * 4), Image.Resampling.NEAREST)  # whole pixels first: even sizes
fit(big, 840, 500).quantize(colors=256, method=Image.Quantize.MEDIANCUT,
                            dither=Image.Dither.FLOYDSTEINBERG).save(os.path.join(LA, "bg.png"))
print("->", os.path.normpath(os.path.join(LA, "bg.png")))

src = os.path.join(ROOT, "startup.png")
if os.path.exists(src):
    art = Image.open(src).convert("RGBA")
    assert art.width <= 280 and art.height <= 158, art.size
    # rounded corners. The art has a square white border (BORDER px, inset 1 px): it is redrawn as
    # a rounded frame, and the picture inside is clipped to the matching inner rounded corner.
    # Masks are 4x supersampled for smooth edges.
    def rounded(box, r):
        m = Image.new("L", (art.width * 4, art.height * 4), 0)
        ImageDraw.Draw(m).rounded_rectangle([v * 4 for v in box], r * 4, fill=255)
        return np.asarray(m.resize(art.size, Image.Resampling.BOX), dtype=np.float64) / 255
    w, h = art.size
    outer = rounded((1, 1, w - 1, h - 1), CORNER_RADIUS)
    inner = rounded((1 + BORDER, 1 + BORDER, w - 1 - BORDER, h - 1 - BORDER), CORNER_RADIUS - BORDER)
    px = np.asarray(art, dtype=np.float64)
    rgb = px[:, :, :3] * inner[:, :, None] + 255 * (1 - inner[:, :, None])  # picture, else white frame
    art = Image.fromarray(np.dstack([rgb, outer * 255]).round().astype(np.uint8), "RGBA")
    gate = Image.new("RGBA", (280, 158), (0, 0, 0, 0))
    gate.paste(art, ((280 - art.width) // 2, (158 - art.height) // 2))
    gate.quantize(colors=256, method=Image.Quantize.FASTOCTREE,
                  dither=Image.Dither.FLOYDSTEINBERG).save(os.path.join(LA, "startup.png"))
    print("->", os.path.normpath(os.path.join(LA, "startup.png")))
