// THE GROUND WAVES' VEHICLES' NUMBERS (army_vehicle.hpp drives them, army_drive.hpp reads the snow
// ahead of them). Every number carries its unit.
//
// WHAT EACH IS, by the band the service vehicles of its class sit in:
// - THE TRACKED CARRIER: the articulated over-snow carrier northern armies move troops in. Two
//   short units on four wide rubber band tracks: a cab with the engine in front, a troop body
//   behind, joined by a steered hitch. Some 6.8-7.6 m long overall, 1.9-2.2 m wide, 2.2-2.4 m
//   tall, 4.5 t unarmoured; about 50 km/h on a road and half that in deep snow, and on tracks that
//   press the snow about as hard as a man on skis it climbs up to 100 % on firm snow. The front
//   unit seats the driver and three or four, the rear a section of eight to eleven.
// - THE TRUCK: a cross-country cargo truck, six wheels driven, the cab forward, a canvas tilt over
//   the bed, chains on in winter. Some 8 m long, 2.5 m wide, 3.4 m to the tilt, about 15 t laden;
//   85 km/h on a road, and some 30 % on packed snow before its chains spin. Its bed seats two squads
//   facing in.
// - THE TANK: a main battle tank. A 7.5-8 m hull, 3.6-3.8 m wide, 2.4-3 m to the turret's roof,
//   55-65 t; about 70 km/h on a road and 40-50 off it; 60 % grades. Its turret turns a full circle
//   in about nine seconds (its main gun is a later row: here it drives, stands off and runs down).
// Each is driven at a mountain's pace, well under its road speed, and slowed among the trees.
//
// ge:summary The army's ground vehicles' numbers: the kinds, the call, the drive, the drop, stuck
// ge:keywords army vehicle carrier truck tank rules hitch climb cliff creep cruise woods drop
// ge:keywords unload stand-off ram strike stuck home pursuers
// ge:skill sim
// ge:test tests/fall_line_army_vehicle_test.cpp
#pragma once

#include <array>
#include <cstdint>

#include "fall_line/sim/wanted_rules.hpp"

