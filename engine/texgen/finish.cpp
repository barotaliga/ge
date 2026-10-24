// The finish painters: what turns a built surface into a painted one. `shade` bakes the light
// into the colour off the height (the lit upper-left edge and the shadowed lower-right one a
// low-resolution texture draws its relief with, and the dark in its joints); `speckle` is the
// grain a texel at a time (salt and pepper in the brightness, and specks of a second colour that
// gather in the pits: patina, flecks of ore); `palette` snaps every colour onto a few ramps of
// shades with an optional ordered dither, the banded, crunchy colour of a texture painted in a
// small palette. They run last, over everything the layers before them laid down.
//
// ge:summary Finish painters: light baked from the height, per-texel grain, ramped palettes
// ge:keywords finish shade baked-light bevel emboss speckle grain salt-and-pepper patina
// ge:keywords palette quantise posterize ramps dither bayer low-colour
// ge:skill textures
// ge:test tests/texgen_bake_test.cpp
#include <algorithm>
#include <array>
#include <cmath>

#include "texgen/layers.hpp"
#include "texgen/noise.hpp"

namespace ge::texgen {
namespace {

// The light's direction across the tile, toward the light: x right, y down the tile.
void light_dir(const std::string& word, double& lx, double& ly) {
  lx = -1;
  ly = -1;
  if (word == "top") lx = 0;
  if (word == "top-right") lx = 1;
  if (word == "left") ly = 0;
  if (word == "right") {
    lx = 1;
    ly = 0;
  }
  const double n = std::sqrt(lx * lx + ly * ly);
  lx /= n;
  ly /= n;
}

// A 4x4 ordered-dither threshold in (0, 1).
double bayer(int x, int y) {
  constexpr std::array<int, 16> kM = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
  return (kM[static_cast<size_t>((y & 3) * 4 + (x & 3))] + 0.5) / 16.0;
}

}  // namespace

void paint_shade(Surface& s, const Args& a) {
  const int n = s.size, soft = a.i("soft");
  const double strength = a.n("strength"), occlusion = a.n("occlusion");
  const double hi = a.n("highlight");
  double lx = 0, ly = 0;
  light_dir(a.w("light"), lx, ly);
  const std::vector<float> h = soft > 0 ? blur(s.plane[kHeight], n, soft) : s.plane[kHeight];
  const std::vector<float> pits = occlusion > 0 ? relief(s, std::max(2, n / 64)) : h;
  for_texels(n, [&](int x, int y, double, double) {
    // The slope in height per tile, from the wrapped neighbours.
    const double gx = (h[s.wrapped(x + 1, y)] - h[s.wrapped(x - 1, y)]) * 0.5 * n;
    const double gy = (h[s.wrapped(x, y + 1)] - h[s.wrapped(x, y - 1)]) * 0.5 * n;
    // A face rising away from the light leans toward it; 1 at a 45 degree bevel at bump 1.
    const double lit = std::clamp((gx * -lx + gy * -ly) * 0.05 * strength, -1.0, 1.0);
    const size_t i = s.at(x, y);
    Rgb c = colour_at(s, i);
    if (lit < 0) {
      c = scaled(c, 1 + lit);
    } else {
      c = mix(scaled(c, 1 + lit), {1, 1, 1}, lit * hi);
    }
    if (occlusion > 0) c = scaled(c, 1 - std::clamp(pits[i] * 4 * occlusion, 0.0, 0.85));
    set_colour(s, i, c);
  });
}

void paint_speckle(Surface& s, const Args& a) {
  // A grain cell is a power of two texels, so the cells tile.
  int cell = 1;
  while (cell * 2 <= a.i("size") && cell * 2 <= s.size) cell *= 2;
  const int period = s.size / cell;
  const double contrast = a.n("contrast"), amount = a.n("amount"), dh = a.n("height");
  const double cavity = a.n("cavity");
  const Rgb speck = a.c("color");
  const std::vector<float> pits = amount > 0 && cavity > 0 ? relief(s, 3) : std::vector<float>{};
  for_texels(s.size, [&](int x, int y, double, double) {
    const auto cx = static_cast<uint32_t>((x / cell) % period);
    const auto cy = static_cast<uint32_t>((y / cell) % period);
    const double g = hash01(cx, cy, a.seed) - 0.5;
    const size_t i = s.at(x, y);
    Rgb c = scaled(colour_at(s, i), 1 + 2 * contrast * g);
    if (amount > 0) {
      // A speck where the second draw clears the bar; the pits lower the bar.
      const double bias = pits.empty() ? 0 : std::clamp(pits[i] * 6 * cavity, -0.5, 0.5);
      const double t = hash01(cx, cy, a.seed + 1);
      if (t < amount * (1 + bias)) c = mix(c, speck, 0.85);
    }
    set_colour(s, i, c);
    if (dh != 0) s.plane[kHeight][i] = static_cast<float>(clamp01(s.plane[kHeight][i] + dh * g));
  });
}

void paint_palette(Surface& s, const Args& a) {
  const int steps = a.i("steps");
  const double dither = a.n("dither"), amount = a.n("amount"), top = a.n("top");
  // Each base colour a ramp: from near black up to the colour at three quarters, then on toward
  // white by `top`.
  std::vector<Rgb> pal;
  for (const char* key : {"color", "color2", "color3", "color4"}) {
    if (key != std::string_view("color") && !a.given(key)) continue;
    const Rgb c = a.c(key);
    for (int k = 0; k < steps; ++k) {
      const double t = (k + 0.5) / steps;
      pal.push_back(t < 0.75 ? scaled(c, 0.06 + 0.94 * t / 0.75)
                             : mix(c, {1, 1, 1}, top * (t - 0.75) / 0.25));
    }
  }
  const double spread = dither / steps;
  for_texels(s.size, [&](int x, int y, double, double) {
    const size_t i = s.at(x, y);
    const Rgb c0 = colour_at(s, i);
    const double d = spread * (bayer(x, y) - 0.5);
    const Rgb c{c0.r + d, c0.g + d, c0.b + d};
    double best = 1e9;
    Rgb pick = c0;
    for (const Rgb& p : pal) {
      // Weighted toward green, as the eye is.
      const double dr = p.r - c.r, dg = p.g - c.g, db = p.b - c.b;
      const double e = 0.3 * dr * dr + 0.59 * dg * dg + 0.11 * db * db;
      if (e < best) {
        best = e;
        pick = p;
      }
    }
    set_colour(s, i, mix(c0, pick, amount));
  });
}

}  // namespace ge::texgen
