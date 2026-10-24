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
// THE SKIS AHEAD OF HIM: the tips stand this share of the pair's length ahead of the boots (the
// boots mounted a little behind the middle), and the pair, side by side, is this wide to a trunk
// either side of its line, m. A circle at the tip and one halfway to it meet a trunk before his
// body does; a glance off them turns the skis with his way when he is going faster than
// `kTipTurnSpeed`, m/s.
inline constexpr double kTipShare = 0.55;
inline constexpr double kTipRadius = 0.12;
inline constexpr double kTipTurnSpeed = 0.5;
// A skier put down from outside (`put_down`) slides on this share of his load for the one step
// before his body is thrown.
inline constexpr double kBodyFriction = 0.6;
// How far inside the world's edge a skier is held, m.
inline constexpr double kEdgeMargin = 2;
}  // namespace wipeout

// THE THROWN BODY (`ragdoll.hpp`): thirteen points at a skier's measures, the throw a wipeout
// gives them, the joints, the muscles, the air and the snow.
namespace ragdoll {
// The bones, m: the thigh, the shin, the upper arm, the forearm; the spine from between the hips
// to between the shoulders, the half widths of the shoulders and the hips, the neck.
inline constexpr double kThigh = 0.44;
inline constexpr double kShin = 0.46;
inline constexpr double kUpperArm = 0.31;
inline constexpr double kForearm = 0.34;
inline constexpr double kSpine = 0.5;
inline constexpr double kShoulder = 0.2;
inline constexpr double kHip = 0.12;
inline constexpr double kNeck = 0.18;
// Each point's mass, kg (80 in all).
inline constexpr double kMassHip = 14;
inline constexpr double kMassShoulder = 11;
inline constexpr double kMassHead = 5;
inline constexpr double kMassKnee = 5;
inline constexpr double kMassFoot = 4;
inline constexpr double kMassElbow = 2;
inline constexpr double kMassHand = 1.5;
// The radius the snow and the trunks keep each point out by, m: the torso's, the head's, a
// limb's.
inline constexpr double kRadiusTorso = 0.11;
inline constexpr double kRadiusHead = 0.13;
inline constexpr double kRadiusLimb = 0.06;
// The drag area of a body tumbling through the air, Cd A, m^2 (belly to the air: some 55 m/s
// falling).
inline constexpr double kDragArea = 0.5;
// The height of his centre of mass over the skis' base as he stands, m.
inline constexpr double kStand = 0.95;
// Passes over the joints a step: enough that no limb is seen to stretch.
inline constexpr int kIterations = 8;
// THE JOINTS a body cannot pass: the thigh no further than `kHipBack` of its length behind the
// hip nor `kHipUp` above it; a hand no nearer its shoulder than `kFoldArm` m; the knee and the
// elbow folded no tighter than `kKneeFold` and `kElbowFold` rad between their bones (35 and 30
// degrees); no limb nearer the line of the spine than `kTorso` m.
inline constexpr double kHipBack = 0.35;
inline constexpr double kHipUp = 0.3;
inline constexpr double kFoldArm = 0.2;
inline constexpr double kKneeFold = 0.6;
inline constexpr double kElbowFold = 0.52;
inline constexpr double kTorso = 0.16;
// THE MUSCLES: every limb driven toward a pose in the trunk's frame by a damped spring, `kBrace`
// rad/s at `kBraceDamp` of critical damping in the air (the hands thrown out `kReach` of the
// arm's length toward the snow, the knees drawn up), giving way at `kRelax` 1/s once his trunk is
// down to the slack pose a body lies in, `kLie` rad/s at `kLieDamp`.
inline constexpr double kBrace = 10;
inline constexpr double kBraceDamp = 0.8;
inline constexpr double kLie = 2;
inline constexpr double kLieDamp = 1.2;
inline constexpr double kRelax = 12;
inline constexpr double kReach = 0.85;
// THE SNOW under every point: settled `kSink` m into powder (the packed share's complement),
// Coulomb friction along it on the weight and the arrival (`kFrictionPacked` on the groomer,
// `kFrictionPowder` in powder), the plough (1/s per unit of powder) a buried point loses its way
// at, the share of every point's loss taken off the body as a whole (`kPatch`: a back, not a
// point to cartwheel over), the turn the snow scrubs a second once a trunk's weight of him lies on
// it (`kSpinDrag`), and the most of the push out of the snow a point keeps, m/s.
inline constexpr double kSink = 0.15;
inline constexpr double kFrictionPacked = 0.45;
inline constexpr double kFrictionPowder = 0.8;
inline constexpr double kPlough = 1.2;
inline constexpr double kPatch = 0.7;
inline constexpr double kSpinDrag = 3;
inline constexpr double kPushOut = 0.3;
// THE THROW: he leaves with `kKeep` of his velocity and a turn at his speed over `kTumbleRadius`
// m, no faster than `kMaxSpin` rad/s, a sideways fall at least `kTopple` rad/s, with `kCarry` of
// his own turning on top.
inline constexpr double kKeep = 0.85;
inline constexpr double kTumbleRadius = 2;
inline constexpr double kMaxSpin = 6;
inline constexpr double kTopple = 3;
inline constexpr double kCarry = 0.5;
// Lying: still under `kRestSpeed` m/s on the snow; a flight is one off the snow longer than
// `kFlightGap` s (a bounce in a tumble is not).
inline constexpr double kRestSpeed = 0.6;
inline constexpr double kFlightGap = 0.4;
}  // namespace ragdoll

}  // namespace ge::skier::tuning
