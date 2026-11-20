// Quake II's poses (q2_poses.hpp): the players' sequences by priority and the monsters' runs
// by what they do, a frame a rules frame.
//
// ge:summary Quake II's poses: the player's priorities and the monsters' runs, stepped at 10 Hz
// ge:keywords q2 poses animation player-animation monster-animation frame-table
// ge:skill shooter
// ge:test tests/shooters_edge_q2_poses_test.cpp
#include "shooters_edge/server/q2_poses.hpp"

#include <cstdint>
#include <string_view>

#include "shooters_edge/server/q2_monster_kinds.hpp"
#include "shooters_edge/server/q2_monster_poses.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

using P = SqPriority;
namespace f = sq_frames;

int count(const SqSpan* spans, int n) {
  int c = 0;
  while (c < n && !spans[c].none()) c++;
  return c;
}

// A pick among `n` that is the same for the same body and frame, and draws nothing from the game's
// stream (a pose changes nothing the game decides).
int pick(int entity, int64_t frame, int n) {
  if (n <= 1) return 0;
  uint32_t h = uint32_t(entity) * 2654435761u ^ uint32_t(frame) * 40503u;
  h ^= h >> 15;
  h *= 2246822519u;
  h ^= h >> 13;
  return int(h % uint32_t(n));
}

// Players.

SqSpan basic_of(const SqPose& p) {
  if (p.ducked) return p.running ? f::kCrouchWalk : f::kCrouchStand;
  return p.running ? f::kRun : f::kStand;
}

void start(SqPose& p, P priority, SqSpan span) {
  p.priority = priority;
  p.frame = span.first - 1;  // the step shows the first
  p.end = span.last;
}

// The basic loop from its first frame (the step shows it).
void start_basic(SqPose& p) {
  p.priority = P::basic;
  const SqSpan b = basic_of(p);
  p.frame = b.first - 1;
  p.end = b.last;
}

// misc_insane's runs, by its model's frames.
namespace insane {
constexpr SqSpan kStandA{59, 64}, kStandB{64, 93}, kGoDown{0, 39}, kGetUp{40, 58},
    kDropDown{95, 99}, kDown{99, 159}, kWalk{160, 172}, kWalkMad{173, 198}, kPainStand{199, 209},
    kDeathStand{210, 226}, kCrawl{227, 235}, kPainLow{236, 244}, kDeathLow{245, 251},
    kCross{252, 266}, kStruggle{267, 281};
bool is(SqSpan a, SqSpan b) { return a.first == b.first && a.last == b.last; }
// Whether `a` is one of his runs (not the spawn's stand over all his first frames).
bool his(SqSpan a) {
  for (SqSpan r : {kStandA, kStandB, kGoDown, kGetUp, kDropDown, kDown, kWalk, kWalkMad, kPainStand,
                   kDeathStand, kCrawl, kPainLow, kDeathLow, kCross, kStruggle})
    if (is(a, r)) return true;
  return false;
}
}  // namespace insane

