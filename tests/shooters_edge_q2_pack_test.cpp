// Tests: Quake II's mission pack switch (games/shooters_edge/server/q2_pack.hpp): the game folder
// each pack plays from, a pack's rows found only under that pack, and the base game's item numbers
// unchanged now that they are looked up by name in each pack's list.
//
// ge:summary Tests: Quake II's pack switch, the folders, the rows each pack plays, the numbers
// ge:keywords test q2 mission-pack pack-switch xatrix rogue item-number
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_pack_test.cpp
#include "check.hpp"
#include "shooters_edge/server/q2_bar.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_pack.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace sv = shooters_edge::server;
using sv::Q2Pack;

TEST(each_pack_plays_from_its_own_folder) {
  CHECK(sv::q2_pack_of_folder("xatrix") == Q2Pack::reckoning);
  CHECK(sv::q2_pack_of_folder("ROGUE") == Q2Pack::ground_zero);
  CHECK(sv::q2_pack_of_folder("/games/quake2/rogue/") == Q2Pack::ground_zero);
  CHECK(sv::q2_pack_of_folder("C:\\Quake2\\xatrix") == Q2Pack::reckoning);
  CHECK(sv::q2_pack_of_folder("baseq2") == Q2Pack::base);
  CHECK(sv::q2_pack_of_folder("ctf") == Q2Pack::base);
  CHECK(sv::q2_pack_of_folder("") == Q2Pack::base);
  CHECK(sv::q2_pack_name(Q2Pack::ground_zero) == "Ground Zero");
  CHECK(sv::q2_pack_name(Q2Pack::reckoning) == "The Reckoning");
  CHECK(sv::q2_pack_name(Q2Pack::base) == "Quake II");
}

TEST(a_pack_s_rows_play_only_under_that_pack) {
  CHECK(sv::q2_pack_has(Q2Pack::base, Q2Pack::base));
  CHECK(sv::q2_pack_has(Q2Pack::ground_zero, Q2Pack::base));
  CHECK(sv::q2_pack_has(Q2Pack::ground_zero, Q2Pack::ground_zero));
  CHECK(!sv::q2_pack_has(Q2Pack::base, Q2Pack::ground_zero));
  CHECK(!sv::q2_pack_has(Q2Pack::reckoning, Q2Pack::ground_zero));
  CHECK(!sv::q2_pack_has(Q2Pack::ground_zero, Q2Pack::reckoning));
  // Every base row is found under every pack.
  for (int i = 0; i < sv::kSqItemCount; i++) {
    if (sv::kSqItems[i].pack != Q2Pack::base) continue;
    CHECK(sv::sq_item(sv::kSqItems[i].classname) == i);
    CHECK(sv::sq_item(sv::kSqItems[i].classname, Q2Pack::reckoning) == i);
    CHECK(sv::sq_item(sv::kSqItems[i].classname, Q2Pack::ground_zero) == i);
  }
}

TEST(the_base_game_s_item_numbers_are_unchanged) {
  CHECK(sv::sq_numbers() == sv::kSqItemNumbers);
  CHECK(sv::sq_number_of("item_armor_body") == 1);
  CHECK(sv::sq_number_of("weapon_blaster") == sv::kSqBlasterNumber);
  CHECK(sv::sq_number_of("ammo_grenades") == 12);
  CHECK(sv::sq_number_of("ammo_shells") == 18);
  CHECK(sv::sq_number_of("item_quad") == 23);
  CHECK(sv::sq_number_of("item_health") == 41);
  CHECK(sv::sq_number_of("no_such_item") == 0);
  CHECK(sv::sq_item_number(sv::sq_item("item_health_mega")) == 41);
  CHECK(sv::sq_weapon_number(0) == 7);    // the blaster
  CHECK(sv::sq_weapon_number(10) == 12);  // the hand grenades
  // The kinds by name match the base list's ranges: the weapons 7..17, the power-ups with a use
  // 23..27, and what a pickup selects: the power armours too.
  for (int n = 0; n < sv::kSqItemNumbers; n++) {
    CHECK(sv::sq_weapon_numbered(n) == (n >= 7 && n <= 17));
    CHECK(sv::sq_powerup_numbered(n) == (n >= 23 && n <= 27));
    CHECK(sv::sq_selects(n) == ((n >= 5 && n <= 17) || (n >= 23 && n <= 27)));
  }
  // The armour worn counts under its own number only.
  sv::SqBody b;
  b.armour = sv::SqArmour::combat;
  b.armour_count = 70;
  CHECK(sv::sq_count(b, 1) == 0);
  CHECK(sv::sq_count(b, 2) == 70);
  CHECK(sv::sq_count(b, 3) == 0);
  CHECK(sv::sq_count(b, 4) == 0);   // the shard
  CHECK(sv::sq_count(b, 41) == 0);  // health
  CHECK(sv::sq_count(b, 7) == 1);   // the blaster
}

