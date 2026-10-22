// THE BOT: Fall Line's pilot, the player's stand-in in every drive lab and every rival. It reads
// only what a player could see (where he is, how fast, the piste ahead) and plans on the
// skier's own limits (`skier/limits.hpp`), never a ceiling of its own.
//
// - THE PLAN, once a map (`plan_bot`): the piste's curvature at every point, and the fastest the
//   bot means to be there: what the pair can carve round that bend (`carve_speed`) times
//   `kCornerShare`, eased under `kKickerSpeed` up to every kicker on the piste, then carried back
//   up the piste so every slowing can be braked for (`kBrakeDecel` on a flat piste, less as the
//   pitch steepens).
// - THE LINE: pure pursuit of the racer's LANE (his `Rider::lane`, right of the centreline) a
//   speed-scaled look ahead; the curvature to the point is turned into the edge it needs
//   (`edge_for_curvature`) and that into a steer. On a course whose SLALOM GATES stand off the
//   line (a ski area's, R28), the plan first lays a WEAVE through them (`BotPlan::line`: from each
//   gate's offset to the next's on a smoothstep, so it passes every gate parallel to the piste)
//   and the lane is held about that weave, inside the gate's panels by `kLaneMargin`; the weave's
//   own bends are what the speed plan slows for.
// - THE SPEED: the brake past the plan's speed over the next stretch times his `Rider::pace`,
//   the tuck under it on a straight (which at a crawl is the skater's push). The player's rider
//   is pace 1 on the centreline: the plain bot.
// - A missed gate is taken again: the bot asks for the reset.
// - ON THE FREE RIDE (`plan_free`) the line is the chain of runs from the head of his run down to
//   a foot (`run_chain`, free.hpp), every kicker within a run's width of it planned for at its
//   nearest point; at the foot it brakes to a stop. On a line laid to a lift (`to_lift`) it skis on
//   past the foot to the line's end, and a lift that has him is left to carry him.
//
// ge:summary The bot: a speed plan off the skier's limits, pure pursuit down the piste
// ge:keywords bot pilot ai plan_bot bot_controls pure-pursuit lookahead speed-plan curvature
// ge:keywords corner kicker brake tuck rival plan_free free-ride chain
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
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
// The least room a lane leaves between the line and the piste's edge, m.
inline constexpr double kLaneMargin = 2;
}  // namespace bot

struct BotPlan {
  // The line the plan is laid along: the course's piste in a race (the map's own, not a copy),
  // the chain of runs on the free ride. Every vector below has an entry for each of its points.
  std::shared_ptr<const ge::mountain::mapgen::Track> track;
  std::vector<double> curvature;  // 1/m at each piste point, signed (+ bending right)
  std::vector<double> speed;      // m/s, the fastest the bot means to be at each point
  // The weave through the slalom gates, m right of the centreline at each piste point, and the
  // half-room a lane has about it; both empty on a piste whose gates all stand on the line.
  std::vector<double> line;
  std::vector<double> room;
  // A line laid to a lift's load zone (outing.hpp): skied on past a run's foot, slowing to its end.
  bool to_lift = false;
};

BotPlan plan_bot(const ge::mountain::mapgen::Level& level, const ge::skier::SkiSpec& spec);
// The free ride's plan: from the head of `run` down to a foot.
BotPlan plan_free(const ge::mountain::mapgen::Level& level, int32_t run,
                  const ge::skier::SkiSpec& spec);

// The free ride's plan along any line the bot is to ski (an outing's legs, outing.hpp), held
// under `slow` m/s from `slow_from` m along it.
BotPlan plan_line(const ge::mountain::mapgen::Level& level, ge::mountain::mapgen::Track line,
                  const ge::skier::SkiSpec& spec, double slow_from = INFINITY, double slow = 0);

// What the bot does this step as racer `i`, riding as his `Rider`.
Controls bot_controls(const GameState& state, int32_t i, const BotPlan& plan);

}  // namespace fall_line::sim
