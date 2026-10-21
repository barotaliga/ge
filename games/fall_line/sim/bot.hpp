// THE BOT: Fall Line's pilot, the player's stand-in in every drive lab and (from M5c) every
// rival. It reads only what a player could see (where he is, how fast, the piste ahead) and
// plans on the skier's own limits (`skier/limits.hpp`), never a ceiling of its own.
//
// - THE PLAN, once a map (`plan_bot`): the piste's curvature at every point, and the fastest the
//   bot means to be there: what the pair can carve round that bend (`carve_speed`) times
//   `kCornerShare`, eased under `kKickerSpeed` up to every kicker on the piste, then carried back
//   up the piste so every slowing can be braked for (`kBrakeDecel` on a flat piste, less as the
//   pitch steepens).
// - THE LINE: pure pursuit of the piste's centreline a speed-scaled look ahead; the curvature to
//   the point is turned into the edge it needs (`edge_for_curvature`) and that into a steer.
// - THE SPEED: the brake past the plan's speed over the next stretch, the tuck under it on a
//   straight (which at a crawl is the skater's push).
// - A missed gate is taken again: the bot asks for the reset.
//
// ge:summary The bot: a speed plan off the skier's limits, pure pursuit down the piste
// ge:keywords bot pilot ai plan_bot bot_controls pure-pursuit lookahead speed-plan curvature
// ge:keywords corner kicker brake tuck rival
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "fall_line/sim/state.hpp"
#include "mountain/mapgen/types.hpp"
#include "skier/skis.hpp"

namespace fall_line::sim {

namespace bot {
// The share of the carve's speed the bot takes a bend at.
inline constexpr double kCornerShare = 0.8;
// The speed it takes a kicker at, m/s, and how far up the piste from the lip it holds it, m.
inline constexpr double kKickerSpeed = 16;
inline constexpr double kKickerReach = 30;
// The braking it plans on, m/s^2, on a flat piste (it brakes harder, and the steep takes it
// back), and the least it ever counts on.
inline constexpr double kBrakeDecel = 5;
inline constexpr double kBrakeLeast = 0.8;
// The look ahead: `kLookTime` s at his speed plus `kLookLeast` m, at most `kLookMost` m.
inline constexpr double kLookTime = 0.5;
inline constexpr double kLookLeast = 7;
inline constexpr double kLookMost = 30;
// How far ahead it reads the plan's speed, s at his speed plus m.
inline constexpr double kReadTime = 0.6;
inline constexpr double kReadLeast = 4;
}  // namespace bot

struct BotPlan {
  std::vector<double> curvature;  // 1/m at each piste point, signed (+ bending right)
  std::vector<double> speed;      // m/s, the fastest the bot means to be at each point
};

BotPlan plan_bot(const ge::mountain::mapgen::Level& level, const ge::skier::SkiSpec& spec);

// What the bot does this step as racer `i`.
Controls bot_controls(const GameState& state, int32_t i, const BotPlan& plan);

}  // namespace fall_line::sim
