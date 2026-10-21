// R27 — THE RUNS, WALKED: every run of a resort laid down the mountain from its top station (or,
// a lane, off a piste part-way down), asking the index of the runs laid before it
// (net_index.hpp) "is another run near here?".
//
// A run is walked the way R5's piste is (track.hpp): two metres a step down the mountain, its
// heading bending either side of a CENTRE on a slow wave, the untouched ground steering it off
// any heading it would climb along or fall along steeper than its colour allows, the turn each
// step capped at what R6 lets a bend that steep and that wide make, and the heading never
// within a right angle of straight across the face, so a run can never double back over
// itself. What is new is that it is not alone:
//
//   * ITS CENTRE IS A TARGET, not the fall line: the bearing to where it is going (the
//     village, the foot of a lift, the far side of the face).
//   * IT MEETS THE RUNS LAID BEFORE IT. A run it would come within `network.gap` of, it MERGES
//     into: from there its wanted heading is the other's, turned in toward the other's line,
//     until it runs inside the other along it. A run that would have to CROSS another to reach
//     it is refused, and the resort walks it again. A lane crosses every run it meets square
//     but the one it is laid to join.
//   * A RUN THAT MEETS NOTHING ENDS ON THE VALLEY FLOOR, turned to the fall line for R5's
//     finish straight into the village.
//
// The walk draws everything it draws before its first step, in a fixed order, so a refused walk
// costs the stream nothing beyond its own draws. Held by tests/mountain_resort_build_test.cpp
// (each walked run).
//
// ge:summary R27 a run's spec and its walk down the mountain: merging, crossing, the finish
// ge:keywords network run-spec walked-run walk-run choose-heading clearance rank merge lane
// ge:keywords run-colour smooth-run zig-zag neck sweep
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "core/heightfield.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/net_index.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/terrain.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// Ground a piste may not run over at (x, z), `half` m either side: a drag lift's line (R26),
// or the room kept for a ramp.
using AvoidAt = std::function<bool(double x, double z, double half)>;

// One run as the plan asks for it, and as the walk is handed it.
struct RunSpec {
  std::string id;    // the run's number on the piste map, from 1
  std::string from;  // the lift whose top it leaves
  RunKind kind;
  const GradeRow* row;  // the colour it is built to (R23); a lane's is the road row
  double x;             // where it starts…
  double z;
  double heading;    // …and the heading it would like to leave on
  PlanPoint target;  // the point down the mountain it steers its centre at
  double amplitude;  // the wave's amplitude, rad
  double lean;       // which way across the fall line it leans off its top (−1 … 1)
  // A LANE's destination (R27): the run (its index in the walk) it is laid to join.
  std::optional<int> join = std::nullopt;
  // A LANE's route (lanes.hpp): the line it follows down to its join; empty for none.
  std::vector<PlanPoint> route = {};
  // A BRANCH LANE's start: the run it leaves part-way down and the arc on it.
  struct Branch {
    int run;
    double s;
  };
  std::optional<Branch> branch = std::nullopt;
  // A LINK LANE's end: the lift whose bottom station it runs to.
  std::optional<std::string> to = std::nullopt;
  AvoidAt avoid = nullptr;
};

// Where a run merges into another: the other's index in the walk and the arc on it.
struct RunJoin {
  int run;
  double s;
};

// A run as it is walked and then built.
struct WalkedRun {
  RunSpec spec;
  Track track;                // its points and length
  std::vector<double> raw;    // the ground under each station as it was graded on
  std::vector<double> cross;  // the cross-fall the corridor keeps there (R8)
  std::optional<RunJoin> into;
  double merge_from;   // from here its corridor touches the other's, m (its length if none)
  double core_from;    // from here its centre runs inside the other's width and flat shoulder
  double merge_start;  // where it began closing on the other, m
  // The steepest its start may fall (R12) when the mountain made it gentler than its colour.
  std::optional<double> start_slope;
  int turned;  // how often its walk turned away from a harder run (R29)
};

// The widest a run is anywhere (R27), m.
inline constexpr double kWidest = resort_rules::piste::most;
// The bench either side of a run's width: the flat shoulder and the windrow's (R8, R18).
inline constexpr double kBench = rules::track::shoulder_flat + rules::berm::width;
// How far up from the end of the run it merges into a run must have joined it, m: from one that
// reaches the floor, its finish straight, its run-out and a course's ease across the junction
// (R28); from one that merges in turn, twice that ease.
inline constexpr double kJoinFinish =
    rules::track::finish + rules::track::runout + resort_rules::course::merge;
inline constexpr double kJoinEnd = 2 * resort_rules::course::merge;

// The least plan distance two runs' centrelines keep where neither merges into the other (R27).
inline double clearance(double a, double b) {
  return a / 2 + b / 2 + 2 * kBench + resort_rules::network::gap;
}

// How hard a run is built to, for what may merge into it: a lane and a green 0 … a black 3.
int rank_of(const RunSpec& spec);

// R27 — THE HEADING A RUN TAKES at a point: of every heading it may take, the one that best keeps
// the ground falling between `track.min_fall` and `cap` over every look-ahead (a lane's at its
// `aim` pitch too), nearest the heading it wants and the one it has; `side` holds a traverse.
double choose_heading(const ge::core::Heightfield& ground, double x, double z, double want,
                      double current, double cap, std::optional<double> aim, double swing,
                      int side = 0);

// Which runs leave the same top as the run walked (exempt from the gap near it).
using SharedTop = std::function<bool(int run)>;

// R27 — walk one run down the mountain from its spec: the run, or why it could not be walked.
std::variant<WalkedRun, std::string> walk_run(ge::core::Rng& rng, const TerrainPlan& plan,
                                              const ge::core::Heightfield& ground,
                                              const RunSpec& spec, const NetIndex& net,
                                              const SharedTop& shared);

// R27 — THE COLOUR A RUN MEASURES: R23's steepest colour window along its own line, up to where
// it runs inside the run it merges into (`onto`, null for none).
PisteGrade run_colour(const std::vector<TrackPoint>& points, const std::vector<TrackPoint>* onto);

}  // namespace ge::mountain::mapgen
