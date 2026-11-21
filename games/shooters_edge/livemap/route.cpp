// A bot's route on the live map (route.hpp).
//
// ge:summary The route picture: the plate copied, the track, kills, secrets and exits painted
// ge:keywords draw_route route track kills secrets exits plate
// ge:skill map-design bots labs
// ge:test tests/shooters_edge_live_map_test.cpp
#include "shooters_edge/livemap/route.hpp"

#include <cmath>
#include <cstdint>

#include "core/libm/libm.hpp"

namespace shooters_edge::livemap {

namespace {

struct Rgb {
  uint8_t r, g, b;
};

void put(ge::ui::Image& img, int x, int y, Rgb c) {
  if (x < 0 || y < 0 || x >= img.width || y >= img.height) return;
  uint8_t* p =
      &img.rgba[(static_cast<size_t>(y) * static_cast<size_t>(img.width) + static_cast<size_t>(x)) *
                4];
  p[0] = c.r;
  p[1] = c.g;
  p[2] = c.b;
  p[3] = 255;
}

void disc(ge::ui::Image& img, float cx, float cy, float r, Rgb c) {
  const int x0 = static_cast<int>(std::floor(cx - r)), x1 = static_cast<int>(std::ceil(cx + r));
  const int y0 = static_cast<int>(std::floor(cy - r)), y1 = static_cast<int>(std::ceil(cy + r));
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      const float dx = static_cast<float>(x) + 0.5f - cx, dy = static_cast<float>(y) + 0.5f - cy;
      if (dx * dx + dy * dy <= r * r) put(img, x, y, c);
    }
}

void line(ge::ui::Image& img, float ax, float ay, float bx, float by, float r, Rgb c) {
  const float dx = bx - ax, dy = by - ay;
  const int steps = 1 + static_cast<int>(std::sqrt(dx * dx + dy * dy));
  for (int i = 0; i <= steps; i++) {
    const float t = static_cast<float>(i) / static_cast<float>(steps);
    disc(img, ax + dx * t, ay + dy * t, r, c);
  }
}

// Blue at the run's start, through purple, to red at its end.
Rgb when(float t) {
  return {static_cast<uint8_t>(40 + 200 * t), 50, static_cast<uint8_t>(230 - 190 * t)};
}

}  // namespace

ge::ui::Image draw_route(const Plate& plate, const RouteMarks& m) {
  ge::ui::Image img = plate.image;
  const ge::overview::Frame& fr = plate.frame;
  const float w = std::fmax(2.f, static_cast<float>(img.width) / 500.f);  // the line's half width
  const auto px = [&](const std::array<float, 3>& p) { return fr.px(p[0]); };
  const auto py = [&](const std::array<float, 3>& p) { return fr.py(p[1]); };
  const size_t n = m.track.size();
  for (size_t i = 0; i + 1 < n; i++) {
    const std::array<float, 3>& a = m.track[i];
    const std::array<float, 3>& b = m.track[i + 1];
    const float dx = b[0] - a[0], dy = b[1] - a[1];
    if (dx * dx + dy * dy > kRouteJump * kRouteJump) continue;
    const Rgb c = when(static_cast<float>(i) / static_cast<float>(n > 1 ? n - 1 : 1));
    line(img, px(a), py(a), px(b), py(b), w, c);
  }
  for (const std::array<float, 3>& e : m.exits) {
    for (float r = 4 * w; r <= 5.5f * w; r += 0.5f)
      for (int k = 0; k < 96; k++) {
        const double a = k * (6.283185307179586 / 96);
        put(img, static_cast<int>(px(e) + r * static_cast<float>(ge::core::libm::cos(a))),
            static_cast<int>(py(e) + r * static_cast<float>(ge::core::libm::sin(a))),
            {255, 190, 30});
      }
  }
  for (const std::array<float, 3>& k : m.kills) {
    const float x = px(k), y = py(k), s = 2.5f * w;
    line(img, x - s, y - s, x + s, y + s, w * 0.6f, {20, 20, 20});
    line(img, x - s, y + s, x + s, y - s, w * 0.6f, {20, 20, 20});
  }
  for (const std::array<float, 3>& k : m.secrets) {
    const float x = px(k), y = py(k), s = 4 * w;
    for (float d = -s; d <= s; d += 0.5f) {
      const float half = s - std::fabs(d);
      line(img, x - half, y + d, x + half, y + d, 0.6f, {255, 215, 0});
    }
  }
  if (n > 0) {
    const float x = px(m.track[0]), y = py(m.track[0]), s = 3.5f * w;
    for (float yy = y - s; yy <= y + s; yy += 1)
      line(img, x - s, yy, x + s, yy, 0.6f, {40, 50, 230});
  }
  img.revision++;
  return img;
}

}  // namespace shooters_edge::livemap
