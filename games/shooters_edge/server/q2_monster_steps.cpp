// Quake II's monsters' footsteps: which frame of which run sounds a step, by kind, and the step
// sounded; and the sounds tied to a frame of a run (a prisoner's cries).
//
// ge:summary Quake II's monsters' footsteps and frame sounds by kind and frame, g_monsterfootsteps
// ge:keywords q2 monsters footsteps g_monsterfootsteps monster-steps step-sounds frame-sounds
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

// ---- Frame sounds: a cry or a blow at a frame of a run, whatever g_monsterfootsteps says ----

// A sound at `frame` of the run starting at `first`: one of `sounds` at even odds (null past the
// last; one listed twice counts twice), on channel `channel` (0 any, 1 the weapon, 2 the voice,
// 4 the body), at the attenuation given (0 the whole map, 1 normal, 2 idle), `odds` times in a
// hundred.
struct Cue {  // NOLINT(clang-analyzer-optin.performance.Padding): a constant table, rows by place
  const char* classname;
  int16_t first, frame;
  const char* sounds[10];
  int8_t channel, attenuation;
  int8_t odds = 100;
};

#define GE_INSANE_SCREAMS                                                                 \
  "insane/insane1.wav", "insane/insane2.wav", "insane/insane3.wav", "insane/insane4.wav", \
      "insane/insane6.wav", "insane/insane8.wav", "insane/insane9.wav", "insane/insane10.wav"
constexpr Cue kCues[] = {
    // The prisoner: a shake as his second stand starts, a moan and two fists going down, a fist,
    // a moan and a scream lying down, a scream as each walk or crawl comes round, a moan on the
    // cross and a scream as he struggles.
    {"misc_insane", 64, 64, {"insane/insane5.wav"}, 2, 2},
    {"misc_insane", 0, 7, {"insane/insane7.wav"}, 2, 2},
    {"misc_insane", 0, 27, {"insane/insane11.wav"}, 2, 2},
    {"misc_insane", 0, 33, {"insane/insane11.wav"}, 2, 2},
    {"misc_insane", 99, 115, {"insane/insane11.wav"}, 2, 2},
    {"misc_insane", 99, 132, {"insane/insane7.wav"}, 2, 2},
    {"misc_insane", 99, 152, {GE_INSANE_SCREAMS}, 2, 2},
    {"misc_insane", 160, 160, {GE_INSANE_SCREAMS}, 2, 2},
    {"misc_insane", 173, 173, {GE_INSANE_SCREAMS}, 2, 2},
    {"misc_insane", 227, 227, {GE_INSANE_SCREAMS}, 2, 2},
    {"misc_insane", 252, 252, {"insane/insane7.wav"}, 2, 2},
    {"misc_insane", 267, 267, {GE_INSANE_SCREAMS}, 2, 2},
    // Idle fidgets: a soldier's mutter as it stands, its gun cocked in its second stand; the
    // gunner's, berserker's and chick's calls in theirs; the parasite's taps and scratches; the
    // medic's call as its stand comes round.
    {"monster_soldier_light", 146, 146, {"soldier/solidle1.wav"}, 2, 2, 20},
    {"monster_soldier", 146, 146, {"soldier/solidle1.wav"}, 2, 2, 20},
    {"monster_soldier_ss", 146, 146, {"soldier/solidle1.wav"}, 2, 2, 20},
    {"monster_soldier_light", 176, 197, {"infantry/infatck3.wav"}, 1, 2},
    {"monster_soldier", 176, 197, {"infantry/infatck3.wav"}, 1, 2},
    {"monster_soldier_ss", 176, 197, {"infantry/infatck3.wav"}, 1, 2},
    {"monster_gunner", 30, 37, {"gunner/gunidle1.wav"}, 2, 2},
    {"monster_berserk", 5, 5, {"berserk/beridle1.wav"}, 1, 2},
    {"monster_chick", 151, 159, {"chick/chkidle1.wav", "chick/chkidle2.wav"}, 2, 2},
    {"monster_parasite", 83, 85, {"parasite/paridle1.wav"}, 1, 2},
    {"monster_parasite", 83, 87, {"parasite/paridle1.wav"}, 1, 2},
    {"monster_parasite", 83, 91, {"parasite/paridle1.wav"}, 1, 2},
    {"monster_parasite", 83, 93, {"parasite/paridle1.wav"}, 1, 2},
    {"monster_parasite", 83, 97, {"parasite/paridle1.wav"}, 1, 2},
    {"monster_parasite", 83, 99, {"parasite/paridle1.wav"}, 1, 2},
    {"monster_parasite", 100, 104, {"parasite/paridle2.wav"}, 1, 2},
    {"monster_parasite", 100, 107, {"parasite/paridle2.wav"}, 1, 2},
    {"monster_parasite", 100, 110, {"parasite/paridle2.wav"}, 1, 2},
    {"monster_medic", 12, 12, {"medic/idle.wav"}, 2, 2},
    // The tanks: the thud as the body lands, the windup and the blow stamping on a corpse.
    {"monster_tank", 222, 249, {"tank/tnkdeth2.wav"}, 4, 1},
    {"monster_tank_commander", 222, 249, {"tank/tnkdeth2.wav"}, 4, 1},
    {"monster_tank", 77, 95, {"tank/tnkatck4.wav"}, 1, 1},
    {"monster_tank", 77, 102, {"tank/tnkatck5.wav"}, 1, 1},
    {"monster_tank_commander", 77, 95, {"tank/tnkatck4.wav"}, 1, 1},
    {"monster_tank_commander", 77, 102, {"tank/tnkatck5.wav"}, 1, 1},
    // The super tank's treads as each run comes round.
    {"monster_supertank", 128, 128, {"bosstank/btkengn1.wav"}, 2, 1},
    // Jorg: his call as his stand comes round, the guns' call and the BFG's at its shot.
    {"monster_jorg", 112, 112, {"boss3/bs3idle1.wav"}, 2, 1},
    {"monster_jorg", 0, 0, {"boss3/bs3atck1.wav"}, 2, 1},
    {"monster_jorg", 18, 18, {"boss3/bs3atck2.wav"}, 2, 1},
    {"monster_jorg", 18, 24, {"boss3/bs3atck2.wav"}, 2, 1},
    // The Makron: the BFG's call, the rail's rising whine, the pop-up and a taunt in its long
    // pain, the blow and the brain as it falls.
    {"monster_makron", 201, 204, {"makron/bfg_fire.wav"}, 2, 1},
    {"monster_makron", 235, 235, {"makron/rail_up.wav"}, 1, 1},
    {"monster_makron", 387, 402, {"makron/popup.wav"}, 4, 0},
    {"monster_makron",
     387,
     410,
     {"makron/voice4.wav", "makron/voice4.wav", "makron/voice4.wav", "makron/voice3.wav",
      "makron/voice3.wav", "makron/voice3.wav", "makron/voice.wav", "makron/voice.wav",
      "makron/voice.wav", "makron/voice.wav"},
     0,
     0},
    {"monster_makron", 251, 341, {"makron/bhit.wav"}, 0, 0},
    {"monster_makron", 251, 343, {"makron/brain1.wav"}, 2, 1},
};
#undef GE_INSANE_SCREAMS

