// GAP JUMPS READ OFF A RECORDED PLAYER: the plain jumps (no rocket, no grenade) he makes OVER a
// gap, a pit or a drop he would fall into, from one floor to another: the strafe jump over a
// pit of lava people make to save the way round, the leap across a chasm. Read off his hops
// (hops.hpp: every jump from takeoff to landing, the velocity at each frame), so a recording
// that holds no keys still tells where he took off, how fast he came in and where he landed.
//
// A GAP JUMP is a hop that
// - took off at `least_speed` or faster across and flew `least_across` or further (a hop on the
//   flat at a hopper's speed is not one: nothing lies under it but the floor);
// - passed over a GAP: under some frame of its flight the player's box, traced down `deep`
//   units through the world and the solid brush models at rest over that spot (a door that is a
//   floor, a bridge: the caller's `models`), finds a floor `gap` or more under the lower of the
//   takeoff and the landing, or none
//   (a pit, a chasm, the void); lava or slime on that floor marks it HARM. A jump down a stair
//   is not one: the floor under it never falls below the landing;
// - flew no longer than a jump falls to its landing's height (`air_slack` more): a "hop" longer
//   than any flight is a walk across a floor the hops did not see (a brush entity's top);
// - was no rocket or grenade jump (`blasts`: the frames from a blast jump's shot to its landing
//   are passed in and skipped) and gained no blast's push in the air (its landing no faster
//   than `boost` over its takeoff).
// For each, the takeoff and landing (origins), the speed across at takeoff, its heading, how far
// across and how high (landing less takeoff), the air time, the gap's depth and whether a RUN
// (a jump at `run` units/s, the move's maxspeed) could make it: the flight's time from the
// jump's rise (`jump_speed`, `gravity`) and the height, times the run's speed, against the
// distance across. One a run cannot make is a SPEED JUMP: the speed must be built first (by
// hopping), which is what a bot copying it has to plan.
//
// Pure, deterministic (floats and core's libm), allocates only `out`. Held by
// tests/shooter_gap_jumps_test.cpp; tabled by the `se-rj` lab (the gap tricks).
//
// ge:summary Gap jumps read off a recorded player: plain jumps over a pit or a chasm, their speed
// ge:keywords gap-jump speed-jump strafe-jump-over long-jump pit chasm lava-jump read demo mvd
// ge:keywords takeoff landing trick copy-movement movement-skill read_gap_jumps
// ge:skill shooter bots
// ge:test tests/shooter_gap_jumps_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "shooter/brush_hull.hpp"
#include "shooter/hops.hpp"
#include "shooter/world.hpp"

namespace ge::shooter {

struct GapJumpReadSettings {
  float least_speed = 300;   // the takeoff's speed across, at least, units/s
  float least_across = 128;  // the flight across, at least, units
  float gap = 64;            // a floor this far under the lower end, or none, is a gap, units
  float deep = 1024;         // the floor looked for this far down, units
  float boost = 120;         // a landing this much faster than the takeoff was pushed: skipped
  float air_slack = 0.1f;    // a flight this much longer than a jump's fall is no flight, s
  float run = 320;           // a run's speed (the move's maxspeed), units/s
  float jump_speed = 270;    // a jump's rise, units/s
  float gravity = 800;       // units/s/s
};

struct GapJump {
  int32_t takeoff = 0, land = 0;  // frame indices of the hop
  double t = 0;                   // the takeoff, s
  Vec3f from, to;                 // the takeoff's and the landing's origins
  float speed = 0;                // across at takeoff, units/s
  float heading = 0;              // the velocity's at takeoff, degrees
  float across = 0, height = 0;   // the landing across and over the takeoff, units
  float air_s = 0;
  float depth = 0;            // the gap's floor under the lower end, units (`deep`: none found)
  bool harm = false;          // lava or slime on the gap's floor
  float run_reach = 0;        // how far across a run's jump flies to the landing's height, units
  bool speed_needed = false;  // `across` beyond `run_reach`: a speed jump
};

// The gap jumps in `track` (its hops read by `hops`, on the same frames, on `world`; the floor
// under a flight traced through `world` and those of `models` over it, any number: a map's solid
// brush entities at rest, in world coordinates), skipping the frames of `blasts` (each [first,
// last] frame of a rocket or grenade jump), appended to `out` in time order.
void read_gap_jumps(std::span<const TrackPoint> track, const HopReading& hops,
                    const MoveWorld& world, std::span<const BrushModel> models,
                    std::span<const std::pair<int32_t, int32_t>> blasts,
                    const GapJumpReadSettings& set, std::vector<GapJump>& out);

// How long a jump (rising at `jump_speed`, falling at `gravity`) flies before it comes down to
// `height` over its takeoff (0 when it never rises that high), s.
float jump_time(float height, float jump_speed, float gravity);
// How far across a jump at `speed` flies in that time, units.
float jump_reach(float speed, float height, float jump_speed, float gravity);

}  // namespace ge::shooter
