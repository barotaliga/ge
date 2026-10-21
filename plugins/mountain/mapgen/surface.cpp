// The definitions behind surface.hpp.
//
// ge:summary The crust's blur, patches and fold, on float32 grids
// ge:keywords surface crust blur
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/surface.hpp"

#include "core/math.hpp"
#include "core/noise.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
using ge::core::smoothstep;

namespace {

constexpr uint32_t kCrustSalt = 0x0c2057a1;

// …and the metres the crust eases in over past `kClear`.
constexpr double kEase = 10;

// One box pass along a line of n values `stride` apart, `radius` either way, as a running sum
// in double, stored back as float.
void box_line(const std::vector<float>& src, std::vector<float>& dst, size_t start, size_t stride,
              int n, int radius) {
  double sum = 0;
  double count = 0;
  for (int k = 0; k < std::min(n, radius); k++) {
    sum += src[start + size_t(k) * stride];
    count++;
  }
  for (int i = 0; i < n; i++) {
    const int add = i + radius;
    if (add < n) {
      sum += src[start + size_t(add) * stride];
      count++;
    }
    const int drop = i - radius - 1;
    if (drop >= 0) {
      sum -= src[start + size_t(drop) * stride];
      count--;
    }
    dst[start + size_t(i) * stride] = static_cast<float>(sum / count);
  }
}

// A box blur of a grid, `radius` cells either way, run twice (≈ a gaussian).
std::vector<float> blur(const std::vector<float>& src, int cols, int rows, int radius) {
  std::vector<float> a(src);
  std::vector<float> b(src.size());
  for (int pass = 0; pass < 2; pass++) {
    for (int r = 0; r < rows; r++) box_line(a, b, size_t(r) * size_t(cols), 1, cols, radius);
    for (int c = 0; c < cols; c++) box_line(b, a, size_t(c), size_t(cols), rows, radius);
  }
  return a;
}

}  // namespace

std::optional<ge::core::Heightfield> lay_crust(uint32_t sub, const Region& region,
                                               const ge::core::Heightfield& ground) {
  if (!region.crust) return std::nullopt;
  const Crust& spec = *region.crust;
  ge::core::Rng rng = ge::core::make_rng(sub ^ kCrustSalt);
  const double seed = ge::core::int_in(rng, 1, 1 << 30);
  const int cols = ground.cols;
  const int rows = ground.rows;
  const double cell = ground.cell;
  // What stands proud of the country within fifty metres of it is what the wind scours.
  const std::vector<float> around =
      blur(ground.data, cols, rows, static_cast<int>(num::round(12 / cell)) * 2);
  ge::core::Heightfield crust =
      ge::core::create_heightfield(ground.origin_x, ground.origin_z, cell, cols, rows);
  const double edge = 1 - spec.cover;
  for (int r = 0; r < rows; r++) {
    const double z = ground.origin_z + r * cell;
    for (int c = 0; c < cols; c++) {
      const double x = ground.origin_x + c * cell;
      const size_t o = size_t(r) * size_t(cols) + size_t(c);
      const double n = ge::core::value_noise(x, z, spec.scale, seed) * 0.7 +
                       ge::core::value_noise(x, z, spec.scale * 0.3, seed + 11) * 0.3;
      const double patch = smoothstep(edge - 0.08, edge + 0.08, n);
      const double proud =
          smoothstep(0.4, 3.5, double(ground.data[o]) - double(around[o])) * spec.exposed;
      crust.data[o] = static_cast<float>(num::max(patch, proud));
    }
  }
  return crust;
}

void fold_surface(ge::core::Heightfield& packed, const std::vector<float>& dist,
                  const Region& region, ge::core::Heightfield& crust) {
  const double support = region.crust ? region.crust->packed : 0;
  float* p = packed.data.data();
  float* k = crust.data.data();
  for (size_t o = 0; o < packed.data.size(); o++) {
    const double m = smoothstep(kClear, kClear + kEase, dist[o]);
    k[o] = static_cast<float>(k[o] * m);
    p[o] = static_cast<float>(num::max(double(p[o]), k[o] * support));
  }
}

}  // namespace ge::mountain::mapgen
