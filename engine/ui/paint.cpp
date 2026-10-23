// The 2D layer's paints, evaluated: the stops' lookup, the linear and radial parameters, the
// angle's line through the box.
//
// ge:summary The 2D layer's paints defined: stop lookup, linear and radial t, CSS angles
// ge:keywords ui paint gradient linear radial stop angle evaluate
// ge:skill ui
// ge:test tests/ui_paint_test.cpp
#include "ui/paint.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::ui {

Color lerp(Color a, Color b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
          a.a + (b.a - a.a) * t};
}

namespace {

Color lookup(const Paint& p, float t) {
  if (p.count <= 1 || t <= p.stops[0].at) return p.stops[0].color;
  for (int i = 1; i < p.count; i++) {
    const Stop& b = p.stops[i];
    if (t <= b.at) {
      const Stop& a = p.stops[i - 1];
      const float span = b.at - a.at;
      return span > 0 ? lerp(a.color, b.color, (t - a.at) / span) : b.color;
    }
  }
  return p.stops[p.count - 1].color;
}

Paint with_stops(PaintKind kind, const Stop* stops, int n) {
  Paint p;
  p.kind = kind;
  p.count = std::clamp(n, 1, kMaxStops);
  for (int i = 0; i < p.count; i++) p.stops[i] = stops[i];
  return p;
}

}  // namespace

Color Paint::at(float x, float y) const {
  switch (kind) {
    case PaintKind::solid:
      return stops[0].color;
    case PaintKind::linear: {
      const float dx = to_x - from_x, dy = to_y - from_y;
      const float len2 = dx * dx + dy * dy;
      const float t = len2 > 0 ? ((x - from_x) * dx + (y - from_y) * dy) / len2 : 0;
      return lookup(*this, t);
    }
    case PaintKind::radial: {
      const float dx = x - from_x, dy = (y - from_y) / aspect;
      return lookup(*this, std::sqrt(dx * dx + dy * dy) / radius);
    }
  }
  return stops[0].color;
}

bool Paint::opaque() const {
  for (int i = 0; i < count; i++)
    if (stops[i].color.a < 1) return false;
  return true;
}

Paint solid(Color c) {
  const Stop s{0, c};
  return with_stops(PaintKind::solid, &s, 1);
}

Paint linear(float x0, float y0, float x1, float y1, const Stop* stops, int n) {
  Paint p = with_stops(PaintKind::linear, stops, n);
  p.from_x = x0, p.from_y = y0, p.to_x = x1, p.to_y = y1;
  return p;
}

Paint linear(float x0, float y0, float x1, float y1, Color a, Color b) {
  const Stop s[2] = {{0, a}, {1, b}};
  return linear(x0, y0, x1, y1, s, 2);
}

Paint linear_at_angle(float w, float h, float degrees, const Stop* stops, int n) {
  const double rad = degrees * 3.14159265358979323846 / 180.0;
  // The direction the gradient runs, y down: 0 degrees points up the screen.
  const float dx = static_cast<float>(core::libm::sin(rad));
  const float dy = static_cast<float>(-core::libm::cos(rad));
  const float half = (std::fabs(dx) * w + std::fabs(dy) * h) / 2;
  const float cx = w / 2, cy = h / 2;
  return linear(cx - dx * half, cy - dy * half, cx + dx * half, cy + dy * half, stops, n);
}

Paint radial(float cx, float cy, float radius, const Stop* stops, int n, float aspect) {
  Paint p = with_stops(PaintKind::radial, stops, n);
  p.from_x = cx, p.from_y = cy;
  p.radius = radius > 0 ? radius : 1;
  p.aspect = aspect > 0 ? aspect : 1;
  return p;
}

}  // namespace ge::ui
