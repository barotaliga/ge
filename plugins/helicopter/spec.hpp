// THE HELICOPTER'S NUMBERS: every figure the flight (`heli.hpp`), the bot's hands (`pilot.hpp`)
// and the wash (`wash.hpp`) read, each with its unit, stated once.
//
// THE MACHINE is the light single-engine utility helicopter a heli-ski operation flies: a
// three-bladed main rotor some 10.7 m across turning at about 390 rpm (a tip speed near 220 m/s),
// a two-bladed tail rotor some 1.9 m across at about 2,050 rpm on a slim boom, skid gear on two
// cross tubes, a glazed cabin, and a maximum weight of about 2,250 kg, flown here at a working
// weight of pilot, fuel and kit. A rider sat on a skid is carried on top (`HeliLoad`).
//
// THE FRAME is the world's (`core/math.hpp`): x right, y up, z forward along the nose; the origin
// on the ground under the middle of the skids, THE SKID DATUM, so a machine resting on level snow
// stands at y = ground_at.
//
// ge:summary The helicopter's numbers: the rotor, the power, the drag, the flight, the crash
// ge:keywords helicopter heli spec rotor tail-rotor power momentum-theory ground-effect drag
// ge:keywords flight cyclic collective pedals torque flapback pilot crash skids landing wash
// ge:keywords air-bounds
// ge:skill helicopter
// ge:test tests/helicopter_scenarios_test.cpp
#pragma once

#include "core/math.hpp"

