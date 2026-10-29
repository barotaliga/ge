// The look's numbers off a picture's bytes, in one pass for the colour and one for the steps.
//
// ge:summary The look's numbers measured off a picture's pixels
// ge:keywords look stats grain edges contrast colours measure
// ge:skill textures
// ge:test tests/texgen_look_test.cpp
#include "texgen/look.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ge::texgen {

Look look_of(const lab::Picture& p) {
  Look l;
  const int w = p.width, h = p.height;
  if (w <= 0 || h <= 0) return l;
  const size_t n = static_cast<size_t>(w) * h;
  std::vector<double> luma(n);
  std::vector<uint8_t> seen(32768, 0);
  double sum = 0, sum2 = 0, chroma = 0, warm = 0;
  int distinct = 0;
  for (size_t i = 0; i < n; ++i) {
    const double r = p.rgba[i * 4] / 255.0, g = p.rgba[i * 4 + 1] / 255.0,
                 b = p.rgba[i * 4 + 2] / 255.0;
    const double y = 0.299 * r + 0.587 * g + 0.114 * b;
    luma[i] = y;
    sum += y;
    sum2 += y * y;
    chroma += std::max({r, g, b}) - std::min({r, g, b});
    warm += r - b;
    const size_t key = static_cast<size_t>((p.rgba[i * 4] >> 3) << 10 |
                                           (p.rgba[i * 4 + 1] >> 3) << 5 | p.rgba[i * 4 + 2] >> 3);
    if (seen[key] == 0) {
      seen[key] = 1;
      ++distinct;
    }
  }
  const double dn = static_cast<double>(n);
  l.brightness = sum / dn;
  l.contrast = std::sqrt(std::max(0.0, sum2 / dn - l.brightness * l.brightness));
  l.saturation = chroma / dn;
  l.warmth = warm / dn;
  l.colours = distinct / dn;
  double grain = 0;
  size_t steps = 0;
  auto at = [&](int x, int y) {
    return luma[static_cast<size_t>((y + h) % h) * w + static_cast<size_t>((x + w) % w)];
  };
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      double ring = 0;
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
          if (dx != 0 || dy != 0) ring += at(x + dx, y + dy);
      const double c = at(x, y);
      grain += std::fabs(c - ring / 8);
      if (std::fabs(c - at(x + 1, y)) > 0.08 || std::fabs(c - at(x, y + 1)) > 0.08) ++steps;
    }
  }
  l.grain = grain / dn / std::max(l.brightness, 0.02);
  l.edges = static_cast<double>(steps) / dn;
  return l;
}

std::array<double, 7> look_values(const Look& l) {
  return {l.brightness, l.contrast, l.saturation, l.warmth, l.grain, l.edges, l.colours};
}

double look_distance(const Look& a, const Look& b) {
  // Each number's usual spread across a set of wall textures: a difference of one spread is a
  // clearly different look.
  constexpr std::array<double, 7> kSpread = {0.08, 0.04, 0.06, 0.05, 0.12, 0.15, 0.08};
  const std::array<double, 7> va = look_values(a), vb = look_values(b);
  double sum = 0;
  for (size_t k = 0; k < va.size(); ++k) {
    const double d = (va[k] - vb[k]) / kSpread[k];
    sum += d * d;
  }
  return std::sqrt(sum / static_cast<double>(va.size()));
}

}  // namespace ge::texgen
