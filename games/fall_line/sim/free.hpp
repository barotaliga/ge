// THE FREE RIDE: a run with no gate and no finish line, over the ski area's runs. The skier is
// stood at the head of a run and skis where he likes; the run keeps track of where he is.
//
// - THE RUN UNDER HIS SKIS is the run whose line he is within half its width plus
//   `kOnRunMargin` of, the one he was on kept while it still holds him, else the nearest. It is
//   read only while he is on the snow: a flight keeps the run he left the snow on.
// - Coming onto a run is an `onto` event; each run is noted in `FreeRide::skied` the first time.
// - THE FOOT: a run that flows into no other ends at a lift's bottom station or in the valley.
//   Within `kFootReach` of its end he has ARRIVED (an `arrived` event, once). The free ride does
//   not end there; a pilot may stop.
// - THE RESET stands him at rest `kResetPast` m further down the run he was last on than where
//   he was last on it, facing down it, or at the head of his first run before he was on any.
//   It is his (`Controls::reset`) and the run's (down or stuck, as in a race).
// - On a map with no ski area (the single piste) the piste is the one run.
//
// The free ride draws nothing and allocates nothing in a step.
//
// ge:summary The free ride: the runs as lines, the run under the skis, the foot, the reset
// ge:keywords free-ride free free_map create_free_ride step_free reset_free run_chain runs
// ge:keywords run-tracks onto arrived foot skied odometer distance
// ge:skill sim
// ge:test tests/fall_line_free_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "fall_line/sim/state.hpp"
#include "mountain/mapgen/types.hpp"

namespace fall_line::sim {

// How far past half a run's width he is still on it, m.
inline constexpr double kOnRunMargin = 2;
// How near a run's end its foot is reached, m.
inline constexpr double kFootReach = 15;
// How far down a run's head he is stood at the start, m.
inline constexpr double kHeadIn = 5;
// How far a run's line is looked for about him, m (more than the widest run's half).
inline constexpr double kRunLook = 60;

// The map for `seed` as the free ride rides it: the ski area at the generator's current version
// unless `version` names another (throws the generator's GenerateError).
ge::mountain::mapgen::Level free_map(double seed, std::optional<int> version = {});

// The ski area's runs as lines, in the map's order (the single piste where it has none), each
// with its nearest-point index built.
std::shared_ptr<const std::vector<ge::mountain::mapgen::Track>> run_tracks(
    const ge::mountain::mapgen::Level& level);

// The run `id` names, or -1.
int32_t run_index(const ge::mountain::mapgen::Level& level, std::string_view id);

// The line the bot skis from the head of `run` to a foot: the run, then every run it flows into
// from the arc it joins at, its arc measured afresh from the head.
ge::mountain::mapgen::Track run_chain(const ge::mountain::mapgen::Level& level, int32_t run);

// A free ride on `level`: the player alone, at rest `kHeadIn` m down run `start`.
GameState create_free_ride(std::shared_ptr<const ge::mountain::mapgen::Level> level,
                           int32_t start = 0);

// The free ride after racer `i` moved from (x0, z0) this step: the metres skied, the run under
// his skis, the foot.
void step_free(GameState& state, int32_t i, double x0, double z0);

// Stand racer `i` back on his run (the header says where).
void reset_free(GameState& state, int32_t i);

}  // namespace fall_line::sim
