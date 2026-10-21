// The definitions behind drift.hpp.
//
// ge:summary The drift dealer and the packed field's thinning under a drift
// ge:keywords drift
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/drift.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace D = rules::drift;

namespace {

// What sets the drifts' stream apart from the attempt's own.
constexpr uint32_t kDriftSalt = 0x5eedd71f;

// How many stretches the dealer tries to stand before it settles for the share it has.
constexpr int kTries = 60;

}  // namespace

std::vector<Drift> deal_drifts(uint32_t sub, double length, std::span<const Kicker> kickers,
                               Band share, std::span<const Cliff> drops,
                               std::span<const ArcSpan> keep_off) {
  ge::core::Rng rng = ge::core::make_rng(sub ^ kDriftSalt);
  const double target = in_band(rng, share) * length;
  // The stretches no drift (with its ease) may touch: each kicker's ramp and landing and each
  // drop's shelf and landing, as arc ranges.
  std::vector<Drift> kept;
  for (const Kicker& k : kickers) {
    if (k.on_track && k.s) kept.push_back({*k.s - k.ramp, *k.s + k.landing});
  }
  for (const Cliff& d : drops) kept.push_back({*d.s - d.shelf, *d.s + d.face + d.landing});
  for (const ArcSpan& k : keep_off) kept.push_back({k.from, k.to});
  std::vector<Drift> drifts;
  double laid = 0;
  const double lo = D::clear + D::fade;
  for (int t = 0; t < kTries && laid < target; t++) {
    const double len = num::min(in_band(rng, D::length), target - laid + D::length.min);
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
    const double into = num::min(s - (d.from - D::fade), d.to + D::fade - s);
    if (into <= 0) continue;
    w = num::max(w, ge::core::smoothstep(0, D::fade, into));
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

namespace {

// The step a drift is cut back by, m, and the shortest kept, m.
constexpr double kTrimStep = 4;
constexpr double kTrimLeast = 20;

}  // namespace

void trim_drifts(std::span<const TrackPoint> points, std::vector<Drift>& drifts,
                 ge::core::Heightfield& packed, std::span<const float> before) {
  if (points.empty() || drifts.empty()) return;
  const auto at = [&](double s) -> const TrackPoint& {
    const TrackPoint* best = &points[0];
    for (const TrackPoint& p : points) {
      if (std::abs(p.s - s) < std::abs(best->s - s)) best = &p;
    }
    return *best;
  };
  const auto cell_of = [&](double x, double z) {
    return static_cast<int64_t>(num::round((z - packed.origin_z) / packed.cell)) * packed.cols +
           static_cast<int64_t>(num::round((x - packed.origin_x) / packed.cell));
  };
  const auto inside = [&](int64_t i) { return i >= 0 && i < int64_t(packed.data.size()); };
  const auto hubbed = [&](double s) {
    const TrackPoint& p = at(s);
    const double rx = ge::core::libm::cos(p.heading);
    const double rz = -ge::core::libm::sin(p.heading);
    for (const double u : {-0.5, -0.25, 0.0, 0.25, 0.5}) {
      const int64_t i = cell_of(p.x + rx * u * p.width, p.z + rz * u * p.width);
      if (inside(i) && packed.data[size_t(i)] > before[size_t(i)] + 0.02f) return true;
    }
    return false;
  };
  const auto groom_over = [&](double from, double to) {
    const double cell = packed.cell;
    for (const TrackPoint& p : points) {
      if (p.s < from || p.s > to) continue;
      const double r = p.width / 2 + cell;
      const int steps = static_cast<int>(std::floor((2 * r) / cell));
      for (int a = 0; a <= steps; a++) {
        const double dz = -r + a * cell;
        for (int b = 0; b <= steps; b++) {
          const double dx = -r + b * cell;
          if (dx * dx + dz * dz > r * r) continue;
          const int64_t i = cell_of(p.x + dx, p.z + dz);
          if (inside(i) && packed.data[size_t(i)] < 1.0f) packed.data[size_t(i)] = 1.0f;
        }
      }
    }
  };
  std::vector<Drift> kept;
  for (const Drift& d : drifts) {
    double to = d.to;
    while (to > d.from && hubbed(to)) to -= kTrimStep;
    double from = d.from;
    while (from < to && hubbed(from)) from += kTrimStep;
    if (to < d.to) groom_over(to, d.to + kTrimStep);
    if (from > d.from) groom_over(d.from - kTrimStep, from);
    if (to - from >= kTrimLeast) {
      kept.push_back({from, to});
    } else {
      groom_over(d.from - kTrimStep, d.to + kTrimStep);
    }
  }
  drifts = std::move(kept);
}

}  // namespace ge::mountain::mapgen
