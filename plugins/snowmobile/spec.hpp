// THE SNOWMOBILE'S NUMBERS: a deep-snow MOUNTAIN SLED, the class built to climb, every number
// with its unit. The class's published proportions are kept as the BAND they sit in, and the
// machine here is that band's middle:
//   - an 850 cc liquid-cooled two-stroke twin, 165 hp (123 kW) at about 8,000 rpm, the drive
//     clutch engaging near 4,000 and idling round 1,500, the limiter a few hundred over the peak;
//   - about 200 kg dry (the class is 195-212 kg) and some 30 kg of fuel;
//   - a 165-inch belt (4.19 m) 16 in (0.41 m) wide of 3-inch (76 mm) paddles, unstudded;
//   - a narrow 36-inch (0.91 m) ski stance, so it can be rolled onto its side and steered with
//     the body;
//   - 9 in (0.23 m) of travel in front and 15 in (0.38 m) behind, a tall riser for a standing
//     rider, open running boards;
//   - some 3.3 m long, 1.1 m wide and 1.3 m tall, topping out near 140 km/h on the flat.
//
// THE BODY FRAME is the world's conventions (core/quat.hpp): x the machine's right, y up, z
// forward, the origin at the centre of gravity of the sled AND its rider together. The snow
// under it is the skier's (`skier/snow.hpp`, `skier/tuning.hpp`), read at the belt's and the
// sled skis' own scale (`snow` below).
//
// ge:summary The snowmobile's numbers: the class, the probes, the grip, the drive, the crash
// ge:keywords snowmobile sled spec numbers tuning probes suspension belt tread paddles cvt
// ge:keywords engine two-stroke grip carve roll tip air hull crash inertia mass
// ge:skill snowmobile
// ge:test tests/snowmobile_scenarios_test.cpp
#pragma once

#include <array>
#include <cstdint>

#include "core/quat.hpp"

