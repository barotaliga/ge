// engine/render/terrain.hpp's CPU half: the clipmap's levels nest (each finer square inside the
// next, its edges on the coarser grid, the eye inside every one) wherever the eye stands, and
// the min/max pyramid bounds every sample under every texel of every mip, its last mip the
// whole field's range.
//
// ge:summary Tests for the clipmap's level placement and the min/max pyramid
// ge:keywords test render terrain clipmap levels nesting min-max-pyramid reach clipmap_levels_to
// ge:skill render testing
#include <algorithm>
#include <cmath>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "render/terrain.hpp"

namespace {

using ge::render::ClipLevel;
using ge::render::kLevelTiles;
using ge::render::kTileQuads;

bool on_grid(double v, double s) {
  const double k = v / s;
  return std::fabs(k - std::round(k)) < 1e-9;
}

}  // namespace

TEST(levels_nest_on_each_others_grid) {
  const double eyes[][2] = {{0, 0}, {1234.56, -78.9}, {2999.99, 1500.01}, {-0.4, 0.6}};
  const double side_quads = kLevelTiles * kTileQuads;
  for (const auto& e : eyes) {
    ClipLevel l[8];
    ge::render::clipmap_levels(e[0], e[1], 1.0, 8, l);
    for (int i = 0; i < 8; ++i) {
      const double side = side_quads * l[i].spacing;
      CHECK(l[i].spacing == std::ldexp(1.0, i));
      CHECK(e[0] > l[i].origin_x && e[0] < l[i].origin_x + side);
      CHECK(e[1] > l[i].origin_z && e[1] < l[i].origin_z + side);
      if (i == 0) continue;
      const ClipLevel& f = l[i - 1];
      const double fside = side_quads * f.spacing;
      CHECK(f.origin_x >= l[i].origin_x && f.origin_x + fside <= l[i].origin_x + side);
      CHECK(f.origin_z >= l[i].origin_z && f.origin_z + fside <= l[i].origin_z + side);
      // The finer square's edges lie on the coarser level's vertices; parity is global.
      CHECK(on_grid(f.origin_x, l[i].spacing) && on_grid(f.origin_z, l[i].spacing));
      CHECK(on_grid(l[i].origin_x, 2 * l[i].spacing));
    }
  }
}

// Every level drawn reaches its reach in every direction, wherever the eye stands; a reach asks
// for the fewest levels that cover it, and none (0) or one past them all for every level.
TEST(levels_drawn_cover_the_reach) {
  const double eyes[][2] = {{0, 0}, {1234.56, -78.9}, {2999.99, 1500.01}, {-0.4, 0.6}};
  const double side_quads = kLevelTiles * kTileQuads;
  for (int n = 1; n <= 8; ++n) {
    const double reach = ge::render::clipmap_reach(1.0, n);
    for (const auto& e : eyes) {
      ClipLevel l[8];
      ge::render::clipmap_levels(e[0], e[1], 1.0, n, l);
      const ClipLevel& o = l[n - 1];
      const double side = side_quads * o.spacing;
      CHECK(e[0] - o.origin_x >= reach && o.origin_x + side - e[0] >= reach);
      CHECK(e[1] - o.origin_z >= reach && o.origin_z + side - e[1] >= reach);
    }
    CHECK(ge::render::clipmap_levels_to(reach, 1.0, 8) == n);
    if (n > 1) CHECK(ge::render::clipmap_levels_to(reach * 0.999, 1.0, 8) == n);
    if (n < 8) CHECK(ge::render::clipmap_levels_to(reach * 1.001, 1.0, 8) == n + 1);
  }
  CHECK(ge::render::clipmap_levels_to(0, 1.0, 8) == 8);
  CHECK(ge::render::clipmap_levels_to(1e9, 1.0, 8) == 8);
}

TEST(pyramid_bounds_every_sample) {
  ge::core::Heightfield f = ge::core::create_heightfield(0, 0, 2, 37, 23);
  for (int r = 0; r < f.rows; ++r) {
    for (int c = 0; c < f.cols; ++c) {
      f.data[static_cast<size_t>(r) * f.cols + c] =
          static_cast<float>(std::sin(c * 0.37) * 40 + std::cos(r * 0.21) * 25 + r);
    }
  }
  std::vector<std::pair<int, int>> sizes;
  const std::vector<float> p = ge::render::min_max_pyramid(f, sizes);
  CHECK(sizes.front().first == 36 && sizes.front().second == 22);
  CHECK(sizes.back().first == 1 && sizes.back().second == 1);
  // Each mip halves the last, rounding down (a texture's mip chain).
  for (size_t m = 1; m < sizes.size(); ++m) {
    CHECK(sizes[m].first == std::max(sizes[m - 1].first / 2, 1));
    CHECK(sizes[m].second == std::max(sizes[m - 1].second / 2, 1));
  }
  size_t at = 0;
  for (size_t m = 0; m < sizes.size(); ++m) {
    const auto [w, h] = sizes[m];
    const int span = 1 << m;
    for (int ty = 0; ty < h; ++ty) {
      for (int tx = 0; tx < w; ++tx) {
        const float lo = p[at + static_cast<size_t>((ty * w + tx) * 2)];
        const float hi = p[at + static_cast<size_t>((ty * w + tx) * 2 + 1)];
        // The cells under this texel (the last texel of a row takes the rest).
        const int c1 = tx == w - 1 ? f.cols - 1 : (tx + 1) * span;
        const int r1 = ty == h - 1 ? f.rows - 1 : (ty + 1) * span;
        for (int r = ty * span; r <= r1; ++r) {
          for (int c = tx * span; c <= c1; ++c) {
            const float v = f.data[static_cast<size_t>(r) * f.cols + c];
            CHECK(v >= lo && v <= hi);
          }
        }
      }
    }
    at += static_cast<size_t>(w * h * 2);
  }
  const auto [mn, mx] = std::minmax_element(f.data.begin(), f.data.end());
  CHECK(p[at - 2] == *mn && p[at - 1] == *mx);
}
