// R26 — THE STATIONS BESIDE THE RUNS: a lift's bottom station built beside the pistes, never on
// one. So far its FOOTPRINT, read by the way to the next lift's queue; clearing the
// stations off the runs once they stand arrives with docs/goals.md M4b3.
//
// ge:summary R26 a lift's bottom station footprint, read as points
// ge:keywords station-clear bottom-footprint footprint station lift
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <vector>

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

}  // namespace ge::mountain::mapgen
