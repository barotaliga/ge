// R26, R29 — A DRAG LIFT LAID TO A PISTE'S FOOT: the T-bar or the platter a ski area puts where a
// run comes down with no way back up — from beside the run's foot, up the slope beside it, to the
// station or the lane its skier wants.
//
// A surface lift pulls a skier up on his skis, so it is short (a few hundred metres to a
// kilometre and a bit) and never steep (the rope climbs about forty per cent at most), and its
// track is a groomed lane of its own: it crosses no piste and never runs up one. Its top stands
// beside the station or the lane it serves, on ground level enough to step off onto.
//
// Held by tests/mountain_resort_build_test.cpp (the lifts after the walk).
//
// ge:summary R26 R29 a drag lift from a piste's foot back up: placed, fitted, settled
// ge:keywords drags plan-drag drag-back settle-drags pad-grade plan-lifts t-bar surface-lift
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "mountain/mapgen/access_build.hpp"
#include "mountain/mapgen/resort.hpp"

namespace ge::mountain::mapgen {

// The nearest piste to a point: its distance from the line, width and heading (infinite
// distance where none is near).
struct PisteNear {
  double distance, width, heading;
};

// What a drag lift's line is read against: the mountain's height, the nearest piste, whether a
// bottom station stands in the hub, and how many colours harder than a piste the way back from
// its foot may run (R29; none on a dealt map).
struct DragGround {
  std::function<double(double x, double z)> height;
  std::function<PisteNear(double x, double z)> piste;
  std::function<bool(PlanPoint p)> floor;
  int slack = 0;
};

// A foot a drag lift may leave: a station on a run.
struct Foot {
  double x, z, heading, width;
};

// R26 — a drag lift `id` from beside `foot` up to beside `station`, the first place it fits.
std::optional<LiftPlan> plan_drag(const DragGround& g, const Foot& foot, PlanPoint station,
                                  const std::string& id);

// The most the ground falls across a top station's pad, m per m.
double pad_grade(const std::function<double(double, double)>& height, PlanPoint p);

// The lifts as the access reckoning reads them.
std::vector<PlanLift> plan_lifts(const DragGround& g, std::span<const LiftPlan> lifts);

enum class DragWhy : uint8_t { back, home };

// R26, R29 — A DRAG LIFT TO BRING PISTE `i` BACK (`back`: one that cannot be skied again; `home`:
// one whose skier skis too far below it), kept on `lifts` only where the reckoning's verdict on
// the piste turns. Whether one was kept.
bool drag_back(const DragGround& g, std::span<const PlanRun> runs, std::vector<LiftPlan>& lifts,
               size_t i, DragWhy why = DragWhy::back);

// What settling the drag lifts did: why the area must be refused (if it must), the drags renamed,
// and how many were laid back and home.
struct DragsSettled {
  std::optional<std::string> refused;
  std::map<std::string, std::string> renamed;
  int drags;
  int home;
};

// R26, R29 — THE DRAG LIFTS SETTLED once the runs that stand are graded: the drags that no
// longer fit taken down, one for every piste that cannot be skied again, then for every one
// whose skier skis too far below it; every drag nothing needs taken down but those `pinned`, and
// the rest numbered on from the nursery's D1.
DragsSettled settle_drags(const DragGround& g, std::span<const PlanRun> runs,
                          std::vector<LiftPlan>& lifts,
                          const std::function<std::string(size_t)>& name,
                          const std::function<bool(const std::string&)>& pinned);

}  // namespace ge::mountain::mapgen
