// The 2D layer's paints, evaluated: the stops' lookup, the linear and radial parameters, the
// angle's line through the box.
//
// ge:summary The 2D layer's paints defined: stop lookup, linear and radial t, CSS angles
// ge:keywords ui paint gradient linear radial stop angle evaluate
// ge:skill ui
// ge:test tests/ui_paint_test.cpp
#include "ui/paint.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::ui {

Color lerp(Color a, Color b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
          a.a + (b.a - a.a) * t};
}

bool colour_from_text(std::string_view text, Color& out) {
  static constexpr std::array<std::string_view, 8> kNames{"black", "red",  "green",   "yellow",
                                                          "blue",  "cyan", "magenta", "white"};
  static constexpr std::array<uint32_t, 8> kRgb{0x000000, 0xff0000, 0x00ff00, 0xffff00,
                                                0x0000ff, 0x00ffff, 0xff00ff, 0xffffff};
  for (size_t i = 0; i < kNames.size(); ++i)
    if (text == kNames[i] || (text.size() == 1 && text[0] == static_cast<char>('0' + i))) {
      out = hex(kRgb[i]);
      return true;
    }
  if (text.empty() || text[0] != '#') return false;
  const std::string_view digits = text.substr(1);
  if (digits.size() != 3 && digits.size() != 6 && digits.size() != 8) return false;
  uint32_t v = 0;
  for (const char c : digits) {
    const int d = c >= '0' && c <= '9'   ? c - '0'
                  : c >= 'a' && c <= 'f' ? c - 'a' + 10
                  : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                         : -1;
    if (d < 0) return false;
    v = v * 16 + static_cast<uint32_t>(d);
  }
  uint32_t a = 0xff;
  if (digits.size() == 3)  // each digit twice
    v = ((v >> 8) & 0xf) * 0x110000 + ((v >> 4) & 0xf) * 0x1100 + (v & 0xf) * 0x11;
  if (digits.size() == 8) {
    a = v & 0xff;
    v >>= 8;
  }
  out = hex(v, static_cast<float>(a) / 255.0f);
  return true;
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
