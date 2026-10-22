// R27 — A LANE ROUTED: the line a transport lane is laid along, found before it is walked.
//
// A piste is walked: it goes down the mountain and the ground steers it. A cat track is
// ENGINEERED: it holds a few per cent of fall for a kilometre or more across a face that falls at
// forty, laid along the benches, round the heads of the gullies and across the spurs where they
// are lowest. A greedy walk cannot find that line; a search over the face can. So a lane's line
// is found on a coarse grid from its start down the map one row at a time (a lane never doubles
// back up the map), each step costing its length, more where it falls outside the lane's band of
// pitch, more where the ground across it is steep, and a great deal where it runs beside a piste
// rather than across it — to the cheapest place it can JOIN a piste (or reach a lift's bottom
// station), the way its slot sends it preferred.
//
// Held by tests/mountain_resort_build_test.cpp (each lane's route).
//
// ge:summary R27 a lane's route: a search over a coarse grid to the cheapest join on a piste
// ge:keywords lanes route-lane lane-route lane-ask cat-track search nearest-raster joinable
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "core/heightfield.hpp"
#include "mountain/mapgen/net_index.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// A lane's route and the join it ends on: the run (−1 at a lift's bottom station, the goal).
struct LaneRoute {
  std::vector<PlanPoint> route;
  int run;
  double x, z;
};

// What a lane's search may be asked beyond its start: a lift's bottom station to run to instead
// of a piste, the least plan distance to its join (`road.reach.min` by default), the stretch of
// the map to search (across from x0 to x1, down to z1), and ground it may not cross.
struct LaneAsk {
  std::optional<PlanPoint> goal;
  std::optional<double> least;
  struct Box {
    double x0, x1, z1;
  };
  std::optional<Box> box;
  std::function<bool(double x, double z)> keep_off;
};

// R27 — the route a lane from (x0, z0) is laid along to the cheapest join on a piste `may` joins,
// or to `ask.goal`; the way `side` sends it (+1 toward +x) preferred. None where no route keeps a
// lane's pitch. `may` and `ask.keep_off` are pure readings, asked from several threads at once.
std::optional<LaneRoute> route_lane(const ge::core::Heightfield& ground, const NetIndex& net,
                                    double x0, double z0, double side,
                                    const std::function<bool(int run)>& may,
                                    const LaneAsk& ask = {});

}  // namespace ge::mountain::mapgen
