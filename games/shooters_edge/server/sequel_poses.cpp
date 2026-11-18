// The sequel's poses (sequel_poses.hpp): the players' sequences by priority and the monsters' runs
// by what they do, a frame a rules frame.
//
// ge:summary The sequel's poses: the player's priorities and the monsters' runs, stepped at 10 Hz
// ge:keywords sequel poses animation player-animation monster-animation frame-table
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_poses_test.cpp
#include "shooters_edge/server/sequel_poses.hpp"

#include <cstdint>

#include "shooters_edge/server/sequel_monster_kinds.hpp"
#include "shooters_edge/server/sequel_monster_poses.hpp"
#include "shooters_edge/server/sequel_server.hpp"

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

}  // namespace

bool sq_can_gesture(const SqPose& p, bool on_ground) {
  return on_ground && !p.ducked && p.priority <= P::gesture;
}

const SqMonsterPoses* sq_monster_poses(std::string_view classname) {
  for (const SqMonsterPoses& p : kSqMonsterPoses)
    if (classname == p.classname) return &p;
  return nullptr;
}

void pose_spawn(SequelServer& s, int slot) {
  SequelPlayer& pl = s.players[slot];
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

void pose_player(SequelServer& s, int slot) {
  SequelPlayer& pl = s.players[slot];
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

void pose_monster(SequelServer& s, SequelMonster& m) {
  using Act = SqMonsterPose::Act;
  const SqMonsterPoses* t = sq_monster_poses(roster::kKinds[m.kind].classname);
  SqMonsterPose& p = m.pose;
  const SequelEntity& e = s.entities[size_t(m.entity)];
  const int cue = p.cue_attack;
  const bool pain = p.cue_pain;
  p.cue_attack = -1;
  p.cue_pain = false;
  sh::Vec3f moved = e.origin - p.last;
  moved.z = 0;
  p.last = e.origin;
  if (t == nullptr) return;
  const int64_t now = s.frame_number;
  auto begin = [&](Act act, SqSpan span) {
    p.act = act;
    p.span = span;
    p.frame = span.first;
  };
  if (m.dead) {
    if (p.act != Act::death) {
      const int n = count(t->death, 6);
      if (n == 0) return;  // none: it keeps its frame (a flier's crash)
      begin(Act::death, t->death[pick(m.entity, now, n)]);
    } else if (p.frame < p.span.last) {
      p.frame++;
    }
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
  if (pain && p.act != Act::attack) {
    const int n = count(t->pain, 3);
    if (n > 0) {
      begin(Act::pain, t->pain[pick(m.entity, now, n)]);
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
  Act want = !moving ? Act::stand : m.enemy >= 0 ? Act::run : Act::walk;
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
