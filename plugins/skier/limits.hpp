// WHAT A SKIER CAN DO, stated once and read by both the physics (`skier.cpp`) and any pilot (a
// bot plans its line and its speed on these): the most edge at a speed, the curvature an edge
// carves, the edge a curvature needs, the fastest a bend can be held, and the terminal speed down
// a pitch. Never restate a ceiling in a pilot: ask here.
//
// THE CARVE: a ski stood on its edge and pressed into reverse camber bends along its sidecut.
// The curvature it cuts grows from nothing on a flat ski to the sidecut's own at 45 degrees and
// tighter past it: kappa = tan(edge) / sidecut. THE TERMINAL SPEED is where the air's drag
// meets the slope's pull less the base's friction:
// v = sqrt(2 m g (sin(pitch) - crr cos(pitch)) / (rho CdA)) on a groomer.
//
// ge:summary What a skier can do: edge at speed, carve curvature, corner speed, terminal speed
// ge:keywords limits ceiling edge-most carve curvature corner-speed terminal-speed top-speed
// ge:keywords edge_for_curvature carve_curvature edge_most corner_speed terminal_speed
// ge:skill skier
// ge:test tests/skier_scenarios_test.cpp
#pragma once

#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "skier/skis.hpp"
#include "skier/snow.hpp"
#include "skier/tuning.hpp"

namespace ge::skier {

// The most edge the skier can stand the pair on at `speed` m/s with `tuck` (0..1) folded, rad.
inline double edge_most(const SkiSpec& spec, double speed, double tuck) {
  namespace t = tuning::steer;
  const double fade = 1 - core::clamp(speed / t::kFadeSpeed, 0, 1) / 3;
  return spec.edge_max * fade * (1 - (1 - t::kTuckEdge) * tuck);
}

// The curvature a carve on `edge` rad cuts, 1/m (unsigned).
inline double carve_curvature(const SkiSpec& spec, double edge) {
  const double e = edge < 0 ? -edge : edge;
  return core::libm::tan(e) / spec.sidecut;
}

// The edge a carve of `curvature` 1/m needs, rad (unsigned).
inline double edge_for_curvature(const SkiSpec& spec, double curvature) {
  return core::libm::atan((curvature < 0 ? -curvature : curvature) * spec.sidecut);
}

// The fastest a bend of `curvature` 1/m holds on grip `grip` (a coefficient on the load), m/s;
// infinite for a straight.
inline double corner_speed(double curvature, double grip) {
  const double k = curvature < 0 ? -curvature : curvature;
  if (k <= 0) return std::numeric_limits<double>::infinity();
  return std::sqrt(grip * tuning::kGravity / k);
}

// The fastest the pair carves a bend of `curvature` 1/m on `packed` snow at the edge the bend
// needs (the carve's grip grows with the edge it is cut on), m/s.
inline double carve_speed(const SkiSpec& spec, double curvature, double packed) {
  const double edge = core::clamp(edge_for_curvature(spec, curvature), 0, spec.edge_max);
  return corner_speed(curvature, side_grip(spec, packed, edge));
}

// The speed where the air's drag on `cda` m^2 and the powder's compaction (planed, so no
// plough) meet the pull down a `pitch` rad slope less the base's friction on `packed` snow, m/s
// (0 where the friction holds him): the root of a v^2 + b v = pull.
inline double terminal_speed(const SkiSpec& spec, double pitch, double cda, double packed) {
  const double weight = spec.mass * tuning::kGravity;
  const double cos_p = core::libm::cos(pitch);
  const double pull = weight * (core::libm::sin(pitch) - base_friction(packed) * cos_p);
  if (pull <= 0) return 0;
  const double a = 0.5 * tuning::kAirDensity * cda;
  const double b = tuning::snow::kPowderDrag * (1 - packed) * weight * cos_p;
  return (std::sqrt(b * b + 4 * a * pull) - b) / (2 * a);
}

// The reference expectation every pair is held to: flat out in a tuck down a 20 degree groomer.
inline double top_speed(const SkiSpec& spec) {
  return terminal_speed(spec, core::kPi / 9, spec.cda_tuck, 1);
}

}  // namespace ge::skier
