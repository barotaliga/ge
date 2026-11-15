// Item roles (plugins/classic/item_roles.hpp) with no game's data: two small made-up games swap
// items by role, a variant read before the plain one, a role with no spawn dropped, an unknown item
// not known, the variant bits replaced and the rest left to the caller.
//
// ge:summary Tests item roles: reading by variant, laying down by role, dropped and unknown items
// ge:keywords test item-roles swap_item read_item spawn_of remix variant-bits
// ge:skill classic testing
#include "check.hpp"
#include "classic/item_roles.hpp"

using ge::classic::ItemRole;

namespace {

constexpr ge::classic::ItemReading kRedReads[] = {
    {"weapon_tube", 0, ItemRole::launcher},
    {"item_med", 2, ItemRole::health_mega},
    {"item_med", 0, ItemRole::health_large},
    {"item_flake", 0, ItemRole::armour_shard},
};
constexpr ge::classic::ItemSpawn kRedSpawns[] = {
    {ItemRole::launcher, "weapon_tube", 0},
    {ItemRole::health_large, "item_med", 0},
    {ItemRole::health_mega, "item_med", 2},
};
constexpr ge::classic::ItemReading kBlueReads[] = {
    {"weapon_rocket", 0, ItemRole::launcher},
    {"item_big_med", 0, ItemRole::health_mega},
};
constexpr ge::classic::ItemSpawn kBlueSpawns[] = {
    {ItemRole::launcher, "weapon_rocket", 0},
    {ItemRole::health_large, "item_med_kit", 0},
    {ItemRole::health_mega, "item_big_med", 0},
};
constexpr ge::classic::GameItems kRed{"red", kRedReads, kRedSpawns};
constexpr ge::classic::GameItems kBlue{"blue", kBlueReads, kBlueSpawns};

}  // namespace

TEST(an_item_is_laid_down_by_its_role) {
  const ge::classic::ItemSwap s = ge::classic::swap_item(kRed, kBlue, "weapon_tube", 0);
  CHECK(s.known && s.becomes == "weapon_rocket" && s.bits == 0);
  const ge::classic::ItemSwap back = ge::classic::swap_item(kBlue, kRed, "item_big_med", 0);
  CHECK(back.known && back.becomes == "item_med" && back.bits == 2);  // the spawn's variant
}

TEST(a_variant_is_read_before_the_plain_item) {
  CHECK(ge::classic::swap_item(kRed, kBlue, "item_med", 2).becomes == "item_big_med");
  CHECK(ge::classic::swap_item(kRed, kBlue, "item_med", 2 | 0x800).becomes == "item_big_med");
  CHECK(ge::classic::swap_item(kRed, kBlue, "item_med", 1).becomes == "item_med_kit");
  CHECK(ge::classic::swap_item(kRed, kBlue, "item_med", 0x200).becomes ==
        "item_med_kit");  // above 0xff
}

TEST(a_role_with_no_spawn_drops_and_an_unread_item_is_unknown) {
  const ge::classic::ItemSwap shard = ge::classic::swap_item(kRed, kBlue, "item_flake", 0);
  CHECK(shard.known && shard.becomes.empty());
  const ge::classic::ItemSwap none = ge::classic::swap_item(kRed, kBlue, "item_nothing", 0);
  CHECK(!none.known && none.becomes.empty());
  CHECK(ge::classic::read_item(kBlue, "weapon_tube", 0) == nullptr);
  CHECK(ge::classic::spawn_of(kRed, ItemRole::quiet) == nullptr);
}