TEST(ground_zero_numbers_its_items_in_its_own_order) {
  const auto gz = Q2Pack::ground_zero;
  CHECK(sv::sq_numbers(gz) == 62);  // slot 0 and 61 rows
  CHECK(sv::sq_number_of("weapon_chaingun", gz) == 11);
  CHECK(sv::sq_number_of("weapon_etf_rifle", gz) == 12);
  CHECK(sv::sq_number_of("ammo_grenades", gz) == 13);
  CHECK(sv::sq_number_of("weapon_proxlauncher", gz) == 15);
  CHECK(sv::sq_number_of("weapon_plasmabeam", gz) == 18);
  CHECK(sv::sq_number_of("weapon_disintegrator", gz) == 22);
  CHECK(sv::sq_number_of("ammo_tesla", gz) == 30);
  CHECK(sv::sq_number_of("item_quad", gz) == 33);
  CHECK(sv::sq_number_of("item_double", gz) == 43);
  CHECK(sv::sq_number_of("key_nuke", gz) == 60);
  CHECK(sv::sq_number_of("item_health", gz) == 61);
  // The pack's rows are not found in the base game.
  CHECK(sv::sq_item("weapon_etf_rifle") < 0);
  CHECK(sv::sq_number_of("weapon_etf_rifle") == 0);
  CHECK(sv::sq_weapon_numbered(12, gz));   // the ETF rifle
  CHECK(sv::sq_weapon_numbered(30, gz));   // the tesla, thrown like a grenade
  CHECK(!sv::sq_weapon_numbered(28, gz));  // flechettes
  CHECK(sv::sq_powerup_numbered(43, gz));  // double damage
}

TEST(ground_zero_s_body_carries_its_ammo) {
  sv::SqBody base = sv::sq_new_body();
  CHECK(base.max_ammo[int(sv::SqAmmo::flechettes)] == 0);
  sv::SqBody b = sv::sq_new_body(Q2Pack::ground_zero);
  CHECK(b.max_ammo[int(sv::SqAmmo::flechettes)] == 200);
  CHECK(b.max_ammo[int(sv::SqAmmo::prox)] == 50);
  CHECK(b.max_ammo[int(sv::SqAmmo::tesla)] == 50);
  CHECK(b.max_ammo[int(sv::SqAmmo::rounds)] == 100);
  CHECK(b.max_ammo[int(sv::SqAmmo::magslugs)] == 0);  // The Reckoning's
  // The ammo pack raises the pack's limits and gives one box of each, but no mines.
  const int pack = sv::sq_item("item_pack", Q2Pack::ground_zero);
  CHECK(sv::sq_pick_up(pack, b, true, 1, 0, 0));
  CHECK(b.max_ammo[int(sv::SqAmmo::flechettes)] == 300);
  CHECK(b.max_ammo[int(sv::SqAmmo::rounds)] == 200);
  CHECK(b.ammo[int(sv::SqAmmo::flechettes)] == 50);
  CHECK(b.ammo[int(sv::SqAmmo::rounds)] == 15);
  CHECK(b.ammo[int(sv::SqAmmo::prox)] == 0);
  CHECK(b.ammo[int(sv::SqAmmo::tesla)] == 0);
}

TEST(a_server_is_made_with_its_pack) {
  sv::Q2ServerOptions o;
  o.pack = sv::q2_pack_of_folder("rogue");
  const auto s = sv::make_q2_server(o);
  CHECK(s->opt.pack == Q2Pack::ground_zero);
  CHECK(sv::make_q2_server({})->opt.pack == Q2Pack::base);
}

}  // namespace
