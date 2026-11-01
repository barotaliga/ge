// A PLAYER'S MOVEMENT READ OFF HIS TRACK: where he stood and where he looked at each recorded
// frame (a multiview recording gives every player's, a bot's run gives its own), turned into what
// a bunny hopper does and how well: the velocity, the ground under him, his jumps chained into
// hops, and in the air how hard the velocity turned against the most the move allows.
//
// THE MOVE'S AIR CONTROL, which every number here is read against (pmove.cpp's air_accelerate):
// in the air a command adds speed along the wish only up to 30 units/s of it, so the gain is
// largest with the wish at right angles to the velocity: each command then adds at most 30 across
// it, which turns the velocity by atan(30 / v) and raises v^2 by 900 (v grows by about 900 / 2v).
// A hopper holds the side key alone, keeps the view on the velocity (the wish then lies at 90
// degrees) and turns the mouse with the velocity: the turn rate is the gain. Its most at speed v
// and `cmd_hz` commands a second is `max_turn` (degrees/s). On the ground friction takes about
// 4 x v x dt a command, so a hopper jumps on the command he lands: one recorded frame on the
// ground between two jumps is a chain kept, more is speed lost.
//
// THE READING (`read_hops`), frame by frame:
// - CUT: a step longer than `teleport` units, a gap in the clock over `gap_s`, or a dead player:
//   the frames on each side are not differenced across it.
// - VELOCITY: the central difference of the origins round the frame (positions in a recording
//   are rounded to 1/8 unit, so one frame's difference is noisy; two frames' is not).
// - GROUND: the player's box traced `ground_probe` units down hits a plane with normal z >= 0.7.
// - A TAKEOFF is a frame off the ground after one on it, rising by more than 1.5 units into the
//   next. A HOP runs from a takeoff to the next frame on the ground; `ground_before` is how many
//   frames he stood before it (1: the jump came on the landing's command, chained).
// - The TURN is the velocity's heading's rate (degrees/s, positive counter-clockwise, i.e. to the
//   left), the LAG the view's yaw less that heading (wrapped). In the air at more than `fast`
//   units/s, `turn_share` is |turn| / max_turn: 1 is a hopper turning as hard as the move lets.
//
// THE SUMMARY folds the frames into the numbers a lab tables and a pilot is tuned to: jumps a
// minute, the share chained, the longest chain, the time in the air, the speed's median, 90th
// percentile and top, the turn share's median in the air, the lag's median size, the hops'
// gain a second, and the histograms of ground frames before a jump and of the lag.
//
// Pure, deterministic (floats and core's libm), allocates only the reading's vectors. Held by
// tests/shooter_hops_test.cpp; drawn by the `se-moves` and `se-hop` labs.
//
// ge:summary A player's track read as a bunny hopper's: velocity, ground, chained hops, air turn
// ge:keywords hops bunny-hop bhop strafe-jump air-strafe track read_hops chain takeoff landing
// ge:keywords turn-share max_turn view-lag ground-frames speed-gain hop-summary movement-study
// ge:skill shooter bots
// ge:test tests/shooter_hops_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "shooter/vec.hpp"
#include "shooter/world.hpp"

namespace ge::shooter {

// One recorded frame of one player.
struct TrackPoint {
  double t = 0;              // s
  Vec3f origin;              // units
  float yaw = 0, pitch = 0;  // the view, degrees (yaw counter-clockwise from +x)
  bool dead = false;
};

struct HopSettings {
  float teleport = 64;       // a step longer than this in one frame is a cut, units
  double gap_s = 0.06;       // a clock gap longer than this is a cut
  float ground_probe = 2;    // the ground is traced this far down, units
  double cmd_hz = 77;        // the commands a second the air's most turn is read at
  float fast = 330;          // the air's turn is read above this speed, units/s
  int chain_frames = 1;      // a jump this many frames or fewer after landing is chained
  double chain_gap_s = 1.2;  // and a hop chain breaks at a hop longer than this
};

enum class HopPhase : uint8_t { cut, ground, air };

struct HopFrame {
  double t = 0;
  Vec3f origin, velocity;
  float speed = 0;    // across, units/s
  float yaw = 0;      // the view's, degrees
  float heading = 0;  // the velocity's, degrees
  float turn = 0;     // the heading's rate, degrees/s (+ left)
  float lag = 0;      // yaw - heading, wrapped, degrees
  HopPhase phase = HopPhase::cut;
  bool takeoff = false;
};

struct Hop {
  int32_t takeoff = 0, land = 0;  // frame indices
  float air_s = 0;
  float speed_off = 0, speed_land = 0;
  int32_t ground_before = -1;  // frames on the ground since the last landing (-1: first)
  float turned = 0;            // the heading's turn over the hop, degrees (+ left)
  int32_t switches = 0;        // times the turn changed side in the air
  int32_t chain = 0;           // which chain it belongs to
};

inline constexpr int kGroundBins = 12;  // ground frames 0..10, then 11 and more
inline constexpr int kLagBins = 36;     // the lag in 10-degree bins from -180

struct HopSummary {
  double seconds = 0;  // the track's clock
  int32_t frames = 0, jumps = 0, chained = 0, chains = 0, longest_chain = 0;
  float air_share = 0;                                // of the frames not cut
  float speed_p50 = 0, speed_p90 = 0, speed_top = 0;  // over the frames moving above 10 u/s
  float turn_share_p50 = 0;                           // in the air above `fast`
  float lag_p50 = 0;                                  // |lag| in the air above `fast`, degrees
  float gain_per_s = 0;  // the chained hops' speed gained a second of air, median
  std::array<int32_t, kGroundBins> ground_before{};  // jumps by frames on the ground before
  std::array<int32_t, kLagBins> lag{};               // air frames above `fast` by lag
};

struct HopReading {
  std::vector<HopFrame> frames;
  std::vector<Hop> hops;
  HopSummary summary;
};

// The most a velocity of `speed` turns a second in the air at `cmd_hz` commands a second with the
// wish at right angles, degrees/s.
float max_turn(float speed, double cmd_hz);

// `track` (in time order) on `world` (the ground's traces) into `out`.
void read_hops(std::span<const TrackPoint> track, const MoveWorld& world, const HopSettings& set,
               HopReading& out);

}  // namespace ge::shooter
