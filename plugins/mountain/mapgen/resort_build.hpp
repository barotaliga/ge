// R25–R30 — A RESORT BUILT, stage by stage: the massif, the lifts and their pads, every run
// walked, graded and pressed in order, the lanes and drags that make every piste skiable again,
// the mountain's own features round them, the hub, the woods and the courses. The
// stages so far run through the access; the rest land under docs/goals.md M4b3–M4b5.
//
// THE SHAPE OF THE BUILD. One attempt's state is a `ResortAttempt`, and each stage is a function
// over it that answers a refusal (why the attempt is given up) or nothing; the build runs
// them in order. A test or a lab steps through them one at a time and reads the attempt between
// stages (tests/mountain_resort_build_test.cpp).
//
// ge:summary R25-R30 the ski-area build: its attempt's state and its stages, in order
// ge:keywords resort-build attempt-resort build-resort resort-attempt stage mountain pads
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/lift_chain.hpp"
#include "mountain/mapgen/links.hpp"
#include "mountain/mapgen/net_index.hpp"
#include "mountain/mapgen/network.hpp"
#include "mountain/mapgen/network_stamp.hpp"
#include "mountain/mapgen/regions.hpp"
#include "mountain/mapgen/resort.hpp"
#include "mountain/mapgen/station_pad.hpp"
#include "mountain/mapgen/summit_ramps.hpp"
#include "mountain/mapgen/terrain.hpp"

namespace ge::mountain::mapgen {

// A run that stands once graded and pressed: its walk, the colour it measures on the pressed
// mountain (a lane's is green), and what was laid down it.
struct BuiltRun {
  WalkedRun walked;
  PisteGrade grade = PisteGrade::green;
  std::vector<Kicker> kickers;
  std::vector<Cliff> drops;
  std::vector<Drift> drifts;
};

// One attempt at a resort: everything the build keeps between its stages.
struct ResortAttempt {
  double seed = 0;
  int attempt = 0;
  uint32_t sub = 0;
  const Region* region = nullptr;
  int version = 8;
  ge::core::Rng rng{0};  // the attempt's stream, `createRng(sub)`

  // 1. the massif
  TerrainPlan plan{};
  ge::core::Heightfield ground{};
  // 2. the stations and the lifts, the run slots, the pads and the way to the next queue
  std::vector<LiftPlan> lift_plans;
  std::vector<RunSpec> specs;
  PlanPoint village{};
  PadShape shape{};
  std::vector<StationPad> pads;
  std::optional<ChainWay> way;
  // 3. the runs walked, in slot order, the index every walk asks, and the room kept for the ramp
  // down to every start placed under a leaning pad (a deque: the index reads each run's points
  // where they stand)
  std::deque<WalkedRun> walked;
  NetIndex walking;
  std::vector<RampRoom> rooms;
  // 3b. what the lanes and drags laid for access did, and the pistes left out for it
  AccessLaid access;
  // 4. the runs that stand, graded and pressed in walk order and numbered among themselves (a
  // merge names the run it joins by its place here), the packed field, the press's shared mask,
  // and the index every later stage keeps off (over `built`; `walking` is spent)
  std::deque<BuiltRun> built;
  ge::core::Heightfield packed{};
  NetworkStamp stamp;
  NetIndex net;
  int dropped = 0;  // runs left out by the grading
  // 5. the ramps off every top, by its lift (a drag's top's too, on a leaning pad's area), and the
  // drag lifts settled on the pressed mountain
  RampsByLift ramps;
  int settled_drags = 0;
  int settled_home = 0;
};

// A step's answer: why the attempt is refused, or nothing to go on.
using StageResult = std::optional<std::string>;

// The attempt begun: its stream dealt off `sub`.
ResortAttempt begin_resort(double seed, int attempt, uint32_t sub, const Region& region,
                           int version);

// 1. THE MASSIF (R25), baked once onto the grid.
StageResult resort_mountain(ResortAttempt& a);

// 2. THE STATIONS AND THE LIFTS (R26), the run slots (R27), the station pads pressed, and the
// peak's chair's queue laid ahead of a rider out of the gondola.
StageResult resort_pads(ResortAttempt& a);

// 3. THE PISTES WALKED (R27): every slot's run walked onto the untouched mountain in slot order,
// each started under its top, walked again up to `network.tries` times (a colour harder where
// the face will not carry its own) and merging into the runs walked before it; refused when too
// few pistes stand.
StageResult resort_pistes(ResortAttempt& a);

// 3b. THE LANES BETWEEN THE RUNS (R27, R29): the link lanes and drag lifts that make every piste
// skiable again and reach every lift, then the branch lanes between the sectors.
StageResult resort_links(ResortAttempt& a);

// 4. THE RUNS GRADED AND PRESSED, IN ORDER (R8, R9, R17, R23, R24, R27): each run graded onto
// the mountain as the runs before it left it (a merging run onto the other's surface), its drops,
// kickers and drifts laid, and pressed in; a run that will not grade is left out, and every run
// merging into it or leaving it with it. Then the pads pressed again at their levels, and every
// run's colour measured on the pressed mountain. Refused when too few pistes stand.
StageResult resort_graded(ResortAttempt& a);

// 5. ACCESS, AS THE RUNS STAND (R26, R29): the ramps off the tops laid and pressed; R29 reckoned
// again on the colours the pressed runs measure, the drag lifts settled to it; every bottom
// station slid clear of the runs (refused where one fits nowhere), the way to the peak's chair's
// queue graded again, and the ramps off the drags' tops.
StageResult resort_access(ResortAttempt& a);

}  // namespace ge::mountain::mapgen
