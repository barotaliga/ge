// A classic game's game half (games/shooters_edge/profiles/game_profile.hpp): every game row has
// its content row; the classic game's multiplayer program files put the multiplayer form's own
// name first; the dedicated server's base folder is the content row's; and the item rows give the
// bots a map's items (variants first, ammunition doubled, any other armour), their call and heard
// words, and the sound table.
//
// ge:summary Tests the game profile rows: content rows, program names, the items and sounds
// ge:keywords test shooters-edge game-profile program qwprogs multiplayer items
// ge:skill shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "shooters_edge/bots/ears.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/dedicated.hpp"

namespace pf = shooters_edge::profiles;

TEST(every_game_row_has_its_content_row) {
  for (const pf::GameProfile& g : pf::kGameProfiles) {
    CHECK(ge::classicenhance::find_profile(g.id) != nullptr);
    CHECK(&pf::content_of(g) == ge::classicenhance::find_profile(g.id));
    CHECK(!g.multiplayer_programs.empty() && !g.single_player_programs.empty());
    CHECK(!g.fallback_map.empty());
  }
  CHECK(pf::find_game(pf::classic_game().id) == &pf::classic_game());
  CHECK(pf::find_game("no-such-game") == nullptr);
  CHECK(pf::classic_game().multiplayer_programs[0].name == "qwprogs.dat");
  CHECK(pf::classic_game().demo_extensions[0] == ".dem");
  CHECK(std::string(shooters_edge::server::dedicated_game().base_dir) ==
        std::string(ge::classicenhance::classic_profile().base_dir));
}

TEST(the_item_rows_read_a_maps_items_and_their_words) {
  namespace bt = shooters_edge::bots;
  const auto item = [](const char* cn, int flags) {
    bt::ItemSpot it;
    CHECK(bt::item_of(cn, flags, it));
    return it;
  };
  const bt::ItemSpot mega = item("item_health", 3);  // the mega's bit wins over the small one's
  CHECK(mega.kind == pf::ItemKind::mega && mega.amount == 100 && mega.name == "mega health");
  CHECK(item("item_health", 1).amount == 15 && item("item_health", 0).amount == 25);
  CHECK(item("item_rockets", 1).amount == 10 && item("item_rockets", 0).amount == 5);
  const bt::ItemSpot ra = item("item_armorInv", 0);
  CHECK(ra.kind == pf::ItemKind::armor3 && ra.name == "item_armorInv");
  CHECK(std::string(bt::item_call(ra)) == "ra" && std::string(bt::hears_as(ra)) == "armor");
  const bt::ItemSpot odd = item("item_armorX", 0);  // any other armour: the bot's default kind
  CHECK(odd.kind == bt::ItemSpot{}.kind && odd.name == "item_armorX");
  const bt::ItemSpot rl = item("weapon_rocketlauncher", 0);
  CHECK(rl.weapon == 32 && rl.respawn_s == 30 && std::string(bt::item_call(rl)) == "rl");
  CHECK(std::string(bt::item_call(item("weapon_lightning", 0))) == "lg");
  CHECK(item("item_artifact_super_damage", 0).respawn_s == 60);
  bt::ItemSpot none;
  CHECK(!bt::item_of("info_player_start", 0, none));
  CHECK(pf::classic_game().tables.sounds.data() == bt::sound_table().data());
}