namespace ge::helicopter::spec {

// THE WORKING WEIGHT, kg: about 1,175 kg empty, a pilot, a guide, three quarters of the fuel and
// the ski basket.
inline constexpr double kMass = 1780;
// The centre of gravity's height over the skid datum, m.
inline constexpr double kCog = 1.25;

// THE MAIN ROTOR: radius, m; rpm; the hub's height over the skid datum and its place fore of
// the datum, m.
namespace rotor {
inline constexpr double kRadius = 5.35;
inline constexpr double kRpm = 390;
inline constexpr double kHub = 3.15;
inline constexpr double kAt = 0.15;
inline constexpr double kArea = core::kPi * kRadius * kRadius;
inline constexpr double kOmega = kRpm / 60 * core::kTau;  // rad/s at full rpm
inline constexpr double kTip = kOmega * kRadius;          // m/s at full rpm
}  // namespace rotor

// THE TAIL ROTOR: radius, m; rpm; its hub in the body frame, m (right of the boom's end).
namespace tail {
inline constexpr double kRadius = 0.93;
inline constexpr double kRpm = 2050;
inline constexpr double kX = 0.36, kY = 2.0, kZ = -5.9;
inline constexpr double kOmega = kRpm / 60 * core::kTau;
}  // namespace tail

// THE AIRFRAME, m: the nose's and the fin's reach along z, the cabin's width, and every point the
// crash tests against the snow (body frame): the chin bubble, the ventral fin's foot, the fin's
// top, the boom's belly and the tail rotor's lowest tip.
namespace body {
inline constexpr double kNose = 3.7;
inline constexpr double kTail = -7.2;
inline constexpr double kWidth = 1.86;
struct Point {
  double x, y, z;
};
inline constexpr Point kStrike[] = {
    {0, 1.0, 3.5}, {0, 1.15, -6.75}, {0, 3.37, -7.1}, {0, 1.5, -4.0}, {0.36, 1.07, -5.9},
};
}  // namespace body

// THE SKIDS: the track between them, their fore and aft ends, m.
namespace skid {
inline constexpr double kTrack = 2.2;
inline constexpr double kFront = 1.6;
inline constexpr double kBack = -1.5;
}  // namespace skid

// THE POWER, as momentum theory spends it (`thrust_most`): the shaft power that reaches the
// induced flow, W (the engine's ~630 kW less the profile power and the tail rotor, folded into a
// figure of merit near 0.65: nearly twice its weight at the hover and its weight alone at some
// 18 m/s of climb); the air's density the rotor works in, kg/m^3 (held: there is no ceiling, by
// design); the parasite flat plate, m^2, whose power at speed (rho f V^3 / 2) is taken off what
// the rotor has.
inline constexpr double kPower = 420e3;
inline constexpr double kDensity = 1.0;
inline constexpr double kFlatPlate = 1.4;
// GROUND EFFECT (Cheeseman and Bennett): the thrust at a power times 1 / (1 - (R / 4z)^2), z the
// hub's height over the snow, held to this at the skids.
inline constexpr double kGroundMost = 1.3;

// THE BLADES (blade element theory): the solidity, the lift slope per rad, and the blades' pitch
// at the collective's top stop, rad (18 deg): the most they lift in the air meeting them, so a
// disc moving fast along its own thrust (climbing hard, or turned over and falling) runs out of
// lift once that air takes their angle of attack, by some 45 m/s through it.
namespace blades {
inline constexpr double kSolidity = 0.0535;
inline constexpr double kSlope = 5.7;
inline constexpr double kPitchMost = 0.32;
// rho A (Omega R)^2 sigma a / 2: the thrust per unit of (theta / 3 - lambda / 2) at full rpm, N.
inline constexpr double kLift =
    kDensity * rotor::kArea * rotor::kTip * rotor::kTip * kSolidity * kSlope / 2;
}  // namespace blades

// THE AIR ON THE AIRFRAME: frontal, side and plan areas times their drag coefficients, m^2; and
// the fin turning the nose into the relative wind, 1/s^2 per m/s of side slip (the weathervane
// the pedals hold against).
namespace drag {
inline constexpr double kFront = 1.4;
inline constexpr double kSide = 11;
inline constexpr double kPlan = 14;
inline constexpr double kVane = 0.002;
}  // namespace drag

// THE FLIGHT, BY HAND: the rotor's authority and the airframe's own answer, with nothing of an
// autopilot between them.
// - THE DISC. The cyclic tilts the rotor disc and the thrust goes where it points: `kCyclic*` the
//   pitch and roll acceleration full cyclic gives the disc at full rpm, rad/s^2, against the
//   rotor's damping `kDamping*`, 1/s, so the cyclic sets a rate and, let go, the disc stays where
//   it was left (never levelling itself). `kFlapback` blows the disc back by the air through it,
//   rad/s^2 per m/s: nose up in forward flight, away from a sideways one.
// - THE FUSELAGE hangs under the hub as a damped pendulum (`kHangFrequency`, rad/s, and its
//   damping ratio), off the centre line by the weight of a rider on a skid.
// - THE PEDALS: `kPedal` the yaw acceleration full pedal gives, rad/s^2, against `kYawDamping`,
//   1/s; the main rotor's TORQUE turns the nose left by `kTorque` rad/s^2 for every share of the
//   hover's thrust it pulls over it (and right as it is lowered).
// - THE COLLECTIVE is the thrust's share of what the rotor can give, answered in `kLag` s.
// Nothing stops the disc: held over, it carries on past the vertical and round.
namespace flight {
inline constexpr double kCyclicPitch = 1.6;
inline constexpr double kCyclicRoll = 2.2;
inline constexpr double kDampingPitch = 2.0;
inline constexpr double kDampingRoll = 2.6;
inline constexpr double kFlapback = 0.004;
inline constexpr double kHangFrequency = 4.5;
inline constexpr double kHangDamping = 0.35;
inline constexpr double kPedal = 1.8;
inline constexpr double kYawDamping = 1.5;
inline constexpr double kTorque = 1.2;
inline constexpr double kLag = 0.3;
// The rotor spooled up and down: its share of the full rpm a second.
inline constexpr double kSpool = 0.22;
}  // namespace flight

// THE BOT'S HANDS on the same controls (`pilot.hpp`): the speed it makes for, m/s; how hard it
// plans to slow, m/s^2; the most it tilts the disc for either, rad; the vertical speeds it flies
// at most, m/s (the sink held to `kSinkSlow` near the hover, under the vortex ring's onset); its
// hands' natural frequency on the disc, rad/s, and their damping ratio; its gain on a vertical
// speed missed, 1/s; the height over the snow it flies at with nothing asked, m; the sink it sets
// down at, m/s.
namespace pilot {
inline constexpr double kCruise = 40;
inline constexpr double kBrake = 1.6;
inline constexpr double kTilt = 0.35;
inline constexpr double kClimb = 8;
inline constexpr double kSink = 6;
inline constexpr double kSinkSlow = 2.5;
inline constexpr double kAttitude = 3;
inline constexpr double kDamping = 0.9;
inline constexpr double kHold = 1.5;
inline constexpr double kClear = 45;
inline constexpr double kSettle = 0.8;
}  // namespace pilot

// ON THE SNOW AND INTO IT: the vertical speed the skids take on landing, m/s (the gear is
// certified for 2.5 with a reserve to 3.1) and the speed along the snow, m/s, past either a
// crash; the most the airframe may stand off the snow's own lean, and the steepest snow it can be
// set down on, rad (past it the skid rolls over: a flight manual's slope limits are 6-10 deg, held
// a little wider here for the arcade); the gap the rotor's tips keep from the snow and a crown, m.
namespace crash {
inline constexpr double kSink = 3.2;
inline constexpr double kSlide = 5;
inline constexpr double kTilt = 0.2;
inline constexpr double kSlope = 0.16;
inline constexpr double kClear = 0.15;
}  // namespace crash

// THE AIRBORNE EDGE: inside a band along the map's edge a wind blows the machine back in and a
// hand on its controls turns it toward the middle (`heli_turned_back`); nothing is a wall. The
// band, m, wider by `kTurn` s of the speed it is carried out at; the wind at the edge, m/s,
// growing as the square of the depth into the band; and the heights over the snow it comes on
// over, m (none landing or skimming the snow).
namespace edge {
inline constexpr double kBand = 160;
inline constexpr double kTurn = 6;
inline constexpr double kWind = 16;
inline constexpr double kLow = 40;
inline constexpr double kHigh = 140;
}  // namespace edge

// THE ROTOR'S WASH (`wash_at`): the outwash wall jet along the snow peaks `kCore` rotor radii
// out at `kPeak` times the induced velocity and is gone once the rotor is `kReach` rotor
// diameters up.
namespace wash {
inline constexpr double kCore = 1.7;
inline constexpr double kPeak = 1.9;
inline constexpr double kReach = 2.5;
}  // namespace wash

}  // namespace ge::helicopter::spec
