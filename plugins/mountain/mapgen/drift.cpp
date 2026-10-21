// Port of game5 `engine/mapgen/drift.ts`.
//
// ge:summary The drift dealer and the packed field's thinning under a drift
// ge:keywords drift dealDrifts stampDrifts
// ge:oracle game5/engine/mapgen/drift.ts
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/drift.hpp"

#include <algorithm>

#include "core/js.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"

namespace ge::mountain::mapgen {

namespace js = ge::core::js;
namespace D = rules::drift;

namespace {

// What sets the drifts' stream apart from the attempt's own.
constexpr uint32_t kDriftSalt = 0x5eedd71f;

// How many stretches the dealer tries to stand before it settles for the share it has.
constexpr int kTries = 60;

}  // namespace

std::vector<Drift> deal_drifts(uint32_t sub, double length, std::span<const Kicker> kickers,
                               Band share, std::span<const Cliff> drops) {
  ge::core::Rng rng = ge::core::make_rng(sub ^ kDriftSalt);
  const double target = in_band(rng, share) * length;
  // The stretches no drift (with its ease) may touch: each kicker's ramp and landing and each
  // drop's shelf and landing, as arc ranges.
  std::vector<Drift> kept;
  for (const Kicker& k : kickers) {
    if (k.on_track && k.s) kept.push_back({*k.s - k.ramp, *k.s + k.landing});
  }
  for (const Cliff& d : drops) kept.push_back({*d.s - d.shelf, *d.s + d.face + d.landing});
  std::vector<Drift> drifts;
  double laid = 0;
  const double lo = D::clear + D::fade;
  for (int t = 0; t < kTries && laid < target; t++) {
    const double len = js::min(in_band(rng, D::length), target - laid + D::length.min);
    const double hi = length - D::clear - D::fade - len;
    if (hi <= lo) break;
    const double from = ge::core::range(rng, lo, hi);
    const double to = from + len;
    const bool clash = std::any_of(drifts.begin(), drifts.end(),
                                   [&](const Drift& d) {
                                     return from < d.to + D::gap + 2 * D::fade &&
                                            to > d.from - D::gap - 2 * D::fade;
                                   }) ||
                       std::any_of(kept.begin(), kept.end(), [&](const Drift& k) {
                         return from < k.to + D::fade && to > k.from - D::fade;
                       });
    if (clash) continue;
    drifts.push_back({from, to});
    laid += len;
  }
  std::stable_sort(drifts.begin(), drifts.end(),
                   [](const Drift& a, const Drift& b) { return a.from < b.from; });
  return drifts;
}

double drift_at(std::span<const Drift> drifts, double s) {
  double w = 0;
  for (const Drift& d : drifts) {
    const double into = js::min(s - (d.from - D::fade), d.to + D::fade - s);
    if (into <= 0) continue;
    w = js::max(w, ge::core::smoothstep(0, D::fade, into));
  }
  return w;
}

void stamp_drifts(ge::core::Heightfield& packed, const std::vector<int32_t>& near,
                  const std::vector<float>& along, std::span<const Drift> drifts, double step) {
  if (drifts.empty()) return;
  float* p = packed.data.data();
  for (size_t o = 0; o < packed.data.size(); o++) {
    const int32_t i = near[o];
    if (i < 0 || p[o] <= 0) continue;
    const double w = drift_at(drifts, (i + double(along[o])) * step);
    if (w > 0) p[o] = static_cast<float>(p[o] * (1 - (1 - D::packed) * w));
  }
}

}  // namespace ge::mountain::mapgen
