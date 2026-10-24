// AN IMAGE THE 2D LAYER DRAWS OR DRAWS INTO: RGBA8, straight alpha, sRGB, row-major from the top,
// the same layout as a lab picture (`lab::Picture`), so a lab hands one to the other by moving
// the bytes. A backdrop, a logo, a font's glyph and the CPU raster's target are all images.
//
// `fit_uv` is how an image is placed in a box of another shape: stretched, covering it (cropped
// round a focus point, the way a full-screen backdrop is shown on every screen shape) or
// contained in it (letterboxed). `zoom` above 1 crops further in, which is how a backdrop
// slowly drifts (background.hpp).
//
// ge:summary The 2D layer's RGBA8 image, its bilinear sample, a box blur, and fitting to a box
// ge:keywords ui image rgba picture texture sample bilinear fit cover contain stretch focus
// ge:keywords crop zoom blur backdrop
// ge:skill ui
// ge:test tests/ui_raster_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "ui/paint.hpp"

namespace ge::ui {

// A rectangle in pixels, y down.
struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  float right() const { return x + w; }
  float bottom() const { return y + h; }
  bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};
Rect intersect(Rect a, Rect b);
// `r` grown by `d` on every side (shrunk for d < 0).
Rect inset(Rect r, float d);

struct Image {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgba;  // width * height * 4

  bool empty() const { return width <= 0 || height <= 0; }
};

Image make_image(int width, int height, Color fill);

// The image's colour at (x, y) in its pixels (pixel centres at +0.5), bilinear, clamped at the
// edges.
Color sample(const Image& img, float x, float y);

// A blur of about `radius` px: three box passes each way, which approaches a Gaussian. A
// backdrop behind a menu panel is blurred so the panel's words stay readable over it.
Image blurred(const Image& img, int radius);

enum class Fit : uint8_t { stretch, cover, contain };

// The source rectangle (in the image's pixels) that shows `img` in a box of `box_w` x `box_h`.
// `focus_x`, `focus_y` (0..1) is the point of the image kept in view when it is cropped; `zoom`
// (1 or more) crops further in round it. For `contain` the rectangle is larger than the image
// and the box shows its edges clamped: draw a contained image in a box of the fitted size
// instead (`contain_rect`).
Rect fit_uv(const Image& img, float box_w, float box_h, Fit fit, float focus_x = 0.5f,
            float focus_y = 0.5f, float zoom = 1);
// The largest box of the image's shape inside `box`, centred.
Rect contain_rect(const Image& img, Rect box);

}  // namespace ge::ui
