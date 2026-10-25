// lcdui Image + Graphics: software rasteriser over 0xAARRGGBB buffers.
#include <algorithm>
#include <cstdio>

#include "midp/midp.h"
#include "platform/platform.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../third_party/stb_image.h"

namespace j {

namespace {
[[noreturn]] void throwIAE() {
  Ref<IllegalArgumentException> e = make<IllegalArgumentException>();
  throw Thrown{Ref<Throwable>(e)};
}

inline uint32_t blend(uint32_t src, uint32_t dst) {
  uint32_t a = src >> 24;
  if (a == 0xFF) return src;
  if (a == 0) return dst;
  uint32_t ia = 255 - a;
  uint32_t rb = ((src & 0xFF00FF) * a + (dst & 0xFF00FF) * ia) >> 8;
  uint32_t g = ((src & 0x00FF00) * a + (dst & 0x00FF00) * ia) >> 8;
  return 0xFF000000u | (rb & 0xFF00FF) | (g & 0x00FF00);
}
}  // namespace

// ---------------------------------------------------------------------------------------------
// Image

Image::Image(jint w, jint h, bool isMutable)
    : width(w), height(h), mutable_(isMutable), px(new uint32_t[size_t(w) * size_t(h)]) {}

Ref<Image> Image::createImage(jint w, jint h) {
  if (w <= 0 || h <= 0) throwIAE();
  auto img = make<Image>(w, h, true);
  std::fill_n(img->px.get(), size_t(w) * size_t(h), 0xFFFFFFFFu);  // mutable images start white
  return img;
}

Ref<Image> Image::createImage(Ref<String> resName) {
  std::vector<uint8_t> data = plat::readResource(resName->utf8());
  int w = 0, h = 0, n = 0;
  stbi_uc* rgba = data.empty() ? nullptr : stbi_load_from_memory(data.data(), int(data.size()), &w, &h, &n, 4);
  if (!rgba) {
    Ref<IOException> e = make<IOException>(resName);
    throw Thrown{Ref<Throwable>(e)};
  }
  auto img = make<Image>(w, h, false);
  for (int i = 0; i < w * h; ++i) {
    const stbi_uc* p = rgba + i * 4;
    img->px[size_t(i)] = uint32_t(p[3]) << 24 | uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | p[2];
  }
  stbi_image_free(rgba);
  return img;
}

Ref<Image> Image::createRGBImage(jarr<jint> rgb, jint w, jint h, jboolean processAlpha) {
  if (w <= 0 || h <= 0) throwIAE();
  if (rgb->length < w * h) throwAIOOBE(w * h, rgb->length);
  auto img = make<Image>(w, h, false);
  const jint* src = rgb->raw();
  for (jint i = 0; i < w * h; ++i) {
    uint32_t p = uint32_t(src[i]);
    img->px[size_t(i)] = processAlpha ? p : (p | 0xFF000000u);
  }
  return img;
}

void Image::getRGB(jarr<jint> rgb, jint offset, jint scanlength, jint x, jint y, jint w, jint h) {
  if (x < 0 || y < 0 || x + w > width || y + h > height) throwIAE();
  for (jint j = 0; j < h; ++j)
    for (jint i = 0; i < w; ++i) rgb[offset + j * scanlength + i] = jint(px[size_t(y + j) * width + x + i]);
}

Ref<Graphics> Image::getGraphics() {
  if (!mutable_) {
    Ref<IllegalStateException> e = make<IllegalStateException>();
    throw Thrown{Ref<Throwable>(e)};
  }
  auto g = make<Graphics>(px.get(), width, height);
  g->owner = Ref<Image>(this);
  return g;
}

// ---------------------------------------------------------------------------------------------
// Graphics: clip state

Graphics::Graphics(uint32_t* target, jint w, jint h) : buf(target), bw(w), bh(h) {
  rx0 = 0; ry0 = 0; rx1 = w; ry1 = h;
}

void Graphics::setColor(jint r, jint g, jint b) {
  if ((r | g | b) & ~0xFF) throwIAE();
  color = 0xFF000000u | uint32_t(r) << 16 | uint32_t(g) << 8 | uint32_t(b);
}

void Graphics::setClip(jint x, jint y, jint w, jint h) {
  rx0 = x + tx; ry0 = y + ty;
  rx1 = rx0 + std::max(w, 0); ry1 = ry0 + std::max(h, 0);
}

void Graphics::clipRect(jint x, jint y, jint w, jint h) {
  jint x0 = x + tx, y0 = y + ty;
  jint x1 = x0 + std::max(w, 0), y1 = y0 + std::max(h, 0);
  rx0 = std::max(rx0, x0); ry0 = std::max(ry0, y0);
  rx1 = std::min(rx1, x1); ry1 = std::min(ry1, y1);
  if (rx1 < rx0) rx1 = rx0;
  if (ry1 < ry0) ry1 = ry0;
}

// Effective clip: the requested clip intersected with the surface.
inline void Graphics::clip(jint& x0, jint& y0, jint& x1, jint& y1) const {
  x0 = std::max(rx0, 0); y0 = std::max(ry0, 0);
  x1 = std::min(rx1, bw); y1 = std::min(ry1, bh);
}

// ---------------------------------------------------------------------------------------------
// Primitives

void Graphics::hspan(jint x0, jint x1, jint y) {
  jint c0, c1, cy0, cy1;
  clip(c0, cy0, c1, cy1);
  if (y < cy0 || y >= cy1) return;
  x0 = std::max(x0, c0); x1 = std::min(x1, c1);
  if (x0 < x1) std::fill(buf + size_t(y) * bw + x0, buf + size_t(y) * bw + x1, color);
}

void Graphics::plot(jint x, jint y) { hspan(x, x + 1, y); }

void Graphics::fillRect(jint x, jint y, jint w, jint h) {
  if (w <= 0 || h <= 0) return;
  jint x0 = x + tx, y0 = y + ty;
  for (jint yy = y0; yy < y0 + h; ++yy) hspan(x0, x0 + w, yy);
}

void Graphics::drawRect(jint x, jint y, jint w, jint h) {
  if (w < 0 || h < 0) return;
  jint x0 = x + tx, y0 = y + ty;
  hspan(x0, x0 + w + 1, y0);
  if (h == 0) return;
  hspan(x0, x0 + w + 1, y0 + h);
  for (jint yy = y0 + 1; yy < y0 + h; ++yy) {
    plot(x0, yy);
    plot(x0 + w, yy);
  }
}

void Graphics::drawLine(jint x1, jint y1, jint x2, jint y2) {
  x1 += tx; y1 += ty; x2 += tx; y2 += ty;
  if (y1 == y2) { hspan(std::min(x1, x2), std::max(x1, x2) + 1, y1); return; }
  jint dx = std::abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
  jint dy = -std::abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
  jint err = dx + dy;
  for (;;) {
    plot(x1, y1);
    if (x1 == x2 && y1 == y2) break;
    jint e2 = 2 * err;
    if (e2 >= dy) { err += dy; x1 += sx; }
    if (e2 <= dx) { err += dx; y1 += sy; }
  }
}

// Horizontal inset of a rounded rectangle's edge at row `r` (0..h) for corner radii (rx, ry).
static jint roundInset(jint r, jint h, jint rx, jint ry) {
  if (rx <= 0 || ry <= 0) return 0;
  jint d = r < ry ? ry - r : (r > h - ry ? r - (h - ry) : 0);
  if (d <= 0) return 0;
  // x offset on the ellipse at vertical distance d from the corner centre
  long long num = (long long)ry * ry - (long long)d * d;
  if (num < 0) num = 0;
  double t = double(num) / (double(ry) * ry);
  jint off = jint(rx * __builtin_sqrt(t) + 0.5);
  return rx - off;
}

void Graphics::fillRoundRect(jint x, jint y, jint w, jint h, jint arcW, jint arcH) {
  if (w <= 0 || h <= 0) return;
  jint rx = std::min(std::abs(arcW), w) / 2, ry = std::min(std::abs(arcH), h) / 2;
  jint x0 = x + tx, y0 = y + ty;
  for (jint r = 0; r < h; ++r) {
    jint in = roundInset(r, h - 1, rx, ry);
    hspan(x0 + in, x0 + w - in, y0 + r);
  }
}

void Graphics::drawRoundRect(jint x, jint y, jint w, jint h, jint arcW, jint arcH) {
  if (w < 0 || h < 0) return;
  jint rx = std::min(std::abs(arcW), w) / 2, ry = std::min(std::abs(arcH), h) / 2;
  jint x0 = x + tx, y0 = y + ty;
  jint prev = roundInset(0, h, rx, ry);
  hspan(x0 + prev, x0 + w - prev + 1, y0);
  for (jint r = 1; r <= h; ++r) {
    jint in = roundInset(r, h, rx, ry);
    if (r == h) { hspan(x0 + in, x0 + w - in + 1, y0 + r); break; }
    // connect to the previous row so steep and shallow parts of the arc stay continuous
    jint a = std::min(in, prev), b = std::max(in, prev);
    hspan(x0 + a, x0 + std::max(b, a + 1), y0 + r);
    hspan(x0 + w - std::max(b, a + 1) + 1, x0 + w - a + 1, y0 + r);
    prev = in;
  }
}

// ---------------------------------------------------------------------------------------------
// Images

void Graphics::blit(const uint32_t* src, jint srcStride, jint sx, jint sy, jint w, jint h,
                    jint transform, jint dx, jint dy, bool alpha) {
  bool swap = transform == TRANS_ROT90 || transform == TRANS_ROT270 ||
              transform == TRANS_MIRROR_ROT90 || transform == TRANS_MIRROR_ROT270;
  jint dw = swap ? h : w, dh = swap ? w : h;
  jint c0, cy0, c1, cy1;
  clip(c0, cy0, c1, cy1);
  jint x0 = std::max(dx, c0), y0 = std::max(dy, cy0);
  jint x1 = std::min(dx + dw, c1), y1 = std::min(dy + dh, cy1);
  if (x0 >= x1 || y0 >= y1) return;

  // Source pixel for destination (u, v) within the transformed region: s = base + u*du + v*dv.
  jint bx, by, ux, uy, vx, vy;
  switch (transform) {
    default:
    case TRANS_NONE:          bx = 0;     by = 0;     ux = 1;  uy = 0;  vx = 0;  vy = 1;  break;
    case TRANS_MIRROR:        bx = w - 1; by = 0;     ux = -1; uy = 0;  vx = 0;  vy = 1;  break;
    case TRANS_ROT180:        bx = w - 1; by = h - 1; ux = -1; uy = 0;  vx = 0;  vy = -1; break;
    case TRANS_MIRROR_ROT180: bx = 0;     by = h - 1; ux = 1;  uy = 0;  vx = 0;  vy = -1; break;
    case TRANS_ROT90:         bx = 0;     by = h - 1; ux = 0;  uy = -1; vx = 1;  vy = 0;  break;
    case TRANS_ROT270:        bx = w - 1; by = 0;     ux = 0;  uy = 1;  vx = -1; vy = 0;  break;
    case TRANS_MIRROR_ROT90:  bx = w - 1; by = h - 1; ux = 0;  uy = -1; vx = -1; vy = 0;  break;
    case TRANS_MIRROR_ROT270: bx = 0;     by = 0;     ux = 0;  uy = 1;  vx = 1;  vy = 0;  break;
  }
  for (jint yy = y0; yy < y1; ++yy) {
    jint v = yy - dy;
    uint32_t* d = buf + size_t(yy) * bw;
    for (jint xx = x0; xx < x1; ++xx) {
      jint u = xx - dx;
      jint px_ = sx + bx + u * ux + v * vx;
      jint py_ = sy + by + u * uy + v * vy;
      uint32_t p = src[size_t(py_) * srcStride + px_];
      d[xx] = alpha ? blend(p, d[xx]) : p;
    }
  }
}

static void applyAnchor(jint& x, jint& y, jint w, jint h, jint anchor) {
  if (anchor == 0) return;  // TOP | LEFT
  if (anchor & Graphics::HCENTER) x -= w / 2;
  else if (anchor & Graphics::RIGHT) x -= w;
  if (anchor & Graphics::VCENTER) y -= h / 2;
  else if (anchor & (Graphics::BOTTOM | Graphics::BASELINE)) y -= h;
}

void Graphics::drawImage(Ref<Image> img, jint x, jint y, jint anchor) {
  Image& im = *img;
  x += tx; y += ty;
  applyAnchor(x, y, im.width, im.height, anchor);
  blit(im.px.get(), im.width, 0, 0, im.width, im.height, TRANS_NONE, x, y, !im.mutable_);
}

void Graphics::drawRegion(Ref<Image> src, jint xSrc, jint ySrc, jint w, jint h, jint transform,
                          jint xDst, jint yDst, jint anchor) {
  Image& im = *src;
  if (w <= 0 || h <= 0) return;
  if (xSrc < 0 || ySrc < 0 || xSrc + w > im.width || ySrc + h > im.height || transform < 0 || transform > 7)
    throwIAE();
  if (im.px.get() == buf) throwIAE();  // drawing an image onto itself
  bool swap = transform == TRANS_ROT90 || transform == TRANS_ROT270 ||
              transform == TRANS_MIRROR_ROT90 || transform == TRANS_MIRROR_ROT270;
  jint x = xDst + tx, y = yDst + ty;
  applyAnchor(x, y, swap ? h : w, swap ? w : h, anchor);
  blit(im.px.get(), im.width, xSrc, ySrc, w, h, transform, x, y, !im.mutable_);
}

void Graphics::drawRGB(jarr<jint> rgb, jint offset, jint scanlength, jint x, jint y, jint w, jint h,
                       jboolean processAlpha) {
  if (w <= 0 || h <= 0) return;
  Array<jint>& a = *rgb;
  // Every index the call could touch must be inside the array (MIDP throws otherwise).
  jint first = offset + std::min(0, (h - 1) * scanlength);
  jint last = offset + std::max(0, (h - 1) * scanlength) + w - 1;
  if (first < 0 || last >= a.length) throwAIOOBE(first < 0 ? first : last, a.length);
  x += tx; y += ty;
  jint c0, cy0, c1, cy1;
  clip(c0, cy0, c1, cy1);
  jint x0 = std::max(x, c0), y0 = std::max(y, cy0);
  jint x1 = std::min(x + w, c1), y1 = std::min(y + h, cy1);
  for (jint yy = y0; yy < y1; ++yy) {
    const jint* s = a.raw() + offset + (yy - y) * scanlength;
    uint32_t* d = buf + size_t(yy) * bw;
    for (jint xx = x0; xx < x1; ++xx) {
      uint32_t p = uint32_t(s[xx - x]);
      d[xx] = processAlpha ? blend(p, d[xx]) : (p | 0xFF000000u);
    }
  }
}

}  // namespace j
