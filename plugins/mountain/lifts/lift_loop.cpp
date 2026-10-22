// The lift plan, the loop and its clock (lift_loop.hpp).
//
// ge:summary plan_lift, carrier_at, carrier_speed_at, carrier_place, the load zone
// ge:keywords lift carrier carrier_at loop clock detachable load-zone
// ge:skill mountain
// ge:test tests/mountain_lift_loop_test.cpp
#include "mountain/lifts/lift_loop.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::mountain::lifts {

namespace mg = ge::mountain::mapgen;
namespace num = ge::core::num;
namespace libm = ge::core::libm;
using ge::core::kPi;

namespace {

double speed_sq(double slow, double a, double d) { return slow * slow + 2 * a * num::max(0.0, d); }

// Where `w` m round the loop is (0 the bottom wheel's tangent on the up rope).
LoopSpot loop_spot(const LiftPlan& plan, double w) {
  const double L = plan.length;
  const double T = kPi * plan.radius;
  const double r = plan.radius;
  LoopSpot s;
  if (w < L) {
    s.u = num::max(0.0, w);
  } else if (w < L + T) {
    s.turn = (w - L) / r;
    s.u = L;
    s.side = s.turn < kPi / 2 ? 0 : 1;
  } else if (w < 2 * L + T) {
    s.u = L - (w - L - T);
    s.side = 1;
  } else {
    s.turn = num::min(kPi, (w - 2 * L - T) / r);
    s.u = 0;
    s.side = s.turn < kPi / 2 ? 1 : 0;
  }
  return s;
}

// The loop's length `w` a detachable's clock has run `s` rope-speed metres to.
double loop_at(const LiftPlan& plan, double s) {
  const std::vector<double>& a = plan.clock;
  size_t lo = 0;
  size_t hi = a.size() - 1;
  while (hi - lo > 1) {
    const size_t mid = (lo + hi) / 2;
    if (a[mid] <= s) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  const double f = (s - a[lo]) / num::max(1e-9, a[hi] - a[lo]);
  return (static_cast<double>(lo) + ge::core::clamp(f, 0, 1)) *
         (plan.loop / static_cast<double>(a.size() - 1));
}

}  // namespace

LiftRide lift_ride(mg::LiftKind kind) {
  // The load zone's middle and the gauge are the generator's (`lift_look`), stated once there.
  const mg::LiftLookLane look = mg::lift_look(kind);
  LiftRide r{3, 3, 18, look.gauge, {look.entry_at, 4, look.entry_side, 1.6, 6, 0.9}};
  switch (kind) {
    case mg::LiftKind::gondola:
      r = {6, 0.4, 70, look.gauge, {look.entry_at, 4, look.entry_side, 1.3, 6, 1.4}};
      break;
    case mg::LiftKind::chair:
      r = {5, 1.2, 24, look.gauge, {look.entry_at, 5, look.entry_side, 2.4, 7, 1.2}};
      break;
    case mg::LiftKind::drag:
      break;
  }
  return r;
}

LiftPlan plan_lift(const mg::Lift& lift, int32_t index) {
  LiftPlan p;
  p.lift = index;
  p.kind = lift.kind;
  p.bottom = {lift.bottom.x, lift.bottom.z};
  p.top = {lift.top.x, lift.top.z};
  const double ex = p.top.x - p.bottom.x;
  const double ez = p.top.z - p.bottom.z;
  p.length = num::max(1.0, ge::core::hypot(ex, ez));
  p.dx = ex / p.length;
  p.dz = ez / p.length;
  p.heading = libm::atan2(p.dx, p.dz);
  p.ride = lift_ride(lift.kind);
  p.radius = lift.kind == mg::LiftKind::drag ? mg::kDragArm : p.ride.gauge / 2;
  p.loop = 2 * p.length + 2 * kPi * p.radius;
  p.carriers = std::max<int32_t>(1, static_cast<int32_t>(std::floor(2 * p.length / p.ride.every)));
  if (p.ride.slow < p.ride.speed) {
    const auto n = static_cast<size_t>(std::ceil(p.loop / kClockStep));
    const double step = p.loop / static_cast<double>(n);
    const auto rate = [&p](double w) {
      const LoopSpot at = loop_spot(p, w);
      if (at.turn >= 0) return p.ride.speed / p.ride.slow;
      return p.ride.speed / carrier_speed_at(p, at.u, at.side);
    };
    p.clock.assign(n + 1, 0);
    for (size_t i = 1; i <= n; i++) {
      const double w0 = static_cast<double>(i - 1) * step;
      const double w1 = static_cast<double>(i) * step;
      p.clock[i] = p.clock[i - 1] + (rate(w0) + rate(w1)) / 2 * step;
    }
    p.span = p.clock[n];
  }
  return p;
}

std::vector<LiftPlan> lift_plans(const mg::Level& level) {
  std::vector<LiftPlan> out;
  if (!level.resort) return out;
  const auto& lifts = level.resort->lifts;
  out.reserve(lifts.size());
  for (size_t i = 0; i < lifts.size(); i++)
    out.push_back(plan_lift(lifts[i], static_cast<int32_t>(i)));
  return out;
}

double load_at(const LiftPlan& plan) {
  return plan.kind == mg::LiftKind::gondola ? kGondolaLoad : plan.ride.entry.at;
}

double off_at(const LiftPlan& plan) {
  // Where `mapgen::let_go_of` stands him: a cabin's door, a chair's unload, a drag's let-go.
  return plan.length -
         (plan.kind == mg::LiftKind::gondola ? mg::kGondolaDoor : mg::lift_look(plan.kind).off);
}

double carrier_speed_at(const LiftPlan& plan, double u, int32_t side) {
  const double speed = plan.ride.speed;
  const double slow = plan.ride.slow;
  if (slow >= speed) return speed;
  const double load = load_at(plan);
  const double off = off_at(plan);
  const double w2 =
      side == 0 ? num::min(speed_sq(slow, kAccel, u - load), speed_sq(slow, kDecel, off - u))
                : num::min(speed_sq(slow, kAccel, off - u), speed_sq(slow, kDecel, u - load));
  return num::min(speed, std::sqrt(w2));
}

LoopSpot carrier_at(const LiftPlan& plan, int32_t k, double t) {
  const bool timed = !plan.clock.empty();
  const double span = timed ? plan.span : plan.loop;
  const double raw =
      static_cast<double>(k) * span / static_cast<double>(plan.carriers) + plan.ride.speed * t;
  const double s = num::mod(num::mod(raw, span) + span, span);
  LoopSpot spot = loop_spot(plan, timed ? loop_at(plan, s) : s);
  const double clear = plan.kind == mg::LiftKind::gondola ? 7 : 2;
  spot.out = spot.turn < 0 && spot.u > clear && spot.u < plan.length - clear;
  return spot;
}

int32_t carrier_passing(const LiftPlan& plan, double u, double t, double dt) {
  for (int32_t k = 0; k < plan.carriers; k++) {
    const LoopSpot a = carrier_at(plan, k, t);
    if (a.side != 0 || a.turn >= 0 || a.u >= u) continue;
    const LoopSpot b = carrier_at(plan, k, t + dt);
    if (b.side == 0 && b.turn < 0 && b.u >= u) return k;
  }
  return -1;
}

Place carrier_place(const LiftPlan& plan, const LoopSpot& c) {
  const double r = plan.radius;
  double along = c.u;
  double v = c.side == 0 ? r : -r;
  double tu = c.side == 0 ? 1 : -1;
  double tv = 0;
  if (c.turn >= 0) {
    const double a = c.turn;
    if (c.u > 0) {
      along = plan.length + r * libm::sin(a);
      v = r * libm::cos(a);
      tu = libm::cos(a);
      tv = -libm::sin(a);
    } else {
      along = -r * libm::sin(a);
      v = -r * libm::cos(a);
      tu = -libm::cos(a);
      tv = libm::sin(a);
    }
  }
  Place p;
  p.x = plan.bottom.x + plan.dx * along + plan.dz * v;
  p.z = plan.bottom.z + plan.dz * along - plan.dx * v;
  p.heading = libm::atan2(plan.dx * tu + plan.dz * tv, plan.dz * tu - plan.dx * tv);
  return p;
}

mg::LinePoint line_frame(const LiftPlan& plan, double x, double z) {
  const double rx = x - plan.bottom.x;
  const double rz = z - plan.bottom.z;
  return {rx * plan.dx + rz * plan.dz, rx * plan.dz - rz * plan.dx};
}

bool in_load_zone(const LiftPlan& plan, double x, double z, double heading, double speed) {
  const LiftRide::Entry& e = plan.ride.entry;
  if (speed > e.fastest) return false;
  const mg::LinePoint p = line_frame(plan, x, z);
  if (std::abs(p.u - e.at) > e.along || std::abs(p.v - e.side) > e.across) return false;
  return std::abs(ge::core::angle_diff(plan.heading, heading)) <= e.turned;
}

Place load_spot(const LiftPlan& plan) {
  const LiftRide::Entry& e = plan.ride.entry;
  return {plan.bottom.x + plan.dx * e.at + plan.dz * e.side,
          plan.bottom.z + plan.dz * e.at - plan.dx * e.side, plan.heading};
}

}  // namespace ge::mountain::lifts
