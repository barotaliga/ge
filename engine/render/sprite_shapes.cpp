// Sprite shapes made at load: the flare's glow and the bubble's rim and highlight.
//
// ge:summary The flare's and the bubble's frames, texel by texel
// ge:keywords render sprite flare bubble procedural frame
// ge:skill render
// ge:test tests/render_sprite_shapes_test.cpp
#include "render/sprite_shapes.hpp"

#include <algorithm>
#include <cmath>

namespace ge::render {

namespace {

SpriteFrame blank(uint32_t px) {
  SpriteFrame f;
  f.width = f.height = std::max(px, 8u);
  f.rgba.assign(size_t{f.width} * f.height * 4, 0);
  f.origin[0] = f.origin[1] = static_cast<float>(f.width) * 0.5f;
  return f;
}

uint8_t byte(double v) { return static_cast<uint8_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255)); }

// The texel (x, y) of a `n`-wide frame as a point in [-1, 1]^2 from its middle (y up).
void at(uint32_t x, uint32_t y, uint32_t n, double* u, double* v) {
  *u = (x + 0.5) / n * 2 - 1;
  *v = 1 - (y + 0.5) / n * 2;
}

}  // namespace

SpriteFrame flare_frame(uint32_t px) {
  SpriteFrame f = blank(px);
  for (uint32_t y = 0; y < f.height; ++y) {
    for (uint32_t x = 0; x < f.width; ++x) {
      double u, v;
      at(x, y, f.width, &u, &v);
      const double r2 = u * u + v * v;
      // The core, the halo, and nothing at the frame's edge.
      const double edge = std::clamp(1 - r2, 0.0, 1.0);
      const double glow =
          (0.8 * std::exp(-r2 * 30) + 0.45 * std::exp(-r2 * 9) + 0.18 * std::exp(-r2 * 3)) * edge *
          edge;
      uint8_t* p = &f.rgba[(size_t{y} * f.width + x) * 4];
      p[0] = p[1] = p[2] = 255;
      p[3] = byte(glow);
    }
  }
  return f;
}

SpriteFrame bubble_frame(uint32_t px) {
  SpriteFrame f = blank(px);
  constexpr double kRadius = 0.86;  // the ball's edge, a share of the half frame (room for AA)
  const double texel = 2.0 / f.width;
  for (uint32_t y = 0; y < f.height; ++y) {
    for (uint32_t x = 0; x < f.width; ++x) {
      double u, v;
      at(x, y, f.width, &u, &v);
      const double r = std::sqrt(u * u + v * v) / kRadius;
      // Coverage, smoothed over a texel at the edge.
      const double cover = std::clamp((1 - r) * kRadius / texel + 0.5, 0.0, 1.0);
      if (cover <= 0) continue;
      const double rr = std::min(r, 1.0);
      // The Fresnel rim: brighter as the face turns away (its cosine sqrt(1 - r^2)).
      const double cosine = std::sqrt(std::max(1 - rr * rr, 0.0));
      const double rim = std::pow(1 - cosine, 3.0);
      // The highlight, up and to the left; the bent-away ring just inside the rim.
      const double hx = u / kRadius + 0.38, hy = v / kRadius - 0.42;
      const double spot = std::exp(-(hx * hx + hy * hy) * 60);
      const double ring = std::exp(-std::pow((rr - 0.82) / 0.07, 2.0));
      const double light = std::min(rim * 0.9 + spot, 1.0);
      const double alpha = std::clamp(0.05 + 0.6 * rim + spot + 0.18 * ring, 0.0, 1.0) * cover;
      // Pale and a little blue where it shines, darker in the ring.
      const double shade = light > 0 ? light / std::max(light + 0.18 * ring, 1e-6) : 0.35;
      uint8_t* p = &f.rgba[(size_t{y} * f.width + x) * 4];
      p[0] = byte(0.86 * shade + 0.1);
      p[1] = byte(0.93 * shade + 0.1);
      p[2] = byte(1.0 * shade + 0.1);
      p[3] = byte(alpha);
    }
  }
  return f;
}

}  // namespace ge::render
