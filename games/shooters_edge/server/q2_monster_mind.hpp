// QUAKE II'S MONSTERS' MIND, AS ANY RULES RUN IT: the decisions a Quake II monster takes, in one
// place, so Quake II's own rules (q2_monsters.cpp) and another game's rules wearing Quake II's
// monsters (classic remix's `cr_monsters q2` on Quake's campaign, rules/campaign_q2_kinds.cpp)
// decide alike: when it sees, when it attacks, which attack, whether it keeps on, whom it turns on
// when hurt. Header-only and plain (numbers in, a decision out, random numbers from the caller's
// draw in the order the caller's world draws them), so a rules package that links nothing but the
// program interface can include it. The rules and numbers are porting/shooters-edge/
// q2-monsters.md's and q2-monsters-full.md's.
//
// - **Ranges** (origin to origin): melee under 80, near under 500, mid under 1000, else far.
// - **Sight**: never far; never through anything opaque; at melee range whichever way it faces;
//   near, the foe it already hunts or a player who fired in the last second, whichever way; else
//   only in front (the cosine on the ground plane above 0.3).
// - **Attacking**: facing within 45 degrees first. At melee range a blow (three times in four
//   nothing at the easiest skill, but for a boss), the second of two at even odds; a kind with no
//   blow fires instead. Else, its wait spent and the foe not far, a chance a frame: 0.4 holding its
//   ground, 0.2 melee, 0.1 near, 0.02 mid, halved at skill 0 and doubled at 2 and 3 (the hornet
//   0.8, the riders 0.8, 0.4, 0.2, by no skill). After a ranged attack it waits 2 × random s; its
//   first shot after finding a foe waits 1 s.
// - **Which ranged attack**: the first nearer than the kind's `close`; the tank's by range (within
//   125 the gun 60%, within 250 even, else a third each); else by weight, an attack with a reach of
//   its own left out beyond it. An attack it may not fire this near (`closest`) is held.
// - **Another loop**: by the attack's chance, while the kind's most loops are not spent, the skill
//   is at least the attack's, the foe lives and a blow's foe is in reach (the mutant at skill 3 by
//   chance) or a shot's foe in a clear line.
// - **The mutant's leap**: the heights overlap (its feet below three quarters of his height, its
//   top above a quarter) and he is 100 or more away on the ground plane, then one time in ten.
// - **Hurt by another monster**: one that moves as it does (walks, flies or swims alike), of
//   another kind and not one of the heavy kinds, or that was aiming at it, it turns on; else it
//   joins the attacker against the attacker's foe.
// - **Hearing**: with no enemy, a player's noise within 1000 it may hear (an ambusher must see
//   where) turns it to the noise and sends it there, its first shot a second off; within 64 it
//   stands, and 5 s on forgets it.
// - **Pain**: at most every 3 s; at the hardest skill each blow puts the next 5 s off.
// - **The chase** (a foe out of sight): first to where it last saw him; there, and at each mark
//   after, the search restarts (5 s) and it takes the next mark of the players' trail newer than
//   its last look, facing the way he went; 20 s past a search's start it makes one move straight
//   at him. The trail is 8 marks: a player's place at the last frame is marked once his eye no
//   longer sees the newest mark.
//
// ge:summary Quake II's monsters' decisions as any rules run them: sight, attack, pick, loop, fight
// ge:keywords q2 monster-mind monster-ai cr_monsters remix real-ai attack-chance pick-ranged
// ge:keywords sight in-front infighting leap-test attack-loop q2_attack_chance
// ge:keywords chase pursue last-sighting player-trail trail-marks next-mark
// ge:skill shooter progs
// ge:test tests/shooters_edge_q2_monster_mind_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

#include "shooters_edge/server/q2_monster_kinds.hpp"

