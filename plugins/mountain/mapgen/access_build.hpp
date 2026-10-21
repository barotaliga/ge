// R29 — ACCESS AS BUILT TO: the generator's own reckoning of whether every piste of the area it is
// laying can be skied again without a harder one, and whether every lift is reached — read off
// the PLAN (the runs as walked, by their place in the walk; the lifts as planned; which ends lie
// on the valley floor), so the lanes and the drag lifts R29 asks for are laid to it. The
// analysis holds the finished map to the same rule off what the level publishes, and shares
// nothing with this but the rule book.
//
// THE GRAPH. A skier is at a lift's bottom station, at its top, in the hub, or on a run at some
// arc down it. A bottom takes him to its top; a top to every run that leaves it, every run whose
// line passes a skate from it and every bottom a skate from it; a run, from his arc on down it,
// to every lane that leaves it further down, every bottom its line passes a skate from, and its
// end — the run it merges into, the station a link lane runs to, or the hub, which is one place
// with every valley bottom station in it.
//
// Held by tests/mountain_resort_build_test.cpp (the lanes and drags it lays to).
//
// ge:summary R29 the plan's access reckoning: every piste skiable again, every lift reached
// ge:keywords access-build reckon-access plan-run plan-lift access graph skate way-home orphans
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "mountain/mapgen/network.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// A run as the reckoning reads it: what it is and how hard (a lane is a green, 0), the lift it
// leaves, its line, where it ends, and the run it leaves part-way down.
struct PlanRun {
  RunKind kind;
  int rank;
  std::string from;
  const std::vector<TrackPoint>* points;
  double length;
  std::optional<RunJoin> into;
  double merge_start;  // where it begins closing on the run it merges into; its length if none
  std::optional<RunSpec::Branch> branch;
  std::optional<std::string> to;
  bool floor;  // whether it ends in the hub
};

// A lift as the reckoning reads it, and whether its bottom stands in the hub.
struct PlanLift {
  std::string id;
  PlanPoint bottom, top;
  bool floor;
};

// Every piste's verdict by its index (a lane is always ok), why one is not, how far its skier
// skis on the runs below it before a lift or the hub takes him back to its top (m; 0 for a
// lane), and the lifts no skier reaches.
struct PlanAccess {
  std::vector<bool> ok;
  std::vector<std::string> why;
  std::vector<double> home;
  std::vector<std::string> orphans;
};

// R29 — reckon the area's access off its plan; the way back from a piste's foot may run `slack`
// colours harder than it.
PlanAccess reckon_access(std::span<const PlanRun> runs, std::span<const PlanLift> lifts,
                         int slack = 0);

}  // namespace ge::mountain::mapgen
