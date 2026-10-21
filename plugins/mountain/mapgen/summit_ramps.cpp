// The room a ramp off a top needs, and what keeps off it.
//
// ge:summary The room a ramp off a top needs: the rim, the glide, the lines and cuts it avoids
// ge:keywords summit-ramps ramp-room room-blocked near-room in-approach off-line
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/summit_ramps.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace RR = resort_rules;
namespace K = RR::lift::top::ramp;
using ge::core::hypot;

namespace {

// How much higher than its top's middle a ramp's head on the rim may stand, m.
constexpr double kRimRise = 0.2;
// How many points round the pad's rim a ramp is tried from.
constexpr int kRim = 36;
// The step a ramp's line is read at, m.
constexpr double kLineStep = 1.5;
// How far a ramp's line keeps off a lift's line, m; and how far past the approach cut's own
// half-width it keeps off the way in to a top, m (its head, on the rim, a little further).
constexpr double kLineKeep = 9;
constexpr double kApproachKeep = 5;
constexpr double kApproachHead = RR::lift::top::approach::blend + 2;
// How much more than the glide a start is asked to lie under a ramp's head, the share of the
// steepest it may fall at, and the step its line is read at (each a real face's where the ramps
// are chosen together).
constexpr double kRoomSpare = 1;
constexpr double kRoomSteep = 0.7;
constexpr double kJointSpare = 1.5;
constexpr double kJointSteep = 0.6;
constexpr double kRoomStep = 3;

// The steepest a ramp down to a run of `row`'s colour falls overall.
double steepest(const GradeRow& row, const RampFit& fit) {
  return row.steepest.max <= grade_row(PisteGrade::blue).steepest.max ? fit.gentle : K::steep;
}

// Whether (x, z) is too near the ground R26's approach cut away under the way in to a pad's top.
bool in_approach(std::span<const StationPad> pads, double x, double z, double keep) {
  namespace A = RR::lift::top::approach;
  for (const StationPad& p : pads) {
    if (!padded(p.kind)) continue;
    const bool gondola = p.kind == LiftKind::gondola;
    const double from = gondola ? A::from.gondola : A::from.chair;
    const double half = gondola ? A::half.gondola : A::half.chair;
    const double back = (p.x - x) * p.dx + (p.z - z) * p.dz;
    if (back < from - A::ease || back > num::min(A::length, p.length / 2)) continue;
    const double v = std::abs((x - p.x) * p.dz - (z - p.z) * p.dx);
    if (v < half + keep) return true;
  }
  return false;
}

// How far (x, z) stands off a lift's line from bottom to top, m.
double off_line(const LiftPlan& l, double x, double z) {
  const double ex = l.top.x - l.bottom.x;
  const double ez = l.top.z - l.bottom.z;
  const double len2 = ex * ex + ez * ez;
  const double k = num::max(
      0.0, num::min(1.0, ((x - l.bottom.x) * ex + (z - l.bottom.z) * ez) / (len2 == 0 ? 1 : len2)));
  return hypot(x - (l.bottom.x + ex * k), z - (l.bottom.z + ez * k));
}

}  // namespace

double near_room(const RampRoom& r, double x, double z) {
  const double ex = r.to.x - r.from.x;
  const double ez = r.to.z - r.from.z;
  const double len2 = ex * ex + ez * ez;
  const double k = num::max(
      0.0, num::min(1.0, ((x - r.from.x) * ex + (z - r.from.z) * ez) / (len2 == 0 ? 1 : len2)));
  return hypot(x - (r.from.x + ex * k), z - (r.from.z + ez * k)) -
         r.half.value_or(K::width / 2 + K::blend);
}

std::optional<RampRoom> ramp_room(const StationPad& p, std::span<const StationPad> pads,
                                  std::span<const LiftPlan> lifts, double x, double z, double half,
                                  const GradeRow& row,
                                  const std::function<double(double, double)>& height,
                                  const BlockedAt& blocked, const RampFit& fit) {
  const double step = fit.joint ? kLineStep : kRoomStep;
  for (int a = 0; a < kRim; a++) {
    const double ang = ((double(a) / kRim) * ge::core::kPi) * 2;
    const double fx = p.x + libm::sin(ang) * p.r;
    const double fz = p.z + libm::cos(ang) * p.r;
    if ((x - fx) * (fx - p.x) + (z - fz) * (fz - p.z) <= 0) continue;
    if (in_approach(pads, fx, fz, kApproachHead)) continue;
    if (fit.joint && height(fx, fz) > p.y + kRimRise) continue;
    const double d = hypot(x - fx, z - fz);
    const double length = d - half;
    if (length < K::least) continue;
    // Down to the run's edge on its side: falling enough to the ground there, and not too
    // steeply to its line's.
    const double rim = height(fx, fz);
    const double edge = height(fx + ((x - fx) * length) / d, fz + ((z - fz) * length) / d);
    const double spare = fit.joint ? kJointSpare : kRoomSpare;
    if ((rim - edge) / length < fit.glide * spare) continue;
    const double steep = fit.joint ? kJointSteep : kRoomSteep;
    if ((rim - height(x, z)) / length > steepest(row, fit) * steep) continue;
    bool clear = true;
    const double reach = fit.joint ? d : length;
    double u = 0;
    while (clear && u <= reach) {
      const double k = u / (length + half);
      const double px = fx + (x - fx) * k;
      const double pz = fz + (z - fz) * k;
      clear = !blocked(px, pz) && !in_approach(pads, px, pz, kApproachKeep) &&
              std::none_of(lifts.begin(), lifts.end(),
                           [&](const LiftPlan& l) { return off_line(l, px, pz) < kLineKeep; });
      u += step;
    }
    if (clear) return RampRoom{{fx, fz}, {x, z}};
  }
  return std::nullopt;
}

BlockedAt room_blocked(const StationPad* pad, std::span<const StationPad> pads,
                       std::span<const PlanPoint> stations, std::span<const RampRoom> rooms,
                       double keep_pad, double keep_station, BlockedAt on_run) {
  return [=, on_run = std::move(on_run)](double x, double z) {
    for (const RampRoom& r : rooms) {
      if (near_room(r, x, z) < K::width / 2) return true;
    }
    if (on_run(x, z)) return true;
    for (const StationPad& p : pads) {
      if (&p != pad && hypot(x - p.x, z - p.z) < p.r + keep_pad) return true;
    }
    for (const PlanPoint& s : stations) {
      if (!(pad && hypot(s.x - pad->x, s.z - pad->z) < 1) && hypot(x - s.x, z - s.z) < keep_station)
        return true;
    }
    return false;
  };
}

}  // namespace ge::mountain::mapgen
