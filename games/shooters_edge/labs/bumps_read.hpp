// THE BUMPS READ OFF A RECORDING: how often a player runs into a wall (or another player), read off
// how his body moved, alike for people's recordings and the bots' (a recording carries no
// velocity; se-team's own bump count reads the server's, which people's recordings lack).
//
// At every update of a live, dry player (three in a row, each gap under kKeyGapHi): the flat speed
// over the last update against the one before. A BUMP is a fall from kBumpFrom or more to under
// kBumpKeep of it and by kBumpLoss at least (a frame's speed is off by up to a third, so a smaller
// fall is the recording's noise) with no health lost over the three (a hit's knock is not a bump):
//
// - ANOTHER PLAYER's box within kBumpTouch of his: a player bump;
// - else his box, a step up (a stair's riser is climbed, not a wall), meets a wall (a plane neither
//   floor nor ceiling) within kBumpReach along the way he was moving: a wall bump, on the ground
//   or in the air by where he stood before it;
// - else nothing (a stop of his own: keys let go, a turn on the spot).
//
// ge:summary Lab helper: wall and player bumps read off a recording's motion, people and bots alike
// ge:keywords bumps wall-bump player-bump speed-lost recording se-people people-bumps
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cmath>
#include <cstdint>

#include "shooter/hull.hpp"
#include "shooters_edge/labs/keys_read.hpp"

namespace shooters_edge::labs {

constexpr double kBumpFrom = 250;  // the speed before a bump, at least, u/s
constexpr double kBumpKeep = 0.5;  // the speed after it under this share of the speed before
constexpr double kBumpLoss = 120;  // and under it by this much at least, u/s
constexpr float kBumpReach = 16;   // a wall this near along the old way stopped him, units
constexpr float kBumpTouch = 40;   // another player's box this near (flat, centre to centre)
constexpr float kBumpStep = 18;    // the step up the wall's trace is made from, units

struct BumpsRead {
  int64_t wall = 0, wall_air = 0, player = 0;  // bumps by kind (wall_air: of the wall's)
  double live_s = 0;                           // the dry, live time read, s
  double fast_s = 0;  // of it, the updates run at kBumpFrom or more (a bump's chance), s
};

inline void add(BumpsRead& a, const BumpsRead& b) {
  a.wall += b.wall;
  a.wall_air += b.wall_air;
  a.player += b.player;
  a.live_s += b.live_s;
  a.fast_s += b.fast_s;
}

// One player's last two updates.
struct BumpState {
  ge::shooter::Vec3f at[2];
  double t[2] = {0, 0};
  float health[2] = {0, 0};
  bool ground[2] = {false, false};
  int n = 0;
};

inline void bumps_reset(BumpState& b) { b.n = 0; }

enum class Bump : uint8_t { none, wall, player };

// An update of a live, dry player at `at` (`ground`: standing), with `health`; `near_other`: some
// other live player's box within kBumpTouch of `at`. `body` is the player's hull.
inline Bump bumps_update(BumpState& b, BumpsRead& r, const ge::shooter::Hull& body, double t,
                         ge::shooter::Vec3f at, bool ground, float health, bool near_other) {
  namespace sh = ge::shooter;
  Bump out = Bump::none;
  if (b.n > 0) {
    const double dt = t - b.t[b.n - 1];
    if (dt <= 0 || dt > kKeyGapHi) b.n = 0;
  }
  if (b.n > 0) r.live_s += t - b.t[b.n - 1];
  if (b.n >= 2) {
    const double dt1 = b.t[1] - b.t[0], dt2 = t - b.t[1];
    const double v1 = keys_flat(b.at[0], b.at[1]) / dt1, v2 = keys_flat(b.at[1], at) / dt2;
    if (v1 >= kBumpFrom && v1 < kKeyFastest) r.fast_s += dt2;
    if (v1 >= kBumpFrom && v1 < kKeyFastest && v2 < v1 * kBumpKeep && v1 - v2 >= kBumpLoss &&
        health >= b.health[0]) {  // (the stats may land an update off the knock)
      if (near_other) {
        out = Bump::player;
      } else {
        const double d = keys_flat(b.at[0], b.at[1]);
        const auto dx = static_cast<float>((b.at[1].x - b.at[0].x) / d),
                   dy = static_cast<float>((b.at[1].y - b.at[0].y) / d);
        sh::Vec3f from{at.x, at.y, at.z + kBumpStep};
        sh::HullTrace tr = sh::trace_hull(
            body, from, {from.x + dx * kBumpReach, from.y + dy * kBumpReach, from.z});
        if (tr.startsolid) {  // no room a step up (a low ceiling): from where he stands
          from = at;
          tr = sh::trace_hull(body, from,
                              {from.x + dx * kBumpReach, from.y + dy * kBumpReach, from.z});
        }
        if (!tr.startsolid && tr.fraction < 1 && tr.plane.normal.z < 0.7f &&
            tr.plane.normal.z > -0.7f)
          out = Bump::wall;
      }
      if (out == Bump::wall) {
        r.wall++;
        r.wall_air += !b.ground[1];
      }
      if (out == Bump::player) r.player++;
    }
  }
  if (b.n >= 2) {
    b.at[0] = b.at[1];
    b.t[0] = b.t[1];
    b.health[0] = b.health[1];
    b.ground[0] = b.ground[1];
  }
  const int slot = b.n >= 2 ? 1 : b.n;
  b.at[slot] = at;
  b.t[slot] = t;
  b.health[slot] = health;
  b.ground[slot] = ground;
  if (b.n < 2) b.n++;
  return out;
}

}  // namespace shooters_edge::labs
