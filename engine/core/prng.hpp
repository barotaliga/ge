// THE SEEDED STREAM. Port of oss-game-framework `src/core/prng.ts`
// (mulberry32), bit for bit: every random draw in a simulation comes from
// one of these, never from the platform, so a seed reproduces a world and a
// run. Parity: tests/core_prng_test.cpp against tests/golden/core_prng.txt.
//
// ge:summary The seeded stream (mulberry32): every random draw in a simulation
// ge:keywords prng rng random seed mulberry32 next range chance pick pickIndex createRng
// ge:keywords determinism
// ge:oracle oss-game-framework/src/core/prng.ts
// ge:skill determinism
// ge:test tests/core_prng_test.cpp
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ge::core {

// The whole state is one 32-bit word; copy it to fork a stream.
struct Rng {
  uint32_t a;
};

inline Rng make_rng(uint32_t seed) { return Rng{seed}; }

// Uniform double in [0, 1), the exact value JS `next()` returns.
// JS works in int32 with `|0`, `>>>` and `Math.imul`; all three are the
// low 32 bits of the same uint32 arithmetic, so uint32 here is the identity.
inline double next(Rng& r) {
  r.a += 0x6d2b79f5u;
  uint32_t t = (r.a ^ (r.a >> 15)) * (1u | r.a);
  t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
  return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
}

// Uniform double in [lo, hi).
inline double range(Rng& r, double lo, double hi) { return lo + (hi - lo) * next(r); }

// Uniform integer in [lo, hi] inclusive. Computed in double exactly as JS
// does (`lo + floor(next * (hi - lo + 1))`), so the rounding matches.
inline int32_t int_in(Rng& r, int32_t lo, int32_t hi) {
  const double span = static_cast<double>(hi) - static_cast<double>(lo) + 1.0;
  return static_cast<int32_t>(static_cast<double>(lo) + std::floor(next(r) * span));
}

// True with probability p.
inline bool chance(Rng& r, double p) { return next(r) < p; }

// An index into a non-empty array of `count` items (JS `pick` returns the
// item; C++ returns its index so the caller keeps its own storage).
inline size_t pick_index(Rng& r, size_t count) {
  return static_cast<size_t>(std::floor(next(r) * static_cast<double>(count)));
}

}  // namespace ge::core
