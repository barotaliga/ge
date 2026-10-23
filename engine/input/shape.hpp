// THE SHAPE OF AN AXIS: what a stick, a trigger or a pedal reads becomes what the game is
// handed, through a dead zone, an outer edge, a response curve and an anti-dead zone; and a key
// (on or off) becomes an axis that eases toward where it points (the ramp). Every function is
// pure and deterministic (doubles inside, core::libm for the curve), so a lab can chart a
// shape and a test can hold its numbers.
//
// THE DEAD ZONES (a stick):
//   axial          each axis on its own: |v| under `inner` is 0, the rest rescaled to 0..1.
//                  Snaps to the cardinals; a diagonal near the centre is lost.
//   radial         the push's length under `inner` is 0; above it the stick is passed as is
//                  (a step at the edge of the zone: the classic, kept for comparisons).
//   scaled_radial  the length rescaled from `inner` .. `outer` to 0..1 along the push's own
//                  direction: round, no step, every direction kept. The default.
//   sloped         scaled_radial, then an axial zone on each axis that widens with the other
//                  axis's push (`inner` times it): a push near a cardinal snaps onto it, a
//                  diagonal stays a diagonal; the length stays scaled_radial's. For menus and for
//                  cameras that want straight lines.
// Past `outer` the output is 1: a worn stick that cannot reach its corner still reaches full.
//
// THE CURVE is applied to the rescaled length: `power` raises it to `exponent` (above 1: fine
// control near the centre, a faster end). THE ANTI-DEAD ZONE lifts the first output above 0 to
// `anti`, for a game that already has a dead zone of its own to cross.
//
// ge:summary Dead zones (axial, radial, scaled radial, sloped), curves, anti-dead zone, ramps
// ge:keywords input dead-zone deadzone axial radial scaled-radial sloped outer curve response
// ge:keywords exponent anti-deadzone stick trigger threshold hysteresis ramp ease key-ramp
// ge:keywords shape_stick shape_axis ramp_toward digital stick-shape axis-shape
// ge:skill input
// ge:test tests/input_shape_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ge::input {

enum class Zone : uint8_t { none, axial, radial, scaled_radial, sloped };

struct Curve {
  float exponent = 1;  // the power curve's (1 = linear)
};

struct StickShape {
  Zone zone = Zone::scaled_radial;
  float inner = 0.12f;  // the dead zone, of the push's length
  float outer = 0.97f;  // the push that already reads full
  Curve curve;
  float anti = 0;  // the first output above 0
};

// One axis (a trigger, a pedal, a twist, one half of a stick bound alone). Signed values keep
// their sign; the zone is on |v|.
struct AxisShape {
  float inner = 0.05f;
  float outer = 0.98f;
  Curve curve;
  float anti = 0;
};

// A stick through its shape, in place (-1 .. 1 each; the result's length is at most 1).
void shape_stick(float& x, float& y, const StickShape& s);
// One axis through its shape (-1 .. 1, or 0 .. 1 for a trigger).
float shape_axis(float v, const AxisShape& s);

// An analog value read as a button, with hysteresis: on above `on`, off below `off`.
struct Threshold {
  float on = 0.5f;
  float off = 0.4f;
};
bool digital(bool was_on, float v, const Threshold& t);

// THE RAMP (a key held as an axis): walk `value` toward `target` at `attack` per second when
// the target is away from centre and at `release` when it is centre, over `dt_s`: a first-order
// ease, so the first of the travel comes quickly and the last settles, as a hand does. Under
// `snap` a value easing back to centre is 0 exactly. Attack 0 means at once.
struct Ramp {
  float attack = 0;   // 1/s (0: jump to the target)
  float release = 0;  // 1/s (0: jump back)
  float snap = 0.02f;
};
float ramp_toward(float value, float target, double dt_s, const Ramp& r);

std::string_view zone_name(Zone z);
std::optional<Zone> zone_from_name(std::string_view name);

}  // namespace ge::input
