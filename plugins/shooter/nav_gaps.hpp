// THE GAP TRICKS IN THE NAV: the jumps people make over a pit at a hopper's speed (a strafe jump
// over lava to save the way round), read off their recorded games (shooter/gap_jumps.hpp, the
// `se-rj` lab's gap_tricks.tsv; a game keeps each map's as data), each proven by the move as a
// jump link that only a player who builds that speed first can take.
//
// For each trick (`nav_add_gap_tricks`): the floor nodes within `near` of people's takeoff
// (the nearest `takeoffs`) and within `land` of their landing (the nearest `landings`), on the
// main part (`back`: one byte a node, empty any); from each takeoff to each landing, a jump at
// people's speed (their takeoff's quarter, then `faster` times it) proven by `prove_link`, and
// the first that lands is kept, when the same jump at the move's maxspeed (a run's) does not
// land: one a run makes is no speed jump, and the graph's own jumps already hold it. The link is
// a jump (`NavKind::jump`) with its `speed` past the maxspeed and `gap` set, at the trick's level,
// costing its flight and `cost_s`. A route takes it only for a jumper with `NavJumper::gaps` (a
// hopper) of that level or more; the pilot flies it out of a hop (pilot.hpp).
//
// ge:summary The gap tricks in the nav: people's speed jumps over a pit, proven as jump links
// ge:keywords gap-tricks gap-jump speed-jump strafe-jump-over lava-jump nav_add_gap_tricks
// ge:keywords nav-gap-trick hopper copy-movement map-data fly_jump jump-flight look-ahead
// ge:skill shooter bots
// ge:test tests/shooter_nav_gaps_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "shooter/nav.hpp"

namespace ge::shooter {

struct NavGapTrick {
  Vec3f from, to;     // where people took off and landed (their origins)
  float speed = 440;  // the takeoff's speed across people had (its quarter), units/s
  uint8_t level = 0;  // the least level that makes it (`NavLink::level`)
};

struct NavGapSettings {
  float near = 64;      // a takeoff node within this of people's, units
  float land = 64;      // a landing node within this across of people's (and 32 up or down)
  int takeoffs = 4;     // the nearest this many takeoffs tried
  int landings = 4;     // and landings
  float faster = 1.1f;  // the second speed tried, times people's
  float cost_s = 0.2f;  // the link costs its flight and this (a miss is lava)
};

struct NavGapCounts {
  int64_t held = 0, proofs = 0;
  int64_t run_makes = 0;      // tricks a run's jump makes too: left to the graph's own jumps
  std::vector<int32_t> from;  // each trick's link start, -1 where none was kept
};

NavGapCounts nav_add_gap_tricks(const MoveWorld& world, const MoveSettings& move, NavGraph& g,
                                std::span<const NavGapTrick> tricks, std::span<const uint8_t> back,
                                const NavGapSettings& set = {});

// WHERE A JUMP PRESSED NOW COMES DOWN (the pilot's look before a hop's takeoff by a gap): the
// move flown from `origin` moving at `velocity`, the jump on the first command and the forward
// key toward `aim` held after (`msec` a command), until it stands again or `limit_s` passes.
struct JumpFlight {
  bool landed = false;
  Vec3f end;
  float time_s = 0;
};
JumpFlight fly_jump(const MoveWorld& world, const MoveSettings& move, Vec3f origin, Vec3f velocity,
                    Vec3f aim, int msec, float limit_s = 1.5f);

}  // namespace ge::shooter