// The prisoner (misc_insane) by his spawnflags: 4 crawls, 8 hangs on a cross, 16 holds his
// ground, 32 always stands. Standing he plays one of two stands, after each going down three
// times in ten (unless he always stands), by a slow kneel or a drop; down he gets up half the
// time at the loop's end (never if he crawls and holds). Hung, he struggles one loop in five.
// He walks in one of two ways or crawls; his pain and death are the low ones when he is down.
void pose_insane(Q2Server& s, Q2Monster& m, bool pain, bool moving) {
  using Act = SqMonsterPose::Act;
  namespace in = insane;
  SqMonsterPose& p = m.pose;
  const int flags = s.entities[size_t(m.entity)].spawnflags;
  const bool crawl = (flags & 4) != 0, cross = (flags & 8) != 0, hold = (flags & 16) != 0;
  const bool upright = (flags & 32) != 0;
  const int64_t now = s.frame_number;
  auto begin = [&](Act act, SqSpan span) {
    p.act = act;
    p.span = span;
    p.frame = span.first;
  };
  auto chance = [&](int salt, int in_hundred) {
    return pick(m.entity, now * 8 + salt, 100) < in_hundred;
  };
  const bool low = (p.frame >= 98 && p.frame <= 159) || (p.frame >= 227 && p.frame <= 244);
  if (m.dead) {
    if (p.act == Act::death) {
      if (p.frame < p.span.last) p.frame++;
    } else if (cross) {  // he hangs on where he was
      p.act = Act::death;
      p.span = {static_cast<int16_t>(p.frame), static_cast<int16_t>(p.frame)};
    } else {
      begin(Act::death, low || crawl ? in::kDeathLow : in::kDeathStand);
    }
    return;
  }
  if (pain) {
    const SqSpan run = cross ? in::kStruggle : crawl || low ? in::kPainLow : in::kPainStand;
    begin(Act::pain, run);
    m.pain_hold = s.level_time + 0.1 * (run.last - run.first + 1);
    return;
  }
  auto stand = [&](bool after_stand) {
    if (cross) {
      begin(Act::stand, in::kCross);
    } else if (crawl && hold) {
      begin(Act::stand, in::kDown);
    } else if (after_stand && !upright && chance(1, 30)) {
      begin(Act::stand, chance(2, 50) ? in::kGoDown : in::kDropDown);
    } else {
      begin(Act::stand, chance(3, 50) ? in::kStandA : in::kStandB);
    }
  };
  auto walk = [&] {
    begin(Act::walk, crawl ? in::kCrawl : chance(4, 50) ? in::kWalk : in::kWalkMad);
  };
  const bool walking = p.act == Act::walk;
  if (moving && !cross && !walking && p.act != Act::pain) {
    walk();
    return;
  }
  if (!moving && walking) {
    stand(false);
    return;
  }
  if (!in::his(p.span) || (p.act != Act::stand && p.act != Act::walk && p.act != Act::pain)) {
    stand(false);  // his first frame
    return;
  }
  if (p.frame < p.span.last) {
    p.frame++;
    return;
  }
  // A run over: the next.
  const SqSpan was = p.span;
  if (walking) {
    walk();
  } else if (in::is(was, in::kCross)) {
    begin(Act::stand, chance(5, 80) ? in::kCross : in::kStruggle);
  } else if (in::is(was, in::kStruggle)) {
    begin(Act::stand, in::kCross);
  } else if (in::is(was, in::kGoDown) || in::is(was, in::kDropDown) ||
             (in::is(was, in::kPainLow) && (hold || crawl))) {
    begin(Act::stand, in::kDown);
  } else if (in::is(was, in::kDown)) {
    begin(Act::stand, !(crawl && hold) && chance(6, 50) ? in::kGetUp : in::kDown);
  } else {
    stand(in::is(was, in::kStandA) || in::is(was, in::kStandB));
  }
}

}  // namespace

bool sq_can_gesture(const SqPose& p, bool on_ground) {
  return on_ground && !p.ducked && p.priority <= P::gesture;
}

const SqMonsterPoses* sq_monster_poses(std::string_view classname) {
  for (const SqMonsterPoses& p : kSqMonsterPoses)
    if (classname == p.classname) return &p;
  return nullptr;
}

void pose_spawn(Q2Server& s, int slot) {
  Q2Player& pl = s.players[slot];
  SqPose& p = pl.pose;
  p = SqPose{};
  p.health = pl.health;
  p.armour = pl.body.armour_count;
  p.weapon = pl.hand.weapon;
  // The first weapon comes up: the third pain from its first frame (the step shows the second).
  p.priority = P::pain;
  p.frame = f::kPain[2].first;
  p.end = f::kPain[2].last;
}

