// THE CROWD'S TWO HALVES' SEAM (crowd.hpp): the run as the crowd reads it (crowd.cpp) and the
// lifts as it rides them (crowd_lift.cpp), each calling the other.
//
// - A RUN READ AT AN ARC (`sample_run`): its line's point, heading, width, its pitch (the fall
//   along it, rise over run, positive down) and how fast its heading turns, rad/m. `place_on_run`
//   writes where an amateur's `s` and `d` put him on the mountain; `lateral_of` how far right of a
//   run's line at an arc a point is.
// - A RUN'S WEIGHT for a group whose least skilled has `skill` (`run_weight`): the colour's crowd
//   per metre of it and the skill it asks; `lengthy` weighs it by its length (where people stand at
//   any moment), else by its root (which run they pick).
// - THE LIFTS: a queue joined at a run's foot (`join_queue`), a group dealt into one
//   (`queue_group`) or riding one (`ride_group`), and every lift's queue, carriers and top stepped
//   (`step_crowd_lifts`).
//
// ge:summary The crowd's seam: the run sampled and placed on, a run's weight, the lifts' queues
// ge:keywords crowd sample_run place_on_run lateral_of run_weight join_queue queue_group
// ge:keywords step_crowd_lifts on_lift along pitch bend load_place
// ge:skill sim
// ge:test tests/fall_line_crowd_test.cpp
#pragma once

#include <cstdint>

#include "fall_line/sim/crowd_state.hpp"
#include "fall_line/sim/state.hpp"
#include "mountain/lifts/lift_loop.hpp"
#include "mountain/mapgen/types.hpp"

namespace fall_line::sim::crowd {

struct Along {
  double x = 0, z = 0, y = 0;
  double heading = 0;
  double width = 0;
  double pitch = 0;
  double bend = 0;
};

Along sample_run(const ge::mountain::mapgen::Track& run, double s);
double lateral_of(const Along& at, double x, double z);
void place_on_run(const GameState& state, Amateur& a);
double run_weight(const CrowdRun& r, double skill, bool lengthy);
// The least skill among a group's members.
double group_skill(const CrowdState& crowd, const CrowdGroup& g);

// Whether a lift has him (or he is up one unseen): off the runs, and met by nobody.
inline bool on_lift(const Amateur& a) {
  return a.mode == AmateurMode::queue || a.mode == AmateurMode::ride ||
         a.mode == AmateurMode::skate || a.mode == AmateurMode::unseen;
}

// Where a lift's carriers take riders on: its load point on the up rope, facing up the line.
ge::mountain::lifts::Place load_place(const ge::mountain::lifts::LiftPlan& plan);
void join_queue(GameState& state, Amateur& a, int32_t lift);
void queue_group(GameState& state, CrowdGroup& g, int32_t lift);
// A group dealt riding a lift part-way up: seated on its empty carriers out on the line, the rest
// in its queue; whether any found a seat.
bool ride_group(GameState& state, CrowdGroup& g, int32_t lift);
void step_crowd_lifts(GameState& state);

}  // namespace fall_line::sim::crowd
