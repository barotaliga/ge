// THE LAUNCHER TAKEN, READ OFF A RECORDING: what a team does with a rocket launcher's spot once one
// of its players takes the launcher there, alike for people's recordings and the bots' (the
// maintainer's play: the taker stays, calls it safe and times it, and his mates come to take the
// next one, so the team gains launchers and holds the area).
//
// At each pickup of a launcher (its taker the nearest live player, areas_read.hpp's rule):
// - STAY: of the taker's samples alive over the next kLauncherStayS, the share within
//   kLauncherNear of the spot;
// - WITH ONE ALREADY: whether the taker held a launcher at the sample before (a second launcher is
//   ammunition and a denial, not a launcher more for the team);
// - BACK: when the launcher shows on its spot again, how many of the last taker's team stand alive
//   within kLauncherNear of it WITHOUT a launcher (the mates come to take the next one), and how
//   many of the enemy;
// - THE NEXT TAKER, against the last one: himself, a mate without a launcher (the team gains one),
//   a mate with one, an enemy; and the wait from its showing to its going.
//
// ge:summary Lab helper: what a team does after taking a launcher, read off recordings alike
// ge:keywords launcher rl-control stay mates-waiting next-taker stacking rl-safe se-people
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "shooter/hull.hpp"

namespace shooters_edge::labs {

constexpr double kLauncherStayS = 20;  // the taker's stay read this long after his pickup, s
constexpr float kLauncherNear = 600;   // near the spot: within this, units
constexpr float kLauncherOn = 200;     // on the spot, waiting: within this, units

struct LauncherRead {
  int64_t takes = 0, with_one = 0;  // pickups, and those by a player already holding one
  int64_t stay_n = 0, stay_in = 0;  // the takers' samples alive after, and those near the spot
  int64_t back = 0;                 // showings again after a known take
  int64_t mates_waiting = 0, mates_any = 0, enemies_waiting = 0;  // at a showing, summed
  int64_t mates_on = 0, enemies_on = 0;  // showings with one of each within kLauncherOn
  int64_t next_self = 0, next_mate_without = 0, next_mate_with = 0, next_enemy = 0;
  double next_wait = 0;  // the showing to the going, summed over the next takes after a showing
  int64_t next_waited = 0;
  std::vector<float> waits;         // each of those waits, s
  std::vector<float> waits_mate;    // those with a mate without one waiting at the showing
  std::vector<float> waits_nobody;  // those with nobody of the last taker's team near
};

inline void add(LauncherRead& a, const LauncherRead& b) {
  a.takes += b.takes;
  a.with_one += b.with_one;
  a.stay_n += b.stay_n;
  a.stay_in += b.stay_in;
  a.back += b.back;
  a.mates_waiting += b.mates_waiting;
  a.mates_any += b.mates_any;
  a.enemies_waiting += b.enemies_waiting;
  a.mates_on += b.mates_on;
  a.enemies_on += b.enemies_on;
  a.next_self += b.next_self;
  a.next_mate_without += b.next_mate_without;
  a.next_mate_with += b.next_mate_with;
  a.next_enemy += b.next_enemy;
  a.next_wait += b.next_wait;
  a.next_waited += b.next_waited;
  a.waits.insert(a.waits.end(), b.waits.begin(), b.waits.end());
  a.waits_mate.insert(a.waits_mate.end(), b.waits_mate.begin(), b.waits_mate.end());
  a.waits_nobody.insert(a.waits_nobody.end(), b.waits_nobody.begin(), b.waits_nobody.end());
}

// A live player at a sample: his slot, team, place and whether he holds a launcher.
struct LauncherPlayer {
  int slot = -1;
  std::string team;
  ge::shooter::Vec3f at;
  bool has = false;
};

// One launcher spot's last take.
struct LauncherSpot {
  ge::shooter::Vec3f at;
  int slot = -1;  // the last taker, -1 none yet
  std::string team;
  double t = -1, shown = -1;  // his pickup; its showing again since (-1: not shown since)
  int mates = 0;              // mates without a launcher near at the showing
};

inline float launcher_dist(ge::shooter::Vec3f a, ge::shooter::Vec3f b) {
  const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// A pickup at spot `sp` at `t` by `taker` (who held a launcher at the sample before: `had`).
inline void launcher_take(LauncherSpot& sp, LauncherRead& r, double t, const LauncherPlayer& taker,
                          bool had) {
  r.takes++;
  r.with_one += had;
  if (sp.slot >= 0) {
    if (taker.slot == sp.slot)
      r.next_self++;
    else if (taker.team != sp.team)
      r.next_enemy++;
    else if (had)
      r.next_mate_with++;
    else
      r.next_mate_without++;
    if (sp.shown >= 0) {
      const auto w = static_cast<float>(t - sp.shown);
      r.next_wait += w;
      r.next_waited++;
      r.waits.push_back(w);
      (sp.mates > 0 ? r.waits_mate : r.waits_nobody).push_back(w);
    }
  }
  sp.slot = taker.slot;
  sp.team = taker.team;
  sp.t = t;
  sp.shown = -1;
}

// The launcher shows on `sp` again at `t`, the live players as `live`.
inline void launcher_shown(LauncherSpot& sp, LauncherRead& r, double t,
                           const std::vector<LauncherPlayer>& live) {
  if (sp.slot < 0 || sp.shown >= 0) return;
  sp.shown = t;
  r.back++;
  int mates = 0;
  bool mate_on = false, enemy_on = false;
  for (const LauncherPlayer& p : live) {
    const float d = launcher_dist(p.at, sp.at);
    if (d <= kLauncherOn) (p.team != sp.team ? enemy_on : mate_on) |= p.team != sp.team || !p.has;
    if (d > kLauncherNear) continue;
    if (p.team != sp.team)
      r.enemies_waiting++;
    else if (!p.has)
      mates++;
  }
  r.mates_waiting += mates;
  r.mates_on += mate_on;
  r.enemies_on += enemy_on;
  sp.mates = mates;
  r.mates_any += mates > 0;
}

// A sample at `t`: the last taker's stay read while within kLauncherStayS of his pickup.
inline void launcher_sample(const LauncherSpot& sp, LauncherRead& r, double t,
                            const std::vector<LauncherPlayer>& live) {
  if (sp.slot < 0 || t - sp.t > kLauncherStayS || t <= sp.t) return;
  for (const LauncherPlayer& p : live)
    if (p.slot == sp.slot) {
      r.stay_n++;
      r.stay_in += launcher_dist(p.at, sp.at) <= kLauncherNear;
    }
}

}  // namespace shooters_edge::labs
