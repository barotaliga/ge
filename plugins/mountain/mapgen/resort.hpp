// R26, R27 — THE RESORT'S PLAN: where the lifts go on the massif, and which runs leave the top
// of each, built to which colour, heading where. A real
// face's hints, which lean the stations and the pistes onto the real ones, wait for
// docs/goals.md M4d.
//
// A gondola climbs from the village to the MID-STATION on the bench; a chair from beside its
// top to the PEAK; a chair from the valley floor to the SHOULDER; a drag beside the village to
// the top of the NURSERY slopes; and, where the face beyond the peak has room, a chair to an
// OUTER top. Every run is a SLOT (a top, a colour, a target, a side); the pistes are walked the
// gentlest first so the harder come down onto them (R29), and the lanes last. A region (R21)
// tilts the colours a slot is built to.
// Held by tests/mountain_resort_build_test.cpp (the lifts).
//
// ge:summary R26 R27 the resort's plan: the stations, the lifts and the run slots
// ge:keywords resort plan-resort place-stations road-row lift-plan slots tilt nursery gondola chair
// ge:keywords drag
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <string>
#include <vector>

#include "core/prng.hpp"
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/network.hpp"
#include "mountain/mapgen/terrain.hpp"

namespace ge::mountain::mapgen {

// A lift as planned: its two ends in plan.
struct LiftPlan {
  std::string id;
  LiftKind kind;
  PlanPoint bottom;
  PlanPoint top;
};

// THE TRANSPORT LANE's row (R27): the green's, the lane's numbers over it.
extern const GradeRow kRoadRow;

struct ResortPlan {
  std::vector<LiftPlan> lifts;
  std::vector<RunSpec> specs;  // in the order they are walked
  PlanPoint village;
};

// R26, R27 — plan the resort on a massif's plan: the lifts, and the run slots in the order they
// are walked, each turned into a spec with its colour's row.
ResortPlan plan_resort(ge::core::Rng& rng, const TerrainPlan& plan);

}  // namespace ge::mountain::mapgen
