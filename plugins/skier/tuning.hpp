// THE NUMBERS THAT SHAPE THE SKIER'S FEEL, shared by every pair (a pair's own are `skis.hpp`'s).
// Every number carries its unit, and the model it feeds says where it comes from at the function
// that implements it (`snow.hpp`, `limits.hpp`, `skier.cpp`). Change one, then run the `ride` lab
// before and after (`make lab LAB=ride ARGS=--tag=before`, then `--compare=before`).
//
// ge:summary The skier's shared tuning numbers: snow, grip, steering, drive, jump, air, wipeout
// ge:keywords tuning numbers constants snow grip edge steer pivot tuck drive skate pole jump
// ge:keywords pop air landing harsh wipeout trees gravity air-density
// ge:skill skier
// ge:test tests/skier_scenarios_test.cpp
#pragma once

namespace ge::skier::tuning {

// Standard gravity, m/s^2.
inline constexpr double kGravity = 9.81;
// The air's density at -8 C two thousand metres up, kg/m^3 (the standard atmosphere).
inline constexpr double kAirDensity = 1.05;

// THE SNOW (`snow.hpp`).
namespace snow {
// How deep a standing skier sinks into untouched powder (to the boot tops), and into a groomed
// piste (a centimetre of corduroy pressed), m.
inline constexpr double kPowderSink = 0.22;
inline constexpr double kPackedSink = 0.012;
// THE PLANING SPEED, m/s: the sink falls as exp(-(v / plane)^2), so at this speed a ski carries
// a third of its rest sink and by 30 km/h it rides a few centimetres into the powder.
inline constexpr double kPlaneSpeed = 6;
// How quickly the support under the skis follows the sink the speed asks for, s.
inline constexpr double kSinkLag = 0.25;
// The base's sliding friction, as a share of the load: 0.03-0.05 on cold hard snow, more in
// powder.
inline constexpr double kCrrPacked = 0.035;
inline constexpr double kCrrPowder = 0.07;
// THE PLOUGH: the snow a sunk pair shoves aside, N per metre of sink per (m/s)^2 of speed, so a
// skier bogged in powder wants momentum.
inline constexpr double kPlough = 12;
// POWDER DRAG: compacting fresh snow at speed, as a share of the load per m/s (none on packed).
inline constexpr double kPowderDrag = 0.004;
}  // namespace snow

// THE GRIP across the skis (`snow.hpp`'s `side_grip`): a coefficient on the load.
namespace grip {
// A steel edge biting a groomed surface at the pair's full edge, and the share of it a ski lying
// flat keeps (a flat ski slides; the rest comes with the edge). An arcade racer's edge holds a
// little over a real carve's 1-1.2 g.
inline constexpr double kEdgePacked = 1.35;
inline constexpr double kFlatShare = 0.3;
// In powder the edge is buried and the ski turns on its base: what the base holds sideways.
inline constexpr double kBasePowder = 0.6;
// THE BRAKE: skis pivoted across the way and pushed sideways shove snow, a decelerating share of
// the load at full brake: a hockey stop pulls 0.5-0.8 g on a groomer, a snowplough in powder
// less.
inline constexpr double kBrakePacked = 0.65;
inline constexpr double kBrakePowder = 0.4;
// The share of the edge's sideways grip a braking skier keeps (his skis are sliding, not
// cutting).
inline constexpr double kBrakeHold = 0.5;
}  // namespace grip

// THE STEER (`skier.cpp`): the edge, the carve and the pivot.
namespace steer {
// How fast the skis roll from edge to edge, rad/s.
inline constexpr double kEdgeRate = 3.4;
// The edge fades with speed: the pair's full edge at a crawl, two thirds of it by this speed,
// m/s; a skier at a schuss stands his skis flatter than one snaking a nursery slope.
inline constexpr double kFadeSpeed = 25;
// A full tuck keeps this share of the edge (a tucked skier only steers gentle bends).
inline constexpr double kTuckEdge = 0.6;
// THE PIVOT: a skier twists his skis round under him, rad/s at full steer, at a crawl and at
// speed (a skidded turn's swing), the fast rate whole by `kPivotFade` m/s.
inline constexpr double kPivotSlow = 1.8;
inline constexpr double kPivotFast = 0.35;
inline constexpr double kPivotFade = 10;
// THE SKIS TRACK: a ski sliding sideways is turned back toward its way by its own edge's drag
// at the tail, 1/s of the slip angle, whole past `kAlignSpeed` m/s.
inline constexpr double kAlign = 2.5;
inline constexpr double kAlignSpeed = 3;
// The carve turns the skis no faster than this share of what the edge's grip can hold.
inline constexpr double kCarveHold = 0.95;
// How quickly the yaw rate follows the one the steer asks for, s.
inline constexpr double kYawLag = 0.08;
// How quickly the body's inclination follows the turn's balance, s, and its most, rad.
inline constexpr double kLeanLag = 0.1;
inline constexpr double kLeanMost = 1.0;
}  // namespace steer

// THE TUCK and THE DRIVE a skier makes himself (skating and poling, `skier.cpp`).
namespace drive {
// How fast the body folds into the tuck and stands back up, 1/s.
inline constexpr double kCrouchRate = 3;
// The skier's push: the power he puts in skating and poling, W, and the most force, N. The push
// is whole up to `kFrom` m/s and gone by `kTo` (past a run's pace, a push buys nothing).
inline constexpr double kPower = 320;
inline constexpr double kForceMost = 170;
inline constexpr double kFrom = 3;
inline constexpr double kTo = 7;
// The tuck past which the lever means "push" at a crawl.
inline constexpr double kTuckPush = 0.5;
}  // namespace drive

// THE JUMP: the legs loaded while the jump is held and sprung when it is let go, straight off
// the snow's normal; `kPopMin` m/s for a tap, `kPopMax` at `kPopFull` s held.
namespace jump {
inline constexpr double kPopMin = 2.2;
inline constexpr double kPopMax = 3.5;
inline constexpr double kPopFull = 1.0;
}  // namespace jump

// THE AIR AND THE LANDING (`skier.cpp`).
namespace air {
// How far the snow may fall away from under the skis, m, before he is off it: the legs reach
// that far down to keep the skis on a crest.
inline constexpr double kLeaveGap = 0.03;
// A flight shorter than this is a skip over the snow's grain, not a jump, s.
inline constexpr double kFlightMin = 0.15;
// The steer in the air turns him at most this fast about the vertical, rad/s.
inline constexpr double kSpin = 2.5;
// THE LEGS stop a landing over this much stroke, m: the load is 1 + v_n^2 / (2 stroke g).
inline constexpr double kStroke = 0.45;
// A landing past this load folds the legs: a wipeout, g. A drop of three metres onto the flat
// lands at about 8 g; this is a drop of five or more.
inline constexpr double kHarshG = 12;
// Landed this far off his way (the skis against the direction of travel), rad, at more than
// `kAcrossSpeed` m/s: the skis catch and he goes over.
inline constexpr double kAcross = 1.1;
inline constexpr double kAcrossSpeed = 6;
}  // namespace air

// THE TRUNKS AND THE WIPEOUT.
namespace wipeout {
// How wide a skier is to a trunk, m (half his shoulders).
inline constexpr double kBodyRadius = 0.35;
// Meeting a trunk faster than this along the line between them, m/s, is a wipeout; slower, he
// glances off it.
inline constexpr double kTreeSpeed = 3;
// A body down on the snow slides on this share of its load, and counts as still under this
// speed, m/s.
inline constexpr double kBodyFriction = 0.6;
inline constexpr double kStill = 0.4;
// How far inside the world's edge a skier is held, m.
inline constexpr double kEdgeMargin = 2;
}  // namespace wipeout

}  // namespace ge::skier::tuning
