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
// THE FIGHT AT A SHOWING (`showfight.<item>.*`): the deaths within kShowNear of a key spot from
// kFightBefore_s before its showing to kFightAfter_s after (a fixed window, so an item left lying
// draws no more deaths than one taken), each death's killer the enemy whose frags rose nearest in
// time (within kKillerGap_s; none for a fall or a suicide). Per showing: fought or not; per death:
// one of the last taker's team (the holders) or a comer, the victim's and the killer's launcher
// and armour a sample before. Who wins a launcher's fight (one of the two carrying it), who wins
// on armour (50 points apart or more), and whether the last killer's team takes the item. At the
// showing: a STRONG player within kShowNear (the launcher and kStrongArmour or more), of either
// team and of the holders'.
//
// ge:summary Lab helper: who takes a key item as it shows, and the armour worn by type,
// ge:summary off recordings
// ge:keywords key-item showing shown taken taker denial holder wait armour-worn armour-type
// ge:keywords se-people fight-at-showing killer rl-duel armour-duel
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "shooter/vec.hpp"

namespace shooters_edge::labs {

constexpr float kShowOn = 200;    // on the spot at its showing: within this, units
constexpr float kShowNear = 600;  // near it: within this, units
constexpr int kShowKinds = 7;
constexpr int32_t kStrongArmour = 100;  // strong: the launcher and this much armour     //
                                        // se-people's item kinds (ga ya ra mh quad pent rl)

struct ShowRead {
  int64_t shown = 0;  // showings after a known take
  int64_t mate_on = 0, enemy_on = 0, mate_near = 0, enemy_near = 0, nobody_near = 0;
  int64_t taken = 0;  // goings after a counted showing
  int64_t by_self = 0, by_mate = 0, by_enemy = 0;
  int64_t taker_waited = 0;  // the taker within kShowNear at the showing
  int64_t taker_full = 0;    // taken full (over 100 armour worn, a launcher held)
  int64_t taker_rl = 0;      // the taker carrying the launcher
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
  a.taker_rl += b.taker_rl;
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
// launcher held for the launcher; `rl` the taker carrying the launcher).
inline void show_taken(ShowSpot& sp, ShowRead& r, double t, const ShowPlayer& taker, bool full,
                       bool rl = false) {
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
    r.taker_rl += rl;
    r.waits.push_back(static_cast<float>(t - sp.shown));
  }
  sp.slot = taker.slot;
  sp.team = taker.team;
  sp.shown = -1;
}

// The same going counted into a second read (a spot's own) without moving the spot on: its taker
// is set by show_taken alone.
inline void show_taken_spot(const ShowSpot& sp, ShowRead& r, double t, const ShowPlayer& taker,
                            bool full, bool rl = false) {
  ShowSpot copy = sp;
  show_taken(copy, r, t, taker, full, rl);
}

// THE ARMOUR WORN: live samples by the armour type worn (0 none, 1 green, 2 yellow, 3 red) and
// the armour points while wearing it.
struct ArmourWear {
  std::array<int64_t, 4> n{};
  std::array<double, 4> points{};
  int64_t strong = 0;  // the launcher and kStrongArmour or more
};

inline void add(ArmourWear& a, const ArmourWear& b) {
  for (size_t k = 0; k < 4; k++) {
    a.n[k] += b.n[k];
    a.points[k] += b.points[k];
  }
  a.strong += b.strong;
}

// The armour type from the item bits (the green 8192, the yellow 16384, the red 32768).
inline int armour_type(int32_t items, int32_t points) {
  if (points <= 0) return 0;
  if ((items & 32768) != 0) return 3;
  if ((items & 16384) != 0) return 2;
  if ((items & 8192) != 0) return 1;
  return 0;
}

// THE FIGHT AT A SHOWING (above): what a recording logs, joined once it is read.
constexpr double kFightBefore_s = 5;  // deaths this long before a showing count for it
constexpr double kFightAfter_s = 10;  // and this long after
constexpr double kKillerGap_s = 0.3;  // a frag this near in time to a death is its killer's

struct FightSide {  // a player at a death or a frag: his team, launcher and armour a sample before
  int slot = -1;
  std::string team;
  bool rl = false;
  int32_t armour = 0;
};

struct FightDeath {
  double t = 0;
  ge::shooter::Vec3f at;
  FightSide victim;
};

struct FightFrag {  // a slot's frags rose
  double t = 0;
  FightSide killer;
};

struct FightShow {
  int area = -1, kind = -1;
  ge::shooter::Vec3f at;
  double t = 0;         // shown
  std::string holders;  // the last taker's team
  double gone = -1;     // taken (-1: not before the recording ended or it showed again)
  std::string taker;    // the taker's team
  bool strong = false, strong_holder = false;  // a strong player near at the showing, a holder
};

struct FightLog {
  std::vector<FightDeath> deaths;
  std::vector<FightFrag> frags;
  std::vector<FightShow> shows;
};

struct FightRead {
  int64_t shows = 0, fought = 0;               // showings, those with a death in the window
  int64_t deaths = 0, holders = 0, known = 0;  // deaths, the holders', a killer known
  int64_t victim_rl = 0, killer_rl = 0;        // of the known: the victim's launcher, the killer's
  int64_t rl_duels = 0, rl_won = 0;            // one of the two carrying it: the carrier the killer
  int64_t armour_duels = 0, armour_won = 0;    // armour 50 apart: the better armoured the killer
  double victim_armour = 0, killer_armour = 0;  // summed over the known
  int64_t decided = 0,
          winner_took = 0;  // fought and taken, the last death's killer known: his team
  int64_t strong = 0, strong_holder = 0;  // showings with a strong player near, a holder
};

inline void add(FightRead& a, const FightRead& b) {
  a.shows += b.shows;
  a.fought += b.fought;
  a.deaths += b.deaths;
  a.holders += b.holders;
  a.known += b.known;
  a.victim_rl += b.victim_rl;
  a.killer_rl += b.killer_rl;
  a.rl_duels += b.rl_duels;
  a.rl_won += b.rl_won;
  a.armour_duels += b.armour_duels;
  a.armour_won += b.armour_won;
  a.victim_armour += b.victim_armour;
  a.killer_armour += b.killer_armour;
  a.decided += b.decided;
  a.winner_took += b.winner_took;
  a.strong += b.strong;
  a.strong_holder += b.strong_holder;
}

// The shown item on `area` goes at `t` to `taker_team`: its open showing closed.
inline void fight_taken(FightLog& log, int area, double t, const std::string& taker_team) {
  for (size_t i = log.shows.size(); i-- > 0;) {
    FightShow& sh = log.shows[i];
    if (sh.area != area) continue;
    if (sh.gone < 0) {
      sh.gone = t;
      sh.taker = taker_team;
    }
    return;
  }
}

// The killer of `d`: the frag of an enemy nearest in time within kKillerGap_s, or null.
inline const FightFrag* fight_killer(const FightLog& log, const FightDeath& d) {
  const FightFrag* best = nullptr;
  double gap = kKillerGap_s;
  for (const FightFrag& f : log.frags) {
    if (f.t < d.t - kKillerGap_s) continue;
    if (f.t > d.t + kKillerGap_s) break;
    if (f.killer.team == d.victim.team || f.killer.slot == d.victim.slot) continue;
    const double g = std::fabs(f.t - d.t);
    if (g <= gap) gap = g, best = &f;
  }
  return best;
}

// Every showing in `log` read against its deaths into `out`, by the item's kind.
template <size_t N>
void fight_join(const FightLog& log, std::array<FightRead, N>& out) {
  for (const FightShow& sh : log.shows) {
    if (sh.kind < 0 || static_cast<size_t>(sh.kind) >= N) continue;
    FightRead& r = out[static_cast<size_t>(sh.kind)];
    r.shows++;
    r.strong += sh.strong;
    r.strong_holder += sh.strong_holder;
    int64_t n = 0;
    const FightFrag* last = nullptr;
    double last_t = -1;
    for (const FightDeath& d : log.deaths) {
      if (d.t < sh.t - kFightBefore_s || d.t > sh.t + kFightAfter_s) continue;
      if (show_dist(d.at, sh.at) > kShowNear) continue;
      n++;
      r.deaths++;
      r.holders += d.victim.team == sh.holders;
      const FightFrag* k = fight_killer(log, d);
      if (sh.gone < 0 || d.t <= sh.gone) last = k, last_t = d.t;
      if (k == nullptr) continue;
      const FightSide& v = d.victim;
      const FightSide& w = k->killer;
      r.known++;
      r.victim_rl += v.rl;
      r.killer_rl += w.rl;
      r.victim_armour += v.armour;
      r.killer_armour += w.armour;
      if (v.rl != w.rl) {
        r.rl_duels++;
        r.rl_won += w.rl;
      }
      if (std::abs(v.armour - w.armour) >= 50) {
        r.armour_duels++;
        r.armour_won += w.armour > v.armour;
      }
    }
    r.fought += n > 0;
    if (last != nullptr && last_t >= 0 && sh.gone >= 0) {
      r.decided++;
      r.winner_took += sh.taker == last->killer.team;
    }
  }
}

}  // namespace shooters_edge::labs
