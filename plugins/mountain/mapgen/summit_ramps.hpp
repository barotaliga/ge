// R26 — THE RAMPS OFF A TOP: from the rim of a top station's pad (station_pad.hpp) down to the
// head of every run a rider can ski onto from it, a WIDE groomed ramp graded into the mountain,
// so the way from the lift to the slope is one smooth run-out rather than the raw face between.
//
// The ROOM a ramp needs is read while the runs are walked: a start off a leaning pad is placed
// only where a straight line from the pad's rim reaches it over no other run, pad, station, lift
// line or approach cut, falling at least a glide all the way and no steeper than the run's colour
// lets a rider meet; and every run walked after keeps off that room.
//
// The RAMPS are laid once the runs are pressed: off every pad, one to each run leaving its top
// that lies far enough under the deck and near enough, from the point of the rim whose straight
// line first meets the run's snow over no other run's ground, station, lift line or approach
// cut. Of those, the shortest that falls at least a glide and no more than a green's or a blue's
// `grade` (the gentlest where none does), never steeper than the run's colour, never across
// another ramp off the same pad. Each is eased off the pad and falls evenly to its foot, pressed
// into the ground everywhere but a run's own ground and a station's, only ever cut under a
// lift's line, and groomed. Every number is `resort_rules::lift::top::ramp`'s.
//
// Held by tests/mountain_resort_build_test.cpp (`rooms`, and the ramps after the access stage).
//
// ge:summary R26 the ramps off a top: their room kept as runs are walked, then laid and pressed
// ge:keywords summit-ramps ramp-room room-blocked near-room ramp-fit approach-cut lift-line
// ge:keywords lay-ramps plan-ramps press-ramps groom-ramps ramp-frame ramp-height off-ramp
// ge:keywords run-cover even-fall
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/network.hpp"
#include "mountain/mapgen/resort.hpp"
#include "mountain/mapgen/station_pad.hpp"

namespace ge::mountain::mapgen {

// How a ramp off a top may fall (R26): at the least (`glide`, m per m) and at the most down to a
// green's or a blue's run (`gentle`); `joint` where a pad's ramps are chosen together.
struct RampFit {
  double glide = resort_rules::lift::top::ramp::fall;
  double gentle = resort_rules::lift::top::ramp::grade;
  bool joint = false;
};

// A ramp's room kept as a start is placed: its line from the rim to the run's start, and how far
// either side of the line its ground reaches (a ramp's half-width and blend unless given).
struct RampRoom {
  PlanPoint from;
  PlanPoint to;
  std::optional<double> half = std::nullopt;
};

// How far (x, z) stands off a ramp's room, m, past the ramp's own ground.
double near_room(const RampRoom& r, double x, double z);

// Whether a point is blocked for a ramp's line.
using BlockedAt = std::function<bool(double x, double z)>;

// R26, R27 — WHETHER A RAMP HAS ROOM to come down off pad `p`'s rim to a run starting at (x, z),
// `half` m wide: the line from its head on the rim, or none.
std::optional<RampRoom> ramp_room(const StationPad& p, std::span<const StationPad> pads,
                                  std::span<const LiftPlan> lifts, double x, double z, double half,
                                  const GradeRow& row,
                                  const std::function<double(double, double)>& height,
                                  const BlockedAt& blocked, const RampFit& fit = {});

// What a ramp's room off `pad` keeps off as a start is placed (R26): every room before it, every
// run walked (`on_run`), every other pad by `keep_pad` m past its rim, every other station by
// `keep_station` m.
BlockedAt room_blocked(const StationPad* pad, std::span<const StationPad> pads,
                       std::span<const PlanPoint> stations, std::span<const RampRoom> rooms,
                       double keep_pad, double keep_station, BlockedAt on_run);

// Which run's ground (x, z) is on, by its place among the runs handed in: its snow (`past` 0) or
// out to `past` m beyond its edge; -1 for none.
using RunCover = std::function<int(double x, double z, double past)>;

// Where (x, z) stands against a ramp: `t` along it from the rim (0) to its foot (1), `d` m off
// its line either side, and its plan length.
struct RampAt {
  double t;
  double d;
  double length;
};
RampAt ramp_frame(const SummitRamp& r, double x, double z);

// A ramp's surface at `t` (0..1) along it: eased off the pad, even on down to its foot.
double ramp_height(const SummitRamp& r, double t);

// How far (x, z) stands off a lift's line from its bottom to its top, m.
double off_line(const LiftPlan& l, double x, double z);

// How far (x, z) stands outside a ramp's corridor (its width and the ground eased into it), m;
// negative within it.
double off_ramp(const SummitRamp& r, double x, double z);

// A run as the ramps read it: its spec (its id, the lift it leaves, its kind and colour) and its
// line as pressed.
struct RampRun {
  const RunSpec* spec;
  const std::vector<TrackPoint>* points;
};

// Every pad's ramps, by the lift whose top it is.
using RampsByLift = std::map<std::string, std::vector<SummitRamp>>;

// R26 — THE RAMPS off every pad laid and pressed into `ground`, off every run's ground (`cover`),
// every lift's bottom station, every run's head but the one it makes for, every lift's line and
// approach cut, and the way to the next lift's queue (`way`: how far off its ground, m).
RampsByLift lay_ramps(ge::core::Heightfield& ground, std::span<const StationPad> pads,
                      std::span<const RampRun> runs, const RunCover& cover,
                      std::span<const LiftPlan> lifts,
                      const std::function<double(double x, double z)>& way,
                      const RampFit& fit = {});

// R26 — groom every ramp, up to a run's edge and never over its snow, nor over another run's
// ground out to `past` m past its edge.
void groom_ramps(ge::core::Heightfield& packed, const RampsByLift& ramps,
                 std::span<const StationPad> pads, std::span<const RampRun> runs,
                 const RunCover& cover, double past);

}  // namespace ge::mountain::mapgen
