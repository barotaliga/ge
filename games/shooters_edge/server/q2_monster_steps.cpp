// Quake II's monsters' footsteps: which frame of which run sounds a step, by kind, and the step
// sounded (porting/shooters-edge/q2-switches.md § 7, q2-monster-steps.md).
//
// ge:summary Quake II's monsters' footsteps by kind and frame, g_monsterfootsteps, heavy kinds
// ge:keywords q2 monsters footsteps g_monsterfootsteps monster-steps step-sounds
// ge:skill shooter
// ge:test tests/shooters_edge_q2_monsters_test.cpp
#include "shooters_edge/server/q2_monster_steps.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_kinds.hpp"
#include "shooters_edge/server/q2_poses.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr uint32_t kEveryone = 0xffffffffu;

void event(Q2Server& s, NetEvent ev) {
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

// ---- Footsteps (porting/shooters-edge/q2-switches.md § 7, q2-monster-steps.md) ----

using Act = SqMonsterPose::Act;

// A step: the run of frames it belongs to and the model frame that sounds it, with the sound
// (its index in the kind's list; -1 one picked). A frame steps only in the run it is listed for,
// so the same frame shown by another run (Jorg's walk over his run's frames) stays silent.
struct StepAt {
  Act act;
  int16_t frame;
  int8_t sound;
};

constexpr int8_t kAny = -1;

struct Steps {
  const char* classname;
  const char* sounds[4];  // null past the last
  float volume;
  int8_t attenuation;  // 1 normal, 2 the idle's shorter reach
  int8_t channel;      // 4 the body, 2 the voice
  bool always;         // steps whatever g_monsterfootsteps says
  StepAt at[24];       // up to the first with frame 0 and act stand
};

constexpr StepAt W(int16_t f, int8_t snd = kAny) { return {Act::walk, f, snd}; }
constexpr StepAt R(int16_t f, int8_t snd = kAny) { return {Act::run, f, snd}; }
constexpr StepAt S(int16_t f, int8_t snd = kAny) { return {Act::stand, f, snd}; }
constexpr StepAt A(int16_t f, int8_t snd = kAny) { return {Act::attack, f, snd}; }
constexpr StepAt P(int16_t f, int8_t snd = kAny) { return {Act::pain, f, snd}; }
constexpr StepAt D(int16_t f, int8_t snd = kAny) { return {Act::death, f, snd}; }
constexpr StepAt K(int16_t f, int8_t snd = kAny) { return {Act::duck, f, snd}; }

#define GE_PLAYER_STEPS \
  "player/step1.wav", "player/step2.wav", "player/step3.wav", "player/step4.wav"
#define GE_SOLDIER_STEPS(classname)                                                               \
  {                                                                                               \
    classname, {GE_PLAYER_STEPS}, 1, 1, 4, false, {                                               \
      A(40), A(44), P(64), P(79), R(100), R(103), A(111), A(115), A(118), A(121), W(218), W(223), \
          W(256), W(260)                                                                          \
    }                                                                                             \
  }

// The frames are the models' own, counted from 0. A walk and a run over one stretch of frames
// step alike unless listed apart; a stand that steps is listed as stand.
constexpr Steps kSteps[] = {
    {"monster_infantry",
     {"infantry/step1.wav", "infantry/step2.wav"},
     1,
     1,
     4,
     false,
     {S(6), S(48), W(74), W(80), R(93), R(97), P(104), P(109), P(114), P(119), K(124), D(129),
      D(133), D(150), D(151), A(202)}},
    GE_SOLDIER_STEPS("monster_soldier_light"),
    GE_SOLDIER_STEPS("monster_soldier"),
    GE_SOLDIER_STEPS("monster_soldier_ss"),
    {"monster_gladiator",
     {"gladiator/step1.wav", "gladiator/step2.wav"},
     1,
     1,
     4,
     false,
     {W(11), W(19), R(25), R(28)}},
    {"monster_chick",
     {"bitch/step1.wav", "bitch/step2.wav"},
     1,
     1,
     4,
     false,
     {A(8), A(15), A(17), A(22), A(25), A(31), A(34), A(47), D(63), D(70), D(76), W(192), W(197),
      R(192), R(197)}},
    {"monster_berserk",
     {"berserk/step1.wav", "berserk/step2.wav"},
     1,
     1,
     4,
     false,
     {W(28), W(33), R(37), R(40), A(86), A(98), A(104), A(109)}},
    {"monster_medic",
     {"medic/step1.wav", "medic/step2.wav"},
     1,
     1,
     4,
     false,
     {W(1), W(7), R(102), R(106), P(119), P(130), A(216), A(230)}},
    {"monster_gunner",
     {"gunner/step1.wav", "gunner/step2.wav"},
     1,
     1,
     4,
     false,
     {W(76), W(81), W(88), R(95), R(99), A(103), A(106), A(138), A(158), P(161), P(171), P(175),
      P(179), P(184)}},
    {"monster_brain",
     {"brain/step1.wav", "brain/step2.wav"},
     1,
     1,
     4,
     false,
     {W(3), W(9), R(3), R(9), A(56), A(70), P(90), P(107), K(149), K(153)}},
    {"monster_parasite",
     {"parasite/paridle1.wav"},
     1,
     2,
     4,
     false,
     {W(72), W(74), W(75), W(79), R(72), R(73), R(75), R(79)}},
    {"misc_insane",
     {GE_PLAYER_STEPS},
     0.7f,
     1,
     4,
     false,
     {W(166), W(172), R(166), R(172), W(179), W(184), W(191), W(197), R(179), R(184), R(191),
      R(197), P(203)}},
    // The heavy kinds step always, as the game was made.
    {"monster_tank",
     {"tank/step.wav"},
     1,
     1,
     4,
     true,
     {W(33), W(41), W(49), W(54), R(33), R(41), R(49), R(54), P(221), A(76), A(83), A(87), A(114),
      A(130), A(160)}},
    {"monster_tank_commander",
     {"tank/step.wav"},
     1,
     1,
     4,
     true,
     {W(33), W(41), W(49), W(54), R(33), R(41), R(49), R(54), P(221), A(76), A(83), A(87), A(114),
      A(130), A(160)}},
    {"monster_mutant",
     {"mutant/step1.wav", "mutant/step2.wav", "mutant/step3.wav"},
     1,
     1,
     2,  // on its voice, cutting off a cry
     true,
     {W(128), W(130), W(132), W(136), R(57), R(59)}},
    {"monster_jorg",
     {"boss3/step1.wav", "boss3/step2.wav"},  // left foot, right foot
     1,
     1,
     4,
     true,
     {S(146, 0), S(150, 1), S(159, 0), S(162, 1), R(168, 0), R(175, 1), P(89, 0), P(91, 1),
      P(107, 0), P(111, 1)}},
    {"monster_makron",
     {"makron/step1.wav", "makron/step2.wav"},  // left foot, right foot
     1,
     1,
     4,
     true,
     {W(477, 0), W(481, 1), R(477, 0), R(481, 1), D(254, 0), D(268, 1), D(286, 0), D(306, 1),
      D(308, 0), D(315, 1), D(317, 0), D(320, 1), D(323, 0)}},
};
#undef GE_SOLDIER_STEPS
#undef GE_PLAYER_STEPS

const Steps* steps_of(std::string_view classname) {
  for (const Steps& t : kSteps)
    if (classname == t.classname) return &t;
  return nullptr;
}

// The step the frame shown is in the run it plays, or null.
const StepAt* step_at(const Steps& t, const SqMonsterPose& p) {
  for (const StepAt& a : t.at) {
    if (a.frame == 0 && a.act == Act::stand) break;
    if (a.act == p.act && a.frame == p.frame) return &a;
  }
  return nullptr;
}

std::span<const char* const> monster_step_sounds_of(const Steps* t) {
  if (t == nullptr) return {};
  int n = 0;
  while (n < 4 && t->sounds[n] != nullptr) n++;
  return {t->sounds, size_t(n)};
}

// A step's sound when the frame shown is new and one of the kind's steps in the run it plays.
// The light kinds step only under g_monsterfootsteps (read each frame, so it takes hold at once);
// the heavy ones always. No ground or speed check: a step sounds wherever the frame plays.
}  // namespace

