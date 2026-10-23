// THE PARAMOTOR'S NUMBERS: every figure the flight (`para.hpp`), the air it flies in (`air.hpp`)
// and the bot's hands (`pilot.hpp`) read, each with its unit, stated once.
//
// THE RIG is a MOTORISED SPEED WING flown on skis: a small ram-air wing of the class speed riders
// launch off a summit on (8-18 m2 flat, an aspect ratio near 4, a glide of 3-5, 40-90 km/h) over
// a foot-launch paramotor on the skier's back: a single-cylinder two-stroke of about 185 cc
// swinging a 1.25 m two-bladed propeller in a guard cage, about 75 kgf of static thrust at full
// rpm, the frame, cage and tank about 25 kg. A larger wing would float; this one flies FAST, close
// to the snow, and climbs only on the engine. A paramotor wing proper (22-30 m2, a trim near
// 37 km/h, a minimum sink near 1.1 m/s, a glide near 8) is the slow end of the same polar, and
// these numbers are its fast end.
//
// THE FRAME is the world's (`core/math.hpp`): x right, y up, z forward. The pilot is the skier's
// own body, hung by his harness `kHarness` m over his skis' base; the WING is a point mass the
// lines hang him from.
//
// ge:summary The paramotor's numbers: the wing, the polar, the controls, the engine, the air
// ge:keywords paramotor para spec wing canopy polar lift drag stall brakes speed-bar trimmers
// ge:keywords toggles turn engine thrust propeller launch collapse fold ridge-lift eddies
// ge:keywords air-bounds harness
// ge:skill paramotor
// ge:test tests/paramotor_scenarios_test.cpp
#pragma once

