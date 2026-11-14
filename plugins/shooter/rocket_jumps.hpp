// READING ROCKET JUMPS OFF A RECORDED PLAYER: his track (one point a frame: the clock, the origin,
// the view) read as hops (hops.hpp: each frame's velocity, on the ground or in the air) and the
// rockets he fired (each seen first and gone, where), into the rocket jumps he made and how.
//
// A ROCKET JUMP is one of his rockets (flown along his view at the shot: the shooter a
// recording names is the player nearest where it first shows, and in a crowd that may be another)
// gone within `feet` of his middle and below it (it burst by him, at the floor or low on a
// wall), not at
// another player standing by (a point-blank shot in a fight shoves the shooter too), after which
// his speed rose by `gain` or more within `gain_s`, the push within 60 degrees of straight away
// from the burst (another's rocket from behind throws him toward it), rising at `least_rise` or
// more, and he flew `least_air_s` or longer. For each, read off the recording:
// - THE KIND by the view at the shot against the way the jump flew (the velocity just after the
//   blast, across): within 60 degrees he looked the way he flew (the shot at the feet), past 120
//   he looked back (the flip), between, to a side; STANDING when slower than `standing` at the
//   shot. The shot's pitch (down positive), the run's speed and its way off the flight's.
// - THE KEYS. A recording carries no keys, so they are read off the move: on the ground a run at
//   speed accelerates nowhere (the keys' push just cancels the friction), so the friction is put
//   back (4 times the speed, against the velocity) and what is left is the wish; in the air there
//   is no friction and the wish is the acceleration across. The wish in the view's frame is the
//   keys: forward or back when it lies within 67.5 degrees of the view's axis, a side likewise.
//   Kept as SEQUENCES (each set of keys held `hold` frames or more, in order: a turn strafed into
//   a flip reads F, F+R, R, B): the run's before the shot, the flight's after the blast, and the
//   keys at the shot itself.
// - THE FLIGHT: the speed and rise just after the blast, where he landed (the first frame on the
//   ground after it), how high and far that was, the air time, the apex, how far the view turned
//   in the air and, for a flip, how long it took to come within 45 degrees of the way he flew.
//
// ge:summary Rocket jumps read off a recorded player: kind, pitch, keys, run, flight, landing
// ge:keywords rocket-jump rj flip back-rocket-jump demo mvd read keys wish strafe-turn
// ge:keywords takeoff landing trick player-skill movement-skill copy-movement
// ge:skill shooter bots
// ge:test tests/shooter_rocket_jumps_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "shooter/hops.hpp"

namespace ge::shooter {

// A rocket a player fired, as a recording shows it: first seen at `t0` at `from`, gone at `t1` at
// `to` (s, the track's clock).
struct RocketShot {
  double t0 = 0, t1 = 0;
  Vec3f from, to;
  bool at_another = false;  // another player stood near where it went: a shot at him, no jump
};

struct RocketJumpReadSettings {
  float feet = 128;          // the rocket gone this near his middle, units
  float gain = 150;          // his speed up by this or more, units/s
  float least_rise = 150;    // and rising this fast or more just after, units/s
  float least_air_s = 0.3f;  // and in the air this long or more (a flight, not a shove), s
  double gain_s = 0.12;      // within this of the rocket's end, s
  double run_s = 0.6;        // the run before the shot read over this long (a turn into it), s
  float standing = 120;      // slower than this at the shot: a standing jump, units/s
  double land_s = 3;         // a landing looked for this long, s
  double window_s = 0.04;    // a velocity or a wish read over this either side of a frame, s
  double shot_s = 0.05;      // the keys at the shot: those held most over this before it, s
  int hold = 2;              // a sequence keeps keys held this many frames or more
  float least_wish = 600;    // a wish weaker than this reads as no key, units/s/s
  float friction = 4;        // the ground's friction the run's wish is read past
  float stopspeed = 100;     // below this the friction reads as at this, units/s
};

enum class RocketJumpKind : uint8_t { feet, side, flip };
const char* rocket_jump_kind_name(RocketJumpKind k);

// The keys a wish reads as.
inline constexpr uint8_t kKeyForward = 1, kKeyBack = 2, kKeyLeft = 4, kKeyRight = 8;
// "F", "B+L", "-" (none).
std::string keys_name(uint8_t keys);

// Sets of keys in the order they were held (the first `count` of `keys`).
inline constexpr int kMostKeySteps = 8;
struct KeySequence {
  uint8_t keys[kMostKeySteps] = {};
  int count = 0;
  bool has(uint8_t bits) const {
    for (int i = 0; i < count; i++)
      if ((keys[i] & bits) != 0) return true;
    return false;
  }
};
// "F>F+R>R>B", "-" (none read).
std::string sequence_name(const KeySequence& s);

struct RocketJump {
  int32_t shot = 0, burst = 0, land = -1;  // frame indices (land -1: cut before a landing)
  double t = 0;                            // the shot, s
  RocketJumpKind kind = RocketJumpKind::feet;
  bool standing = false;
  float pitch = 0;                  // the view's pitch at the shot, down positive, degrees
  float view_off = 0;               // the view's yaw at the shot off the flight's heading, degrees
  float run_speed = 0;              // across at the shot, units/s
  float run_off = 0;                // the run's heading off the flight's, degrees (0 when standing)
  float speed_after = 0, rise = 0;  // across and up just after the blast, units/s
  float burst_off = 0;              // the rocket's end from his middle, units
  Vec3f from, to;                   // where he stood at the shot and where he landed
  float height = 0, across = 0;     // the landing over and across from the shot, units
  float air_s = 0, apex = 0;        // the flight's time, s; its top over the shot, units
  float turned = 0;                 // the view's yaw turned in the air, degrees (+ left)
  float turn_back_s = -1;           // a flip's view within 45 degrees of the flight, s (-1 never)
  uint8_t keys_shot = 0;            // the keys at the shot
  KeySequence keys_run;             // the run's before the shot
  KeySequence keys_air;             // the flight's after the blast
};

// The rocket jumps in `track` (the frames `hops` read off it, on the same indices) by `shots`
// (his own rockets, any order), appended to `out` in time order.
void read_rocket_jumps(std::span<const TrackPoint> track, const HopReading& hops,
                       std::span<const RocketShot> shots, const RocketJumpReadSettings& set,
                       std::vector<RocketJump>& out);

// The keys the wish `wish` (across, units/s/s) reads as with the view's yaw at `yaw` (degrees);
// none when weaker than `least`.
uint8_t keys_of_wish(Vec3f wish, float yaw, float least);

// The wish at frame `i` of `hops` (across, units/s/s): the acceleration over `set.window_s` either
// side, the ground's friction put back on the ground. False when a cut lies that near.
bool read_wish(const HopReading& hops, size_t i, const RocketJumpReadSettings& set, Vec3f& out);

}  // namespace ge::shooter