namespace fall_line::sim::army_vehicle {

// ONE KIND'S BODY AND DRIVE.
// - The plan as a box, m: half its length (the front unit's, for the carrier), half its width, and
//   its top over the snow. Its mass laden, kg.
// - THE DRIVE: the pace over open snow and among trees, m/s; how hard it gathers and sheds pace,
//   m/s²; its fastest turn, rad/s; and the tightest turn its wheels allow, m (0: it pivots on its
//   tracks).
// - THE GROUND IT TAKES: the steepest rise it climbs and drop it goes down (slopes), and the share
//   of its pace it keeps up a rise at its limit (it slows to it in proportion to the rise).
// - Its crew's eyes over the snow (the commander in his hatch), m; the men it carries, and its crew.
struct Spec {
  double half_length;
  double half_width;
  double height;
  double mass;
  double cruise;
  double woods;
  double accel;
  double brake;
  double turn;
  double radius;
  double climb;
  double cliff;
  double creep;
  double eyes;
  int32_t seats;
  int32_t crew;
};

inline constexpr Spec kCarrier{1.75, 0.95, 2.3, 5500, 9,   4,   1.6, 4, 0.7,
                               0,    0.8,  0.9, 0.35, 2.5, 8, 2};
inline constexpr Spec kTruck{4, 1.25, 3.3, 15000, 11, 4, 1.4, 4, 0.45, 11, 0.3, 0.4, 0.4, 2.6, 12, 2};
inline constexpr Spec kTank{3.9, 1.85, 2.6, 60000, 9, 3.5, 1.2, 5, 0.45, 0, 0.6, 0.7, 0.3, 3, 0, 3};

// THE CARRIER'S REAR UNIT: its middle `kHitchM` behind the front's to the hitch and `kBackM` back
// from it; it trails the front about the hitch as a trailer does (its heading turns toward the
// front's at speed over the hitch's length times the sine of the fold), and never folds past
// `kFoldRad` either way.
inline constexpr double kHitchM = 2.05;
inline constexpr double kBackM = 1.85;
inline constexpr double kFoldRad = 0.6;

// THE MOST OUT AT ONCE, by stars: what they cost a step is bounded by it.
inline constexpr std::array<int32_t, wanted::kMaxStars + 1> kMost{0, 0, 2, 4, 8, 12};
// The most kept in the state at once, every mode counted.
inline constexpr size_t kMostVehicles = 16;

// WHERE THEY COME FROM: the way the wave climbs, `kSetOffBackM` further down it than its column forms up
// (out of his sight), or the way's foot; the next of a wave `kStaggerS` after the one before, and
// none setting off with another within `kRoomM` of its spot. A truck finding the village's road out
// comes in along it from the map's edge, and on up the way from the road's end.
inline constexpr double kSetOffBackM = 260;
inline constexpr double kStaggerS = 6;
inline constexpr double kRoomM = 14;

// READING THE SNOW (army_drive.hpp): the way ahead read `kLookM`, every `kStepM`, `kFan` headings
// a side across `kSpread` rad, chosen again every `kDecideS`; a point of its way taken within
// `kReachM` of it or once past it. What a heading costs: `kTurnCost` a radian off the way wanted,
// `kBlockedCost` a metre short of `kLookM`, `kTreeCost` a trunk within `kTrunkM` past its half
// width, `kKeepCost` a radian off the way held; a tunnel's tube within `kTunnelM` and the map's
// edge within `kEdgeM` are no way at all.
inline constexpr double kLookM = 40;
inline constexpr double kStepM = 8;
inline constexpr int32_t kFan = 6;
inline constexpr double kSpread = 1.4;
inline constexpr double kDecideS = 0.25;
inline constexpr double kReachM = 12;
inline constexpr double kTurnCost = 6;
inline constexpr double kBlockedCost = 1.2;
inline constexpr double kTreeCost = 4;
inline constexpr double kKeepCost = 1.5;
inline constexpr double kTrunkM = 1.5;
inline constexpr double kTunnelM = 5;
inline constexpr double kEdgeM = 40;
// Swung off to a side, it steers this far off the way wanted, rad.
inline constexpr double kDetourRad = 1.2;
// A trunk thinner than this, m (a sapling) is no obstacle to several tonnes: driven over.
inline constexpr double kFlattenM = 0.12;

// PUTTING THE MEN DOWN: a carrier halts and its section gets out once it is within `kDropCarrierM`
// of him with him in sight (or `kDropBlindM` out of it); a truck within `kDropTruckM` in sight
// (`kDropBlindM` out of it), or held by a rise it cannot take: up against one, or `kHeldS` making
// for one point of its way without reaching it. A man out of the back every
// `kOutEveryS`, `kOutM` behind it, in three files `kFileM` apart; it moves on `kLingerS` after the
// last. A truck's men put down further than `kChaseM` from him climb on with the wave up its way;
// nearer, they go straight at him.
inline constexpr double kDropCarrierM = 170;
inline constexpr double kDropBlindM = 110;
inline constexpr double kDropTruckM = 200;
inline constexpr double kHeldS = 30;
inline constexpr double kChaseM = 450;
inline constexpr double kOutEveryS = 0.45;
inline constexpr double kOutM = 3;
inline constexpr double kFileM = 1.4;
inline constexpr double kLingerS = 2;

// THE TANK: it stands off him `kTankLethalM` where the army shoots to kill and `kTankArrestM`
// where it does not, and runs him down where he is within `kRamM` and it shoots to kill. Its
// turret turns toward him at `kTraverse` rad/s (back over the nose with nothing to lay on).
inline constexpr double kTankLethalM = 260;
inline constexpr double kTankArrestM = 90;
inline constexpr double kRamM = 45;
inline constexpr double kTraverse = 0.7;

// CLOSING ON HIM: every vehicle but a tank running him down stops `kStopM` short of him, crawling
// in over the last `kCrawlM` at up to `kCrawl` m/s. Its front meeting him faster than
// `kStrikeClosing` m/s, where the army shoots to kill, knocks him down.
inline constexpr double kStopM = 9;
// A WHEELED TURN: a truck held short with its way more than `kBackRad` off its nose backs at
// `kBackPace` m/s, its wheels turned, until it is within `kBackDoneRad`.
inline constexpr double kBackRad = 0.5;
inline constexpr double kBackDoneRad = 0.15;
inline constexpr double kBackPace = 2.5;
inline constexpr double kCrawlM = 6;
inline constexpr double kCrawl = 4;
inline constexpr double kStrikeClosing = 2.5;
// The steel he is held out of: from `kHullBaseM` under its middle's snow to its top.
inline constexpr double kHullBaseM = 0.3;

// NOT GETTING STUCK: progress checked every `kWindowS`; under `kLeastM` in one, it swings off to a
// side for `kDetourS`; a second, it backs `kDigM` down its own track; a third, it is put back on
// its way at the point it was making for (the lab counts those: always 0).
inline constexpr double kWindowS = 5;
inline constexpr double kLeastM = 2.5;
inline constexpr double kDetourS = 4;
inline constexpr double kDigM = 8;

// SENT HOME: further than `kBehindM` from where he was last seen while the hunt runs, or, the hunt
// over, `kHomeM` from him, it is taken off the map; nearer, it drives back down the way it came and
// is gone at its foot or after `kLeaveS`.
inline constexpr double kBehindM = 1300;
inline constexpr double kHomeM = 450;
inline constexpr double kLeaveS = 90;

// THE CREWS IN THE PURSUERS: `kPursuerBase` + the vehicle's id (mod `kPursuerSpan`); a vehicle's
// men, once out, are the squad `kSquadBase` + its id.
inline constexpr int32_t kPursuerBase = 800;
inline constexpr int32_t kPursuerSpan = 100;
inline constexpr int32_t kSquadBase = 60000;
// Its crew look every `wanted::kLookEveryS`, and a sighting is kept this long, s.
inline constexpr double kSawS = 0.5;

}  // namespace fall_line::sim::army_vehicle
