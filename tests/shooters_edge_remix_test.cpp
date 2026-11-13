// Classic remix (games/shooters_edge/client/remix.hpp, profiles/remix.hpp): the table swaps each
// game's items by role and variant, keeps what is not an item and the skill and mode bits, drops
// an item with no match; the check wants two full games; cr_items names whose items a map gets.
//
// ge:summary Tests classic remix: the item table, the entity rewrite, the check, cr_items
// ge:keywords test remix classic-remix item-table remix_entities remix_check cr_items cr_enabled
// ge:skill shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "console/cvar.hpp"
#include "progs/entity_text.hpp"
#include "shooters_edge/client/remix.hpp"
#include "shooters_edge/profiles/remix.hpp"

namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;

namespace {

std::string value(const ge::progs::EntityBlock& b, const char* key) {
  for (const auto& [k, v] : b.pairs)
    if (k == key) return v;
  return "";
}

std::vector<ge::progs::EntityBlock> blocks_of(const std::string& text) {
  std::vector<ge::progs::EntityBlock> b;
  std::string error;
  CHECK(ge::progs::parse_entity_text(text.c_str(), b, error));
  return b;
}

}  // namespace

TEST(every_row_names_known_games_and_items) {
  for (const pf::RemixRow& r : pf::kRemixRows) {
    CHECK(ge::classicenhance::find_profile(r.from) != nullptr);
    CHECK(ge::classicenhance::find_profile(r.to) != nullptr);
    CHECK(r.from != r.to);
    CHECK(pf::is_item_class(r.class_name));
    CHECK(r.becomes.empty() || pf::is_item_class(r.becomes));
    CHECK((r.needs & ~pf::kItemBits) == 0 && (r.bits & ~pf::kItemBits) == 0);
  }
}

TEST(the_classic_games_weapons_take_the_sequels_by_role) {
  const auto to_sequel = [](const char* cls, int flags) {
    const pf::RemixRow* r = pf::remix_row("classic", "sequel", cls, flags);
    CHECK(r != nullptr);
    return r != nullptr ? std::string(r->becomes) : std::string();
  };
  CHECK(to_sequel("weapon_nailgun", 0) == "weapon_machinegun");
  CHECK(to_sequel("weapon_supernailgun", 0) == "weapon_chaingun");
  CHECK(to_sequel("weapon_lightning", 0) == "weapon_railgun");
  CHECK(to_sequel("item_cells", 0) == "ammo_slugs");
  CHECK(to_sequel("item_health", 2) == "item_health_mega");
  CHECK(to_sequel("item_health", 3) == "item_health_mega");  // the mega's bit wins
  CHECK(to_sequel("item_health", 1) == "item_health");
  CHECK(to_sequel("item_health", 0) == "item_health_large");
  CHECK(to_sequel("item_armorInv", 0) == "item_armor_body");
  const auto to_classic = [](const char* cls) {
    const pf::RemixRow* r = pf::remix_row("sequel", "classic", cls, 0);
    return r != nullptr ? std::string(r->becomes) : std::string("none");
  };
  CHECK(to_classic("weapon_railgun") == "weapon_lightning");
  CHECK(to_classic("weapon_hyperblaster") == "weapon_lightning");
  CHECK(to_classic("item_quad") == "item_artifact_super_damage");
  CHECK(to_classic("item_armor_shard").empty());  // a row that drops it
  CHECK(to_classic("item_no_such") == "none");
}

