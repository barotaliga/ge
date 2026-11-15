// THE JUMP FINDER: the rocket jumps a map offers that nobody wrote down, CALCULATED from its
// floors (one a LEDGE), so a map's picture (the `map-overview` lab) shows the likely ones on any
// map with no hint and no recording of anyone making them.
//
// - A LEDGE is a run of floor nodes of the nav graph (nav.hpp) next to each other in the grid and
//   within a step of one height, at least one of them on an edge (a neighbouring column with no
//   floor near its height): a balcony, a window's sill, a platform, a roof.
// - A jump is CALCULATED, not flown command by command: the take-off (the run at the start along
//   the way to the ledge, the jump's 270 units/s up, what the floor adds: a RAMP's rise along the
//   run, a LIFT's climb when it is still going up), the rocket's burst where its line from the eye
//   meets the world (`rocket_burst`), the blast's push by the distance to the player's middle as
//   the game deals it, then the arc under gravity, its box swept through the map step by step,
//   sliding along what it meets, until it stands on a floor (`calc_rocket_jump`).
// - Each ledge's STARTS are the floor nodes below it, between `near` and `lift` times the rocket's
//   REACH under it, and the lift tops 48 units under it at least, within `across` units; each start aims at the ledge's
//   edge nodes nearest it; the ways are the shot at the feet and behind (the flip), standing and
//   running, at three pitches each. Of the jumps that land on the ledge, the HIGHEST (the one
//   nearest the most a rocket gives) is the ledge's jump, unless a walk (`shortcut_s`) matches it:
//   up a stair is no trick. A ledge no walk reaches is kept only when it holds a place (`places`). A jump over `reach` is LIFTED: made only by the ramp or the lift.
// - REACH (`rocket_reach`): the highest ledge a standing rocket jump lands on, calculated the same
//   way: the arc's top on an open floor less the 32 units the box needs over a lip.
//
// Off the hot path (it allocates), once a map. Deterministic: the graph's order, the floats, the
// engine's libm.
// Held by tests/shooter_jump_finder_test.cpp; drawn by the `map-overview` lab.
//
// ge:summary Rocket jumps a map offers, one a ledge, calculated from its floors and arcs
// ge:keywords jump-finder rocket-jump ledge likely-jump lifted-jump ramp-lift lift-jump flip
// ge:keywords find_jumps calc_rocket_jump rocket_reach prove_any_rocket_jump arc map-design
// ge:keywords overview
// ge:skill shooter map-design
// ge:test tests/shooter_jump_finder_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "shooter/nav.hpp"

