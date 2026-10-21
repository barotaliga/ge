// WHAT THE SNOW DOES TO A PAIR OF SKIS, stated once for the physics, the bot and the labs. The
// snow under a skier is one number, the PACKED SHARE (0 untouched powder ... 1 a groomed piste,
// the level's `packed_at`), and every answer here blends its powder and its packed halves by it.
//
// - THE SINK: a ski sinks into powder as a planing hull sits in water, buried to the boot at rest
//   and floating up with speed, exp(-(v / plane)^2); a groomer only takes a centimetre.
// - THE BASE'S FRICTION along the ski (Coulomb, a share of the load) and THE PLOUGH and the
//   powder's compaction against a sunk ski at speed.
// - THE SIDEWAYS GRIP: on packed snow a steel edge bites harder the further it is stood over; in
//   powder the base holds by its float alone.
// - THE BRAKE: skis pivoted across and pushed sideways, a share of the load.
//
// ge:summary The snow under the skis: sink, base friction, plough, sideways grip, brake
// ge:keywords snow powder packed groomer sink plane planing friction crr plough drag
// ge:keywords grip edge side-grip brake skid sink_target
// ge:skill skier
// ge:test tests/skier_scenarios_test.cpp
#pragma once

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "skier/skis.hpp"
#include "skier/tuning.hpp"

namespace ge::skier {

// How deep the skis sit below the untouched surface at `speed` m/s, m.
inline double sink_target(double packed, double speed) {
  namespace t = tuning::snow;
  const double q = speed / t::kPlaneSpeed;
  return t::kPackedSink * packed + t::kPowderSink * (1 - packed) * core::libm::exp(-(q * q));
}

// The base's sliding friction along the skis, as a share of the load.
inline double base_friction(double packed) {
  return core::lerp(tuning::snow::kCrrPowder, tuning::snow::kCrrPacked, packed);
}

// What the loose snow costs a pair sunk `sink` m at `speed` m/s under `load` N: the plough's
// bow wave and the compaction, N, against the way. Both are the loose snow's alone, so a
// groomer's pressed centimetre costs nothing.
inline double powder_drag(double packed, double sink, double speed, double load) {
  namespace t = tuning::snow;
  const double loose = 1 - packed;
  return (t::kPlough * sink * speed * speed + t::kPowderDrag * load * speed) * loose;
}

// What the skis hold sideways on `edge` rad of edge (either sign), as a coefficient on the load.
inline double side_grip(const SkiSpec& spec, double packed, double edge) {
  namespace t = tuning::grip;
  const double share = core::clamp(edge < 0 ? -edge / spec.edge_max : edge / spec.edge_max, 0, 1);
  const double bite = t::kEdgePacked * (t::kFlatShare + (1 - t::kFlatShare) * share);
  return core::lerp(t::kBasePowder, bite, packed);
}

// What a full brake decelerates by, as a share of the load.
inline double brake_grip(double packed) {
  return core::lerp(tuning::grip::kBrakePowder, tuning::grip::kBrakePacked, packed);
}

}  // namespace ge::skier
