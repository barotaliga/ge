// THE FIELD: the rivals on one grid with the player, how each rides, skier against skier, and
// who leads.
//
// - THE DEAL (`deal_field`): every racer has a `Rider` profile. The player's is the plain bot's
//   (pace 1, the piste's centre). Each rival's PACE (the share of the bot's speed plan he dares,
//   `kPaceLeast`..`kPaceMost`) is drawn off a stream of the field's own, seeded off the map's
//   seed and `kFieldSalt`, never the run's: the only draws a race makes, made once at the start,
//   in slot order. His LANE is his slot's offset across the start line, held down the piste.
// - SKIER AGAINST SKIER (`meet_racers`): two bodies closer than `kShoulder` are pushed apart
//   half each, and the closing speed between them is shared as two equal masses with a little
//   bounce (`kBounce`): a shoulder, never a wipeout.
// - WHO LEADS (`standings`, `place_of`): each racer reduced to the racing plugin's `Standing`
//   (home by the clock, then gates taken plus the share of the leg to the next), ordered by
//   `ge::racing::field_order`.
//
// ge:summary The field: rivals dealt their pace and lane, skier against skier, the standings
// ge:keywords field rivals rider profile pace lane deal deal_field shoulder meet_racers
// ge:keywords standings place place_of leaderboard grid
// ge:skill sim racing
// ge:test tests/fall_line_field_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "fall_line/sim/state.hpp"
#include "mountain/mapgen/types.hpp"
#include "racing/standings.hpp"

namespace fall_line::sim {

inline constexpr uint32_t kFieldSalt = 0x5eed7a11u;
inline constexpr double kPaceLeast = 0.9;
inline constexpr double kPaceMost = 1.06;
// Two skiers closer than this are touching, m (two bodies' half-shoulders and their skis).
inline constexpr double kShoulder = 0.8;
// The share of the closing speed that bounces back.
inline constexpr double kBounce = 0.2;

// Every racer's profile (`Rider`, state.hpp), in field order, the player's first. `create_game_on`
// deals it once.
std::vector<Rider> deal_field(const ge::mountain::mapgen::Level& level, int32_t racers);

// Skier against skier, after every racer has stepped. Writes `shoulder` events.
void meet_racers(GameState& state);

// Each racer's standing, in field order (`out` sized to the field).
void standings(const GameState& state, std::span<ge::racing::Standing> out);
// Racer `i`'s place, 1-based.
int32_t place_of(const GameState& state, int32_t i);

}  // namespace fall_line::sim
