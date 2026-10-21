// R26 — THE STATIONS BESIDE THE RUNS: a lift's station is built beside the pistes, never on one,
// and a drag's track, ridden on the snow, never crosses a piste at all.
//
// The stations are placed before the runs are walked and the drags while access is cured, and the
// runs come down where the mountain sends them; so once every run stands, each BOTTOM station's
// FOOTPRINT (`lift.footprint`: `back` m behind its wheel, `ahead` in front, `half` either side of
// the line) is read against every run's surface, and a drag's whole line against every piste. A
// station on one is SLID across its line, the nearest way first, to where it stands clear: on
// ground level enough to build on, a link lane still reaching it, still on the valley floor if it
// was, and every piste still skiable again. A drag is re-laid, its top round where it was and its
// foot along or up its line. A lift that fits nowhere is named, and the attempt refused.
//
// Pure over the runs and the ground; draws nothing from the stream.
//
// ge:summary R26 the bottom stations slid clear of the runs once they stand; drags re-laid
// ge:keywords station-clear bottom-footprint footprint station lift clear-stations stands-level
// ge:keywords station-ground slide drag-relaid
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "mountain/mapgen/resort.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// A kind's footprint: `back` m behind its wheel, `ahead` in front of it, `half` either side.
inline const resort_rules::lift::Footprint& footprint_of(LiftKind kind) {
  namespace LR = resort_rules::lift;
  return kind == LiftKind::gondola ? LR::footprint_gondola
         : kind == LiftKind::chair ? LR::footprint_chair
                                   : LR::footprint_drag;
}

// A bottom station's footprint at `bottom` on a line to `top`, as points every 3 m.
std::vector<PlanPoint> bottom_footprint(LiftKind kind, PlanPoint bottom, PlanPoint top);

// What the stations are read against: whether a point stands on some run's surface (`pad` m past
// its half-width) and on a piste's, the ground's height, whether a point is on the valley floor,
// and where the link lanes to a lift's bottom station end.
struct StationGround {
  std::function<bool(double x, double z, double pad)> on_run;
  std::function<bool(double x, double z, double pad)> on_piste;
  std::function<double(double x, double z)> height;
  std::function<bool(PlanPoint p)> floor;
  std::function<std::vector<PlanPoint>(const std::string& id)> link_ends;
};

// Whether a bottom station at `bottom` on a line to `top` stands on ground level enough to build
// on: the ground across its footprint falls no more than a little over a fifth of a metre a metre.
bool stands_level(const std::function<double(double x, double z)>& height, LiftKind kind,
                  PlanPoint bottom, PlanPoint top);

// R26 — every lift's bottom station (and a drag, re-laid) stood clear of the runs where it stands
// on one, each the nearest way that leaves `ok` true. The lift that fits nowhere, or none. Moves
// the lifts in place.
std::optional<std::string> clear_stations(
    const StationGround& g, std::vector<LiftPlan>& lifts,
    const std::function<bool(std::span<const LiftPlan> lifts)>& ok);

}  // namespace ge::mountain::mapgen
