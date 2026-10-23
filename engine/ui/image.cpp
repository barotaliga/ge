// The 2D layer's image, defined: filling, the bilinear sample, the three-pass box blur, fitting
// an image to a box, and the rectangle helpers.
//
// ge:summary The 2D layer's image defined: fill, bilinear sample, box blur, fit and contain
// ge:keywords ui image sample bilinear blur box-blur fit cover contain rect intersect inset
// ge:skill ui
// ge:test tests/ui_raster_test.cpp
#include "ui/image.hpp"

#include <algorithm>
#include <cmath>

namespace ge::ui {

Rect intersect(Rect a, Rect b) {
  const float x0 = std::max(a.x, b.x), y0 = std::max(a.y, b.y);
  const float x1 = std::min(a.right(), b.right()), y1 = std::min(a.bottom(), b.bottom());
  return {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
}

Rect inset(Rect r, float d) { return {r.x - d, r.y - d, r.w + 2 * d, r.h + 2 * d}; }

namespace {

uint8_t to_byte(float v) {
  return static_cast<uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
}

}  // namespace

Image make_image(int width, int height, Color fill) {
  Image img;
  img.width = std::max(0, width);
  img.height = std::max(0, height);
  img.rgba.resize(static_cast<size_t>(img.width) * img.height * 4);
  const uint8_t px[4] = {to_byte(fill.r), to_byte(fill.g), to_byte(fill.b), to_byte(fill.a)};
  for (size_t i = 0; i < img.rgba.size(); i += 4)
    for (int k = 0; k < 4; k++) img.rgba[i + k] = px[k];
  return img;
}

Color sample(const Image& img, float x, float y) {
  if (img.empty()) return {0, 0, 0, 0};
  const float fx = std::clamp(x - 0.5f, 0.0f, static_cast<float>(img.width - 1));
  const float fy = std::clamp(y - 0.5f, 0.0f, static_cast<float>(img.height - 1));
  const int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy);
  const int x1 = std::min(x0 + 1, img.width - 1), y1 = std::min(y0 + 1, img.height - 1);
  const float tx = fx - static_cast<float>(x0), ty = fy - static_cast<float>(y0);
  auto at = [&](int px, int py, int k) {
    return static_cast<float>(img.rgba[(static_cast<size_t>(py) * img.width + px) * 4 + k]);
  };
  float out[4];
  for (int k = 0; k < 4; k++) {
    const float top = at(x0, y0, k) + (at(x1, y0, k) - at(x0, y0, k)) * tx;
    const float bot = at(x0, y1, k) + (at(x1, y1, k) - at(x0, y1, k)) * tx;
    out[k] = (top + (bot - top) * ty) / 255.0f;
  }
  return {out[0], out[1], out[2], out[3]};
}

namespace {

// One box pass of half-width r along rows (step 4) or columns (step width*4), in integers.
void box_pass(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int count, int lines,
              size_t step, size_t line_step, int r) {
  const int win = 2 * r + 1;
  for (int line = 0; line < lines; line++) {
    const size_t base = static_cast<size_t>(line) * line_step;
    for (int k = 0; k < 4; k++) {
      auto at = [&](int i) {
        return static_cast<int>(
            src[base + static_cast<size_t>(std::clamp(i, 0, count - 1)) * step + k]);
      };
      int sum = 0;
      for (int i = -r; i <= r; i++) sum += at(i);
      for (int i = 0; i < count; i++) {
        dst[base + static_cast<size_t>(i) * step + k] = static_cast<uint8_t>((sum + win / 2) / win);
        sum += at(i + r + 1) - at(i - r);
      }
    }
  }
}

}  // namespace

Image blurred(const Image& img, int radius) {
  if (radius <= 0 || img.empty()) return img;
  // Three passes of a box of half-width r have the variance of a Gaussian of sigma ~ radius / 2.
  const int r = std::max(1, radius / 2);
  Image out = img;
  std::vector<uint8_t> tmp(img.rgba.size());
  for (int pass = 0; pass < 3; pass++) {
    box_pass(out.rgba, tmp, img.width, img.height, 4, static_cast<size_t>(img.width) * 4, r);
    box_pass(tmp, out.rgba, img.height, img.width, static_cast<size_t>(img.width) * 4, 4, r);
  }
  return out;
}

Rect fit_uv(const Image& img, float box_w, float box_h, Fit fit, float focus_x, float focus_y,
            float zoom) {
  const float iw = static_cast<float>(img.width), ih = static_cast<float>(img.height);
  if (img.empty() || box_w <= 0 || box_h <= 0) return {0, 0, iw, ih};
  zoom = std::max(zoom, 1.0f);
  float w = iw, h = ih;
  const float box_aspect = box_w / box_h, img_aspect = iw / ih;
  switch (fit) {
    case Fit::stretch:
      break;
    case Fit::cover:
      if (img_aspect > box_aspect) {
        w = ih * box_aspect;  // the image is wider: crop its sides
      } else {
        h = iw / box_aspect;  // taller: crop top and bottom
      }
      break;
    case Fit::contain:
      if (img_aspect > box_aspect) {
        h = iw / box_aspect;
      } else {
        w = ih * box_aspect;
      }
      break;
  }
  w /= zoom, h /= zoom;
  // The window keeps the focus point where it is in the image, within the image's edges.
  float x = focus_x * iw - focus_x * w, y = focus_y * ih - focus_y * h;
  if (fit != Fit::contain) {
    x = std::clamp(x, 0.0f, iw - w);
    y = std::clamp(y, 0.0f, ih - h);
  }
  return {x, y, w, h};
}

Rect contain_rect(const Image& img, Rect box) {
  if (img.empty() || box.w <= 0 || box.h <= 0) return box;
  const float s =
      std::min(box.w / static_cast<float>(img.width), box.h / static_cast<float>(img.height));
  const float w = static_cast<float>(img.width) * s, h = static_cast<float>(img.height) * s;
  return {box.x + (box.w - w) / 2, box.y + (box.h - h) / 2, w, h};
}

}  // namespace ge::ui
