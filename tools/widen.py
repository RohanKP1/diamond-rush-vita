# Widens pixel-art pictures by exemplar-based inpainting (Criminisi et al.): unknown pixels are
# filled patch by patch, from the edges of what is known inwards, each with the best-matching
# patch taken from the picture itself. Pixels are copied, never blended, so the art keeps its
# pixels. Used by tools/make_banners_wide.py and tools/make_title_art.py. Deterministic.
#
# widen():         the new side areas are synthesised outwards from the picture's edges.
# reflect_widen(): the sides are the picture mirrored at its edges (seamless joins); only the
#                  mirrored copies of excluded things (logos, characters...) are synthesised.
import numpy as np


def _regions_mask(shape, regions):
    m = np.zeros(shape, bool)
    for x0, y0, x1, y1 in regions:
        m[max(y0, 0):max(y1, 0), max(x0, 0):max(x1, 0)] = True
    return m


def inpaint(canvas, known, paint, exclude=(), half=5, ycost=0.0, reuse_cost=0.5, mirror=True):
    """Fills canvas (H, W, 3) where known is False, with patches from paint (h, w, 3) that avoid
    the exclude regions (x0, y0, x1, y1) of paint. Returns the filled canvas.

    ycost: penalty per squared row of distance between a source patch and the spot it fills,
      relative to the typical match cost; keeps sky at the top, ground at the bottom.
    reuse_cost: each reuse of a source patch makes it this much (relatively) worse.
    mirror: also offer every patch mirrored left-right.
    """
    H, W = canvas.shape[:2]
    ph, pw = paint.shape[:2]
    P = 2 * half + 1

    # working canvas padded by `half` on every side (padding: never known, never filled)
    img = np.zeros((H + 2 * half, W + 2 * half, 3))
    img[half:half + H, half:half + W] = canvas
    kn = np.zeros(img.shape[:2], bool)
    kn[half:half + H, half:half + W] = known
    inside = np.zeros(img.shape[:2], bool)
    inside[half:half + H, half:half + W] = True
    conf = kn.astype(np.float64)

    excl = _regions_mask((ph, pw), exclude)
    centres = [(y, x) for y in range(half, ph - half) for x in range(half, pw - half)
               if not excl[y - half:y + half + 1, x - half:x + half + 1].any()]
    patches = [paint[y - half:y + half + 1, x - half:x + half + 1] for y, x in centres]
    src = [p.reshape(-1) for p in patches]
    cy = [y for y, _ in centres]
    if mirror:
        src += [p[:, ::-1].reshape(-1) for p in patches]
        cy += cy
    src = np.stack(src)
    cy = np.array(cy, dtype=np.float64)
    uses = np.zeros(len(src))

    def gray(a):
        return a @ np.array([0.299, 0.587, 0.114])

    while True:
        unknown = inside & ~kn
        if not unknown.any():
            break
        nb = np.zeros_like(kn)
        nb[1:, :] |= kn[:-1, :]
        nb[:-1, :] |= kn[1:, :]
        nb[:, 1:] |= kn[:, :-1]
        nb[:, :-1] |= kn[:, 1:]
        front = np.argwhere(unknown & nb)

        # priority = confidence * data term (isophote strength across the front)
        g = gray(img) * kn
        gy, gx = np.gradient(g)
        ny, nx = np.gradient(kn.astype(np.float64))
        best, bp = -1.0, None
        for y, x in front:
            if y < half or x < half or y >= H + half or x >= W + half:
                continue
            win = (slice(y - half, y + half + 1), slice(x - half, x + half + 1))
            c = conf[win].sum() / (P * P)
            kw = kn[win]
            pgx, pgy = gx[win] * kw, gy[win] * kw
            i = np.argmax(pgx ** 2 + pgy ** 2)
            iso = np.array([-pgy.flat[i], pgx.flat[i]])
            n = np.array([nx[y, x], ny[y, x]])
            nn = np.hypot(*n)
            d = abs(iso @ (n / nn)) / 255.0 if nn > 0 else 0.0
            pr = c * (d + 0.05)
            if pr > best:
                best, bp = pr, (y, x)
        y, x = bp
        win = (slice(y - half, y + half + 1), slice(x - half, x + half + 1))

        # best source patch: SSD over the target's known pixels, plus reuse and row penalties
        tgt = img[win].reshape(-1)
        tk = np.repeat(kn[win].reshape(-1), 3)
        ssd = ((src[:, tk] - tgt[tk]) ** 2).sum(axis=1)
        cost = ssd * (1 + reuse_cost * uses)
        if ycost:
            cost = cost + ycost * (np.median(ssd) + 1.0) * (cy - (y - half)) ** 2
        j = int(np.argmin(cost))
        uses[j] += 1
        patch = src[j].reshape(P, P, 3)

        fill = inside[win] & ~kn[win]
        img[win][fill] = patch[fill]
        conf[win][fill] = conf[win].sum() / (P * P)
        kn[win] |= fill

    return img[half:half + H, half:half + W]


def widen(paint, out_w, exclude=(), **kw):
    """paint centred in out_w columns; the sides are synthesised outwards from its edges."""
    ph, pw = paint.shape[:2]
    ox = (out_w - pw) // 2
    canvas = np.zeros((ph, out_w, 3))
    known = np.zeros((ph, out_w), bool)
    canvas[:, ox:ox + pw] = paint
    known[:, ox:ox + pw] = True
    return inpaint(canvas, known, paint, exclude, **kw)


def reflect_widen(paint, out_w, exclude=(), **kw):
    """paint centred in out_w columns; the sides are paint mirrored at its edges (so the joins are
    seamless), except where that would mirror an excluded region: those holes are synthesised."""
    ph, pw = paint.shape[:2]
    ox = (out_w - pw) // 2
    canvas = np.zeros((ph, out_w, 3))
    known = np.zeros((ph, out_w), bool)
    excl = _regions_mask((ph, pw), exclude)
    for x in range(out_w):
        c = x - ox
        if c < 0:
            c = -1 - c          # mirrored at the left edge
        elif c >= pw:
            c = 2 * pw - 1 - c  # mirrored at the right edge
        c = min(max(c, 0), pw - 1)
        canvas[:, x] = paint[:, c]
        inside = 0 <= x - ox < pw
        known[:, x] = True if inside else ~excl[:, c]
    return inpaint(canvas, known, paint, exclude, **kw)
