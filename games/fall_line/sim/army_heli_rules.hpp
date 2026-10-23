// THE ARMY'S HELICOPTERS' NUMBERS (army_heli.hpp flies them, army_land.hpp finds where they set
// down): the machines the wanted level's roster asks for, how they hunt a man, and how they put
// a section down near him. Every number carries its unit.
//
// THE MACHINE is the medium twin-engine utility helicopter armies fly troops in: a four-bladed
// rotor some 14 m across, a cruise of about 225 km/h, a crew of two, a section in the back and a
// sliding door on each side. It is flown as a point with a pilot's limits (an acceleration no
// greater than a hard turn's bank gives, the attitude read off it), not on the free ride's rotor
// physics: that is one light machine's, stepped for the man who flies it.
//
// HOW IT HUNTS is the door gunner's geometry: a PYLON TURN round the man, a steady circle of some
// 220 m at 70 m over him, the inward door looking down at him. A quarry that runs too fast to
// circle is held from ECHELON, behind and to the side. Out of sight they SEARCH a wide circle
// round where he was last seen, higher and slower.
//
// HOW IT PUTS MEN DOWN is the air assault's: set down on ground that leans no more than a flight
// manual allows (some 7 degrees), the disc and a margin clear of trunks, lifts and tunnels; the
// men out of both doors in turn, run out to their side and kneel facing out until it has lifted.
// Where nothing in reach will do, it hovers and they come down a FAST ROPE. A section left far
// behind is picked up and put down again ahead of him: the leapfrog.
//
// ge:summary The army's helicopters' numbers: the call, the flight, the pylon turn, the landing
// ge:keywords army helicopter army-heli rules cruise pylon-turn orbit echelon search landing
// ge:keywords fast-rope dismount leapfrog section overwatch separation leave
// ge:skill sim
// ge:test tests/fall_line_army_heli_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "fall_line/sim/wanted_rules.hpp"

