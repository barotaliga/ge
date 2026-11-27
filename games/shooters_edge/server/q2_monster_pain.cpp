// Quake II's monsters' pains and deaths as data: the pain table's rule on the server's random
// numbers (the table itself is q2_monster_pain_rules.hpp, which any rules read), each kind's death
// runs and their cries (porting/shooters-edge/q2-monsters-round4.md § 1 to 3).
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

using q2_pain::PainBand;
using q2_pain::PainRule;
using q2_pain::PainRun;
using q2_pain::rule_of;

double draw(ge::core::Rng& rng) { return ge::core::next(rng); }

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
    {.classname = "monster_medic_commander",
     .sounds = {"medic_commander/meddeth.wav"},
     .runs = {{0}}},
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
  return q2_pain::pain(classname, damage, skill, up, frame, first, act_pain, waiting,
                       [&] { return draw(rng); });
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
