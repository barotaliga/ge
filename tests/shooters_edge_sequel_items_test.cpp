// Tests: the sequel's items as data (games/shooters_edge/server/sequel_items.hpp): every class name
// finds its row; ammo stops at the carry limit and is refused there; a weapon is always taken;
// health is refused at the maximum except the small one and the mega; armour converts by the
// ratio of the shares, the better one winning; a shard adds 2 or starts a jacket; powerups are held
// to the skill's count; the bandolier and the pack raise the limits.
//
// ge:summary Tests: the sequel's items: the table, ammo, weapons, health, armour, powerups, packs
// ge:keywords test sequel items pickup armour health ammo powerup bandolier carry-limit
// ge:skill shooter testing
#include <string>

#include "check.hpp"
#include "shooters_edge/server/sequel_items.hpp"

namespace {

namespace sv = shooters_edge::server;

int row(const char* name) {
  const int i = sv::sq_item(name);
  CHECK_MSG(i >= 0, std::string("no item row for ") + name);
  return i;
}

bool take(const char* name, sv::SqBody& b, int skill = 1, bool dm = true) {
  return sv::sq_pick_up(row(name), b, dm, skill, 10);
}

int ammo(const sv::SqBody& b, sv::SqAmmo a) { return b.ammo[static_cast<int>(a)]; }

}  // namespace

TEST(every_item_s_class_name_finds_its_row_and_others_none) {
  for (int i = 0; i < sv::kSqItemCount; i++) CHECK(sv::sq_item(sv::kSqItems[i].classname) == i);
  CHECK(sv::sq_item("info_player_start") == -1);
}

TEST(ammo_stops_at_the_carry_limit_and_a_weapon_is_always_taken) {
  sv::SqBody b = sv::sq_new_body();
  CHECK(b.health == 100 && b.max_health == 100 && b.blaster);
  for (int i = 0; i < 10; i++) CHECK(take("ammo_rockets", b));  // 50 rockets
  CHECK(ammo(b, sv::SqAmmo::rockets) == 50);
  CHECK(!take("ammo_rockets", b));          // at the limit
  CHECK(take("weapon_rocketlauncher", b));  // taken though the rockets are full
  CHECK(b.counts[row("weapon_rocketlauncher")] == 1 && ammo(b, sv::SqAmmo::rockets) == 50);
  CHECK(take("weapon_railgun", b) && ammo(b, sv::SqAmmo::slugs) == 10);
}

TEST(health_is_refused_at_the_maximum_but_the_small_one_and_the_mega_go_past) {
  sv::SqBody b = sv::sq_new_body();
  CHECK(!take("item_health", b));
  CHECK(!take("item_health_large", b));
  CHECK(take("item_health_small", b) && b.health == 102);
  CHECK(take("item_health_mega", b) && b.health == 202 && b.mega_until_drain == 15);
  b.health = 95;
  CHECK(take("item_health", b) && b.health == 100);  // held to the maximum
  CHECK(take("item_adrenaline", b, 1, true) && b.max_health == 100);
  CHECK(take("item_adrenaline", b, 1, false) && b.max_health == 101 && b.health == 101);
  CHECK(take("item_ancient_head", b) && b.max_health == 103);
}

TEST(armour_converts_by_the_shares_and_the_better_one_wins) {
  sv::SqBody b = sv::sq_new_body();
  CHECK(take("item_armor_shard", b) && b.armour == sv::SqArmour::jacket && b.armour_count == 2);
  CHECK(take("item_armor_jacket", b) && b.armour == sv::SqArmour::jacket);
  CHECK(b.armour_count == 2 + 25);  // the same share: 25 × 1 onto the worn one
  // Combat is better: 27 × 0.3 / 0.6 = 13 (truncated) carried onto its 50.
  CHECK(take("item_armor_combat", b) && b.armour == sv::SqArmour::combat);
  CHECK(b.armour_count == 50 + 13);
  // A jacket onto combat: 25 × 0.3 / 0.6 = 12 added.
  CHECK(take("item_armor_jacket", b) && b.armour_count == 75);
  b.armour_count = 100;  // combat's maximum: a jacket adds nothing and is refused
  CHECK(!take("item_armor_jacket", b) && b.armour_count == 100);
  CHECK(take("item_armor_body", b) && b.armour == sv::SqArmour::body);
  CHECK(b.armour_count == 100 + 75);  // 100 × 0.6 / 0.8
}

TEST(powerups_are_held_to_the_skill_s_count) {
  sv::SqBody b = sv::sq_new_body();
  CHECK(take("item_quad", b, 1) && take("item_quad", b, 1) && !take("item_quad", b, 1));
  sv::SqBody hard = sv::sq_new_body();
  CHECK(take("item_quad", hard, 2) && !take("item_quad", hard, 2));
  sv::SqBody easy = sv::sq_new_body();
  for (int i = 0; i < 5; i++) CHECK(take("item_quad", easy, 0));
}

TEST(the_bandolier_and_the_pack_raise_the_limits_and_fill_some) {
  sv::SqBody b = sv::sq_new_body();
  CHECK(take("item_bandolier", b));
  CHECK(b.max_ammo[static_cast<int>(sv::SqAmmo::bullets)] == 250);
  CHECK(ammo(b, sv::SqAmmo::bullets) == 50 && ammo(b, sv::SqAmmo::shells) == 10);
  CHECK(take("item_pack", b));
  CHECK(b.max_ammo[static_cast<int>(sv::SqAmmo::rockets)] == 100);
  CHECK(ammo(b, sv::SqAmmo::rockets) == 5 && ammo(b, sv::SqAmmo::bullets) == 100);
  CHECK(take("key_blue_key", b) && take("key_blue_key", b));  // keys are always taken
}
