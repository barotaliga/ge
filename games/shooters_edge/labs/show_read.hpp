// A KEY ITEM AS IT SHOWS, READ OFF A RECORDING: who takes each key item (the armours, the mega, the
// quad, the launcher) once it is back on its spot, alike for people's recordings and the bots'
// (people's key items hardly lie: the holder or a mate is there as it shows, or an enemy denies
// it; the bots' lie three to four times as long).
//
// At each showing (the item back on a spot whose last taker is known): who of the last taker's team
// and of the other stands alive within kShowOn and kShowNear of it (`on`, `near`). At the going
// after it: the wait from the showing; the taker against the last one (himself, a mate, an enemy);
// whether the taker was within kShowNear at the showing (he waited for it) or came after; and
// whether he took it FULL (an armour taken over 100 armour worn already, the launcher with one
// held: a denial, not a need). Before the first take of a spot nothing is counted (the match's
// start is a rush, not a timing).
//
// The armour worn by type (`armour.wear.*`): of the live samples, the share wearing each armour
// and none, and the armour points while wearing each, so a mean armour reads as who wears what.
//
// ge:summary Lab helper: who takes a key item as it shows, and the armour worn by type,
// ge:summary off recordings
// ge:keywords key-item showing shown taken taker denial holder wait armour-worn armour-type
// ge:keywords se-people
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "shooter/vec.hpp"

namespace shooters_edge::labs {

constexpr float kShowOn = 200;    // on the spot at its showing: within this, units
constexpr float kShowNear = 600;  // near it: within this, units
constexpr int kShowKinds = 7;     // se-people's item kinds (ga ya ra mh quad pent rl)

struct ShowRead {
  int64_t shown = 0;  // showings after a known take
  int64_t mate_on = 0, enemy_on = 0, mate_near = 0, enemy_near = 0, nobody_near = 0;
  int64_t taken = 0;  // goings after a counted showing
  int64_t by_self = 0, by_mate = 0, by_enemy = 0;
  int64_t taker_waited = 0;  // the taker within kShowNear at the showing
  int64_t taker_full = 0;    // taken full (over 100 armour worn, a launcher held)
  std::vector<float> waits;  // the showing to the going, s
};

inline void add(ShowRead& a, const ShowRead& b) {
  a.shown += b.shown;
  a.mate_on += b.mate_on;
  a.enemy_on += b.enemy_on;
  a.mate_near += b.mate_near;
  a.enemy_near += b.enemy_near;
  a.nobody_near += b.nobody_near;
  a.taken += b.taken;
  a.by_self += b.by_self;
  a.by_mate += b.by_mate;
  a.by_enemy += b.by_enemy;
  a.taker_waited += b.taker_waited;
  a.taker_full += b.taker_full;
  a.waits.insert(a.waits.end(), b.waits.begin(), b.waits.end());
}

// A live player at a sample: his slot, team and place.
struct ShowPlayer {
  int slot = -1;
  std::string team;
  ge::shooter::Vec3f at;
};

// One key spot as a recording is read.
struct ShowSpot {
  int kind = -1;
  ge::shooter::Vec3f at;
  int slot = -1;  // the last taker, -1 none yet
  std::string team;
  double shown = -1;     // its showing since the last take (-1: not shown since)
  uint32_t near_at = 0;  // the slots within kShowNear at the showing
};

inline float show_dist(ge::shooter::Vec3f a, ge::shooter::Vec3f b) {
  const ge::shooter::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

// The item shows on `sp` at `t`, the live players as `live`.
inline void show_shown(ShowSpot& sp, ShowRead& r, double t, const std::vector<ShowPlayer>& live) {
  if (sp.slot < 0 || sp.shown >= 0) return;
  sp.shown = t;
  sp.near_at = 0;
  r.shown++;
  bool mon = false, eon = false, mnear = false, enear = false;
  for (const ShowPlayer& p : live) {
    const float d = show_dist(p.at, sp.at);
    if (d > kShowNear) continue;
    if (p.slot >= 0 && p.slot < 32) sp.near_at |= 1u << static_cast<unsigned>(p.slot);
    const bool mate = p.team == sp.team;
    (mate ? mnear : enear) = true;
    if (d <= kShowOn) (mate ? mon : eon) = true;
  }
  r.mate_on += mon;
  r.enemy_on += eon;
  r.mate_near += mnear;
  r.enemy_near += enear;
  r.nobody_near += !mnear && !enear;
}

// The item on `sp` goes at `t`, taken by `taker` (`full`: over 100 armour worn for an armour, a
// launcher held for the launcher).
inline void show_taken(ShowSpot& sp, ShowRead& r, double t, const ShowPlayer& taker, bool full) {
  if (sp.slot >= 0 && sp.shown >= 0) {
    r.taken++;
    if (taker.slot == sp.slot)
      r.by_self++;
    else if (taker.team == sp.team)
      r.by_mate++;
    else
      r.by_enemy++;
    r.taker_waited += taker.slot >= 0 && taker.slot < 32 &&
                      (sp.near_at >> static_cast<unsigned>(taker.slot) & 1u) != 0;
    r.taker_full += full;
    r.waits.push_back(static_cast<float>(t - sp.shown));
  }
  sp.slot = taker.slot;
  sp.team = taker.team;
  sp.shown = -1;
}

// The same going counted into a second read (a spot's own) without moving the spot on: its taker
// is set by show_taken alone.
inline void show_taken_spot(const ShowSpot& sp, ShowRead& r, double t, const ShowPlayer& taker,
                            bool full) {
  ShowSpot copy = sp;
  show_taken(copy, r, t, taker, full);
}

// THE ARMOUR WORN: live samples by the armour type worn (0 none, 1 green, 2 yellow, 3 red) and
// the armour points while wearing it.
struct ArmourWear {
  std::array<int64_t, 4> n{};
  std::array<double, 4> points{};
};

inline void add(ArmourWear& a, const ArmourWear& b) {
  for (size_t k = 0; k < 4; k++) {
    a.n[k] += b.n[k];
    a.points[k] += b.points[k];
  }
}

// The armour type from the item bits (the green 8192, the yellow 16384, the red 32768).
inline int armour_type(int32_t items, int32_t points) {
  if (points <= 0) return 0;
  if ((items & 32768) != 0) return 3;
  if ((items & 16384) != 0) return 2;
  if ((items & 8192) != 0) return 1;
  return 0;
}

}  // namespace shooters_edge::labs