void pose_player(Q2Server& s, int slot) {
  Q2Player& pl = s.players[slot];
  SqPose& p = pl.pose;
  const bool ducked = (pl.move.flags & ge::classic::kSqDucked) != 0;
  const bool running = pl.move.velocity[0] != 0 || pl.move.velocity[1] != 0;
  const bool hurt = pl.health < p.health || pl.body.armour_count < p.armour;
  const bool shot = p.shot, threw = p.threw;
  const int gesture = p.gesture;
  p.shot = p.threw = false;
  p.gesture = -1;
  p.health = pl.health;
  p.armour = pl.body.armour_count;
  if (!pl.alive) {
    if (p.priority != P::death) {
      if (p.ducked) {  // as he stood when he fell (the dead are always ducked)
        start(p, P::death, f::kCrouchDeath);
      } else {
        s.death_turn = (s.death_turn + 1) % 3;  // 2, 3, 1, ...
        start(p, P::death, f::kDeath[s.death_turn]);
      }
    }
    if (p.frame < p.end) p.frame++;
    return;
  }
  if (p.priority == P::death) start_basic(p);  // alive again
  // The weapon: put away (its last four frames) and brought up.
  const SqHand& h = pl.hand;
  if (h.weapon != p.weapon) {
    p.weapon = h.weapon;
    p.priority = P::pain;
    const SqSpan up = ducked ? f::kCrouchPain : f::kPain[2];
    p.frame = up.first;
    p.end = up.last;
  } else if (h.state == SqHandState::dropping && p.priority < P::reverse &&
             kSqWeapons[h.weapon].last_deactivate - h.frame < 4) {
    const SqSpan down = ducked ? f::kCrouchPain : f::kPain[2];
    p.priority = P::reverse;
    p.frame = down.last + 1;
    p.end = down.first;
  }
  if (ducked != p.ducked) {  // crouching or standing up cuts all below death
    p.ducked = ducked;
    p.running = running;
    if (p.priority < P::death) start_basic(p);
  }
  if (running != p.running) {
    p.running = running;
    if (p.priority == P::basic) start_basic(p);
  }
  if (hurt && p.priority < P::pain) {
    if (ducked) {
      start(p, P::pain, f::kCrouchPain);
    } else {
      s.pain_turn = (s.pain_turn + 1) % 3;
      start(p, P::pain, f::kPain[s.pain_turn]);
    }
  }
  if (gesture >= 0 && gesture < 5 && !ducked && p.priority <= P::gesture)
    start(p, P::gesture, f::kGestures[gesture]);  // a gesture over a gesture starts again
  if (threw) {
    if (ducked) {
      start(p, P::attack, {f::kCrouchAttack.first, int16_t(f::kCrouchAttack.first + 2)});
    } else {
      p.priority = P::reverse;  // the wave backwards from its eighth frame
      p.frame = f::kWave.first + 7;
      p.end = f::kWave.first;
    }
  } else if (shot) {
    start(p, P::attack, ducked ? f::kCrouchAttack : f::kAttack);
  }
  if (!pl.on_ground && p.priority <= P::gesture) {  // in the air
    p.priority = P::jump;
    if (p.frame != f::kJump.first + 1) p.frame = f::kJump.first - 1;
    p.end = f::kJump.first + 1;
  } else if (pl.on_ground && p.priority == P::jump) {  // landing
    p.priority = P::gesture;
    if (p.frame < f::kJump.first + 1) p.frame = f::kJump.first;  // the second shown once first
    p.end = f::kJump.last;
  }
  // The step.
  if (p.priority == P::reverse) {
    if (p.frame > p.end) {
      p.frame--;
      return;
    }
  } else if (p.frame < p.end) {
    p.frame++;
    return;
  }
  if (p.priority == P::jump) return;  // holds its second frame in the air
  if (p.priority == P::basic) {
    p.frame = basic_of(p).first;  // the loop round again
    return;
  }
  start_basic(p);  // the sequence over: the basic loop's first frame now
  p.frame++;
}