namespace shooters_edge::server::q2_mind {

using roster::Attack;
using roster::Fire;
using roster::Kind;
using roster::Test;

enum class Range : uint8_t { melee, near, mid, far };

inline constexpr float kMelee = 80, kNear = 500, kMid = 1000;
inline constexpr float kInFront = 0.3f;      // the cosine of in front
inline constexpr float kFaceWithin = 45;     // degrees off the foe it may attack from
inline constexpr double kFirstWait = 1;      // s from finding a foe to its first shot
inline constexpr double kRefire = 2;         // × random s after a ranged attack
inline constexpr double kPainEvery = 3;      // s
inline constexpr double kPainEveryHard = 5;  // s at the hardest skill
inline constexpr float kLeapLeast = 100;     // the mutant's leap: the least ground distance
inline constexpr double kLeapChance = 0.1;
inline constexpr double kBlowSkipped = 0.75;  // at skill 0, the chance a blow is let go
inline constexpr float kHear = 1000;          // a noise's reach
inline constexpr float kSoundNear = 64;       // near enough the noise: it stands there
inline constexpr double kSoundForget = 5;     // s at the noise before it forgets it

constexpr Range range_of(float dist) {
  return dist < kMelee  ? Range::melee
         : dist < kNear ? Range::near
         : dist < kMid  ? Range::mid
                        : Range::far;
}

// A boss with its own attack test (the hornet, the riders): never lets a blow go by skill.
constexpr bool boss(Test t) { return t == Test::hover || t == Test::rider; }

// What a monster knows of a foe when it looks.
struct Sighting {
  Range range = Range::far;
  bool clear = false;   // nothing opaque from its eye to him
  bool hunted = false;  // he is the foe it already has
  bool fired = false;   // a player who fired in the last second
  bool ahead = false;   // in front (the ground plane's cosine above kInFront)
};

constexpr bool sees(const Sighting& s) {
  if (s.range == Range::far || !s.clear) return false;
  if (s.range == Range::melee) return true;
  if (s.range == Range::near && (s.hunted || s.fired)) return true;
  return s.ahead;
}

// Whether the direction (`dx`, `dy`, already of length 1 on the ground plane) lies in front of a
// face whose forward is (`fx`, `fy`).
constexpr bool in_front(float dx, float dy, float fx, float fy) {
  return dx * fx + dy * fy > kInFront;
}

// How far `yaw` is off `want`, in -180..180 degrees.
constexpr float yaw_off(float yaw, float want) {
  float off = yaw - want;
  while (off > 180) off -= 360;
  while (off < -180) off += 360;
  return off;
}

constexpr bool faces(float yaw, float want) {
  const float off = yaw_off(yaw, want);
  return (off < 0 ? -off : off) <= kFaceWithin;
}

// The chance a frame of a ranged attack, its wait spent and the foe not far.
constexpr double attack_chance(Test test, bool hold, Range r, int skill) {
  if (test == Test::hover) return hold ? 0.4 : 0.8;
  if (test == Test::rider)
    return hold ? 0.4 : r == Range::melee ? 0.8 : r == Range::near ? 0.4 : 0.2;
  double chance = hold ? 0.4 : r == Range::melee ? 0.2 : r == Range::near ? 0.1 : 0.02;
  if (skill == 0) chance *= 0.5;
  if (skill >= 2) chance *= 2;
  return chance;
}

// Whether a blow at melee range is let go this frame (the easiest skill, three times in four);
// draws only when it may.
template <class Roll>
bool blow_skipped(Test test, int skill, Roll&& roll) {
  return !boss(test) && skill == 0 && roll() < kBlowSkipped;
}

// The blow of a kind's two at melee range: the second at even odds where it has one (0 or 1).
template <class Roll>
int pick_blow(const Kind& k, Roll&& roll) {
  return k.melee[1].fire != Fire::none && roll() < 0.5 ? 1 : 0;
}

constexpr int ranged_count(const Kind& k) {
  int n = 0;
  while (n < 3 && k.missile[n].fire != Fire::none) n++;
  return n;
}

// The ranged attack chosen at `dist` (2 + its index among the kind's ranged attacks).
template <class Roll>
int pick_ranged(const Kind& k, float dist, Roll&& roll) {
  const int n = ranged_count(k);
  if (k.close > 0 && dist < k.close) return 2;
  if (k.by_range && n == 3) {  // the tank: blaster or gun within 125 (60/40) and 250 (even)
    const double r = roll();
    if (dist <= 125) return r < 0.4 ? 4 : 2;
    if (dist <= 250) return r < 0.5 ? 4 : 2;
    return r < 1.0 / 3 ? 2 : r < 2.0 / 3 ? 3 : 4;  // a third each farther
  }
  const auto weight = [&](int i) {
    const Attack& a = k.missile[i];
    return a.farthest > 0 && dist > a.farthest ? 0.0f : a.weight;
  };
  float total = 0;
  for (int i = 0; i < n; i++) total += weight(i);
  const float r = float(roll()) * total;
  float sum = 0;
  for (int i = 0; i < n; i++) {
    sum += weight(i);
    if (r < sum) return 2 + i;
  }
  return 2 + n - 1;
}

// What a monster knows at the end of a loop of its attack.
struct Loop {
  int loops = 0;  // loops it went on for already
  int skill = 0;
  bool foe_alive = false;
  bool blow = false;  // a blow (else a shot)
  Range range = Range::far;
  bool mutant = false;  // the mutant's reach at skill 3
  bool clear = false;   // a clear line to the foe
  double extra = 0;     // a chance added by the world's rules
};

// Whether the attack goes on for another loop; draws only once the rest allows it.
template <class Roll>
bool goes_on(const Attack& a, const Loop& l, Roll&& roll) {
  if (a.again <= 0 || (a.again_max > 0 && l.loops >= a.again_max)) return false;
  if (l.skill < a.again_skill) return false;
  if (!l.foe_alive) return false;
  if (l.blow) {
    if (l.range != Range::melee && !(l.skill == 3 && l.mutant)) return false;
  } else if (!l.clear) {
    return false;
  }
  return roll() < a.again + l.extra;
}

// The mutant's leap: its feet and top against his bottom and height, `ground` the distance across.
template <class Roll>
bool leap_test(float my_feet, float my_top, float his_bottom, float his_height, float ground,
               Roll&& roll) {
  if (my_feet > his_bottom + 0.75f * his_height) return false;
  if (my_top < his_bottom + 0.25f * his_height) return false;
  if (ground < kLeapLeast) return false;
  return roll() < kLeapChance;
}

// The kinds another monster's shot never sets against their attacker.
inline constexpr std::string_view kHeavy[] = {"monster_tank", "monster_tank_commander",
                                              "monster_supertank", "monster_makron",
                                              "monster_jorg"};

constexpr bool heavy(std::string_view classname) {
  for (std::string_view h : kHeavy)
    if (classname == h) return true;
  return false;
}

// Hurt by another monster: whom it turns on.
enum class Turn : uint8_t { none, attacker, attackers_foe };

struct Attacker {
  bool alike = false;      // it walks, flies or swims as the one it hurt does
  bool same_kind = false;  // of the hurt one's kind
  bool heavy = false;      // one of the heavy kinds (where the world's rules count them)
  bool aiming_at_me = false;
  bool has_live_foe = false;
};

constexpr Turn infight(const Attacker& a) {
  if ((a.alike && !a.same_kind && !a.heavy) || a.aiming_at_me) return Turn::attacker;
  if (a.has_live_foe) return Turn::attackers_foe;
  return Turn::none;
}

// ---- The chase ----

inline constexpr int kTrailMarks = 8;
inline constexpr double kSearchAgain = 5;    // s a search lasts from a mark reached
inline constexpr double kSearchGiveUp = 20;  // s past a search's start: one move straight at him

// The newest mark of a trail whose oldest is `head`.
constexpr int newest_mark(int head) { return (head + kTrailMarks - 1) % kTrailMarks; }

// The trail mark after a monster's last look (`after`, a time): the first newer than it from the
// oldest on; from the last sighting (`first`), the one before it when only that one is in sight
// (`sees(i)`). -1 when there is none.
template <class Sees>
int next_mark(const double (&times)[kTrailMarks], int head, double after, bool first, Sees&& sees) {
  for (int n = 0; n < kTrailMarks; n++) {
    const int i = (head + n) % kTrailMarks;
    if (times[i] <= after) continue;
    if (!first || sees(i)) return i;
    const int before = (i + kTrailMarks - 1) % kTrailMarks;
    if (n > 0 && times[before] > 0 && sees(before)) return before;
    return i;
  }
  return -1;
}

// One frame of the chase of a foe out of sight. `M` holds the chase's state (`lost`, `on_trail`,
// `side_step`, `goal`, `real_goal`, `search_time`, `trail_time`); `W` is the world: `now()`,
// `last_sight()`, `foe_at()`, `far_from(goal)` (not yet there), `next_mark(first)`, `mark_at(i)`,
// `mark_time(i)`, `face(yaw)` with `mark_yaw(i)`, and `correct()` (a side step round what blocks
// the new goal, where the world has one).
template <class M, class W>
void pursue(M& m, W& w) {
  if (!m.lost) {
    m.lost = true;
    m.on_trail = m.side_step = false;
    m.goal = w.last_sight();
    w.correct();
    return;
  }
  if (m.search_time > 0 && w.now() > m.search_time + kSearchGiveUp) {
    m.goal = w.foe_at();
    m.side_step = false;
    m.search_time = 0;
    return;
  }
  if (w.far_from(m.goal)) return;  // not there yet
  m.search_time = w.now() + kSearchAgain;
  if (m.side_step) {  // round the obstacle: on to the point it was making for
    m.side_step = false;
    m.goal = m.real_goal;
    w.correct();
    return;
  }
  const int i = w.next_mark(!m.on_trail);
  if (i < 0) return;
  m.on_trail = true;
  m.goal = w.mark_at(i);
  m.trail_time = w.mark_time(i);
  w.face(w.mark_yaw(i));
  w.correct();
}

}  // namespace shooters_edge::server::q2_mind
