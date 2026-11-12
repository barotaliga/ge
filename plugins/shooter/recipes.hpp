// MOVEMENT RECIPES: a short stretch of a person's movement read off his recorded track and kept as
// what a bot can play again (docs/goals/M18r.md § 0r). Each kind of move is cut out of a track as
// INSTANCES: frames from the instance's ENTRY (where a bot settles before it) to its end, each
// frame the feet, the velocity, the view, the keys read off the move (rocket_jumps.hpp's wish:
// the acceleration across with the ground's friction put back, in the view's frame) and the jump.
//
// THE FIRST KIND, A HOP START: from a stand or a walk to speed by chained jumps, the start people
// use after a spawn, an item or a stop. Read off the hops (hops.hpp) of a track:
// - its FIRST TAKEOFF is the first hop of a chain (hops.hpp's `chain`: each jump within
//   `chain_frames` of the last landing keeps the chain);
// - its ENTRY is the last frame on the ground slower than `slow` before that takeoff, looked for
//   back over the ground (an air frame ends the look: a landing at speed is no start) within
//   `back_s`; none, and the chain was entered already running: no hop start;
// - it runs to the chain's `takeoffs`-th takeoff (or its last landing when the chain is shorter),
//   on past it to the first frame at `target` when that comes later in the chain; a chain of
//   fewer than `least_takeoffs` jumps is a single jump (up a ledge, onto a lift), no start;
// - its LOW is the slowest frame on the ground before the first takeoff, looked for as the entry
//   is (back over the ground within `back_s`): under `stood` it started from a stand;
// - MADE when the speed across reaches `target` within `within_s` of the entry; a start that
//   stalled is kept too, unmade (a novice's recipe draws them at people's rate);
// - a blast in its air (the speed up `blast_rise` within 0.1 s, a rocket's or a grenade's) is no
//   hop start: left out.
// THE PLAY (`play_recipe`): an instance's frames played back through the move (pmove.hpp) from any
// spot, its view turned by any angle: each frame's keys as the classic client builds its axes (400
// a key), the view as recorded (turned), the jump pressed on the command that leaves the ground,
// each command as long as the recording's frame. `lead` shifts which frame's keys and view drive
// the command from frame i to i + 1 (the recording's view at a frame is the command's that moved
// the player there: lead 1). The proof of a recipe: played on an open floor it reaches the speed
// the person reached; played on the map from a bot's spot it shows where the move takes it.
// What a lab tables off it: the entry's speed and whether he stood, the run before the first jump
// and its keys, the takeoff's speed and the view's lag there, the time to `target`.
//
// Pure, deterministic (floats and core's libm), allocates only the instances' vectors. Held by
// tests/shooter_recipes_test.cpp; read over folders of recordings by the `se-recipes` lab.
//
// ge:summary Movement recipes read off recorded players: hop starts as frames with keys and jumps
// ge:keywords recipe recipes hop-start copy-movement demo mvd entry-zone keys wish takeoff
// ge:keywords stand-start time-to-400 chain movement-skill read_hop_starts
// ge:skill shooter bots
// ge:test tests/shooter_recipes_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "shooter/hops.hpp"
#include "shooter/pmove.hpp"
#include "shooter/rocket_jumps.hpp"

namespace ge::shooter {

// One frame of a recipe's instance.
struct RecipeFrame {
  float t = 0;               // since the instance's entry, s (the low's frames before it < 0)
  Vec3f feet, velocity;      // the origin, units; units/s
  float yaw = 0, pitch = 0;  // the view, degrees (yaw counter-clockwise from +x, pitch down +)
  uint8_t keys = 0;          // read off the move (kKeyForward ...)
  bool jump = false;         // he left the ground on this frame
  bool ground = false;
};

struct HopStartSettings {
  float slow = 250;             // the entry: the last ground frame slower than this, units/s
  double back_s = 1.5;          // looked for this far back from the first takeoff, s
  float target = 400;           // the speed a start reaches, units/s across
  double within_s = 2.5;        // within this of the entry, s (else it stalled)
  int takeoffs = 3;             // it runs to this chained takeoff
  int least_takeoffs = 2;       // a chain of fewer jumps is no start
  float blast_rise = 150;       // the air's speed up this much within 0.1 s: a blast, units/s
  float stood = 30;             // the low slower than this: from a stand, units/s
  RocketJumpReadSettings keys;  // the wish and the keys, read as rocket jumps read them
};

struct HopStart {
  int32_t entry = 0, first = 0, end = 0;  // frame indices: the entry, the first takeoff, the last
  int32_t reach = -1;                     // the first frame at `target` (-1: never)
  double t = 0;                           // the entry's clock, s
  Vec3f feet;                             // at the entry
  float speed_in = 0;                     // across at the entry, units/s
  float low = 0;                          // the slowest on the ground before it, units/s
  int32_t low_frame = 0;                  // its frame (at or before the entry)
  bool stood = false;                     // the low slower than `stood`
  float yaw_in = 0;                       // the view's yaw at the entry, degrees
  float heading = 0;                      // the velocity's heading at the first takeoff, degrees
  float run_s = 0;                        // the entry to the first takeoff, s
  float speed_off = 0;                    // across at the first takeoff, units/s
  float lag_off = 0;             // the view's yaw less the velocity's heading at it, degrees
  uint8_t run_keys = 0;          // the keys held most on the ground before the first takeoff
  int32_t takeoffs = 0;          // chained takeoffs in it
  float to_target_s = -1;        // the entry to `target`, s (-1: never)
  float low_to_target_s = -1;    // the low to `target`, s (-1: never)
  float speed_out = 0, top = 0;  // across at the end and the most, units/s
  bool made = false;
  std::vector<RecipeFrame> frames;  // the low to the end
};

// The hop starts in `track` (the frames `hops` read off it, on the same indices), appended to
// `out` in time order.
void read_hop_starts(std::span<const TrackPoint> track, const HopReading& hops,
                     const HopStartSettings& set, std::vector<HopStart>& out);

struct RecipePlay {
  float speed_takeoff = 0;       // across on the command after the first jump, units/s
  float top = 0, speed_end = 0;  // the most and at the end, units/s
  int32_t takeoff = -1;          // the frame it first left the ground (-1 never)
  int32_t blocked = 0;           // commands a wall or a step stopped
  Vec3f end;                     // the origin at the end
};

// `frames` played from `origin` with the first frame's velocity (both turned by `turn` degrees
// counter-clockwise), under `move` on `world`; every origin after each command appended to
// `trail` when given.
void play_recipe(std::span<const RecipeFrame> frames, const MoveWorld& world,
                 const MoveSettings& move, Vec3f origin, float turn, int lead, RecipePlay& out,
                 std::vector<Vec3f>* trail = nullptr);

}  // namespace ge::shooter
