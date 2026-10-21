// THE LIFT LINE'S GEOMETRY the generator places stations by: where a lift's boarding ring lies
// in its line's frame, and where a rider is let go at its top. This is the
// generator's half of the lift line: the ring, the let-go point, the queue lane they are read off
// and the measures of each kind of lift they read. The rope, the towers, the carriers and the
// station houses belong to the run and the renderer, and arrive with M5 and M8.
//
// Pure arithmetic over a lift's two ends; draws nothing from any stream.
// Held by tests/mountain_resort_build_test.cpp (the lifts and the way).
//
// ge:summary Where a lift's boarding ring lies and where a rider is let go at its top
// ge:keywords lift-line ring-frame let-go-of queue-lane boarding-ring lift-look corral-tail
// ge:keywords drag-arm
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The measures of a kind of lift the queue's lane is laid by (`LIFT_LOOK`): the load zone's
// middle up the line (`entry_at`) and right of it (`entry_side`), the station house's width,
// the gauge between the ropes, and how far short of the top a rider leaves it (`off`).
struct LiftLookLane {
  double entry_at;
  double entry_side;
  double house_width;
  double gauge;
  double off;
};

inline constexpr LiftLookLane lift_look(LiftKind kind) {
  switch (kind) {
    case LiftKind::gondola:
      return {-17.5, 0, 6, 6, 0};
    case LiftKind::chair:
      return {5, 2.5, 5, 5, mapgen::resort_rules::lift::unload::at};
    case LiftKind::drag:
      break;
  }
  return {4, 1.2, 3, 0, mapgen::resort_rules::lift::drag::let_go};
}

// How far past the corral's mouth its rope fences run on, m, and the boarding ring past it.
inline constexpr double kCorralTail = 8;
inline constexpr double kBoardingRingRadius = 3.5;
inline constexpr double kBoardingRingGap = 1;
// How far right of the track a drag's rider hangs off its arm, m.
inline constexpr double kDragArm = 1.2;
// TUNING.lift: how far short of a gondola's top its door lets a rider out, m, and how far off
// the line a chair's rider is turned over its unload ramp, rad.
inline constexpr double kGondolaDoor = 10;
inline constexpr double kChairRampTurn = 0.3;

struct LinePoint {
  double u;  // m up the line from the bottom wheel
  double v;  // m right of it
};

// Where a kind of lift's boarding ring lies in its line's frame: on the queue lane's last leg,
// past the corral's open end (`ring_frame`, read off the queue lane).
inline LinePoint ring_frame(LiftKind kind) {
  const LiftLookLane look = lift_look(kind);
  LinePoint a{look.entry_at, 0};
  LinePoint b{look.entry_at - 40, 0};
  if (kind != LiftKind::gondola) {
    const double clear = look.house_width / 2 + look.gauge / 2 + 3;
    const LinePoint mouth{look.entry_at - 7, clear + 1};
    const double du = mouth.u - (look.entry_at - 1);
    const double dv = mouth.v - look.entry_side;
    const double h = ge::core::hypot(du, dv);
    const double k = 40 / (h != 0 ? h : 1);
    a = mouth;
    b = {mouth.u + du * k, mouth.v + dv * k};
  }
  const double h = ge::core::hypot(b.u - a.u, b.v - a.v);
  const double len = h != 0 ? h : 1;
  const double out = kCorralTail + kBoardingRingGap + kBoardingRingRadius;
  return {a.u + ((b.u - a.u) / len) * out, a.v + ((b.v - a.v) / len) * out};
}

struct LetGo {
  double x;
  double z;
  double heading;
};

// WHERE A RIDER IS LET GO AT A LIFT'S TOP, read off its two ends alone, and the way he faces.
inline LetGo let_go_of(LiftKind kind, mapgen::PlanPoint bottom, mapgen::PlanPoint top) {
  const double ex = top.x - bottom.x;
  const double ez = top.z - bottom.z;
  const double length = ge::core::num::max(1.0, ge::core::hypot(ex, ez));
  const double dx = ex / length;
  const double dz = ez / length;
  const double heading = ge::core::libm::atan2(dx, dz);
  const LiftLookLane look = lift_look(kind);
  const auto at = [&](double u, double v, double h) {
    return LetGo{bottom.x + dx * u + dz * v, bottom.z + dz * u - dx * v, h};
  };
  if (kind == LiftKind::gondola) return at(length - kGondolaDoor, 0, heading + ge::core::kPi);
  if (kind == LiftKind::chair)
    return at(length - look.off, look.gauge / 2, heading + kChairRampTurn);
  return at(length - look.off, kDragArm, heading);
}

}  // namespace ge::mountain::mapgen
