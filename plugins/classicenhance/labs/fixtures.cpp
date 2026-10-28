// The lab's fixture palette and painted textures (fixtures.hpp).
//
// ge:summary Paints the classicenhance lab's fixture palette and textures
// ge:keywords fixture palette paint brick panel planks stripes rock water noise
// ge:skill classicenhance labs
#include "classicenhance/labs/fixtures.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/noise.hpp"

namespace ge::classicenhance::labs {

namespace {

// Smooth tiling noise on a 64-texel loop: value noise over a lattice that wraps every `cells`.
double tile_noise(int x, int y, int cells, double seed) {
  const double fx = x * cells / 64.0, fy = y * cells / 64.0;
  const int ix = int(std::floor(fx)), iy = int(std::floor(fy));
  const double tx = core::smooth(fx - ix), ty = core::smooth(fy - iy);
  auto h = [&](int a, int b) {
    return core::hash2(((a % cells) + cells) % cells, ((b % cells) + cells) % cells, seed);
  };
  const double top = h(ix, iy) + (h(ix + 1, iy) - h(ix, iy)) * tx;
  const double bottom = h(ix, iy + 1) + (h(ix + 1, iy + 1) - h(ix, iy + 1)) * tx;
  return top + (bottom - top) * ty;
}

double grit(int x, int y, double seed) { return core::hash2(x & 63, y & 63, seed); }

struct Painter {
  const Palette& pal;
  FixtureTexture t;
  void put(int x, int y, double r, double g, double b) {
    t.indexed[size_t(y) * 64 + size_t(x)] =
        nearest_index(pal, int(std::clamp(r, 0.0, 255.0)), int(std::clamp(g, 0.0, 255.0)),
                      int(std::clamp(b, 0.0, 255.0)));
  }
};

}  // namespace

Palette fixture_palette() {
  // Each ramp's brightest colour; the ramp runs from near black up to it.
  static const int kRamps[16][3] = {
      {255, 255, 255}, {190, 150, 110}, {150, 160, 200}, {120, 170, 90},
      {210, 90, 70},   {230, 190, 90},  {170, 120, 70},  {140, 140, 120},
      {200, 110, 150}, {110, 190, 200}, {220, 150, 60},  {160, 100, 90},
      {100, 110, 130}, {200, 200, 160}, {255, 230, 120}, {255, 90, 60}};
  Palette p;
  for (int r = 0; r < 16; r++) {
    for (int s = 0; s < 16; s++) {
      const double k = r >= 14 ? 0.55 + 0.45 * s / 15.0 : (s + 1) / 16.0;
      for (int c = 0; c < 3; c++) {
        p.rgb[size_t(r * 16 + s) * 3 + size_t(c)] =
            uint8_t(std::clamp(kRamps[r][c] * k, 0.0, 255.0));
      }
    }
  }
  return p;
}

uint8_t nearest_index(const Palette& palette, int r, int g, int b) {
  int best = 0;
  int best_d = 1 << 30;
  for (int i = 0; i < kFullbrightFirst; i++) {
    const int dr = palette.rgb[size_t(i) * 3] - r, dg = palette.rgb[size_t(i) * 3 + 1] - g,
              db = palette.rgb[size_t(i) * 3 + 2] - b;
    const int d = 3 * dr * dr + 4 * dg * dg + 2 * db * db;
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  return uint8_t(best);
}

std::vector<FixtureTexture> fixture_textures(const Palette& pal) {
  std::vector<FixtureTexture> out;
  auto begin = [&](const char* name) {
    return Painter{pal, {name, 64, 64, std::vector<uint8_t>(size_t{64} * 64)}};
  };

  {  // Brick: courses of 16 × 8, staggered, each brick its own shade, dark mortar between.
    Painter p = begin("brick_fixture");
    for (int y = 0; y < 64; y++) {
      for (int x = 0; x < 64; x++) {
        const int course = y / 8;
        const int bx = (x + (course % 2) * 8) & 63;
        const bool mortar = y % 8 == 7 || bx % 16 == 15;
        const int block = bx / 16;
        const double shade = 0.75 + 0.25 * core::hash2(block, course, 3);
        const double n = 0.8 + 0.4 * grit(x, y, 4) * tile_noise(x, y, 8, 5);
        if (mortar) {
          p.put(x, y, 70 * n, 66 * n, 60 * n);
        } else {
          p.put(x, y, 170 * shade * n, 80 * shade * n, 60 * shade * n);
        }
      }
    }
    out.push_back(p.t);
  }
  {  // A metal panel: bevelled edges, a rivet in each corner, a glowing strip in the middle.
    Painter p = begin("metal_panel_fixture");
    for (int y = 0; y < 64; y++) {
      for (int x = 0; x < 64; x++) {
        const double n = 0.85 + 0.25 * grit(x, y, 7) * tile_noise(x, y, 4, 8);
        double r = 105 * n, g = 112 * n, b = 130 * n;
        const int ex = std::min(x, 63 - x), ey = std::min(y, 63 - y);
        if (std::min(ex, ey) < 3) {
          const double lit = (x < 3 || y < 3) ? 1.35 : 0.55;
          r *= lit;
          g *= lit;
          b *= lit;
        }
        for (const int cx : {8, 55}) {
          for (const int cy : {8, 55}) {
            const int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy <= 6) {
              const double lit = dx + dy < 0 ? 1.5 : 0.6;
              r = 140 * lit;
              g = 140 * lit;
              b = 150 * lit;
            }
          }
        }
        p.put(x, y, r, g, b);
        if (x >= 20 && x < 44 && y >= 28 && y < 36) {
          // The strip: the fullbright yellow ramp, brightest in the middle.
          const int s = 15 - std::min(15, std::abs(y - 31) * 3 + int(4 * grit(x, y, 9)));
          p.t.indexed[size_t(y) * 64 + size_t(x)] = uint8_t(224 + s);
        }
      }
    }
    out.push_back(p.t);
  }
  {  // Wood planks: four boards 16 wide, a grain that wanders down each, dark gaps between.
    Painter p = begin("wood_planks_fixture");
    for (int y = 0; y < 64; y++) {
      for (int x = 0; x < 64; x++) {
        const int board = x / 16;
        const double wob = 2.0 * core::libm::sin((y + board * 17) * 6.283185307179586 / 32.0);
        const double grain = 0.5 + 0.5 * core::libm::sin((x + wob) * 2.4 + board);
        const double n = 0.85 + 0.2 * grit(x, y, 11);
        const double k = (0.7 + 0.3 * grain) * n * (0.85 + 0.15 * core::hash2(board, 0, 12));
        if (x % 16 == 0) {
          p.put(x, y, 50, 34, 22);
        } else {
          p.put(x, y, 165 * k, 112 * k, 66 * k);
        }
      }
    }
    out.push_back(p.t);
  }
  {  // Hazard stripes: ochre and black at 45°, grimed.
    Painter p = begin("hazard_fixture");
    for (int y = 0; y < 64; y++) {
      for (int x = 0; x < 64; x++) {
        const bool ochre = ((x + y) / 8) % 2 == 0;
        const double n = 0.75 + 0.35 * grit(x, y, 13) * tile_noise(x, y, 8, 14);
        if (ochre) {
          p.put(x, y, 220 * n, 175 * n, 70 * n);
        } else {
          p.put(x, y, 40 * n, 38 * n, 34 * n);
        }
      }
    }
    out.push_back(p.t);
  }
  {  // Rock: three octaves of tiling noise, grey-brown, with dark cracks.
    Painter p = begin("rock_fixture");
    for (int y = 0; y < 64; y++) {
      for (int x = 0; x < 64; x++) {
        const double v = 0.5 * tile_noise(x, y, 4, 15) + 0.3 * tile_noise(x, y, 8, 16) +
                         0.2 * tile_noise(x, y, 16, 17);
        const double crack = std::fabs(tile_noise(x, y, 4, 18) - 0.5) < 0.03 ? 0.45 : 1.0;
        const double k = (0.45 + 0.75 * v) * crack;
        p.put(x, y, 150 * k, 138 * k, 120 * k);
      }
    }
    out.push_back(p.t);
  }
  {  // Water: two blues in a slow swirl.
    Painter p = begin("*water_fixture");
    for (int y = 0; y < 64; y++) {
      for (int x = 0; x < 64; x++) {
        const double v = 0.6 * tile_noise(x, y, 4, 19) + 0.4 * tile_noise(x, y, 8, 20);
        p.put(x, y, 40 + 50 * v, 80 + 70 * v, 120 + 90 * v);
      }
    }
    out.push_back(p.t);
  }
  return out;
}

}  // namespace ge::classicenhance::labs