namespace ge::paramotor::spec {

// THE WING: its flat area, m2; its mass with the air held in its cells, kg (the cloth about 3 kg,
// some 4 m3 of air inside and its apparent mass besides); the lines' length from the harness to
// the canopy, m (a speed wing's short lines).
namespace wing {
inline constexpr double kArea = 16;
inline constexpr double kMass = 10;
inline constexpr double kLines = 6;
}  // namespace wing

// The harness's height over the skis' base, m: where the lines meet the pilot.
inline constexpr double kHarness = 1.0;
// The frame, cage and tank on his back, kg.
inline constexpr double kMotorMass = 25;
// The air's density, kg/m3 (a cold day at some 1,500 m).
inline constexpr double kAirDensity = 1.05;
inline constexpr double kGravity = 9.81;

// THE POLAR: the lift slope per radian of attack (aspect ratio near 4); the angle of zero lift
// (its camber); the angle it stalls at and the lift left past it; the zero-lift drag (the cloth,
// the cells' mouths, the lines); the induced drag's factor 1/(pi e A); the drag past the stall;
// the SURGE that throws a stalled canopy forward over the pilot as it fills again, the brakes let
// up; and the most lift coefficient. Trimmed, the wing settles near 8 deg of attack and a lift
// coefficient near 0.6: 55 km/h at the weight.
namespace polar {
inline constexpr double kSlope = 3.2;
inline constexpr double kZero = -0.05;
inline constexpr double kStall = 0.3;
inline constexpr double kStalled = 0.55;
inline constexpr double kDrag0 = 0.045;
inline constexpr double kInduced = 0.133;
inline constexpr double kStallDrag = 0.45;
inline constexpr double kSurge = 0.5;
inline constexpr double kMost = 2;
}  // namespace polar

// THE TRIM: the chord's angle to the plane square to the lines, rad, nose down negative; it sets
// the angle of attack the wing settles at.
inline constexpr double kRig = -0.05;
// THE BRAKES pull the trailing edge down: more trim, more lift and drag, and a stall sooner.
namespace brakes {
inline constexpr double kRig = 0.1;
inline constexpr double kLift = 0.45;
inline constexpr double kDrag = 0.15;
inline constexpr double kStall = 0.08;
}  // namespace brakes
// THE RISERS: the accelerator (the bar pushed, forward) pulls the front risers down, rad of trim;
// the trimmers let out (the bar pulled back) slow it.
namespace bar {
inline constexpr double kAccelerator = -0.06;
inline constexpr double kTrimmers = 0.03;
}  // namespace bar
// THE PILOT'S LEAN IN THE HARNESS, rad at full throw (forward over the bar, back reclined), and
// the share of it the thrust line tilts by: reclined on the throttle he climbs, leant forward he
// drives.
namespace lean {
inline constexpr double kPitch = 0.32;
inline constexpr double kThrust = 0.5;
}  // namespace lean

// THE TURN: the side force a toggle pulled all the way down puts on the canopy (a coefficient on
// the dynamic pressure and the area: the braked tip dragging, the canopy yawing and banking toward
// it) and the drag the braked side adds; the pilot's WEIGHT SHIFT adds to it; THE ENGINE'S TORQUE
// turns the wing left a little at full power; and THE CANOPY RIGHTS ITSELF, its arc swinging it
// back over the pilot as it banks, so a toggle held is a turn of a steady bank and a toggle let go
// levels it.
namespace turn {
inline constexpr double kSide = 0.22;
inline constexpr double kDrag = 0.04;
inline constexpr double kWeight = 0.05;
inline constexpr double kTorque = -0.025;
inline constexpr double kRighting = 0.3;
}  // namespace turn

// THE SWING DAMPED: the canopy's motion about the pilot square to the lines, against the air,
// N per m/s.
inline constexpr double kDamping = 28;
// How fast the toggles, the brakes, the throttle and the bar follow the hands, s.
inline constexpr double kHands = 0.12;

// THE ENGINE: the static thrust at full rpm, N (75 kgf); the speed at which the propeller's
// thrust has fallen to nothing, m/s (its pitch speed); the rpm at idle and full; how fast the rpm
// answers the throttle, s; the thrust line's tilt up off the pilot's way, rad; and how fast a
// stopped motor's propeller runs down, s.
namespace engine {
inline constexpr double kThrust = 735;
inline constexpr double kPitchSpeed = 38;
inline constexpr double kIdle = 2200;
inline constexpr double kFull = 8500;
inline constexpr double kLag = 0.35;
inline constexpr double kTilt = 0.08;
inline constexpr double kSpinDown = 1.2;
}  // namespace engine

// THE LAUNCH: the wing held inflated overhead until the air meets it from ahead at `kRelease`
// m/s, standing `kLean` rad back of plumb. A flight is counted (the pilot hung in the harness)
// after `kFlight` s off the snow.
namespace launch {
inline constexpr double kRelease = 8;
inline constexpr double kLean = 0.2;
inline constexpr double kFlight = 0.6;
}  // namespace launch

// THE WING BROUGHT DOWN: its lowest point within `kGround` m of the snow, or its cloth within
// `kCrown` m of a crown's edge under the tree's top, and it COLLAPSES: the rig is cut away. The
// canopy's lowest point hangs `kBelly` m under its centre.
namespace down {
inline constexpr double kGround = 0.4;
inline constexpr double kBelly = 0.5;
inline constexpr double kCrown = 1.5;
}  // namespace down

// THE GEAR DROPPED (the release press): the canopy's drag area streaming as cloth, m2, and the
// motor's; the shove the motor leaves his back with, m/s back off it, and up.
namespace dropped {
inline constexpr double kCanopyDrag = 6;
inline constexpr double kMotorDrag = 0.3;
inline constexpr double kShove = 2.5;
inline constexpr double kLift = 0.8;
}  // namespace dropped

// NEAR THE SNOW the pilot stands up out of the seat and his skis turn to his way over it, to
// meet the snow square: from `kStand` m over it, fully by `kSquare` m. Aloft he turns to the
// wing's heading over `kSwing` s.
namespace flare {
inline constexpr double kStand = 6;
inline constexpr double kSquare = 1.5;
inline constexpr double kSwing = 0.3;
}  // namespace flare

// THE AIR THE WING FLIES IN (`air.hpp`). The 10 m wind grows up the log law (`kRoughness` m, the
// snow's) to `kTop` m over the snow and is the same above; the mountain exposes it from `kValley`
// of itself on the valley floor to `kSummit` at the top. RIDGE LIFT: the flow rises along the
// slope it meets at the wind's speed times the slope, read over a spur's flank and over the whole
// face (`kSpan` m either side), each `kShare` of it, dying out over the ground in `kDepth` m, no
// slope steering it steeper than `kSteepest`. Down a slope the flow separates: `kSeparate` of the
// sink is kept smooth and the rest is the lee's rotor, a full lee where the air falls `kLeeSlope`
// of its speed. THE EDDIES: their sigma as shares of the mean wind, in the open air, near the
// snow (dying out in `kGroundDepth` m) and in the lee, times `kStorm` under a storm; three octaves
// (`kWavelengths`, m) of two waves each, churning at `kChurn` m/s a radian of the wave as they are
// carried, their up-and-down pressed flat under `kFlat` m.
namespace air {
inline constexpr double kRoughness = 0.001;
inline constexpr double kRef = 10;
inline constexpr double kTop = 300;
inline constexpr double kValley = 0.7;
inline constexpr double kSummit = 1.3;
struct Ridge {
  double span, share, depth;
};
inline constexpr Ridge kRidges[] = {{25, 0.45, 60}, {140, 0.55, 260}};
inline constexpr double kSteepest = 0.7;
inline constexpr double kSeparate = 0.55;
inline constexpr double kLeeSlope = 0.25;
inline constexpr double kOpen = 0.09;
inline constexpr double kGround = 0.12;
inline constexpr double kGroundDepth = 35;
inline constexpr double kLee = 0.3;
inline constexpr double kStorm = 1.35;
inline constexpr double kWavelengths[] = {18, 55, 160};
inline constexpr double kChurn = 1.5;
inline constexpr double kFlat = 12;
}  // namespace air

// A FOLD: rough air pushing the wing under `kAlpha` rad of attack tucks its leading edge under (a
// FRONTAL), or one tip alone (an ASYMMETRIC), the canopy turning toward the folded side. The span
// the tips stand apart, m; how deep a fold goes per radian under the angle (at least `kLeast`, at
// most `kMost`); the lift a fold takes off (a frontal all of its share, a side fold `kSideLift` of
// it), the drag it adds, the turn toward the folded side; the refill, by itself in `kRefill` s for
// the whole fold, faster with the brakes pumped (`kPump`), slowly under `kSlow` m/s of air; and
// the roll the tips' difference throws on the canopy, per radian of it.
namespace fold {
inline constexpr double kAlpha = 0.0;
inline constexpr double kSpan = 6.4;
inline constexpr double kDeep = 6;
inline constexpr double kLeast = 0.35;
inline constexpr double kMost = 0.95;
inline constexpr double kLift = 0.85;
inline constexpr double kSideLift = 0.45;
inline constexpr double kDrag = 0.2;
inline constexpr double kTurn = 0.4;
inline constexpr double kRefill = 1.6;
inline constexpr double kPump = 1.5;
inline constexpr double kSlow = 8;
inline constexpr double kRoll = 0.6;
}  // namespace fold

// THE AIRBORNE EDGE: inside a band along the map's edge a wind blows the wing back in and the
// toggles turn toward the middle, by how deep into the band it is; nothing is a wall. The band,
// m; the wind at the edge, m/s, growing as the square of the depth; the heights over the snow it
// comes on over, m.
namespace edge {
inline constexpr double kBand = 160;
inline constexpr double kWind = 12;
inline constexpr double kLow = 30;
inline constexpr double kHigh = 120;
}  // namespace edge

}  // namespace ge::paramotor::spec
