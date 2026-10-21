// R26 — THE RAMPS OFF A TOP: from the rim of a top station's pad (station_pad.hpp) down to the
// head of every run a rider can ski onto from it, a WIDE groomed ramp graded into the mountain,
// so the way from the lift to the slope is one smooth run-out rather than the raw face between.
//
// This half is the ROOM a ramp needs, read while the runs are walked: a start off a leaning pad
// is placed only where a straight line from the pad's rim reaches it over no other run, pad,
// station, lift line or approach cut, falling at least a glide all the way and no steeper than
// the run's colour lets a rider meet; and every run walked after keeps off that room. Every
// number is `resort_rules::lift::top::ramp`'s. The ramps themselves are laid once the runs are
// pressed (docs/goals.md M4b3).
//
// Held by tests/mountain_resort_build_test.cpp (`rooms`).
//
// ge:summary R26 the room a ramp off a top needs: placed with a run's start, kept by the rest
// ge:keywords summit-ramps ramp-room room-blocked near-room ramp-fit approach-cut lift-line
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "mountain/mapgen/grades.hpp"
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

}  // namespace ge::mountain::mapgen
