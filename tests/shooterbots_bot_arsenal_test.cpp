// The reach a bot's loaded weapons give it (plugins/shooterbots/bot_arsenal.hpp `loaded_reach`):
// a blade alone is a reach of its own range, a gun carried with no ammunition adds nothing, and a
// loaded gun's range is the reach. The fighter turns from a foe kept in sight past a blade's reach
// (`outreach`): on e1m4 a bot out of shells held a fight with the axe for nine minutes against a
// monster across the pit.
//
// ge:summary Tests for the reach of a bot's loaded weapons: a blade's alone, a gun's when loaded
// ge:keywords test arsenal loaded_reach reach blade axe out-of-ammo outreach fighter
// ge:skill shooterbots testing
#include <array>

#include "check.hpp"
#include "shooterbots/bot_arsenal.hpp"

namespace {

namespace sb = ge::shooterbots;

struct TwoWeapons {
  std::array<sb::BotWeapon, 2> w{};
  sb::BotArsenal a;
  TwoWeapons() {
    w[0].bit = 1;  // a blade: no ammunition
    w[0].range = 72;
    w[1].bit = 2;  // a gun on shells
    w[1].ammo = 0;
    w[1].cost = 1;
    w[1].range = 1200;
    a.weapons = w;
  }
};

}  // namespace

TEST(a_blade_alone_reaches_its_own_range) {
  const TwoWeapons t;
  sb::Stats st;
  st.arsenal = &t.a;
  st.items = 1;
  CHECK(sb::loaded_reach(st) == 72);
  // The gun carried with no shells: still the blade's.
  st.items = 3;
  CHECK(sb::loaded_reach(st) == 72);
  st.ammo[0] = 5;
  CHECK(sb::loaded_reach(st) == 1200);
}

TEST(nothing_carried_reaches_nothing) {
  const TwoWeapons t;
  sb::Stats st;
  st.arsenal = &t.a;
  CHECK(sb::loaded_reach(st) == 0);
}