TEST(a_map_rewritten_keeps_what_is_not_an_item_and_the_mode_bits) {
  const std::string text =
      "{\n\"classname\" \"worldspawn\"\n\"message\" \"The Hall\"\n}\n"
      "{\n\"classname\" \"info_player_deathmatch\"\n\"origin\" \"0 0 24\"\n}\n"
      "{\n\"classname\" \"item_health\"\n\"spawnflags\" \"2050\"\n\"origin\" \"8 8 0\"\n}\n"
      "{\n\"classname\" \"item_rockets\"\n\"spawnflags\" \"1\"\n\"origin\" \"64 0 0\"\n}\n"
      "{\n\"classname\" \"weapon_lightning\"\n\"origin\" \"128 0 0\"\n}\n"
      "{\n\"classname\" \"item_key1\"\n\"origin\" \"256 0 0\"\n}\n";
  std::string out, error;
  cl::RemixReport rep;
  CHECK(cl::remix_entities(text, "classic", "sequel", out, &rep, error));
  const auto b = blocks_of(out);
  CHECK(b.size() == 5);  // the key has no row: left out
  CHECK(value(b[0], "message") == "The Hall");
  CHECK(value(b[1], "classname") == "info_player_deathmatch");
  CHECK(value(b[2], "classname") == "item_health_mega");
  CHECK(value(b[2], "spawnflags") == "2048");  // not in deathmatch: kept; the mega bit: gone
  CHECK(value(b[2], "origin") == "8 8 0");
  CHECK(value(b[3], "classname") == "ammo_rockets" && value(b[3], "spawnflags") == "0");
  CHECK(value(b[4], "classname") == "weapon_railgun" && value(b[4], "spawnflags").empty());
  CHECK(rep.items == 4 && rep.swapped == 3 && rep.dropped == 1);
  CHECK(rep.swaps.size() == 4 && rep.swaps[3].from == "item_key1" && rep.swaps[3].to.empty());
  CHECK(cl::remix_line(rep, "sequel").find("3 of the map's 4 items") != std::string::npos);

  std::string same;
  CHECK(cl::remix_entities(text, "classic", "classic", same, &rep, error) && same == text);
  CHECK(rep.items == 0);
}

TEST(the_sequels_variant_bits_become_the_classic_games) {
  const std::string text =
      "{\n\"classname\" \"item_pack\"\n\"spawnflags\" \"1\"\n}\n"
      "{\n\"classname\" \"item_health_small\"\n}\n";
  std::string out, error;
  CHECK(cl::remix_entities(text, "sequel", "classic", out, nullptr, error));
  const auto b = blocks_of(out);
  CHECK(b.size() == 2);
  CHECK(value(b[0], "classname") == "item_rockets" && value(b[0], "spawnflags") == "1");
  CHECK(value(b[1], "classname") == "item_health" && value(b[1], "spawnflags") == "1");
}

TEST(the_remix_wants_two_full_games_and_cr_enabled) {
  cl::RemixSettings s;
  const std::vector<cl::KnownGame> both = {{"classic", "/games/one", cl::Edition::full},
                                           {"sequel", "/games/two", cl::Edition::full}};
  CHECK(!cl::remix_check(s, "classic", both).on);  // off by default
  CHECK(cl::remix_check(s, "classic", both).why.empty());
  s.enabled = true;
  const cl::RemixState on = cl::remix_check(s, "classic", both);
  CHECK(on.on && on.other == "sequel" && on.other_basedir == "/games/two");
  const std::vector<cl::KnownGame> demo = {{"classic", "/games/one", cl::Edition::full},
                                           {"sequel", "/games/two", cl::Edition::shareware}};
  const cl::RemixState no = cl::remix_check(s, "classic", demo);
  CHECK(!no.on && no.why.find("second classic game") != std::string::npos);
  const std::vector<cl::KnownGame> mine_demo = {{"classic", "/games/one", cl::Edition::shareware},
                                                {"sequel", "/games/two", cl::Edition::full}};
  CHECK(!cl::remix_check(s, "classic", mine_demo).on);
  // A player's line: no command syntax in it.
  CHECK(no.why.find("next:") == std::string::npos && no.why.find('`') == std::string::npos);
}

TEST(cr_items_names_whose_items_a_map_gets) {
  ge::console::Cvars cvars;
  cl::RemixSettings s;
  cl::bind_remix(cvars, &s);
  CHECK(!s.enabled && s.items == 0);
  CHECK(std::string(cl::items_game(s, "sequel")) == "sequel");
  CHECK(cvars.set("cr_items", "2", ge::console::Source::local) == ge::console::SetResult::changed);
  CHECK(s.items == 2 && std::string(cl::items_game(s, "classic")) == "sequel");
  cvars.set("cr_items", "1", ge::console::Source::local);
  CHECK(std::string(cl::items_game(s, "sequel")) == "classic");
  cvars.set("cr_enabled", "1", ge::console::Source::local);
  CHECK(s.enabled);
}