uint32_t frame_hash(const Q2Server& s, const Q2Monster& m) {
  uint32_t h = uint32_t(m.entity) * 2654435761u ^ uint32_t(s.frame_number) * 40503u;
  return h ^ (h >> 15);
}

void sound_at(Q2Server& s, const Q2Monster& m, const char* sample, int channel, float volume,
              int attenuation) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = map_row(m.entity);
  ev.level = channel;  // 0 any, 1 the weapon, 2 the voice, 4 the body
  ev.origin = s.entities[size_t(m.entity)].origin;
  ev.values[0] = volume * 255.0f;
  ev.values[1] = float(attenuation);
  ev.text = sample;
  event(s, std::move(ev));
}

// The frame's own sound, when the frame shown is new.
void cue(Q2Server& s, const Q2Monster& m) {
  const std::string_view name = roster::looks_like(roster::kKinds[m.kind]);
  for (const Cue& c : kCues) {
    if (name != c.classname || c.first != m.pose.span.first || c.frame != m.pose.frame) continue;
    int n = 0;
    while (n < 10 && c.sounds[n] != nullptr) n++;
    const uint32_t h = frame_hash(s, m);
    if (n > 0 && int((h >> 8) % 100u) < c.odds)
      sound_at(s, m, c.sounds[h % uint32_t(n)], c.channel, 1, c.attenuation);
  }
}

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
  cue(s, m);
  const Steps* t = steps_of(roster::kKinds[m.kind].classname);
  if (t == nullptr || (!t->always && s.cvars.find("g_monsterfootsteps")->integer() == 0)) return;
  const StepAt* a = step_at(*t, p);
  if (a == nullptr) return;
  int n = 0;
  while (n < 4 && t->sounds[n] != nullptr) n++;
  if (n == 0) return;
  // A pick hashes the body and the frame, so it draws nothing from the game's stream.
  const uint32_t h = frame_hash(s, m);
  sound_at(s, m, t->sounds[a->sound >= 0 ? a->sound : int(h % uint32_t(n))],
           t->channel == 2 ? 2 : 4, t->volume, t->attenuation);
}

std::span<const char* const> monster_step_sounds(std::string_view classname, bool steps_on) {
  const Steps* t = steps_of(classname);
  if (t == nullptr || !(steps_on || t->always)) return {};
  return monster_step_sounds_of(t);
}

void monster_cue_sounds(std::string_view classname, void (*add)(void*, const char*), void* to) {
  for (const Cue& c : kCues) {
    if (classname != c.classname) continue;
    for (const char* name : c.sounds)
      if (name != nullptr) add(to, name);
  }
}

}  // namespace shooters_edge::server