namespace ge::snowmobile::spec {

// Standard gravity, m/s^2, and the air's density, kg/m^3 (the skier's: -8 C, 2000 m up).
inline constexpr double kGravity = 9.81;
inline constexpr double kAirDensity = 1.05;

// The machine dry and its fuel, kg; the rider is whoever stands on it (`sled_mass`).
inline constexpr double kDryMass = 200;
inline constexpr double kFuel = 30;
// The envelope, m: bumper to the tunnel's end, ski tip to ski tip, snow to the riser's top.
inline constexpr double kLength = 3.3;
inline constexpr double kWidth = 1.08;
inline constexpr double kHeight = 1.3;
// The centre of gravity over the snow at rest, sled and rider, m: a standing rider carries it
// high.
inline constexpr double kCogHeight = 0.66;
// THE SKIS: their stance centre to centre, how far ahead of the CoG their contact centre stands,
// their running width, m.
inline constexpr double kSkiStance = 0.91;
inline constexpr double kSkiForward = 1.09;
inline constexpr double kSkiWidth = 0.17;
// THE BELT on the snow: its width, and its run from `kTreadFront` ahead of the CoG to
// `kTreadRear` behind it, m.
inline constexpr double kTreadWidth = 0.41;
inline constexpr double kTreadFront = 0.17;
inline constexpr double kTreadRear = -1.36;

// THE SUSPENSION, per probe: spring rate N/m, damping N.s/m compressing and extending, travel m.
// About a third of the weight rides on the skis, each strut sags a third of its travel, a little
// under 2 Hz in heave at half critical damping: a machine that settles in one bob.
struct Suspension {
  double rate, bump, rebound, travel;
};
inline constexpr Suspension kFront{7000, 400, 880, 0.23};
inline constexpr Suspension kRear{2700, 210, 440, 0.38};
// The bump stop's rate and damping as multiples of the spring's own, the share of it a stop
// gives back on the way out (a bumper, not a spring), and the most a probe may push as a
// multiple of its rest load (the fuse on a strut bottomed on a steep face).
inline constexpr double kStopRate = 12;
inline constexpr double kStopDamp = 4;
inline constexpr double kStopRelease = 0.2;
inline constexpr double kMaxLoad = 15;
// Bottoming control: the compression damping rises over the last share of the stroke, to
// (1 + this) times its own at the end.
inline constexpr double kBottomZone = 0.3;
inline constexpr double kBottomDamp = 2;
// The body's down axis must point at least this far down for a probe to be read (a sled on its
// side has no suspension), and the least cosine between a strut and the snow's normal its load
// is resolved through.
inline constexpr double kProbeMinDown = 0.25;
inline constexpr double kTiltMin = 0.5;
// The fastest the body may turn about any axis, rad/s.
inline constexpr double kMaxSpin = 25;

// THE ENGINE: peak power kW at `kPeakRpm`; the limiter, the idle and the clutch's engagement,
// rpm; the power curve's shape under the peak (a two-stroke's: little down low, a rush onto the
// pipe); engine braking shut, N per m/s of belt (a two-stroke freewheels).
inline constexpr double kPowerKw = 123;
inline constexpr double kPeakRpm = 8000;
inline constexpr double kMaxRpm = 8300;
inline constexpr double kIdleRpm = 1500;
inline constexpr double kEngageRpm = 4000;
inline constexpr double kCurve = 2;
inline constexpr double kEngineBrake = 45;
// THE CVT: the belt's speed at the redline in its top ratio, m/s, and the ratio's span to its
// lowest (geared short, for the climb); the share of the crank's power that reaches the belt.
inline constexpr double kGearTop = 42;
inline constexpr double kGearSpan = 3.8;
inline constexpr double kDriveline = 0.8;
// THE BELT as a mass of its own, kg (the belt, the drivers and the driven clutch); its rails'
// and idlers' losses, N per (m/s)^2 and per m/s; the throttle's and the rpm's lags, 1/s; the
// belt speed the drive's force is divided by at the least, m/s (the clutch slipping off the
// line); the brake's most on the belt, N; and the thumb on the brake holding the belt just short
// of lock, turning at the way less this slip, m/s (a locked belt holds nothing sideways).
inline constexpr double kBeltMass = 26;
inline constexpr double kBeltLossQuad = 0.34;
inline constexpr double kBeltLossLin = 6;
inline constexpr double kThrottleRate = 8;
inline constexpr double kRpmRate = 9;
inline constexpr double kLaunchFloor = 2.5;
inline constexpr double kBrakeForce = 3200;
inline constexpr double kBrakeSlip = 2.8;
// Drag area, sled and standing rider nose on, m^2.
inline constexpr double kCdA = 1.0;

// THE RIDER on the boards: how far his weight moves across, m (a mountain rider hangs off the
// side to hold a sidehill), and fore and aft, m; the lag his body follows the bars and the lean
// with, s.
inline constexpr double kRiderReach = 0.42;
inline constexpr double kAftReach = 0.35;
inline constexpr double kRiderLag = 0.16;

// DOCUMENTED EXPECTATIONS, held by tests/snowmobile_scenarios_test.cpp and never inputs: km/h
// flat out on packed snow.
inline constexpr double kTopSpeedKmh = 140;

// THE GRIP, as coefficients on a probe's load: the belt driving and holding sideways on the
// groomer (rubber and folded paddles) and in powder (the 3-inch paddles bite); the skis' keels
// sideways. The belt's shear saturates with its slip (`kSlipRef` m/s of belt over the snow,
// Janosi and Hanamoto's shear in velocity form) and its slide (`kSideRef` m/s across).
namespace grip {
inline constexpr double kTreadPacked = 0.72;
inline constexpr double kTreadPowder = 0.85;
inline constexpr double kSidePacked = 0.8;
inline constexpr double kSidePowder = 0.5;
inline constexpr double kSkiPacked = 0.85;
inline constexpr double kSkiPowder = 0.35;
// THE SNOW'S COHESION the paddles shear over the belt's footprint, N in all: c.A in Janosi and
// Hanamoto's thrust, some 0.75 kPa of settled powder over the 0.6 m^2 the belt lays down. In
// powder alone: on the groomer the paddles fold.
inline constexpr double kCohesion = 450;
inline constexpr double kSlipRef = 1.6;
inline constexpr double kSideRef = 0.5;
// A belt held still by the brake bites at this share of `kSlipRef`: its paddles stand in the snow
// they sit in and hold like a static grip, so a machine left on its parking brake stays on a slope
// rather than creeping down it at the slip its shear needs.
inline constexpr double kLockedRef = 0.02;
// THE SIDEWAYS HOLD stops growing past this multiple of a probe's rest load: under a landing's
// load the crust shears and the machine slides, rather than tripping over its low side.
inline constexpr double kSideLoad = 2.5;
}  // namespace grip

// THE SNOW under it as multiples of a skier's (`skier/snow.hpp`): a belt at 4-5 kPa sinks
// further at rest than a pair of skis and planes later; a sled ski a little less; and how much
// of the powder's compaction drag the belt pays, a multiple of a ski's. The plough is the snow a
// sunk footprint shoves aside, N per metre of width per metre of sink per (m/s)^2.
namespace snow {
inline constexpr double kTreadSink = 1.35;
inline constexpr double kSkiSink = 0.9;
inline constexpr double kPlane = 1.7;
inline constexpr double kDrag = 2.5;
inline constexpr double kPlough = 70;
// Below this speed along its line a probe's resistance fades out, m/s.
inline constexpr double kDragFade = 0.3;
}  // namespace snow

// STEERING: the full ski lock at a standstill, rad (a mountain machine's ~32 deg), halved by
// `kFadeSpeed` m/s; how fast the skis swing, rad/s; the ski-to-tread base, m; and THE ARCADE'S
// HAND on the yaw, which models nothing and says so: the yaw rate held toward the one the skis
// ask, no faster than `kPathShare` of the grip turns the way (N.m per rad/s), and the nose held to
// the way it goes past `kSlipFrom` m/s (N.m per rad of slide), both capped; in powder the grip
// it may turn on is `kReachPowder` g, a sled rolled onto its edge and carved; and the scrub a
// turn on the groomer costs, a share of the turn's own pull.
namespace steer {
inline constexpr double kSkiLock = 0.56;
inline constexpr double kFadeSpeed = 20;
inline constexpr double kRate = 2.8;
inline constexpr double kBase = 1.75;
inline constexpr double kYawHold = 10000;
inline constexpr double kSlipHold = 1200;
inline constexpr double kYawHoldMax = 6000;
inline constexpr double kPathShare = 1;
inline constexpr double kReachPowder = 0.7;
inline constexpr double kSlipFrom = 3;
inline constexpr double kScrub = 0.15;
}  // namespace steer

// THE LEAN INTO A TURN, and RIDE IT LIKE A BIKE in powder: the roll the chassis settles at with
// the bars over (small on the groomer, in powder the whole of how a mountain sled turns), taken
// there no faster than `kLeanRate` rad/s; the righting the rider and the springs hold it with,
// N.m per rad, its damping past the lean's own rate, N.m.s, and the most of each, N.m; THE CARVE,
// a share of the belt's load per radian of roll pulling a sled on its edge toward its low side
// once it has `kCarveSpeed` m/s on; in loose snow the hold the buried skis lose, the SOFT SIDE
// GIVING as a share of the weight at the CoG per radian and the share left planing on top, and
// the rider's BALANCE against that give, N.m (`kBalanceCrawl` of it at a standstill, all of it
// from `kBalanceSpeed` m/s); `kLoose` the least loose share untouched powder is read at. THE TIP:
// past the angle its CoG passes over the low edge of what it stands on (the skis' 0.91 m stance
// on firm snow, about 35 deg; the 0.41 m belt as the low ski sinks, about 17 deg) and `kHang`
// rad more for the rider hung off the high side, the righting fades out over `kTipBand` rad.
namespace roll {
inline constexpr double kPacked = 0.1;
inline constexpr double kPowder = 0.5;
inline constexpr double kStiff = 7000;
inline constexpr double kDamp = 1200;
inline constexpr double kMost = 2700;
inline constexpr double kCarve = 1.6;
inline constexpr double kCarveSpeed = 5;
inline constexpr double kDeepHold = 0.75;
inline constexpr double kDeepTip = 2.2;
inline constexpr double kDeepPlaning = 0.5;
inline constexpr double kLoose = 0.35;
inline constexpr double kLeanRate = 1;
inline constexpr double kHang = 0.35;
inline constexpr double kTipBand = 0.35;
inline constexpr double kBalance = 3000;
inline constexpr double kBalanceCrawl = 0.25;
inline constexpr double kBalanceSpeed = 8;
}  // namespace roll

// THE AIR: the lean's pitch at full lean, N.m (back is nose up); the belt's gyro, the throttle
// lifting the nose and the brake dropping it, N.m; the bars' yaw, N.m; the damping, N.m.s; the
// rider levelling the roll, N.m per rad, its damping and the most his body lends, N.m (a lip
// tipped a little is levelled, one tipped hard comes down on its side); THE NOSE SET FOR THE
// LANDING (the arcade's hand, `assist`): toward the pitch of the snow it will come down on,
// `kSetNose` rad high, N.m per rad, N.m.s and the most it lends, N.m.
namespace air {
inline constexpr double kLean = 520;
inline constexpr double kThrottle = 110;
inline constexpr double kBrake = 460;
inline constexpr double kSteer = 90;
inline constexpr double kDamping = 60;
inline constexpr double kRollLevel = 1600;
inline constexpr double kRollDamp = 300;
inline constexpr double kRollMost = 300;
inline constexpr double kSetStiff = 5300;
inline constexpr double kSetDamp = 2400;
inline constexpr double kSetMost = 3000;
inline constexpr double kSetNose = 0.06;
// A flight shorter than this is a skip over the snow's grain, not a jump, s.
inline constexpr double kCounts = 0.25;
}  // namespace air

// THE HULL against the snow when the springs run out: its restitution and the friction it slides
// on (plastic on snow); and its points, body frame, m: the nose, the belly, the tunnel's corners,
// the riser.
inline constexpr double kHullBounce = 0.1;
inline constexpr double kHullFriction = 0.35;
inline constexpr std::array<core::Vec3, 10> kHull{{
    {-0.32, -0.3, 1.75},
    {0.32, -0.3, 1.75},
    {0, -0.15, 2.05},
    {-0.25, -0.38, -1.5},
    {0.25, -0.38, -1.5},
    {-0.45, 0.15, 0.6},
    {0.45, 0.15, 0.6},
    {-0.3, 0.55, 0.95},
    {0.3, 0.55, 0.95},
    {0, 0.65, -0.4},
}};

// WHAT PUTS THE RIDER OFF, and nothing else does on snow: a trunk met harder than `kTree` m/s;
// the machine past `kOver` rad of roll or pitch off the snow for `kOverFor` s (rolled, or
// looped); a landing into the snow harder than `kLanding` m/s on the groomer, up to
// `kLandingPowder` m/s in untouched powder (drops of some 11 and 18 m), or set down `kTilt` rad
// off its belt any harder than `kTiltFrom` m/s; and the way it was going stopped `kWall` m/s in a
// step (a bank or a cliff band ridden into). A trunk met slower than `kHitSay` m/s is a nudge.
namespace crash {
inline constexpr double kTree = 7;
inline constexpr double kOver = 1.25;
inline constexpr double kOverFor = 0.25;
inline constexpr double kLanding = 15;
inline constexpr double kLandingPowder = 19;
inline constexpr double kTilt = 0.7;
inline constexpr double kTiltFrom = 4;
inline constexpr double kWall = 8;
inline constexpr double kHitSay = 2.5;
}  // namespace crash
// The half width of the machine's plan a trunk is held out of, m (its half length is
// `kLength / 2`).
inline constexpr double kHalfWidth = 0.5;
// Where the rider's feet stand on the boards, body frame, m: the boards' middle.
inline constexpr core::Vec3 kBoards{0.335, -0.315, 0.12};

// ONE PROBE: a ski each side, three stations down each edge of the belt. Where its strut hangs at
// full extension, body frame, m; its suspension; the share of the whole weight it carries at
// rest; how wide it ploughs, m.
enum class Station : uint8_t { ski, front, mid, rear };
struct Probe {
  bool tread;
  Station station;
  int32_t side;  // -1 left, +1 right
  double bx, by, bz;
  Suspension susp;
  double share;
  double width;
};
inline constexpr int32_t kProbes = 8;

// THE PROBES. The rest shares come off the moment balance of the ski line and the belt's centroid
// about the CoG; each strut hangs so it sits at a third of its travel under its share.
constexpr std::array<Probe, kProbes> make_probes() {
  const double tread_mid = (kTreadFront + kTreadRear) / 2;
  const double front = -tread_mid / (kSkiForward - tread_mid);
  std::array<Probe, kProbes> p{};
  int32_t i = 0;
  for (const int32_t side : {-1, 1}) {
    p[static_cast<size_t>(i++)] = Probe{false,
                                        Station::ski,
                                        side,
                                        side * kSkiStance / 2,
                                        -kCogHeight + kFront.travel * (2.0 / 3.0),
                                        kSkiForward,
                                        kFront,
                                        front / 2,
                                        kSkiWidth};
  }
  const Station stations[] = {Station::front, Station::mid, Station::rear};
  const double zs[] = {kTreadFront, tread_mid, kTreadRear};
  for (int32_t k = 0; k < 3; k++) {
    for (const int32_t side : {-1, 1}) {
      p[static_cast<size_t>(i++)] = Probe{true,
                                          stations[k],
                                          side,
                                          side * kTreadWidth * 0.4,
                                          -kCogHeight + kRear.travel * (2.0 / 3.0),
                                          zs[k],
                                          kRear,
                                          (1 - front) / 6,
                                          kTreadWidth / 2};
    }
  }
  return p;
}
inline constexpr std::array<Probe, kProbes> kProbeTable = make_probes();

}  // namespace ge::snowmobile::spec

namespace ge::snowmobile {

// The sled and a rider of `rider` kg together, kg.
inline constexpr double sled_mass(double rider) { return spec::kDryMass + spec::kFuel + rider; }

// The principal moments of inertia about the body's right (pitch), up (yaw) and forward (roll)
// axes, kg.m^2: a solid box of the envelope.
inline constexpr core::Vec3 sled_inertia(double m) {
  constexpr double L = spec::kLength;
  constexpr double W = spec::kWidth;
  constexpr double H = spec::kHeight;
  return {m * (L * L + H * H) / 12, m * (L * L + W * W) / 12, m * (W * W + H * H) / 12};
}

}  // namespace ge::snowmobile
