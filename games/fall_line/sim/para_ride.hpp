// THE FREE RIDE'S PARAMOTOR: a free ride begun on the summit with a powered speed wing
// (`plugins/paramotor`) ready over the skier, the player's way off the top of the mountain and
// down it through the air.
//
// - WHERE IT BEGINS (`para_start`): the highest head of the free ride's runs, facing down it (the
//   piste's own start on a map with no ski area). A pure function of the map.
// - BEGUN (`create_para_ride`, `start_para`): the free ride on the run whose head that is, the
//   skier stood on it at rest, the wing laid ready over him (`ready_para`). The snowmobile and the
//   helicopter wait where they always do.
// - FLOWN BY HIM (`para_controls`): the tuck the throttle, the brake both brakes, the steer the
//   toggles and `Controls::lean` the speed bar (forward, -1, pushes it). On the snow under the rig
//   his skis are his as ever: the throttle tucks him, the toggles steer them. Hung in the harness
//   his skis do nothing (`para_held`); his free ride's run waits until he is back on the snow.
// - LET GO (the machine press): the rig falls free and he skis on (`drop`). It also comes off by
//   itself when the wing is down on the snow after a landing (`ground`), meets a crown (`crown`)
//   or he goes down under it (`rider`). Off him, the canopy and the motor fall and lie where they
//   come down, and the free ride is the free ride: its reset is his run's.
// - HIS RESET while the rig is on him begins the ride again on the summit (`restart`).
//
// Only the player (racer 0) flies it. Pure over the map and the state: draws nothing, and a step
// allocates nothing.
//
// ge:summary The free ride's paramotor: begun on the summit, flown, let go, begun again
// ge:keywords para paramotor free-ride para_start create_para_ride start_para para_controls
// ge:keywords para_held para_press step_para_ride restart_para launch take-off touch fold drop
// ge:keywords crown machine-press speed-bar summit
// ge:skill sim paramotor
// ge:test tests/fall_line_para_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "fall_line/sim/state.hpp"
#include "mountain/mapgen/types.hpp"
#include "paramotor/para.hpp"
#include "skier/skier.hpp"

namespace fall_line::sim {

// Where the ride begins: the run, the place on the snow and the way down it.
struct ParaStart {
  int32_t run = 0;
  double x = 0, z = 0;
  double heading = 0;
};

// THE HIGHEST HEAD of `runs` (the free ride's runs as lines, free.hpp's `run_tracks`).
ParaStart para_start(const std::vector<ge::mountain::mapgen::Track>& runs);

// A free ride on `level` begun under the wing on the summit.
GameState create_para_ride(std::shared_ptr<const ge::mountain::mapgen::Level> level);

// The ride begun (again) on the summit: the player stood there at rest, the rig ready over him.
void start_para(GameState& state);

// WHAT HIS HANDS DO under the wing, off his own controls.
ge::paramotor::ParaControls para_controls(const Controls& c);

// The rig's world: the map, its trees, the air's eddies, the map's wind and the run's clock.
ge::paramotor::ParaWorld para_world(const GameState& state);

// Whether the rig is on racer `i`.
inline bool under_para(const GameState& state, int32_t i) {
  return i == 0 && state.para.present && ge::paramotor::para_rigged(state.para.body);
}

// Whether racer `i` hangs in the harness off the snow.
inline bool hung_in_para(const GameState& state, int32_t i) {
  return under_para(state, i) && ge::paramotor::para_hangs(state.para.body);
}

// BEFORE HIS SKIS' STEP: the machine press lets the rig go (and is cleared from `c`, so nothing
// else this step reads it again).
void para_press(GameState& state, int32_t i, Controls& c);

// What racer `i`'s skis are stepped on: nothing while he hangs in the harness, else `in`.
ge::skier::SkierInput para_held(const GameState& state, int32_t i, const ge::skier::SkierInput& in);

// AFTER HIS SKIS' STEP: the rig flown on him, or its pieces falling and lying where they came
// down; its moments as `para` events.
void step_para_ride(GameState& state, int32_t i, const Controls& c);

// THE RUN'S RESET while the rig is on him: the ride begun again on the summit. False when it is
// not (the free ride's own reset is the caller's).
bool restart_para(GameState& state, int32_t i);

const char* para_phase_name(ParaPhase p);

}  // namespace fall_line::sim
