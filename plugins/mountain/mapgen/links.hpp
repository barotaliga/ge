// R27, R29 — THE LANES BETWEEN THE RUNS: the cat tracks that thread a ski area together, laid
// once every piste and every lane off a top station is — and the ones R29 asks for, so every
// piste can be skied again.
//
//   * A BRANCH LANE leaves a piste part-way down, on the edge the other sector lies, and runs
//     across the face to join a piste off ANOTHER lift, so a skier can cross the mountain. Each
//     two sectors side by side are linked one way, or the other where the ground will not carry
//     a lane the first, and the way back while the area has room for more lane.
//   * A LINK LANE runs from a piste's lower part to a lift's BOTTOM station where R29 asks for
//     one: to a station no skier reaches, off a piste whose skier could not otherwise get back
//     without a harder run (where no drag lift brings him), and home off a piste that would
//     leave him far to ski below it.
//
// Both are found by the lane search (lanes.hpp) and walked like every other lane (network.hpp).
//
// Held by tests/mountain_resort_build_test.cpp (the lanes after the walk).
//
// ge:summary R27 R29 the lanes between the runs: branch lanes across sectors, link lanes home
// ge:keywords links lay-links lay-access plan-runs branch-lane link-lane sectors lane-share
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "mountain/mapgen/access_build.hpp"
#include "mountain/mapgen/drags.hpp"
#include "mountain/mapgen/lanes.hpp"
#include "mountain/mapgen/network.hpp"
#include "mountain/mapgen/resort.hpp"

namespace ge::mountain::mapgen {

// What the resort's builder lends the links: the runs walked so far, the lifts (a drag lift laid
// for R29 added to them), the lane search, the walk that lays a run (true when it stood), how
// steeply the untouched mountain falls at a point, and the drag lifts' ground.
struct LinkBuilder {
  DragGround ground;
  const std::deque<WalkedRun>* walked;
  std::vector<LiftPlan>* lifts;
  std::function<std::optional<LaneRoute>(double x, double z, double side,
                                         const std::function<bool(int)>& may, const LaneAsk& ask)>
      route;
  std::function<bool(const RunSpec& spec, const SharedTop& shared)> lay;
  std::function<double(double x, double z)> fall;
};

// The runs as walked, as R29's reckoning reads them, each as hard as the colour it was built to.
std::vector<PlanRun> plan_runs(const std::deque<WalkedRun>& walked);

// What `lay_access` laid: the pistes it could not bring back (by their place in the walk), and
// how many link lanes and drag lifts it laid for pistes that could not be skied again, and link
// lanes home.
struct AccessLaid {
  std::vector<size_t> lost;  // in the order they were left out
  bool is_lost(size_t i) const;
  int lanes = 0;
  int drags = 0;
  int home = 0;
};

// R29 — BUILD TO ACCESS: drag lifts and link lanes until every piste can be skied again and
// every lift is reached, then link lanes home. Each lane laid with the next free number.
AccessLaid lay_access(const LinkBuilder& b, const std::function<std::string()>& next_id);

// R27 — lay the branch lanes between the sectors, one way or the other, then the way back where
// the area has room for it.
void lay_links(const LinkBuilder& b, const std::function<std::string()>& next_id);

}  // namespace ge::mountain::mapgen
