// THE FIGHT'S END, READ OFF A RECORDING: when an enemy a player fought goes out of his sight alive,
// does the player follow him or go on with his own way? People and bots by one rule (the
// maintainer's rule, M18r.md 6f: an enemy gone is followed only when he is believed killable, or
// he holds the launcher and the follower does not; a key item timed comes first).
//
// A CONTACT of a pair (a watcher, an enemy) runs while the enemy is in a clear line of the
// watcher's eye, gaps under kChaseGapS held. It is a FIGHT once the enemy stood in the watcher's
// view cone kChaseFacedS in all. The fight ENDS GONE when the enemy has been out of the clear line
// kChaseGapS with both alive (an enemy dead, or the watcher dead, ends it otherwise and is not
// read). From then, for kChaseWatchS:
// - FOLLOWED: the watcher came within kChaseNear of where the enemy was last seen, or halved his
//   distance to it (when he stood within kChaseNear already, the gone is not read: he is there);
// - SEEN AGAIN: the enemy came back into his clear line;
// - KILLED: the enemy died within kChaseKillS of going (by anyone: the recording's frags are not
//   read by killer);
// by the enemy's body when last seen (health plus the armour's share it takes: LOW under
// kChaseLowBody) and the launchers (his loaded, the watcher's).
//
// A BOT'S CAUSE (se-team, where the watcher's mind is known): its fight mode kChaseCauseS after the
// enemy was last seen (a second past the fight's beat, once it has thought again: chase, fight)
// or, roaming or backing off, whether its goal lies the way
// the enemy went (the place he was last seen nearer the goal than the bot is) or elsewhere.
//
// ge:summary Lab helper: a fight's end with the enemy gone from sight alive, followed or not, read
// ge:keywords chase follow fight-end gone-from-sight killable launcher-held se-people
// ge:keywords goals-combined
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "shooter/hull.hpp"

