// THE LIFTS RUN: every chair, cabin and T-bar of a ski area's lifts round its loop, a pure
// function of the clock. Never stepped and never stored, so a run, a replay and a picture all
// find carrier `k` of a lift in one place at the same time `t`.
//
// - THE PLAN, once a lift (`plan_lift`): the line's plan length and direction up it, its look
//   (`LiftRide`: the rope's speed and a terminal's, a carrier every `every` m of rope, the load
//   zone at the bottom station) and, for a lift that slows in its terminals, the loop's clock.
// - THE LOOP: up the line on the up rope (`side` 0, `gauge`/2 right of the line, a drag's arm
//   right of its track), half round the top wheel, back down on the down rope, half round the
//   bottom wheel. `carrier_at` says where on it carrier `k` is; `carrier_place` where in the world.
// - A DETACHABLE LIFT (a chair's, a gondola's: `slow` under `speed`) lets its carriers go of the
//   rope in each terminal: slowed at `kDecel` onto the unload and into the bottom station, round
//   the wheels and through the load at `slow`, and taken up at `kAccel` after it. Its carriers are
//   spaced evenly in the loop's TIME, so they bunch in a station and spread out on the line. A
//   drag's are on a fixed grip, spaced in the loop's length.
// - THE LOAD ZONE at the bottom: `entry.at` m up the line, `entry.along` m either way,
// `entry.across`
//   m either side of `entry.side` m right of it; a skier is taken from it no faster than
//   `entry.fastest` m/s, his skis within `entry.turned` rad of the way up the line.
// - WHERE A RIDER IS LET GO is `off_at` m up the line (the unload over a chair's ramp, a cabin's
//   door short of the top, a drag's let-go), placed by `mapgen::let_go_of`.
//
// Pure over the lift and the clock: nothing here draws from any stream.
//
// ge:summary The lifts run: the lift plan, every carrier round its loop as a function of the clock
// ge:keywords lift lifts carrier carrier_at carrier_place lift-plan plan_lift loop rope chair
// ge:keywords gondola drag t-bar detachable terminal load-zone in_load_zone off_at rope-speed
// ge:skill mountain
// ge:test tests/mountain_lift_loop_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "mountain/mapgen/lift_line.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::lifts {

// How a kind of lift runs and is boarded: the rope's speed and a carrier's through a terminal,
// m/s; a carrier every `every` m of rope; the gauge between the ropes, m; and the load zone.
struct LiftRide {
  double speed;
  double slow;
  double every;
  double gauge;
  struct Entry {
    double at;       // m up the line from the bottom wheel, the zone's middle
    double along;    // m either way of `at`
    double side;     // m right of the line
    double across;   // m either side of `side`
    double fastest;  // m/s, the fastest a skier may come in
    double turned;   // rad, the most his skis may point off the way up the line
  } entry;
};

LiftRide lift_ride(mapgen::LiftKind kind);

// A detachable's carriers are slowed into a terminal and taken up out of it, m/s^2.
inline constexpr double kDecel = 0.8;
inline constexpr double kAccel = 1.2;
// Where a gondola's cabins take riders on, m up the line (its platform).
inline constexpr double kGondolaLoad = 2;
// The loop's clock is tabled every this many metres of loop.
inline constexpr double kClockStep = 0.5;

struct LiftPlan {
  int32_t lift = 0;  // its index in the ski area
  mapgen::LiftKind kind = mapgen::LiftKind::chair;
  mapgen::PlanPoint bottom{};
  mapgen::PlanPoint top{};
  double length = 0;  // plan length of the line, m
  double dx = 0;      // unit direction up the line
  double dz = 0;
  double heading = 0;  // up the line, rad
  LiftRide ride{};
  double radius = 0;  // the turn round each wheel, m
  double loop = 0;    // the whole loop's length, m
  int32_t carriers = 1;
  // A detachable's clock: the rope-speed metres a carrier has run to each `kClockStep` of the
  // loop, and the whole loop in them; empty for a lift that never slows.
  std::vector<double> clock;
  double span = 0;
};

LiftPlan plan_lift(const mapgen::Lift& lift, int32_t index);
// Every lift of the map's ski area, in its order; none on a map without one.
std::vector<LiftPlan> lift_plans(const mapgen::Level& level);

// Where on its loop a carrier is: `u` m up the line, on the up rope (`side` 0) or the down (1),
// `turn` rad round a wheel (negative on a rope), and whether it is out on the line.
struct LoopSpot {
  double u = 0;
  int32_t side = 0;
  double turn = -1;
  bool out = false;
};

// Where carrier `k` of the lift is at the clock `t`, s.
LoopSpot carrier_at(const LiftPlan& plan, int32_t k, double t);
// How fast a carrier runs `u` m up the line on `side`, m/s.
double carrier_speed_at(const LiftPlan& plan, double u, int32_t side);

// The carrier that passes `u` m up the line on its way up between the clock's `t` and `t + dt`
// (the one that takes a rider waiting there), or -1.
int32_t carrier_passing(const LiftPlan& plan, double u, double t, double dt);

// A spot on the loop in the world: plan position and the way it runs.
struct Place {
  double x = 0;
  double z = 0;
  double heading = 0;
};
Place carrier_place(const LiftPlan& plan, const LoopSpot& spot);

// Where the lift's carriers take a rider on, m up the line, and where they let him go.
double load_at(const LiftPlan& plan);
double off_at(const LiftPlan& plan);
// Where a point is in the line's frame: m up the line from the bottom wheel, m right of it.
mapgen::LinePoint line_frame(const LiftPlan& plan, double x, double z);
// Whether a skier at (x, z), heading so and going so fast, may be taken from the load zone.
bool in_load_zone(const LiftPlan& plan, double x, double z, double heading, double speed);
// The load zone's middle in the world, facing up the line.
Place load_spot(const LiftPlan& plan);

}  // namespace ge::mountain::lifts