void monster_footstep(Q2Server& s, Q2Monster& m) {
  SqMonsterPose& p = m.pose;
  if (p.frame == p.heard) return;
  p.heard = p.frame;
  const Steps* t = steps_of(roster::kKinds[m.kind].classname);
  if (t == nullptr || (!t->always && s.cvars.find("g_monsterfootsteps")->integer() == 0)) return;
  const StepAt* a = step_at(*t, p);
  if (a == nullptr) return;
  int n = 0;
  while (n < 4 && t->sounds[n] != nullptr) n++;
  if (n == 0) return;
  // A pick hashes the body and the frame, so it draws nothing from the game's stream.
  uint32_t h = uint32_t(m.entity) * 2654435761u ^ uint32_t(s.frame_number) * 40503u;
  h ^= h >> 15;
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = map_row(m.entity);
  ev.level = t->channel == 2 ? 2 : 4;  // the voice or the body
  ev.origin = s.entities[size_t(m.entity)].origin;
  ev.values[0] = t->volume * 255.0f;
  ev.values[1] = float(t->attenuation);
  ev.text = t->sounds[a->sound >= 0 ? a->sound : int(h % uint32_t(n))];
  event(s, std::move(ev));
}

std::span<const char* const> monster_step_sounds(std::string_view classname, bool steps_on) {
  const Steps* t = steps_of(classname);
  if (t == nullptr || !(steps_on || t->always)) return {};
  return monster_step_sounds_of(t);
}

}  // namespace shooters_edge::server
