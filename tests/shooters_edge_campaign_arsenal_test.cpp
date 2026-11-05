// Tests: our programs fire and absorb through the arsenal's rows (games/shooters_edge/qc/common/
// weapons.qc and items.qc; the rows' C++ side is profiles/arsenal.hpp), on the campaign host in the
// arena's west room. With no rows a slot keeps the program's own (a shot of the scattergun costs a
// shell, green armour is 100 at 30 %); a row the server is given changes what the slot does (a
// shot's cost, the armour's amount and share); the program parses a row's numbers the same way in
// either form.
//
// ge:summary Tests: our programs' weapons and armour read the arsenal's rows
// ge:keywords test arsenal weapon-row armour-row arsenal_weapon arsenal_armour cvar_string
// ge:keywords number_of campaign-mod
// ge:skill progs shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/profiles/arsenal.hpp"
#include "shooters_edge_campaign_fixture.hpp"

using namespace campfix;
namespace pf = shooters_edge::profiles;

namespace {

// One shot of the held weapon (the scattergun a new game starts with): the shells it cost.
float shells_a_shot(Game& g) {
  for (int i = 0; i < 4; i++) CHECK_MSG(g.frame(0), g.error);
  const float before = g.named(1, "ammo_shells");
  CHECK_MSG(g.frame(0, 1), g.error);
  return before - g.named(1, "ammo_shells");
}

const char* kArmourHere = R"({
"classname" "item_armor1"
"origin" "-416 0 24"
}
)";

}  // namespace

TEST(with_no_rows_a_slot_keeps_the_programs_own) {
  Game g("");
  CHECK(shells_a_shot(g) == 1);
  Game a(kArmourHere);
  for (int i = 0; i < 4; i++) CHECK_MSG(a.frame(0), a.error);
  CHECK(a.named(1, "armorvalue") == 100);
  CHECK(a.named(1, "armortype") == 0.3f);
  CHECK_MSG(g.s().q.faults == 0 && a.s().q.faults == 0, g.s().q.last_fault + a.s().q.last_fault);
}

TEST(the_classic_rows_given_play_as_the_programs_own) {
  const std::vector<std::string> lines = pf::arsenal_lines(pf::kClassicArsenal);
  Game g("", lines);
  CHECK(shells_a_shot(g) == 1);
  Game a(kArmourHere, lines);
  for (int i = 0; i < 4; i++) CHECK_MSG(a.frame(0), a.error);
  CHECK(a.named(1, "armorvalue") == 100);
  CHECK(a.named(1, "armortype") == 0.3f);
  CHECK_MSG(g.s().q.faults == 0 && a.s().q.faults == 0, g.s().q.last_fault + a.s().q.last_fault);
}

TEST(a_row_given_changes_what_its_slot_does) {
  pf::WeaponRow scatter = pf::kClassicWeapons[1];
  scatter.cost = 3;
  pf::ArmourRow green = pf::kClassicArmour[0];
  green.amount = 77;
  green.absorb = 0.45f;
  const std::vector<std::string> lines = {
      "set arsenal_weapon2 \"" + pf::weapon_text(scatter) + "\"",
      "set arsenal_armour1 \"" + pf::armour_text(green) + "\"",
  };
  Game g("", lines);
  CHECK(shells_a_shot(g) == 3);
  Game a(kArmourHere, lines);
  for (int i = 0; i < 4; i++) CHECK_MSG(a.frame(0), a.error);
  CHECK(a.named(1, "armorvalue") == 77);
  CHECK(a.named(1, "armortype") == 0.45f);  // parsed to the float a literal of it is
  CHECK_MSG(g.s().q.faults == 0 && a.s().q.faults == 0, g.s().q.last_fault + a.s().q.last_fault);
}

// A weapon lying on the map takes its slot's row: the ammunition that comes with it (and the
// model it lies as); with no row, the map's own.
TEST(a_weapon_on_the_map_comes_with_its_rows_ammunition) {
  const char* here = R"({
"classname" "weapon_supershotgun"
"origin" "-416 0 24"
}
)";
  const auto shells_gained = [&](const std::vector<std::string>& lines) {
    Game g(here, lines);
    const float before = g.named(1, "ammo_shells");
    for (int i = 0; i < 4; i++) CHECK_MSG(g.frame(0), g.error);
    CHECK_MSG(g.s().q.faults == 0, g.s().q.last_fault);
    return g.named(1, "ammo_shells") - before;
  };
  const float own = shells_gained({});
  CHECK_MSG(own == 5, std::to_string(own));
  pf::WeaponRow double_row = pf::kClassicWeapons[2];
  double_row.pickup_ammo = 11;
  const float given =
      shells_gained({"set arsenal_weapon3 \"" + pf::weapon_text(double_row) + "\""});
  CHECK_MSG(given == 11, std::to_string(given));
}
