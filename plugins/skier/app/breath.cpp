// A skier's breath in the cold: the puffs at his mouth.
//
// ge:summary Definitions for a skier's breath: breath_pen, mouth_of, breath_puffs
// ge:keywords skier breath puff exhale breath_puffs mouth_of
// ge:skill skier render
// ge:test tests/skier_breath_test.cpp
#include "skier/app/breath.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::skier {

namespace lm = core::libm;

BreathPen breath_pen(uint32_t k) {
  BreathPen pen;
  pen.rng = core::make_rng(0xb7ea0000u + k * 7919u);
  pen.phase = core::range(pen.rng, 0, 1);
  return pen;
}

void mouth_of(const SkierState& s, double out[3]) {
  const double h = kMouthStanding - (kMouthStanding - kMouthTucked) * std::clamp(s.tuck, 0.0, 1.0);
  const double fx = lm::sin(s.heading), fz = lm::cos(s.heading);  // forward
  const double rx = fz, rz = -fx;                                 // right
  const double up = h * lm::cos(s.lean), out_side = h * lm::sin(s.lean);
  out[0] = s.x + 0.2 * fx + out_side * rx;
  out[1] = s.y + up;
  out[2] = s.z + 0.2 * fz + out_side * rz;
}

int breath_puffs(const SkierState& s, double show, double dt, BreathPen& pen,
                 std::vector<SprayGrain>& out) {
  if (dt <= 0) return 0;
  const double period = kBreathPeriod - (kBreathPeriod - kBreathFastest) *
                                            std::clamp(s.speed / kBreathHard, 0.0, 1.0);
  const double start = pen.phase;
  pen.phase += dt / period;
  // The share of [start, start + dt / period) spent breathing out, breath by breath.
  double out_s = 0;
  for (double b = std::floor(start); b < pen.phase; b += 1) {
    const double lo = std::max(start, b), hi = std::min(pen.phase, b + kExhaleShare);
    if (hi > lo) out_s += (hi - lo) * period;
  }
  pen.phase -= std::floor(pen.phase);
  if (show <= 0.02 || out_s <= 0) {
    pen.owed = 0;
    return 0;
  }
  pen.owed += kPuffRate * out_s;
  const int n = static_cast<int>(pen.owed);
  pen.owed -= n;
  double m[3];
  mouth_of(s, m);
  const double fx = lm::sin(s.heading), fz = lm::cos(s.heading);
  const double thick = std::min(show, 1.5);
  for (int i = 0; i < n; ++i) {
    SprayGrain g{};
    // Spread back along the way he came this frame, so a fast breath is a ribbon.
    const double back = core::range(pen.rng, 0, 1) * dt;
    g.x = m[0] - s.vx * back + core::range(pen.rng, -0.03, 0.03);
    g.y = m[1] - s.vy * back + core::range(pen.rng, -0.03, 0.03);
    g.z = m[2] - s.vz * back + core::range(pen.rng, -0.03, 0.03);
    const double blow = core::range(pen.rng, 0.3, 0.8);
    g.vx = s.vx + blow * fx + core::range(pen.rng, -0.25, 0.25);
    g.vy = s.vy + core::range(pen.rng, -0.05, 0.25);
    g.vz = s.vz + blow * fz + core::range(pen.rng, -0.25, 0.25);
    g.life = (0.5 + 0.5 * thick) * core::range(pen.rng, 0.7, 1.3);
    g.size = core::range(pen.rng, 0.12, 0.26);
    g.drag = 3;
    g.fall = 0;
    g.streak = 0;
    g.albedo = 0.95;
    g.opacity = (0.1 + 0.22 * thick / 1.5) * core::range(pen.rng, 0.7, 1.0);
    out.push_back(g);
  }
  return n;
}

}  // namespace ge::skier
