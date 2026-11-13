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
#include "shooters_edge/server/sequel_server.hpp"

namespace shooters_edge::server {

namespace {

using P = SqPriority;
namespace f = sq_frames;

// Each kind's runs, measured from its model's frame names (the frames of one name in order); which
// run plays which action is read from the names (stand, walk, run, attack, pain, death).
constexpr SqMonsterPoses soldier(const char* classname, SqSpan a0, SqSpan a1, int16_t hold) {
  SqMonsterPoses p{classname, {146, 175}, {215, 224}, {99, 104}};
  p.attack[2][0] = a0;
  p.attack[2][1] = a1;
  p.hold[2] = hold;
  p.pain[0] = {50, 54};
  p.pain[1] = {55, 61};
  p.pain[2] = {62, 79};
  p.death[0] = {272, 307};
  p.death[1] = {308, 342};
  p.death[2] = {343, 387};
  p.death[3] = {388, 440};
  p.death[4] = {441, 464};
  p.death[5] = {465, 474};
  return p;
}

constexpr SqMonsterPoses kPoses[] = {
    soldier("monster_soldier_light", {0, 11}, {12, 29}, -1),
    soldier("monster_soldier", {0, 11}, {12, 29}, -1),
    soldier("monster_soldier_ss", {39, 44}, {}, 41),  // holds its third frame through the burst
    {.classname = "monster_infantry",
     .stand = {1, 71},
     .walk = {72, 91},
     .run = {92, 99},
     .attack = {{{199, 206}}, {}, {{184, 198}}},
     .pain = {{100, 109}, {110, 119}},
     .death = {{125, 144}, {145, 169}, {170, 178}}},
    {.classname = "monster_gunner",
     .stand = {0, 69},
     .walk = {70, 93},
     .run = {94, 101},
     .attack = {{}, {}, {{129, 158}}, {{108, 128}}},
     .pain = {{159, 176}, {177, 184}, {185, 189}},
     .death = {{190, 200}}},
    {.classname = "monster_berserk",
     .stand = {0, 4},
     .walk = {25, 35},
     .run = {36, 41},
     .attack = {{{42, 54}, {55, 75}}},
     .pain = {{199, 202}, {203, 222}},
     .death = {{223, 235}, {236, 243}}},
    {.classname = "monster_parasite",
     .stand = {83, 117},
     .walk = {68, 82},
     .run = {68, 82},
     .attack = {{}, {}, {{39, 56}}},
     .pain = {{57, 67}},
     .death = {{32, 38}}},
    {.classname = "monster_flyer",
     .stand = {13, 57},
     .walk = {13, 57},
     .run = {13, 57},
     .attack = {{{79, 95}}, {}, {{58, 78}}},
     .pain = {{134, 142}, {143, 146}, {147, 150}}},
    {.classname = "monster_tank",
     .stand = {0, 29},
     .walk = {30, 54},
     .run = {30, 54},
     .attack = {{}, {}, {{55, 76}}, {{115, 167}}, {{77, 114}}},
     .pain = {{197, 200}, {201, 205}, {206, 221}},
     .death = {{222, 253}}},
    {.classname = "monster_tank_commander",
     .stand = {0, 29},
     .walk = {30, 54},
     .run = {30, 54},
     .attack = {{}, {}, {{55, 76}}, {{115, 167}}, {{77, 114}}},
     .pain = {{197, 200}, {201, 205}, {206, 221}},
     .death = {{222, 253}}},
    {.classname = "monster_medic",
     .stand = {12, 101},
     .walk = {0, 11},
     .run = {102, 107},
     .attack = {{}, {}, {{177, 236}}},
     .pain = {{108, 115}, {116, 130}},
     .death = {{147, 176}}},
    {.classname = "monster_brain",
     .stand = {162, 221},
     .walk = {0, 12},
     .run = {0, 12},
     .attack = {{{53, 70}}, {{71, 87}}},
     .pain = {{88, 108}, {109, 116}, {117, 122}},
     .death = {{123, 140}, {141, 145}}},
    {.classname = "monster_chick",
     .stand = {121, 150},
     .walk = {181, 207},
     .run = {181, 207},
     .attack = {{{32, 47}}, {}, {{0, 31}}},
     .pain = {{90, 94}, {95, 99}, {100, 120}},
     .death = {{48, 59}, {60, 82}}},
    {.classname = "monster_gladiator",
     .stand = {0, 6},
     .walk = {7, 22},
     .run = {23, 28},
     .attack = {{{29, 45}}, {}, {{46, 54}}},
     .pain = {{55, 60}},
     .death = {{61, 82}}},
    {.classname = "monster_mutant",
     .stand = {62, 125},
     .walk = {126, 148},
     .run = {56, 61},
     .attack = {{{0, 14}}, {}, {{0, 14}}},
     .pain = {{34, 38}, {39, 44}, {45, 55}},
     .death = {{15, 23}, {24, 33}}},
    {.classname = "monster_floater",
     .stand = {144, 195},
     .walk = {144, 195},
     .run = {144, 195},
     .attack = {{{70, 103}}, {{45, 69}}, {{31, 44}}},
     .pain = {{117, 123}, {124, 131}, {132, 143}},
     .death = {{104, 116}}},
    {.classname = "monster_hover",
     .stand = {0, 29},
     .walk = {30, 64},
     .run = {30, 64},
     .attack = {{}, {}, {{197, 204}}},
     .pain = {{113, 140}, {141, 152}, {153, 161}},
     .death = {{162, 172}}},
    {.classname = "monster_flipper",
     .stand = {41, 64},
     .walk = {41, 64},
     .run = {41, 64},
     .attack = {{{0, 19}}},
     .pain = {{94, 98}, {99, 103}},
     .death = {{104, 159}}},
    {.classname = "monster_supertank",
     .stand = {194, 253},
     .walk = {128, 145},
     .run = {128, 145},
     .attack = {{}, {}, {{0, 19}}, {{20, 46}}},
     .pain = {{164, 167}, {168, 171}, {172, 175}},
     .death = {{98, 121}}},
    {.classname = "monster_boss2",
     .stand = {0, 49},
     .walk = {50, 69},
     .run = {50, 69},
     .attack = {{}, {}, {{70, 109}}, {{70, 109}}},
     .pain = {{110, 131}},
     .death = {{132, 180}}},
    {.classname = "monster_jorg",
     .stand = {112, 162},
     .walk = {163, 187},
     .run = {163, 187},
     .attack = {{}, {}, {{0, 17}}, {{18, 30}}},
     .pain = {{81, 83}, {84, 86}, {87, 111}},
     .death = {{31, 80}}},
    {.classname = "monster_makron",
     .stand = {414, 473},
     .walk = {474, 490},
     .run = {474, 490},
     .attack = {{}, {}, {{201, 208}}, {{209, 234}}, {{235, 250}}},
     .pain = {{379, 382}, {383, 386}, {387, 413}},
     .death = {{251, 345}}},
    {.classname = "monster_boss3_stand", .stand = {414, 473}, .walk = {}, .run = {}},
    {.classname = "misc_insane",
     .stand = {0, 98},
     .walk = {173, 198},
     .run = {173, 198},
     .pain = {{199, 209}},
     .death = {{210, 226}}},
};

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

const SqMonsterPoses* sq_monster_poses(std::string_view classname) {
  for (const SqMonsterPoses& p : kPoses)
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
  const bool ducked = (pl.move.flags & sh::kSqDucked) != 0;
  const bool running = pl.move.velocity[0] != 0 || pl.move.velocity[1] != 0;
  const bool hurt = pl.health < p.health || pl.body.armour_count < p.armour;
  const bool shot = p.shot, threw = p.threw;
  p.shot = p.threw = false;
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