void pose_monster(Q2Server& s, Q2Monster& m) {
  using Act = SqMonsterPose::Act;
  const SqMonsterPoses* t = sq_monster_poses(roster::kKinds[m.kind].classname);
  SqMonsterPose& p = m.pose;
  const Q2Entity& e = s.entities[size_t(m.entity)];
  const int cue = p.cue_attack;
  const bool pain = p.cue_pain;
  const int shown = p.shown;
  const SqSpan shown_run = p.shown_run, pain_run = p.pain_run;
  const bool fidget = p.cue_fidget;
  p.cue_fidget = false;
  p.cue_attack = -1;
  p.cue_pain = false;
  p.shown = -1;
  p.pain_run = {};
  sh::Vec3f moved = e.origin - p.last;
  moved.z = 0;
  p.last = e.origin;
  if (t == nullptr) return;
  const int64_t now = s.frame_number;
  if (roster::kKinds[m.kind].classname == std::string_view("misc_insane")) {
    pose_insane(s, m, pain, sh::dot(moved, moved) > 0.25f);
    return;
  }
  auto begin = [&](Act act, SqSpan span) {
    p.act = act;
    p.span = span;
    p.frame = span.first;
  };
  if (m.dead) {
    if (p.act != Act::death) {
      const int n = count(t->death, 6);
      if (n == 0) return;  // none: it keeps its frame (a flier's crash)
      int run = m.death_run >= 0 && m.death_run < n ? m.death_run : pick(m.entity, now, n);
      // A soldier's third death is its head shot's: the others are picked among the five left.
      const std::string_view name = roster::kKinds[m.kind].classname;
      if (n == 6 && name.substr(0, 15) == "monster_soldier") {
        run = pick(m.entity, now, 5);
        if (run >= 2) run++;
        if (m.head_shot) run = 2;
      }
      begin(Act::death, t->death[run]);
    } else if (p.frame < p.span.last) {
      p.frame++;
    }
    return;
  }
  if (shown >= 0 && !m.dead) {  // a frame its think plays by hand (a soldier's run-and-shoot)
    p.act = Act::attack;
    p.span = shown_run.none() ? SqSpan{static_cast<int16_t>(shown), static_cast<int16_t>(shown)}
                              : shown_run;
    p.frame = shown;
    return;
  }
  if (m.duck_pose >= 0) {  // a dodge's duck: its frames are the monsters' own (q2_monsters.cpp)
    p.act = Act::duck;
    p.span = {static_cast<int16_t>(m.duck_pose), static_cast<int16_t>(m.duck_pose)};
    p.frame = m.duck_pose;
    return;
  }
  if (cue >= 0 && cue < 5) {
    const int n = count(t->attack[cue], 2);
    if (n > 0) {
      p.slot = cue;
      begin(Act::attack, t->attack[cue][pick(m.entity, now, n)]);
      return;
    }
  }
  if (pain && !pain_run.none()) {  // its kind's pains chose the run, over an attack too
    begin(Act::pain, pain_run);
    m.pain_hold = s.level_time + 0.1 * (p.span.last - p.span.first + 1);
    return;
  }
  if (pain && p.act != Act::attack) {
    const int n = count(t->pain, 3);
    if (n > 0) {
      begin(Act::pain, t->pain[pick(m.entity, now, n)]);
      // Held for the run's frames, a tenth of a second each.
      m.pain_hold = s.level_time + 0.1 * (p.span.last - p.span.first + 1);
      return;
    }
  }
  const bool busy = s.level_time + 0.001 < m.busy_until;
  if (p.act == Act::attack || p.act == Act::pain) {
    const int hold = p.act == Act::attack ? t->hold[p.slot] : -1;
    if (p.frame == hold && s.level_time + 0.001 < m.burst_until) return;
    if (p.frame < p.span.last) {
      p.frame++;
      return;
    }
    if (p.act == Act::attack && busy) {  // the attack lasts: round again
      p.frame = p.span.first;
      return;
    }
  }
  // The basic loop by how it moved.
  const bool moving = sh::dot(moved, moved) > 0.25f;
  // Standing: its fidget played through, then its stand again; begun by its idle call, or at one
  // of its stand's frames by chance.
  if (!moving && !t->fidget.none() && m.enemy == -1) {
    if (p.act == Act::stand && p.span.first == t->fidget.first && p.frame >= p.span.first &&
        p.frame <= p.span.last) {
      if (p.frame < p.span.last) {
        p.frame++;
      } else {
        begin(Act::stand, t->stand);
      }
      return;
    }
    bool start = fidget && t->fidget_odds == 0;
    if (p.act == Act::stand && p.span.first == t->stand.first && t->fidget_odds > 0 && !m.hold)
      for (int16_t at : t->fidget_at)
        if (at >= 0 && p.frame == at && pick(m.entity, now * 3 + 1, 100) < t->fidget_odds)
          start = true;
    if (start) {
      begin(Act::stand, t->fidget);
      return;
    }
  }
  Act want = !moving ? Act::stand : m.enemy != -1 ? Act::run : Act::walk;
  SqSpan span = want == Act::run ? t->run : want == Act::walk ? t->walk : t->stand;
  if (span.none() && want == Act::run) span = t->walk;
  if (span.none() && want == Act::walk) span = t->run;
  if (span.none()) {
    want = Act::stand;
    span = t->stand;
  }
  if (span.none()) return;
  if (p.act != want || p.frame < span.first || p.frame > span.last) {
    begin(want, span);
    return;
  }
  p.span = span;
  p.frame = p.frame < span.last ? p.frame + 1 : span.first;
}

}  // namespace shooters_edge::server
