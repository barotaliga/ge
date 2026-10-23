// THE MOVEMENT SCENARIOS: named, staged runs of the mover (a preset, a world of boxes, ramps and
// stairs laid in metres and scaled to the preset's unit, a scripted hand on the controls, a step
// rate and a length), each a pure function of its staging. The `move-trace` lab draws them, the
// `move-strafe` lab runs the strafe jumpers, and `tests/move_scenarios_test.cpp` holds every
// run's digest and numbers golden, so a change to the model shows which runs it moved and how.
//
// ge:summary Movement scenarios: staged runs (run-up, jumps, strafe jumping, stairs, ramps, ...)
// ge:keywords move movement scenarios scenario run digest golden run-up stop jump bhop
// ge:keywords strafe-jump stairs ramp steep crouch tunnel ledge coyote double-jump slide
// ge:keywords air-control fly run_move_scenario find_move_scenario move_scenarios
// ge:skill move labs
// ge:test tests/move_scenarios_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "move/box_world.hpp"
#include "move/step.hpp"

namespace ge::move {

// What a script remembers between steps (a side it strafes to, a time it waited).
struct ScriptMemory {
  int32_t side = 1;
  int32_t jumps = 0;
  double mark = -1;
};

struct MoveScenario {
  const char* id;
  const char* preset;  // the preset it runs by default (spec.hpp)
  const char* what;    // one line: what it stages and what it shows
  double seconds;
  // The world, laid in metres times `unit` (L per metre).
  void (*build)(BoxWorld& w, double unit);
  double start_x, start_z;  // m: where the feet are put down (on the ground below)
  double start_yaw;         // rad
  MoveCmd (*script)(const MoveSpec& spec, const MoveState& s, double dt, ScriptMemory& m);
  Mode mode = Mode::walk;
};

std::span<const MoveScenario> move_scenarios();
const MoveScenario* find_move_scenario(const char* id);

struct MoveRun {
  uint64_t digest = 0;    // FNV-1a over every step's position and velocity bits
  double top_speed = 0;   // L/s on the plan
  double end_speed = 0;   // L/s on the plan at the end
  double distance = 0;    // L travelled on the plan
  double apex = 0;        // L: the highest the feet rose above where they started
  double end_height = 0;  // L: the feet's height at the end above where they started
  double max_impact = 0;  // L/s: the hardest landing
  int32_t jumps = 0, air_jumps = 0, landings = 0, steps_up = 0, slides = 0;
  int32_t steps = 0;
};

// Called after every step: the state, the command that moved it and what happened.
using OnStep = void (*)(void* ctx, const MoveState& s, const MoveCmd& cmd, const MoveEvents& ev);

// Run `sc` on `spec` at `hz` steps a second. `spec` is normally the scenario's preset (with any
// knobs a lab set); the world is laid in its unit.
MoveRun run_move_scenario(const MoveScenario& sc, const MoveSpec& spec, double hz,
                          OnStep on_step = nullptr, void* ctx = nullptr);

// The scenario's world, as the run lays it (for drawing it).
BoxWorld scenario_world(const MoveScenario& sc, const MoveSpec& spec);

}  // namespace ge::move
