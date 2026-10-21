// R18 — THE WINDROW'S SHAPE: the ridge the groomer's tiller leaves along each edge of the
// piste, as two pure functions of where across it and how far down it. Read by the corridor's
// stamp (track.cpp) and, from M7, the ground
// the app draws. Held by tests/mountain_generate_test.cpp (`ground`).
//
// ge:summary The windrow's cross-section either side of the piste (R18)
// ge:keywords berm windrow
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "mountain/mapgen/rules.hpp"

namespace ge::mountain::mapgen {

// R18 — the windrow's height over the graded line `u` m out from the inner toe
// (0 … berm.width), with its crest `crest` m tall.
inline double berm_profile(double crest, double u) {
  const double w = rules::berm::width;
  if (u <= 0 || u >= w) return 0;
  const double k = ge::core::libm::sin((ge::core::kPi * u) / w);
  return crest * k * k;
}

// R18 — the crest's height `s` m down the piste: two slow sines inside `berm.height`.
inline double berm_crest(double s) {
  namespace B = rules::berm;
  namespace libm = ge::core::libm;
  constexpr double pi = ge::core::kPi;
  const double v = 0.5 + 0.3 * libm::sin((2 * pi * s) / B::wander_a) +
                   0.2 * libm::sin((2 * pi * s) / B::wander_b + 1.3);
  return B::height.min + (B::height.max - B::height.min) * v;
}

}  // namespace ge::mountain::mapgen
