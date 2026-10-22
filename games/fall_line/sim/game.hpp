// A RUN OF FALL LINE: `create_game` builds the state on a map (generated off a seed, or handed
// in), `step` advances it one 1/120 s step. THE ORDER OF A STEP is stated here once and is part
// of every digest:
//
// 1. The events of the last step are cleared.
// 2. For each racer in field order (the player first): a reset asked for is made; then his skis
//    are stepped (`ge::skier::step_skier`) on his controls, or on a held brake once he has
//    finished; then the course (`step_course`: a gate, a miss, the finish), or on the free ride
//    the run under his skis (`step_free`, free.hpp) and a lift's load zone (`board_lift`,
//    lift_ride.hpp); then the skier's events become the run's; then the run's own resets (down
//    `kDownReset` s, stuck `kStuckReset` s; on the free ride stuck only while he pushes), each the
//    mode's. On the free ride a lift that has him steps him instead (`step_lift`), and nothing
//    else of his is stepped; then the snowmobile (`step_sled_ride`, sled_ride.hpp): taken on the
//    machine press, or stepped parked, or ridden, in which case it steps him in place of his skis
//    and nothing else of his is stepped. On the free ride the run's reset stands the machine he
//    rides or was thrown off back on its belt with him on it.
// 3. Skier against skier (`meet_racers`, field.hpp), the field in pair order.
// 4. The army on foot (`step_army`, army.hpp): the lifts' squads and the ground waves sent, every
//    soldier moved, the arrest, and each one written into the hunt's pursuers; nothing on a run
//    whose hunt never reached a star.
// 5. The wanted level (`step_wanted`, wanted.hpp): the look, the climb, the search running out;
//    nothing on a run that never raised the heat.
// 6. The clock advances.
//
// The only draws are the field's, made once when the run is created (`deal_field`, off a stream
// of the map's seed); `step` draws nothing, so a run is a pure function of its map and controls.
//
// ge:summary A run: create_game on a map or a seed, step in its stated order, the state's digest
// ge:keywords game create_game step order run digest state_hash game-options racers
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include "fall_line/sim/state.hpp"
#include "mountain/mapgen/types.hpp"

namespace fall_line::sim {

struct GameOptions {
  double seed = 38;
  int32_t racers = 1;          // the player and his rivals, the player first
  std::optional<int> version;  // the generator's; Fall Line races v1, the single piste
};

// The map for `seed` as Fall Line races it (throws the generator's GenerateError).
ge::mountain::mapgen::Level race_map(double seed, std::optional<int> version = {});

GameState create_game(const GameOptions& opts);
// A run on a map already built: `racers` on the grid's slots in order, at rest, as many as the
// grid has slots (at least the player).
GameState create_game_on(std::shared_ptr<const ge::mountain::mapgen::Level> level, int32_t racers);

// ONE STEP. `controls` holds one entry per racer, in field order.
void step(GameState& state, std::span<const Controls> controls);

// FNV-1a 64 over every racer's skier and progress, the hunt and the army (once raised) and the
// clock, folded into `h`; on the free ride his free ride too (a race's digest reads none of it).
uint64_t state_hash(const GameState& state, uint64_t h);

}  // namespace fall_line::sim
