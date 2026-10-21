// DETERMINISTIC 2D VALUE NOISE. Port of oss-game-framework `src/core/noise.ts`, bit for bit.
// Everything spatial in game5 that must agree across modules (the terrain's shape and colour,
// what grows where, the ground a physics step rides) and every per-tick coin a step flips
// without touching the Rng (`hash2(tick, k, seed)`) comes from here.
// `tiledValueNoise` is not ported: game5 calls it nowhere.
// Parity: tests/core_noise_test.cpp against tests/golden/core_noise.txt.
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

#include "core/js.hpp"

namespace ge::core {

// Deterministic lattice hash in [0, 1). JS sums the three products in DOUBLE before `| 0`
// (`seed * 2246822519` passes 2^53 for a large seed and rounds), so the sum stays in double
// here too, left to right; only then does it become 32 bits. Arguments are doubles because
// game5 passes floored doubles, `| 0` words and plain uint32 seeds alike.
inline double hash2(double ix, double iz, double seed) {
  uint32_t h = js::to_uint32(ix * 374761393.0 + iz * 668265263.0 + seed * 2246822519.0);
  // `Math.imul(h ^ (h >>> 13), 1274126177)` and `(h ^ (h >>> 16)) >>> 0`: the low 32 bits of
  // uint32 arithmetic are the same word as JS's int32 ones.
  h = (h ^ (h >> 13)) * 1274126177u;
  return static_cast<double>(h ^ (h >> 16)) / 4294967296.0;
}

// Hermite fade for interpolation, 0-1 -> 0-1.
inline double smooth(double t) { return t * t * (3.0 - 2.0 * t); }

// Bilinear value noise over a lattice of hash2 values, period `scale` m.
inline double value_noise(double x, double z, double scale, double seed) {
  const double gx = x / scale;
  const double gz = z / scale;
  const double ix = std::floor(gx);
  const double iz = std::floor(gz);
  const double fx = smooth(gx - ix);
  const double fz = smooth(gz - iz);
  const double a = hash2(ix, iz, seed);
  const double b = hash2(ix + 1.0, iz, seed);
  const double c = hash2(ix, iz + 1.0, seed);
  const double d = hash2(ix + 1.0, iz + 1.0, seed);
  return (a + (b - a) * fx) * (1.0 - fz) + (c + (d - c) * fx) * fz;
}

// One value_noise field (a scale and a seed) read point after point. Neighbouring reads
// nearly always land in the same lattice square, so the field caches that square's four
// corner hashes and recomputes them only when a read leaves it. Same arithmetic, same bits:
// sample_noise(f, x, z) == value_noise(x, z, f.scale, f.seed).
struct NoiseField {
  double scale;
  double seed;
  double ix;  // the cached square; NaN until the first read, so the first read always misses
  double iz;
  double a, b, c, d;
};

inline NoiseField noise_field(double scale, double seed) {
  constexpr double nan = std::numeric_limits<double>::quiet_NaN();
  return NoiseField{scale, seed, nan, nan, 0.0, 0.0, 0.0, 0.0};
}

inline double sample_noise(NoiseField& f, double x, double z) {
  const double gx = x / f.scale;
  const double gz = z / f.scale;
  const double ix = std::floor(gx);
  const double iz = std::floor(gz);
  if (ix != f.ix || iz != f.iz) {
    f.ix = ix;
    f.iz = iz;
    f.a = hash2(ix, iz, f.seed);
    f.b = hash2(ix + 1.0, iz, f.seed);
    f.c = hash2(ix, iz + 1.0, f.seed);
    f.d = hash2(ix + 1.0, iz + 1.0, f.seed);
  }
  const double fx = smooth(gx - ix);
  const double fz = smooth(gz - iz);
  return (f.a + (f.b - f.a) * fx) * (1.0 - fz) + (f.c + (f.d - f.c) * fx) * fz;
}

}  // namespace ge::core