namespace fall_line::sim::army_heli {

// THE MACHINE, m: the main rotor's radius, the cabin's half width (where a man comes out of a
// door), the doors' middle fore of the skid datum, and the fast rope's hook out from the middle
// and over the datum.
inline constexpr double kRotorM = 7.01;
inline constexpr double kHalfWidthM = 1.22;
inline constexpr double kDoorM = 0.35;
inline constexpr double kHookOutM = 2.1;
inline constexpr double kHookUpM = 2.55;

// THE CALL: s from the roster asking for a machine to it coming in at the map's edge (the duty
// crew's scramble), and s between one machine and the next called with it; m inside the map's
// edge it comes in at, the bearing between machines called together, rad (each the next either
// side of the first), and its height over the snow coming in, m.
inline constexpr double kRespondS = 10;
inline constexpr double kStaggerS = 7;
inline constexpr double kInsetM = 60;
inline constexpr double kSpread = 0.5;
inline constexpr double kEnterM = 150;

// THE FLIGHT: the transit speed and the dash it makes to hold station on a man who runs, m/s;
// the most the disc is leant to accelerate, rad (a hard turn's bank: g tan of it the most it
// accelerates or turns on); the time its acceleration and attitude take to follow, s; the climb
// and sink, m/s, and the most vertical acceleration, m/s^2; the nose's fastest turn, rad/s; how
// far the nose hangs down per m/s of airspeed, rad; and how hard it closes on its place in
// echelon, 1/s.
inline constexpr double kCruise = 62;
inline constexpr double kDash = 72;
inline constexpr double kTilt = 0.6;
inline constexpr double kLagS = 0.6;
inline constexpr double kClimb = 10;
inline constexpr double kSink = 12;
inline constexpr double kHeave = 4;
inline constexpr double kYaw = 0.6;
inline constexpr double kNose = 0.0017;
inline constexpr double kGain = 0.3;
inline constexpr double kGravity = 9.81;

// THE GROUND AND THE WOODS: the least height of the skid datum over the snow it reads ahead
// while it flies, m; how far ahead it reads, s of its way, sampled every `kLookStepM`; and the
// hard floor over the snow under the disc (and the crowns it sweeps, `kCrownM` over their tops)
// it is never let under, m.
inline constexpr double kClearM = 30;
inline constexpr double kLookS = 7;
inline constexpr double kLookStepM = 20;
inline constexpr double kFloorM = 12;
inline constexpr double kCrownM = 4;

// THE PYLON TURN on a slow man: the circle's radius, m; the speed flown round it, m/s; the height
// over him, m; how hard it closes on the radius, 1/s.
inline constexpr double kOrbitM = 220;
inline constexpr double kOrbitSpeed = 30;
inline constexpr double kOrbitUpM = 70;
inline constexpr double kClose = 0.3;
// THE ECHELON on a fast one: m behind him, m to the side, m over him; and his speed over which it
// flies it rather than the circle, m/s (the circle let go from 60% of it).
inline constexpr double kBackM = 140;
inline constexpr double kAsideM = 90;
inline constexpr double kAboveM = 25;
inline constexpr double kFast = 24;
// THE SEARCH out of sight: the circle round where he was last seen, its share of the stars'
// search radius, flown at `kSearchSpeed` m/s, `kSearchUpM` up.
inline constexpr std::array<double, wanted::kMaxStars + 1> kSearchRadiusM{0,   300, 450,
                                                                          600, 800, 1000};
inline constexpr double kSearchShare = 0.5;
inline constexpr double kSearchSpeed = 35;
inline constexpr double kSearchUpM = 110;
// Two machines are kept this far apart, m: each pushed off the other's station.
inline constexpr double kApartM = 120;
// LEAVING: the height over the snow it climbs to, m, and how far past the map's edge it is gone.
inline constexpr double kLeaveUpM = 180;
inline constexpr double kPastM = 150;

// THE SECTION a machine carries, men.
inline constexpr int32_t kSection = 8;

// THE LANDING (army_land.hpp): the ring round him a place is looked for in, m (nearer, the men
// land on top of him; further, they are out of the fight), the distance preferred, m, and how much
// a place AHEAD of him (where he is going) is preferred over one behind, m of cost; the search's
// grid, m; the most the ground may lean over the disc's circle (rise over run, some 7 degrees, a
// flight manual's limit); the room clear past the disc, m; and how far he may be from a place
// before a man is out on it, m, before it is given up and a nearer one asked for.
inline constexpr double kNearM = 60;
inline constexpr double kFarM = 280;
inline constexpr double kPreferM = 120;
inline constexpr double kAheadCost = 60;
inline constexpr double kGridM = 8;
inline constexpr double kSlope = 0.12;
inline constexpr double kRoomM = 4;
inline constexpr double kStaleM = 450;
// Landings asked for are kept this far from one another, and from the free ride's helicopter, m.
inline constexpr double kAvoidM = 40;
inline constexpr double kAvoidHeliM = 30;
// A drop is asked for only within this of him, m.
inline constexpr double kAskWithinM = 1500;
// THE FAST ROPE where it cannot land: the snow clear of trunks this far round the man's drop, m;
// the hover it is slid from over the snow, m (never under the flight's floor over the woods), and
// the longest rope, m; a man slides at `kSlide` m/s, the next `kRopeGapS` after.
inline constexpr double kRopeClearM = 5;
inline constexpr double kRopeHoverM = 13;
inline constexpr double kRopeReachM = 22;
inline constexpr double kSlide = 4;
inline constexpr double kRopeGapS = 1.2;
// THE APPROACH: braked to the spot at no more than `kFlare` m/s^2 down to `kSlow` m/s; held
// `kOverM` above it until within `kWithinM` and slower than `kSettle` m/s, then sunk at
// `kSettleSink` m/s; `kLiftClimb` m/s back off it.
inline constexpr double kFlare = 4;
inline constexpr double kSlow = 2;
inline constexpr double kOverM = 20;
inline constexpr double kWithinM = 6;
inline constexpr double kSettle = 3;
inline constexpr double kSettleSink = 4;
inline constexpr double kLiftClimb = 6;
// From where it faces the spot over the last stretch (the heading it is set down on), m.
inline constexpr double kFaceM = 30;
// OUT OF THE DOORS: a man every `kEveryS` s, each door in turn, run `kRunM` out to his side
// (`kRopeRunM` off the rope's foot), spread `kAlongM` fore and aft; knelt facing out until it has
// lifted. It waits `kLingerS` after the last is clear, and gives up a landing after `kWaitS`.
inline constexpr double kEveryS = 0.7;
inline constexpr double kRunM = 9;
inline constexpr double kRopeRunM = 3;
inline constexpr double kAlongM = 3;
inline constexpr double kLingerS = 1.5;
inline constexpr double kWaitS = 45;
// THE LEAPFROG: a squad whose every man is further than `kRecallM` from where he was last seen,
// for `kAfterS`, is picked up (a landing `kPickNearM` to `kPickFarM` from them, `kPickPreferM`
// preferred) and put down again ahead of him. A man boarding gets in within `kBoardM` of his
// door, and waits `kStandByM` off the spot until it has landed.
inline constexpr double kRecallM = 350;
inline constexpr double kAfterS = 15;
inline constexpr double kPickNearM = 10;
inline constexpr double kPickFarM = 160;
inline constexpr double kPickPreferM = 30;
inline constexpr double kBoardM = 3;
inline constexpr double kStandByM = 20;
// Whether the man is on the ground (a landing waits for it): within this of the snow, m.
inline constexpr double kOnGroundM = 4;

// THE PURSUERS: a machine's crew look from this far over its skid datum, m, and it is pursuer
// `kPursuerBase` + its id.
inline constexpr double kEyeM = 2;
inline constexpr int32_t kPursuerBase = 900;
// Its decisions are recorded (`GameState::why`) as agent `kAgentBase` + its id.
inline constexpr int32_t kAgentBase = 1000;

// The most machines out at once (the roster's top row).
inline constexpr size_t kMostHelis = 4;

}  // namespace fall_line::sim::army_heli
