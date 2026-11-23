// Quake II's monsters' pains and deaths as data: each kind's pain runs by the blow's size, its
// cries, the blows it shrugs off, what skill 3 leaves of it, the knocked-up pain; each kind's
// death runs and their cries (porting/shooters-edge/q2-monsters-round4.md § 1 to 3).
//
// ge:summary Quake II's monsters' pain and death runs and cries by kind, blow, skill and lift
// ge:keywords q2 monsters pain death pain-animation pain-sound death-sound nightmare knocked-up
// ge:skill shooter
// ge:test tests/shooters_edge_q2_monsters_test.cpp
#include "shooters_edge/server/q2_monster_pain.hpp"

#include <algorithm>
#include <cstdint>

namespace shooters_edge::server {

namespace {

// A pain run and its cry (null: one of the kind's cries at even odds).
struct PainRun {
  SqSpan run;
  const char* sound = nullptr;
};

constexpr int16_t kAny = 32767;

// A band of blows up to `most`: it flinches at all with chance `odds`, by one of its runs at even
// odds (a run listed twice counts twice).
// With `first_odds` set (0 or more), not even odds: its first run with chance `first_odds` +
// `per_point` × the blow + `per_skill` × the skill, at most `first_cap`, else its second.
struct PainBand {
  int16_t most = kAny;
  float odds = 1;
  PainRun runs[3] = {};
  float first_odds = -1, per_point = 0, per_skill = 0, first_cap = 1;
};

enum class Nightmare : uint8_t {
  cry,      // skill 3: the cry, no flinch
  nothing,  // skill 3: neither
  both,     // skill 3: as ever
};

struct PainRule {  // NOLINT(clang-analyzer-optin.performance.Padding): a constant table, rows by
                   // name
  const char* classname;
  const char* sounds[3] = {};  // a run's cry when it names none
  Nightmare nightmare = Nightmare::nothing;
  bool everywhere = false;  // its cries heard across the whole map
  int16_t never = -1;       // a blow this or less: nothing at all, the wait untouched
  int16_t cry_most = -1;    // a blow this or less: its first cry alone, no flinch, the wait spent
  int16_t shrug_most = -1;  // a blow this or less shrugged off with chance `shrug`, the wait spent
  float shrug = 0;
  SqSpan busy[2] = {};         // at skill 2 and up, no pain while one of these plays
  SqSpan air = {};             // knocked up faster than 100: this run
  bool air_nightmare = false;  // the knocked-up run even at skill 3
  PainBand bands[4] = {};
};

constexpr SqSpan S(int a, int b) { return {static_cast<int16_t>(a), static_cast<int16_t>(b)}; }

constexpr PainRule soldier(const char* classname, const char* cry) {
  PainRule r{.classname = classname};
  r.sounds[0] = cry;
  r.nightmare = Nightmare::cry;
  r.air = S(80, 96);
  r.air_nightmare = true;
  r.bands[0] = {kAny, 1, {{S(50, 54)}, {S(55, 61)}, {S(62, 79)}}};
  return r;
}

constexpr PainRule kPains[] = {
    soldier("monster_soldier_light", "soldier/solpain2.wav"),
    soldier("monster_soldier", "soldier/solpain1.wav"),
    soldier("monster_soldier_ss", "soldier/solpain3.wav"),
    {.classname = "misc_actor",  // silent; a taunt in its place first (q2_actor.cpp)
     .nightmare = Nightmare::both,
     .bands = {{kAny, 1, {{S(74, 76)}, {S(77, 79)}, {S(80, 82)}}}}},
    {.classname = "monster_infantry",
     .bands = {{kAny,
                1,
                {{S(100, 109), "infantry/infpain1.wav"}, {S(110, 119), "infantry/infpain2.wav"}}}}},
    {.classname = "monster_gunner",
     .sounds = {"gunner/gunpain2.wav", "gunner/gunpain1.wav"},
     .nightmare = Nightmare::cry,
     .bands = {{10, 1, {{S(185, 189)}}}, {25, 1, {{S(177, 184)}}}, {kAny, 1, {{S(159, 176)}}}}},
    {.classname = "monster_chick",
     .sounds = {"chick/chkpain1.wav", "chick/chkpain2.wav", "chick/chkpain3.wav"},
     .nightmare = Nightmare::cry,
     .bands = {{10, 1, {{S(90, 94)}}}, {25, 1, {{S(95, 99)}}}, {kAny, 1, {{S(100, 120)}}}}},
    {.classname = "monster_gladiator",
     .sounds = {"gladiator/pain.wav", "gladiator/gldpain2.wav"},
     .nightmare = Nightmare::cry,
     .air = S(83, 89),
     .bands = {{kAny, 1, {{S(55, 60)}}}}},
    {.classname = "monster_berserk",
     .sounds = {"berserk/berpain2.wav"},
     .nightmare = Nightmare::cry,
     .bands = {{19, 1, {{S(199, 202)}}}, {kAny, 1, {{S(199, 202)}, {S(203, 222)}}}}},
    {.classname = "monster_medic",
     .bands = {{kAny,
                1,
                {{S(108, 115), "medic/medpain1.wav"}, {S(116, 130), "medic/medpain2.wav"}}}}},
    {.classname = "monster_brain",
     .bands = {{kAny,
                1,
                {{S(88, 108), "brain/brnpain1.wav"},
                 {S(109, 116), "brain/brnpain2.wav"},
                 {S(117, 122), "brain/brnpain1.wav"}}}}},
    {.classname = "monster_parasite",
     .sounds = {"parasite/parpain1.wav", "parasite/parpain2.wav"},
     .bands = {{kAny, 1, {{S(57, 67)}}}}},
    {.classname = "monster_mutant",
     .bands = {{kAny,
                1,
                {{S(34, 38), "mutant/mutpain1.wav"},
                 {S(39, 44), "mutant/mutpain2.wav"},
                 {S(45, 55), "mutant/mutpain1.wav"}}}}},
    {.classname = "monster_flyer",
     .bands = {{kAny,
                1,
                {{S(134, 142), "flyer/flypain1.wav"},
                 {S(143, 146), "flyer/flypain2.wav"},
                 {S(147, 150), "flyer/flypain1.wav"}}}}},
    {.classname = "monster_hover",
     .bands = {{25, 1, {{S(153, 161), "hover/hovpain1.wav"}, {S(141, 152), "hover/hovpain2.wav"}}},
               {kAny, 1, {{S(113, 140), "hover/hovpain1.wav"}}}}},
    // Ground Zero's daedalus: the hover's runs with its own cries, a heavy blow's long flinch
    // less likely as the skill rises (gz-monsters.md § 2.15, § 7.3).
    {.classname = "monster_daedalus",
     .bands = {{25,
                1,
                {{S(153, 161), "daedalus/daedpain1.wav"},
                 {S(141, 152), "daedalus/daedpain2.wav"}}},
               {kAny,
                1,
                {{S(113, 140), "daedalus/daedpain1.wav"}, {S(141, 152), "daedalus/daedpain2.wav"}},
                0.45f,
                0,
                -0.1f}}},
    // Ground Zero's medic commander: a light blow only cries; a heavy one cries louder and
    // flinches long with chance 0.005 a point, at most a half (gz-monsters.md § 7.4).
    {.classname = "monster_medic_commander",
     .sounds = {"medic_commander/medpain1.wav"},
     .cry_most = 34,
     .bands = {{kAny,
                1,
                {{S(116, 130), "medic_commander/medpain2.wav"},
                 {S(108, 115), "medic_commander/medpain2.wav"}},
                0,
                0.005f,
                0,
                0.5f}}},
    {.classname = "monster_floater",
     .bands = {{kAny,
                1,
                {{S(117, 123), "floater/fltpain1.wav"},
                 {S(124, 131), "floater/fltpain2.wav"},
                 {S(124, 131), "floater/fltpain2.wav"}}}}},
    {.classname = "monster_flipper",
     .bands = {{kAny,
                1,
                {{S(99, 103), "flipper/flppain1.wav"}, {S(94, 98), "flipper/flppain2.wav"}}}}},
    {.classname = "monster_tank",
     .sounds = {"tank/tnkpain2.wav"},
     .nightmare = Nightmare::cry,
     .never = 10,
     .shrug_most = 30,
     .shrug = 0.8f,
     .busy = {S(115, 144), S(55, 70)},
     .bands = {{30, 1, {{S(197, 200)}}}, {60, 1, {{S(201, 205)}}}, {kAny, 1, {{S(206, 221)}}}}},
    {.classname = "monster_tank_commander",
     .sounds = {"tank/tnkpain2.wav"},
     .nightmare = Nightmare::cry,
     .never = 10,
     .shrug_most = 30,
     .shrug = 0.8f,
     .busy = {S(115, 144), S(55, 70)},
     .bands = {{30, 1, {{S(197, 200)}}}, {60, 1, {{S(201, 205)}}}, {kAny, 1, {{S(206, 221)}}}}},
    {.classname = "monster_supertank",
     .shrug_most = 25,
     .shrug = 0.2f,
     .busy = {S(20, 33)},
     .bands = {{10, 1, {{S(164, 167), "bosstank/btkpain1.wav"}}},
               {25, 1, {{S(168, 171), "bosstank/btkpain3.wav"}}},
               {kAny, 1, {{S(172, 175), "bosstank/btkpain2.wav"}}}}},
    {.classname = "monster_boss2",
     .nightmare = Nightmare::both,
     .everywhere = true,
     .bands = {{9, 1, {{S(128, 131), "bosshovr/bhvpain3.wav"}}},
               {29, 1, {{S(128, 131), "bosshovr/bhvpain1.wav"}}},
               {kAny, 1, {{S(110, 127), "bosshovr/bhvpain2.wav"}}}}},
    {.classname = "monster_jorg",
     .shrug_most = 40,
     .shrug = 0.6f,
     .bands = {{50, 1, {{S(81, 83), "boss3/bs3pain1.wav"}}},
               {100, 1, {{S(84, 86), "boss3/bs3pain2.wav"}}},
               {kAny, 0.3f, {{S(87, 111), "boss3/bs3pain3.wav"}}}}},
    {.classname = "monster_makron",
     .everywhere = true,
     .shrug_most = 25,
     .shrug = 0.2f,
     .bands = {{40, 1, {{S(379, 382), "makron/pain3.wav"}}},
               {110, 1, {{S(383, 386), "makron/pain2.wav"}}},
               {150, 0.45f, {{S(387, 413), "makron/pain1.wav"}}},
               {kAny, 0.35f, {{S(387, 413), "makron/pain1.wav"}}}}},
};

const PainRule* rule_of(std::string_view classname) {
  for (const PainRule& r : kPains)
    if (classname == r.classname) return &r;
  return nullptr;
}

bool inside(SqSpan s, int frame) { return !s.none() && frame >= s.first && frame <= s.last; }

double draw(ge::core::Rng& rng) { return ge::core::next(rng); }

const char* cry_of(const PainRule& r, const PainRun& run, ge::core::Rng& rng) {
  if (run.sound != nullptr) return run.sound;
  int n = 0;
  while (n < 3 && r.sounds[n] != nullptr) n++;
  if (n <= 1) return r.sounds[0];
  const int i = static_cast<int>(draw(rng) * n);
  return r.sounds[i < n ? i : n - 1];
}

// A death run (its index into the kind's poses) and its cry (null: one of `sounds`).
struct DeathRun {
  int run;
  const char* sound = nullptr;
};

struct DeathRule {
  const char* classname;
  const char* sounds[2] = {};
  bool everywhere = false;
  int16_t blow = -1;  // a killing blow this or more: the first run, else the second
  DeathRun runs[3] = {};
};

constexpr DeathRule kDeaths[] = {
    {.classname = "monster_infantry",
     .runs = {{0, "infantry/infdeth2.wav"},
              {1, "infantry/infdeth1.wav"},
              {2, "infantry/infdeth2.wav"}}},
    {.classname = "monster_chick", .runs = {{0, "chick/chkdeth1.wav"}, {1, "chick/chkdeth2.wav"}}},
    {.classname = "monster_berserk",
     .sounds = {"berserk/berdeth2.wav"},
     .blow = 50,
     .runs = {{0}, {1}}},
    {.classname = "monster_hover",
     .sounds = {"hover/hovdeth1.wav", "hover/hovdeth2.wav"},
     .runs = {{0}}},
    {.classname = "monster_daedalus",
     .sounds = {"daedalus/daeddeth1.wav", "daedalus/daeddeth2.wav"},
     .runs = {{0}}},
    {.classname = "monster_medic_commander", .sounds = {"medic_commander/meddeth.wav"}, .runs = {{0}}},
    {.classname = "monster_boss2",
     .sounds = {"bosshovr/bhvdeth1.wav"},
     .everywhere = true,
     .runs = {{0}}},
    {.classname = "monster_makron",
     .sounds = {"makron/death.wav"},
     .everywhere = true,
     .runs = {{0}}},
};

}  // namespace

bool monster_pain_known(std::string_view classname) { return rule_of(classname) != nullptr; }

const char* pain_class(const roster::Kind& k) {
  if (rule_of(k.classname) != nullptr) return k.classname;
  for (const DeathRule& d : kDeaths)
    if (std::string_view(k.classname) == d.classname) return k.classname;
  return roster::looks_like(k);
}

PainChoice monster_pain(std::string_view classname, int damage, int skill, float up, int frame,
                        int first, bool act_pain, bool waiting, ge::core::Rng& rng) {
  PainChoice c;
  const PainRule* r = rule_of(classname);
  if (r == nullptr || damage <= r->never) return c;
  c.everywhere = r->everywhere;
  const bool rising = !r->air.none() && up > 100;
  if (waiting) {  // a pain run playing swapped for the knocked-up one, no cry
    if (rising && act_pain && first != r->air.first) c.run = r->air;
    return c;
  }
  c.spent = true;
  if (damage <= r->cry_most) {
    c.sound = r->sounds[0];
    return c;
  }
  if (damage <= r->shrug_most && draw(rng) < r->shrug) return c;
  if (skill >= 2)
    for (SqSpan b : r->busy)
      if (inside(b, frame)) return c;
  if (skill == 3 && r->nightmare == Nightmare::nothing) return c;
  const PainBand* band = &r->bands[0];
  for (const PainBand& b : r->bands) {
    band = &b;
    if (damage <= b.most) break;
  }
  if (band->odds < 1 && draw(rng) >= band->odds) return c;
  int n = 0;
  while (n < 3 && !band->runs[n].run.none()) n++;
  if (n == 0) return c;
  int pick = 0;
  // The cry first when skill 3 keeps it (a soldier's comes before its knocked-up test).
  if (r->nightmare == Nightmare::cry) {
    if (n > 1) pick = static_cast<int>(draw(rng) * n);
    pick = pick < n ? pick : n - 1;
    c.sound = cry_of(*r, band->runs[pick], rng);
    if (rising && (r->air_nightmare || skill < 3)) {
      c.run = r->air;
      return c;
    }
    if (skill == 3) return c;
    c.run = band->runs[pick].run;
    return c;
  }
  if (band->first_odds >= 0 && n > 1) {
    const float odds = std::min(band->first_odds + band->per_point * float(damage) +
                                    band->per_skill * float(skill),
                                band->first_cap);
    pick = draw(rng) < odds ? 0 : 1;
  } else if (n > 1) {
    pick = static_cast<int>(draw(rng) * n);
  }
  pick = pick < n ? pick : n - 1;
  c.sound = cry_of(*r, band->runs[pick], rng);
  c.run = rising ? r->air : band->runs[pick].run;
  return c;
}

DeathChoice monster_death(std::string_view classname, int damage, ge::core::Rng& rng) {
  for (const DeathRule& d : kDeaths) {
    if (classname != d.classname) continue;
    DeathChoice c;
    c.everywhere = d.everywhere;
    int n = 0;
    while (n < 3 && (n == 0 || d.runs[n].run != 0)) n++;
    int pick = 0;
    if (d.blow >= 0)
      pick = damage >= d.blow ? 0 : 1;
    else if (n > 1)
      pick = static_cast<int>(draw(rng) * n);
    pick = pick < n ? pick : n - 1;
    c.run = d.runs[pick].run;
    c.sound = d.runs[pick].sound;
    if (c.sound == nullptr) {
      const int m = d.sounds[1] != nullptr ? 2 : 1;
      c.sound = d.sounds[m == 1 ? 0 : (draw(rng) < 0.5 ? 0 : 1)];
    }
    return c;
  }
  return {};
}

void monster_pain_sounds(std::string_view classname, void (*add)(void*, const char*), void* to) {
  if (const PainRule* r = rule_of(classname)) {
    for (const char* s : r->sounds)
      if (s != nullptr) add(to, s);
    for (const PainBand& b : r->bands)
      for (const PainRun& run : b.runs)
        if (run.sound != nullptr) add(to, run.sound);
  }
  for (const DeathRule& d : kDeaths) {
    if (classname != d.classname) continue;
    for (const char* s : d.sounds)
      if (s != nullptr) add(to, s);
    for (const DeathRun& run : d.runs)
      if (run.sound != nullptr) add(to, run.sound);
  }
}

}  // namespace shooters_edge::server