namespace ge::shooter {

struct JumpFinderSettings {
  float reach = 0;         // a standing rocket jump's rise, units (0: `rocket_reach`)
  float near = 0.75f;      // a jump is shown from this share of `reach` up
  float lift = 1.5f;       // a lifted jump is tried up to this share of `reach`
  float across = 800;      // a start this far across from the ledge at most, units
  int starts = 32;         // starts tried a ledge, the nearest first (one a 96-unit cell)
  int targets = 3;         // the ledge's edge nodes a start aims at, the nearest first
  int max_ledges = 96;     // ledges tried, the highest first
  int ledge_nodes = 2;     // a ledge has this many nodes at least
  float run = 320;         // the running start, units/s
  float lift_speed = 150;  // a lift's climb when its speed is not known, units/s
  // A jump is kept only as a SHORTCUT: when the walk there (the graph's walks, drops, plain jumps,
  // rides and teleporters) takes this much longer than the flight, or there is none.
  float shortcut_s = 2;
  // A ledge no walk reaches (a roof, a wall's top) counts only when it holds one of these places
  // (the map's items and starts, standing origins): the rest are no place a player goes.
  std::vector<Vec3f> places;
  // Run-ups `prove_any_rocket_jump` flies (the map-intent lab's proofs), commands (13 ms each).
  int runs[8] = {6, 12, 18, 24, 32, 40, 48, 56};
};

// What lifts a jump past a standing one's reach.
enum class JumpLift : uint8_t { none, ramp, lift };

struct FoundJump {
  Vec3f from, to;     // the start (a standing origin) and where it lands on the ledge
  Vec3f launch;       // where the jump and the shot are (the start)
  float rise = 0;     // to over from, units
  float speed = 0;    // the run at the start, units/s
  float lift_vz = 0;  // what the floor adds up at the take-off, units/s
  JumpLift by = JumpLift::none;
  bool back = false;       // the rocket behind (the flip)
  float pitch = 0;         // the shot's pitch, degrees down
  bool lifted = false;     // over `reach` on a ramp, or off a rising lift: the floor makes it
  float health = 0;        // the blast's points to the jumper
  float time_s = 0;        // the flight
  float apex = 0;          // the arc's top, units over the start
  int ledge = 0;           // which ledge (its index in the finder's order)
  float walk_s = -1;       // the walk there, s (-1: none)
  int run = 0;             // commands run first (0: the calculated finder's)
  std::vector<Vec3f> arc;  // the flight's points, the take-off first
};

struct JumpFinderCounts {
  int64_t ledges = 0, tried = 0, arcs = 0, landed = 0, plain = 0, lifted = 0, walkable = 0;
  float reach = 0;  // the reach the finder used
};

// One calculated rocket jump: from `from` (a standing origin) along the way to `toward`, running
// at `speed` with `lift_vz` added up by the floor, the rocket at the feet (or behind, `back`)
// `pitch` degrees down. `turn`: the take-off's heading off the way, degrees (the view on `toward`
// in the air steers it back: a jump round a corner).
struct RocketArc {
  bool fired = false;   // the rocket met the world and its blast reached the jumper
  bool landed = false;  // came down standing on a floor
  Vec3f end;
  float time_s = 0, health = 0, apex = 0;
  std::vector<Vec3f> points;  // every 1/72 s
};
RocketArc calc_rocket_jump(const MoveWorld& world, const MoveSettings& move, Vec3f from,
                           Vec3f toward, float speed, float lift_vz, bool back, float pitch,
                           const RocketJumpSpec& rj = {}, float limit_s = 3, float turn = 0);

// The highest ledge a standing rocket jump at the feet lands on (its top on an open, flat floor
// less 32 units over the lip), units.
float rocket_reach(const MoveSettings& move, const RocketJumpSpec& rj = {});

// How a proven rocket jump was made: the run at the start, the commands run first, the rocket
// behind or at the feet and its pitch, where the jump and the shot were.
struct RocketJumpTry {
  float speed = 0;
  int run = 0;
  bool back = false;
  float pitch = 0;
  Vec3f launch;
};

// A rocket jump from `from` onto `to` FLOWN command by command every way known (`base` the
// blast's spec): when `direct`, standing and running, the rocket at the feet and behind, at two
// pitches each; then after each run-up in `runs`. The first that stands on `to` (`how` and
// `health` say how), or the last refusal. `proofs`, when given, counts the proofs flown. `lands`,
// when given, also takes a jump that came down standing somewhere else it accepts. The map-intent
// lab proves a plan's written tricks with it.
NavProof prove_any_rocket_jump(const MoveWorld& world, const MoveSettings& move, Vec3f from,
                               Vec3f to, const NavSettings& set, const RocketJumpSpec& base,
                               bool direct, std::span<const int> runs, RocketJumpTry* how = nullptr,
                               float* health = nullptr, int64_t* proofs = nullptr,
                               const std::function<bool(const NavProof&)>& lands = {});

// The jumps of the map `g` was built on (`world` its solids, `move` the move; `g.movers` its
// lifts), one a ledge at most.
std::vector<FoundJump> find_jumps(const MoveWorld& world, const MoveSettings& move,
                                  const NavGraph& g, const JumpFinderSettings& set = {},
                                  JumpFinderCounts* counts = nullptr);

// Every ledge a rocket jump reaches from the floor nearest `at` (48 units up at least, or across
// a drop to one about as high, 192 across at least): what a player standing there can get to.
std::vector<FoundJump> jumps_from(const MoveWorld& world, const MoveSettings& move,
                                  const NavGraph& g, Vec3f at, const JumpFinderSettings& set = {},
                                  JumpFinderCounts* counts = nullptr);

}  // namespace ge::shooter
