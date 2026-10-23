// WHAT A 2D SHAPE IS FILLED WITH: a colour, a linear gradient or a radial gradient, each
// gradient up to eight stops. A menu's backdrop, a scrim that darkens one side of the screen
// behind a column of buttons, a vignette, a button's sheen and a console's panel are all one of
// these. The paint is data: the CPU raster (raster.hpp) evaluates it per pixel, and the GPU pass
// (M22b) hands the same numbers to its fragment shader.
//
// COLOUR is straight (not premultiplied) RGBA, 0..1, in the display's encoding (sRGB): the 2D
// layer blends in that encoding, as every UI toolkit does, so a 50% black over white reads as
// the mid grey a designer expects. A gradient interpolates its stops in the same encoding.
//
// A LINEAR gradient runs from `from` to `to` (pixels in the shape's frame, top-left 0,0, y
// down): before `from` it holds its first stop, past `to` its last. A RADIAL one runs from
// `from` (the centre, t = 0) out to `radius` (t = 1), scaled by `aspect` across y so an ellipse
// fits a wide screen. `linear_at_angle` builds one from an angle, as CSS spells it.
//
// ge:summary The 2D layer's paints: straight sRGB colour, linear and radial gradients, stops
// ge:keywords ui paint color colour gradient linear radial stop scrim vignette srgb hex rgba
// ge:keywords lerp backdrop
// ge:skill ui
// ge:test tests/ui_paint_test.cpp
#pragma once

#include <array>
#include <cstdint>

namespace ge::ui {

struct Color {
  float r = 0, g = 0, b = 0, a = 1;
};

// 0xRRGGBB, and its alpha 0..1: `hex(0x1e88e5)`, `hex(0x000000, 0.6f)`.
constexpr Color hex(uint32_t rgb, float alpha = 1) {
  return {static_cast<float>((rgb >> 16) & 0xff) / 255.0f,
          static_cast<float>((rgb >> 8) & 0xff) / 255.0f, static_cast<float>(rgb & 0xff) / 255.0f,
          alpha};
}
constexpr Color with_alpha(Color c, float a) { return {c.r, c.g, c.b, a}; }
Color lerp(Color a, Color b, float t);

struct Stop {
  float at = 0;  // 0..1 along the gradient, ascending
  Color color;
};

enum class PaintKind : uint8_t { solid, linear, radial };

inline constexpr int kMaxStops = 8;

struct Paint {
  PaintKind kind = PaintKind::solid;
  int count = 1;  // stops in use; a solid paint is stops[0]
  std::array<Stop, kMaxStops> stops{};
  float from_x = 0, from_y = 0;  // linear: the start; radial: the centre
  float to_x = 0, to_y = 0;      // linear: the end
  float radius = 1;              // radial: where t reaches 1
  float aspect = 1;              // radial: the y distance's scale (1: a circle)

  // The paint's colour at (x, y) in its shape's frame.
  Color at(float x, float y) const;
  // Whether every stop is fully opaque (a draw that covers what is under it).
  bool opaque() const;
};

Paint solid(Color c);
// From (x0, y0) to (x1, y1) through `n` stops (2..8, ascending `at`).
Paint linear(float x0, float y0, float x1, float y1, const Stop* stops, int n);
Paint linear(float x0, float y0, float x1, float y1, Color a, Color b);
// Across a w x h box at `degrees` as CSS has it: 0 runs bottom to top, 90 left to right, 180 top
// to bottom; the line is as long as the box's projection on it, so the corners take the end
// stops exactly.
Paint linear_at_angle(float w, float h, float degrees, const Stop* stops, int n);
Paint radial(float cx, float cy, float radius, const Stop* stops, int n, float aspect = 1);

}  // namespace ge::ui