namespace shooters_edge::labs {

constexpr double kChaseGapS = 0.5;    // out of the clear line this long: gone, s
constexpr double kChaseFacedS = 0.5;  // the enemy in the view cone this long makes a fight, s
constexpr double kChaseWatchS = 4;    // the watcher's way read this long after the gone, s
constexpr double kChaseKillS = 6;     // the enemy's death within this of the gone counts, s
constexpr float kChaseNear = 192;     // came this near where he was last seen: followed, units
constexpr float kChaseLowBody = 80;   // a body (health + armour taken) under this: low
constexpr double kChaseCauseS = 1.5;  // a bot's cause read this long after he was last seen, s

// A player at a sample, as the read wants him.
struct ChasePlayer {
  bool live = false;
  int team = -1;  // a number per team name (the caller's)
  ge::shooter::Vec3f at;
  float body = 0;   // health + the armour's share it would take
  bool rl = false;  // a launcher with rockets
  // A bot's mind (se-team; -1 unknown, people): 0 chase, 1 fight, 2 roam, 3 retreat; its goal.
  int mode = -1;
  bool has_goal = false;
  ge::shooter::Vec3f goal;
};

// The causes a bot's gone is put down to (ChaseRead::cause_*).
enum ChaseCause : uint8_t {
  kCauseChase,
  kCauseFight,
  kCauseGoalThatWay,
  kCauseElsewhere,
  kCauses,
  kCausePending = kCauses,  // read later (kChaseCauseS)
};
inline const char* chase_cause_name(int c) {
  return c == kCauseChase         ? "chase"
         : c == kCauseFight       ? "fight"
         : c == kCauseGoalThatWay ? "goal-that-way"
                                  : "elsewhere";
}

// The armour's share of a hit by its type (show_read.hpp's armour_type: 0 none, 1 green, 2
// yellow, 3 red).
inline float chase_body(int32_t health, int32_t armour, int type) {
  const float share = type == 3 ? 0.8f : type == 2 ? 0.6f : type == 1 ? 0.3f : 0.f;
  // The armour takes share of each hit until its points run out: the damage that kills is the
  // health and the armour's points, or the health / (1 - share) when the health goes first.
  const float soaked = share > 0 ? std::fmin(static_cast<float>(armour),
                                             static_cast<float>(health) * share / (1 - share))
                                 : 0.f;
  return static_cast<float>(health) + soaked;
}

// The tally, by the enemy's state when last seen: [0] his body high, [1] low; and with the
// launchers: [2] he holds one and the watcher not, [3] otherwise.
struct ChaseRead {
  int64_t fights = 0, gone = 0, died = 0;  // fights, ended gone, ended in a death
  int64_t by_n[4] = {0, 0, 0, 0}, by_followed[4] = {0, 0, 0, 0};
  int64_t followed = 0, seen_again = 0;
  int64_t kill_followed = 0, kill_left = 0;  // the enemy dead within kChaseKillS, by the choice
  int64_t near_already = 0;                  // gone while the watcher stood at the spot: not read
  double faced_s = 0;                        // the fights' time in the cone, summed
  int64_t cause_n[kCauses] = {}, cause_followed[kCauses] = {};  // a bot's gones by cause
};

inline void add(ChaseRead& a, const ChaseRead& b) {
  a.fights += b.fights;
  a.gone += b.gone;
  a.died += b.died;
  for (int k = 0; k < 4; k++) a.by_n[k] += b.by_n[k], a.by_followed[k] += b.by_followed[k];
  a.followed += b.followed;
  a.seen_again += b.seen_again;
  a.kill_followed += b.kill_followed;
  a.kill_left += b.kill_left;
  a.near_already += b.near_already;
  a.faced_s += b.faced_s;
  for (int k = 0; k < kCauses; k++)
    a.cause_n[k] += b.cause_n[k], a.cause_followed[k] += b.cause_followed[k];
}

// One pair's state.
struct ChasePair {
  bool contact = false, fight = false;
  double first = 0, last = 0, faced = 0;  // the contact's start, last clear sample, cone time
  ge::shooter::Vec3f lost_at;             // the enemy's place at the last clear sample
  float body = 0;
  bool his_rl = false, my_rl = false;
  // The watch after a gone.
  bool watching = false;
  double gone_t = 0;
  float d0 = 0, dmin = 0;
  bool again = false;
  int cls_body = 0, cls_rl = 0;
  int cause = -1;  // a bot's (ChaseCause), -1 unknown
};

inline float chase_dist(ge::shooter::Vec3f a, ge::shooter::Vec3f b) {
  const ge::shooter::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

// Closes a watch: followed or left, killed or not.
inline void chase_close(ChasePair& p, ChaseRead& r, bool killed) {
  const bool fol = p.dmin <= kChaseNear || p.dmin <= p.d0 * 0.5f;
  r.followed += fol;
  r.seen_again += p.again;
  r.by_n[p.cls_body]++;
  r.by_followed[p.cls_body] += fol;
  r.by_n[p.cls_rl]++;
  r.by_followed[p.cls_rl] += fol;
  (fol ? r.kill_followed : r.kill_left) += killed;
  if (p.cause >= 0 && p.cause < kCauses) {
    r.cause_n[p.cause]++;
    r.cause_followed[p.cause] += fol;
  }
  p.watching = false;
}

// One sample: `pairs` 32 x 32 by (watcher, enemy), `clear` and `cone` the same (the enemy in a
// clear line of the watcher's eye; and in his view cone), `players` by slot, `dt` the sample's
// length. Watches past kChaseKillS close; a watch whose enemy died closes as killed.
inline void chase_sample(std::vector<ChasePair>& pairs, ChaseRead& r, double t, double dt,
                         const std::vector<ChasePlayer>& players, const std::vector<uint8_t>& clear,
                         const std::vector<uint8_t>& cone) {
  const size_t n = players.size();
  for (size_t i = 0; i < n; i++)
    for (size_t j = 0; j < n; j++) {
      if (i == j) continue;
      ChasePair& p = pairs[i * n + j];
      const ChasePlayer& me = players[i];
      const ChasePlayer& him = players[j];
      // The watch: the watcher's way, the enemy seen again, the enemy's death.
      if (p.watching) {
        if (!him.live) {
          chase_close(p, r, t - p.gone_t <= kChaseKillS);
        } else if (t - p.gone_t > kChaseKillS) {
          chase_close(p, r, false);
        } else if (me.live && t - p.gone_t <= kChaseWatchS) {
          p.dmin = std::fmin(p.dmin, chase_dist(me.at, p.lost_at));
          p.again = p.again || clear[i * n + j] != 0;
          if (p.cause == kCausePending && t - p.gone_t >= kChaseCauseS) {
            p.cause = me.mode == 0   ? kCauseChase
                      : me.mode == 1 ? kCauseFight
                      : me.mode < 0  ? -1
                      : me.has_goal && chase_dist(me.goal, p.lost_at) < chase_dist(me.goal, me.at)
                          ? kCauseGoalThatWay
                          : kCauseElsewhere;
          }
        }
      }
      if (!me.live || !him.live || me.team == him.team) {
        if (p.contact && p.fight && !(me.live && him.live)) r.died++;
        p.contact = p.fight = false;
        continue;
      }
      if (clear[i * n + j]) {
        if (!p.contact) {
          p.contact = true;
          p.fight = false;
          p.first = t;
          p.faced = 0;
        }
        p.last = t;
        p.lost_at = him.at;
        p.body = him.body;
        p.his_rl = him.rl;
        p.my_rl = me.rl;
        if (cone[i * n + j]) p.faced += dt;
        if (!p.fight && p.faced >= kChaseFacedS) {
          p.fight = true;
          r.fights++;
        }
      } else if (p.contact && t - p.last > kChaseGapS) {
        p.contact = false;
        if (!p.fight) continue;
        r.gone++;
        r.faced_s += p.faced;
        const float d0 = chase_dist(me.at, p.lost_at);
        if (d0 <= kChaseNear) {
          r.near_already++;
          continue;
        }
        if (p.watching) chase_close(p, r, false);  // a watch still open: closed as it stands
        p.watching = true;
        p.gone_t = p.last;
        p.d0 = p.dmin = d0;
        p.again = false;
        p.cls_body = p.body < kChaseLowBody ? 1 : 0;
        p.cls_rl = p.his_rl && !p.my_rl ? 2 : 3;
        p.cause = me.mode < 0 ? -1 : kCausePending;
      }
    }
}

}  // namespace shooters_edge::labs
