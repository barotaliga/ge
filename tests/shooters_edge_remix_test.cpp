// Classic remix (games/shooters_edge/client/remix.hpp, profiles/remix.hpp): the table swaps each
// game's items by role and variant, keeps what is not an item and the skill and mode bits, drops
// an item with no match; the check wants two full games; cr_items names whose items a map gets.
//
// ge:summary Tests classic remix: the item table, the entity rewrite, the check, cr_items
// ge:keywords test remix classic-remix item-table remix_entities remix_check cr_items cr_enabled
// ge:keywords cr_sounds heard_as heard_game
// ge:skill shooter testing
#include <algorithm>
#include <bit>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_move.hpp"
#include "classicenhance/profile.hpp"
#include "console/cvar.hpp"
#include "progs/entity_text.hpp"
#include "progs/layout.hpp"
#include "progs/vm.hpp"
#include "shooter/settings.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/remix.hpp"
#include "shooters_edge/client/remix_bar.hpp"
#include "shooters_edge/client/remix_pictures.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/profiles/remix.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "shooters_edge/profiles/remix_monsters.hpp"
#include "shooters_edge/profiles/remix_sounds.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "vfs/pak.hpp"

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

TEST(every_games_items_name_a_known_game_and_items) {
  for (const pf::GameItems& g : pf::kGameItems) {
    CHECK(ge::classicenhance::find_profile(g.game) != nullptr);
    for (const pf::ItemReading& r : g.reads) {
      CHECK(pf::is_item_class(r.class_name));
      CHECK((r.needs & ~pf::kItemBits) == 0);
    }
    for (const pf::ItemSpawn& s : g.spawns) {
      CHECK(pf::is_item_class(s.class_name));
      CHECK((s.bits & ~pf::kItemBits) == 0);
    }
  }
}

TEST(the_classic_games_weapons_take_q2_by_role) {
  const auto to_q2 = [](const char* cls, int flags) {
    const pf::ItemSwap r = pf::remix_item("q1", "q2", cls, flags);
    CHECK(r.known);
    return std::string(r.becomes);
  };
  CHECK(to_q2("weapon_nailgun", 0) == "weapon_machinegun");
  CHECK(to_q2("weapon_supernailgun", 0) == "weapon_chaingun");
  CHECK(to_q2("weapon_lightning", 0) == "weapon_railgun");
  CHECK(to_q2("item_cells", 0) == "ammo_slugs");
  CHECK(to_q2("item_health", 2) == "item_health_mega");
  CHECK(to_q2("item_health", 3) == "item_health_mega");  // the mega's bit wins
  CHECK(to_q2("item_health", 1) == "item_health");
  CHECK(to_q2("item_health", 0) == "item_health_large");
  CHECK(to_q2("item_armorInv", 0) == "item_armor_body");
  // Dissolution of Eternity's ammunition for its weapons' second kinds.
  CHECK(to_q2("item_lava_spikes", 0) == "ammo_bullets");
  CHECK(to_q2("item_multi_rockets", 0) == "ammo_rockets");
  CHECK(to_q2("item_plasma", 0) == "ammo_cells");
  CHECK(!pf::remix_item("q1", "q2", "item_powerup_belt", 0).known);  // no part to play: dropped
  // Scourge of Armagon's, read by the same table (its program runs on Quake's).
  CHECK(to_q2("weapon_laser_gun", 0) == "weapon_hyperblaster");
  CHECK(to_q2("weapon_mjolnir", 0) == "weapon_bfg");
  CHECK(to_q2("weapon_proximity_gun", 0) == "weapon_grenadelauncher");
  CHECK(to_q2("item_artifact_wetsuit", 0) == "item_enviro");
  CHECK(!pf::remix_item("q1", "q2", "item_artifact_empathy_shields", 0).known);  // dropped
  const auto to_classic = [](const char* cls) {
    const pf::ItemSwap r = pf::remix_item("q2", "q1", cls, 0);
    return r.known ? std::string(r.becomes) : std::string("none");
  };
  CHECK(to_classic("weapon_railgun") == "weapon_lightning");
  CHECK(to_classic("weapon_hyperblaster") == "weapon_lightning");
  CHECK(to_classic("item_quad") == "item_artifact_super_damage");
  CHECK(to_classic("item_armor_shard").empty());  // a row that drops it
  CHECK(to_classic("item_no_such") == "none");
  // Ground Zero's, read by Quake II's table (its library runs on Quake II's).
  CHECK(to_classic("weapon_etf_rifle") == "weapon_nailgun");
  CHECK(to_classic("weapon_heatbeam") == "weapon_lightning");
  CHECK(to_classic("weapon_plasmabeam") == "weapon_lightning");
  CHECK(to_classic("weapon_proxlauncher") == "weapon_grenadelauncher");
  CHECK(to_classic("weapon_disintegrator") == "weapon_rocketlauncher");
  CHECK(to_classic("ammo_flechettes") == "item_spikes");
  CHECK(to_classic("ammo_prox") == "item_rockets");
  CHECK(to_classic("item_double") == "item_artifact_super_damage");
  CHECK(to_classic("weapon_chainfist") == "none");    // a fist: dropped
  CHECK(to_classic("ammo_tesla") == "none");          // thrown: dropped
  CHECK(to_classic("item_sphere_hunter") == "none");  // no part in another set
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
  CHECK(cl::remix_entities(text, "q1", "q2", out, &rep, error));
  const auto b = blocks_of(out);
  CHECK(b.size() == 5);  // the key has no row: left out
  CHECK(value(b[0], "message") == "The Hall");
  CHECK(value(b[1], "classname") == "info_player_deathmatch");
  CHECK(value(b[2], "classname") == "item_health_mega");
  CHECK(value(b[2], "spawnflags") == "2048");  // not in deathmatch: kept; the mega bit: gone
  // The middle of its floor kept: the classic box's corner moved to Quake II's middle, 15 up
  // to its middle and 1 off the floor.
  CHECK(value(b[2], "origin") == "24 24 16");
  CHECK(value(b[4], "origin") == "128 0 16");  // a classic weapon stands on its middle already
  CHECK(value(b[3], "classname") == "ammo_rockets" && value(b[3], "spawnflags") == "0");
  CHECK(value(b[4], "classname") == "weapon_railgun" && value(b[4], "spawnflags").empty());
  CHECK(rep.items == 4 && rep.swapped == 3 && rep.dropped == 1);
  CHECK(rep.swaps.size() == 4 && rep.swaps[3].from == "item_key1" && rep.swaps[3].to.empty());
  CHECK(cl::remix_line(rep, "q2").find("the map's 4 items") != std::string::npos);

  std::string same;
  CHECK(cl::remix_entities(text, "q1", "q1", same, &rep, error) && same == text);
  CHECK(rep.items == 0);
}

// `cr_monsters`: each game's monsters paired by role; a map's are the chosen game's when the played
// game's rules run them (each game's rules run the other's), else the played game's partners.
// Bosses and monsters with no row are kept.
TEST(cr_monsters_pairs_the_monsters_by_role_and_keeps_what_the_rules_cannot_run) {
  CHECK(pf::monster_as("monster_army", "q2") == "monster_soldier");
  CHECK(pf::monster_as("monster_soldier_ss", "q1") == "monster_enforcer");
  CHECK(pf::monster_as("monster_tank_commander", "q1") == "monster_shambler");
  CHECK(pf::monster_as("monster_army", "q1") == "monster_army");
  CHECK(pf::monster_as("monster_boss", "q2") == "monster_boss");  // a boss: no row
  CHECK(pf::runs_monster("q1", "monster_army"));
  CHECK(pf::runs_monster("q1", "monster_soldier"));  // learnt: campaign_q2_kinds.cpp
  CHECK(pf::runs_monster("q2", "monster_army"));     // learnt: q2_monster_kinds.hpp
  CHECK(!pf::runs_monster("q2", "monster_boss"));    // a boss: never
  for (const pf::MonsterMatch& m : pf::kMonsterMatches) {
    CHECK(m.classic.starts_with("monster_") && m.q2.starts_with("monster_"));
    CHECK(!m.what.empty());
    CHECK(pf::runs_monster("q1", m.q2));  // every Quake II monster of the table
  }
  const std::string classic_map =
      "{\n\"classname\" \"worldspawn\"\n}\n"
      "{\n\"classname\" \"monster_army\"\n\"origin\" \"0 0 24\"\n}\n"
      "{\n\"classname\" \"monster_shambler\"\n\"spawnflags\" \"1\"\n}\n"
      "{\n\"classname\" \"monster_boss\"\n}\n";
  std::string out, error;
  cl::RemixReport rep;
  // Played classic with Quake II's monsters: each becomes Quake II partner.
  CHECK(cl::remix_monsters(classic_map, "q1", "q2", out, &rep, error));
  const auto b = blocks_of(out);
  CHECK(b.size() == 4);
  CHECK(value(b[1], "classname") == "monster_soldier" && value(b[1], "origin") == "0 0 24");
  CHECK(value(b[2], "classname") == "monster_tank" && value(b[2], "spawnflags") == "1");
  CHECK(value(b[3], "classname") == "monster_boss");
  CHECK(rep.swaps.size() == 2);
  // Played by Quake II's rules: the classic monsters kept with `cr_monsters q1`, else partners.
  std::string back;
  CHECK(cl::remix_monsters(classic_map, "q2", "q1", back, nullptr, error));
  CHECK(value(blocks_of(back)[1], "classname") == "monster_army");
  CHECK(cl::remix_monsters(classic_map, "q2", "q2", back, nullptr, error));
  CHECK(value(blocks_of(back)[1], "classname") == "monster_soldier");
}

TEST(q2_small_pickups_are_gathered_into_the_classic_games_sizes) {
  const std::string text =
      "{\n\"classname\" \"item_pack\"\n\"spawnflags\" \"1\"\n\"origin\" \"0 0 0\"\n}\n"
      // Four stimpacks and a box of ten together: 18, one fifteen.
      "{\n\"classname\" \"item_health_small\"\n\"origin\" \"1000 0 0\"\n}\n"
      "{\n\"classname\" \"item_health_small\"\n\"origin\" \"1032 0 0\"\n}\n"
      "{\n\"classname\" \"item_health_small\"\n\"origin\" \"1064 0 0\"\n}\n"
      "{\n\"classname\" \"item_health_small\"\n\"origin\" \"1096 0 0\"\n}\n"
      "{\n\"classname\" \"item_health\"\n\"origin\" \"1128 0 0\"\n}\n"
      // Three large boxes in a row: 75, three twenty-fives.
      "{\n\"classname\" \"item_health_large\"\n\"origin\" \"0 2000 0\"\n}\n"
      "{\n\"classname\" \"item_health_large\"\n\"origin\" \"0 2100 0\"\n}\n"
      "{\n\"classname\" \"item_health_large\"\n\"origin\" \"0 2200 0\"\n}\n"
      // A lone stimpack: too small for the smallest box. Ten shards together: a green armour.
      "{\n\"classname\" \"item_health_small\"\n\"origin\" \"-3000 0 0\"\n}\n";
  std::string shards;
  for (int i = 0; i < 10; i++)
    shards += "{\n\"classname\" \"item_armor_shard\"\n\"origin\" \"" + std::to_string(16 * i) +
              " -3000 0\"\n}\n";
  std::string out, error;
  cl::RemixReport rep;
  CHECK(cl::remix_entities(text + shards, "q2", "q1", out, &rep, error));
  const auto b = blocks_of(out);
  CHECK_MSG(b.size() == 6, std::to_string(b.size()) + " blocks");
  if (b.size() != 6) return;
  CHECK(value(b[0], "classname") == "item_rockets" && value(b[0], "spawnflags") == "1");
  CHECK(value(b[1], "classname") == "item_health" && value(b[1], "spawnflags") == "1");
  // On the pool's first spot, Quake II's middle moved to the classic box's corner, 1 up.
  CHECK(value(b[1], "origin") == "984 -16 -14");
  for (int i = 2; i <= 4; i++)
    CHECK(value(b[i], "classname") == "item_health" && value(b[i], "spawnflags").empty());
  CHECK(value(b[5], "classname") == "item_armor1");
  CHECK(rep.pooled == 19 && rep.laid == 5);
  CHECK(cl::remix_line(rep, "q1").find("19 small pickups gathered into 5") != std::string::npos);
}

TEST(a_map_with_no_strong_weapon_gets_one_and_one_with_too_many_loses_some) {
  const auto weapons = [](std::initializer_list<const char*> names) {
    std::string t;
    int x = 0;
    for (const char* n : names) {
      t += "{\n\"classname\" \"" + std::string(n) + "\"\n\"origin\" \"" + std::to_string(x) +
           " 0 0\"\n}\n";
      x += 1000;
    }
    return t;
  };
  std::string out, error;
  cl::RemixReport rep;
  CHECK(
      cl::remix_entities(weapons({"weapon_supershotgun", "weapon_nailgun", "weapon_supernailgun"}),
                         "q1", "q2", out, &rep, error));
  auto b = blocks_of(out);
  CHECK(rep.weapons == 3 && rep.strong_by_role == 0 && rep.strong == 1);
  // The best weak one (the chaingun the super nailgun became) raised to the rocket launcher.
  CHECK(b.size() == 3 && value(b[2], "classname") == "weapon_rocketlauncher");
  CHECK(rep.notes.size() == 1);
  CHECK(cl::remix_entities(weapons({"weapon_rocketlauncher", "weapon_rocketlauncher",
                                    "weapon_lightning", "weapon_lightning", "weapon_nailgun"}),
                           "q1", "q2", out, &rep, error));
  b = blocks_of(out);
  CHECK(rep.strong_by_role == 4 && rep.strong == 2);  // two of five at most
  CHECK(b.size() == 5 && value(b[0], "classname") == "weapon_rocketlauncher" &&
        value(b[3], "classname") == "weapon_chaingun" &&
        value(b[2], "classname") == "weapon_grenadelauncher");  // each to a weak one it lacks
  shooters_edge::profiles::RemixBalance free;
  free.strong_min = 0;
  free.strong_share = 1;
  CHECK(cl::remix_entities(weapons({"weapon_supershotgun", "weapon_nailgun"}), "q1", "q2", out,
                           &rep, error, free));
  CHECK(rep.strong == 0 && rep.notes.empty());  // the settings turn the balance off
}

TEST(a_third_copy_of_a_strong_weapon_is_lowered_the_one_nearest_a_strong_armour_first) {
  // Quake II's hyperblaster, railgun and BFG all become the lightning gun: three of a kind.
  const std::string map =
      "{\n\"classname\" \"weapon_hyperblaster\"\n\"origin\" \"0 0 0\"\n}\n"
      "{\n\"classname\" \"weapon_railgun\"\n\"origin\" \"1000 0 0\"\n}\n"
      "{\n\"classname\" \"weapon_bfg\"\n\"origin\" \"2000 0 0\"\n}\n"
      "{\n\"classname\" \"item_armor_body\"\n\"origin\" \"1100 0 0\"\n}\n";
  shooters_edge::profiles::RemixBalance keep_share;
  keep_share.strong_share = 1;  // only the copies' limit at work
  std::string out, error;
  cl::RemixReport rep;
  CHECK(cl::remix_entities(map, "q2", "q1", out, &rep, error, keep_share));
  auto b = blocks_of(out);
  CHECK(rep.strong_by_role == 3 && rep.strong == 2);
  // The railgun's spot lies beside the body armour: its lightning gun is the one lowered.
  CHECK(b.size() == 4 && value(b[1], "classname") == "weapon_supernailgun");
  CHECK(value(b[0], "classname") == "weapon_lightning" &&
        value(b[2], "classname") == "weapon_lightning");
  CHECK(rep.notes.size() == 1 && rep.notes[0].find("strong armour") != std::string::npos);
  // With no strong pickup on the map, the last copy is the one lowered.
  const std::string bare = map.substr(0, map.rfind("{\n\"classname\" \"item_armor_body"));
  CHECK(cl::remix_entities(bare, "q2", "q1", out, &rep, error, keep_share));
  b = blocks_of(out);
  CHECK(b.size() == 3 && value(b[2], "classname") == "weapon_supernailgun");
  keep_share.strong_same = 0;  // no limit
  CHECK(cl::remix_entities(bare, "q2", "q1", out, &rep, error, keep_share));
  CHECK(rep.strong == 3);
}

TEST(a_third_copy_of_a_weak_weapon_is_laid_down_as_its_ammunition) {
  // Quake II's shotgun and super shotgun both become the classic double-barrelled one: four
  // copies of a weak weapon, of which two are kept and the last two become shells.
  const std::string map =
      "{\n\"classname\" \"weapon_shotgun\"\n\"origin\" \"0 0 0\"\n}\n"
      "{\n\"classname\" \"weapon_supershotgun\"\n\"origin\" \"500 0 0\"\n}\n"
      "{\n\"classname\" \"weapon_shotgun\"\n\"origin\" \"1000 0 0\"\n}\n"
      "{\n\"classname\" \"weapon_supershotgun\"\n\"origin\" \"1500 0 0\"\n}\n"
      "{\n\"classname\" \"weapon_rocketlauncher\"\n\"origin\" \"2000 0 0\"\n}\n";
  std::string out, error;
  cl::RemixReport rep;
  CHECK(cl::remix_entities(map, "q2", "q1", out, &rep, error));
  auto b = blocks_of(out);
  CHECK(b.size() == 5 && rep.to_ammo == 2);
  CHECK(value(b[0], "classname") == "weapon_supershotgun" &&
        value(b[1], "classname") == "weapon_supershotgun");
  CHECK(value(b[2], "classname") == "item_shells" && value(b[3], "classname") == "item_shells");
  CHECK(value(b[4], "classname") == "weapon_rocketlauncher");
  // `cr_weak_same 0`: no limit.
  shooters_edge::profiles::RemixBalance no_limit;
  no_limit.weak_same = 0;
  CHECK(cl::remix_entities(map, "q2", "q1", out, &rep, error, no_limit));
  CHECK(rep.to_ammo == 0 && value(blocks_of(out)[3], "classname") == "weapon_supershotgun");
}

// cr_expansions: with it on, a map remixed for a played mission pack gets the pack's own weapon
// where its role has none of the base game's (Scourge of Armagon's laser cannon and Mjolnir in
// the lightning gun's two places), after the balance; with it off, a pack's own items are not read.
TEST(cr_expansions_lays_the_played_packs_weapons_and_reads_the_packs_items) {
  namespace pf = shooters_edge::profiles;
  const auto weapons = [](std::initializer_list<const char*> names) {
    std::string t;
    int x = 0;
    for (const char* n : names) {
      t += "{\n\"classname\" \"" + std::string(n) + "\"\n\"origin\" \"" + std::to_string(x) +
           " 0 0\"\n}\n";
      x += 1000;
    }
    return t;
  };
  const std::string map =
      weapons({"weapon_hyperblaster", "weapon_bfg", "weapon_shotgun", "weapon_machinegun"});
  std::string out, error;
  cl::RemixReport rep;
  // The base game: both become the lightning gun (the room-clearer kept strong).
  CHECK(cl::remix_entities(map, "q2", "q1", out, &rep, error));
  auto b = blocks_of(out);
  CHECK(b.size() == 4 && value(b[0], "classname") == "weapon_lightning" &&
        value(b[1], "classname") == "weapon_lightning");
  // Scourge of Armagon played: its own two in their places.
  CHECK(cl::remix_entities(map, "q2", "q1", out, &rep, error, {}, {true, "hipnotic"}));
  b = blocks_of(out);
  CHECK_MSG(b.size() == 4 && value(b[0], "classname") == "weapon_laser_gun" &&
                value(b[1], "classname") == "weapon_mjolnir",
            out);
  // Off: the played pack's spawns are not used.
  CHECK(cl::remix_entities(map, "q2", "q1", out, &rep, error, {}, {false, "hipnotic"}));
  b = blocks_of(out);
  CHECK(value(b[0], "classname") == "weapon_lightning");
  // Off: a pack's own items in a map are not read (dropped); the base game's stand.
  CHECK(pf::remix_item("q1", "q2", "weapon_laser_gun", 0).known);
  CHECK(!pf::remix_item("q1", "q2", "weapon_laser_gun", 0, {false, {}}).known);
  CHECK(!pf::remix_item("q2", "q1", "weapon_etf_rifle", 0, {false, {}}).known);
  CHECK(!pf::remix_item("q1", "q2", "item_plasma", 0, {false, {}}).known);
  CHECK(pf::remix_item("q1", "q2", "weapon_nailgun", 0, {false, {}}).known);
  // The setting: on by default, a player's word.
  ge::console::Cvars cvars;
  cl::RemixSettings s;
  cl::bind_remix(cvars, &s);
  CHECK(s.expansions);
  cvars.set("cr_expansions", "0", ge::console::Source::local);
  CHECK(!s.expansions);
  s.enabled = true;
  const std::vector<cl::KnownGame> both = {{"q1", "/games/one", cl::Edition::full},
                                           {"q2", "/games/two", cl::Edition::full}};
  CHECK(!cl::remix_check(s, "q1", both).expansions);
  // Every pack item and pack spawn names a pack of its game.
  for (const pf::PackItem& p : pf::kPackItems) {
    const ge::classicenhance::Profile* g = ge::classicenhance::find_profile(p.game);
    bool known = false;
    for (const ge::classicenhance::MissionPack& m : g->packs) known = known || m.folder == p.pack;
    CHECK_MSG(known, std::string(p.class_name));
  }
}

TEST(the_remix_wants_two_full_games_and_cr_enabled) {
  cl::RemixSettings s;
  const std::vector<cl::KnownGame> both = {{"q1", "/games/one", cl::Edition::full},
                                           {"q2", "/games/two", cl::Edition::full}};
  CHECK(!cl::remix_check(s, "q1", both).on);  // off by default
  CHECK(cl::remix_check(s, "q1", both).why.empty());
  s.enabled = true;
  const cl::RemixState on = cl::remix_check(s, "q1", both);
  CHECK(on.on && on.other == "q2" && on.other_basedir == "/games/two");
  const std::vector<cl::KnownGame> demo = {{"q1", "/games/one", cl::Edition::full},
                                           {"q2", "/games/two", cl::Edition::shareware}};
  const cl::RemixState no = cl::remix_check(s, "q1", demo);
  CHECK(!no.on && no.why.find("second classic game") != std::string::npos);
  const std::vector<cl::KnownGame> mine_demo = {{"q1", "/games/one", cl::Edition::shareware},
                                                {"q2", "/games/two", cl::Edition::full}};
  CHECK(!cl::remix_check(s, "q1", mine_demo).on);
  // A player's line: no command syntax in it.
  CHECK(no.why.find("next:") == std::string::npos && no.why.find('`') == std::string::npos);
}

TEST(cr_items_names_whose_items_a_map_gets) {
  ge::console::Cvars cvars;
  cl::RemixSettings s;
  cl::bind_remix(cvars, &s);
  CHECK(!s.enabled && s.items.empty());
  CHECK(std::string(cl::items_game(s, "q2")) == "q2");
  CHECK(cvars.set("cr_items", "q2", ge::console::Source::local) == ge::console::SetResult::changed);
  CHECK(s.items == "q2" && std::string(cl::items_game(s, "q1")) == "q2");
  cvars.set("cr_items", "q1", ge::console::Source::local);
  CHECK(std::string(cl::items_game(s, "q2")) == "q1");
  cvars.set("cr_enabled", "1", ge::console::Source::local);
  CHECK(s.enabled);
}

// The players' figure (M18u5): `cr_models` names the game whose figure every player is drawn as,
// read from the remix's files: Quake II's marine on Quake's players,
// Quake's own figure (which stands alone, outside players/) on Quake II's.
TEST(cr_models_names_the_figure_every_player_is_drawn_as) {
  ge::console::Cvars cvars;
  cl::RemixSettings s;
  cl::bind_remix(cvars, &s);
  cl::RemixState state;
  state.on = true;
  CHECK(s.models.empty() && cl::worn_figure(s, state, "q1") == nullptr);
  cvars.set("cr_models", "q2", ge::console::Source::local);
  const shooters_edge::profiles::RemixFigure* marine = cl::worn_figure(s, state, "q1");
  CHECK(marine != nullptr && marine->folder == "male" && marine->file.empty());
  CHECK(cl::worn_figure(s, state, "q2") == nullptr);  // its own figure
  cvars.set("cr_models", "q1", ge::console::Source::local);
  const shooters_edge::profiles::RemixFigure* ranger = cl::worn_figure(s, state, "q2");
  CHECK(ranger != nullptr && ranger->file == "progs/player.mdl");
  state.on = false;
  CHECK(cl::worn_figure(s, state, "q2") == nullptr);  // the remix off: each their own
  state.on = true;
  // Dressing the players alone reads from the remix's files, while the rules wear their own.
  cl::RemixFiles r;
  r.state = state;
  CHECK(cl::remix_dress(r, s, "q2") && r.figure == ranger && r.wearing());
  CHECK(r.worn == nullptr && r.looks == nullptr);
  CHECK(!cl::remix_dress(r, s, "q2"));  // nothing changed
  CHECK(cl::worn_line(r).find("the players are " +
                              std::string(ge::classicenhance::find_profile("q1")->what) +
                              "'s and move as its own.") != std::string::npos);
  r.undress();
  CHECK(!r.wearing() && cl::worn_line(r).ends_with("the players are the game's own and move as "
                                                   "its own."));
}

// `cr_sounds`: each multiplayer sound is heard as the chosen game's by its row, a sound that
// game does not make is silent, and a name no row knows is heard as it is.
TEST(cr_sounds_pairs_each_games_multiplayer_sounds) {
  CHECK(pf::heard_as("weapons/rocklf1a.wav", "q1") == "weapons/sgun1.wav");
  CHECK(pf::heard_as("weapons/ROCKLF1A.WAV", "q1") == "weapons/sgun1.wav");  // any case
  CHECK(pf::heard_as("weapons/sgun1.wav", "q2") == "weapons/rocklf1a.wav");
  CHECK(pf::heard_as("weapons/sgun1.wav", "q1") == "weapons/sgun1.wav");  // its own
  CHECK(pf::heard_as("weapons/machgf4b.wav", "q1") == "weapons/rocket1i.wav");
  CHECK(pf::heard_as("weapons/rocket1i.wav", "q2") == "weapons/machgf1b.wav");  // the first
  CHECK(pf::heard_as("player/step3.wav", "q1").empty());  // no footsteps in Quake
  CHECK(pf::heard_as("player/male/jump1.wav", "q1") == "player/plyrjmp8.wav");
  CHECK(pf::heard_as("misc/talk.wav", "q1") == "misc/talk.wav");              // both games' name
  CHECK(pf::heard_as("weapons/rocklf1a.wav", "") == "weapons/rocklf1a.wav");  // no game
  for (const pf::SoundMatch& m : pf::kSoundMatches) {
    CHECK(!m.q2.empty() && !m.what.empty() && m.classic != m.q2);
    CHECK(m.classic.empty() || pf::heard_as(pf::heard_as(m.classic, "q2"), "q1") != "");
  }
}

// `cr_items` alone never changes a sound: every sound the worn things name is heard as the played
// game's own while `cr_sounds` is empty, and as the worn game's once it names that game.
TEST(cr_items_alone_keeps_the_played_games_sounds) {
  const pf::RemixArsenal* worn = pf::remix_arsenal("q1", "q2");
  CHECK(worn != nullptr);
  std::vector<std::string_view> named;
  for (const pf::WeaponRow& w : worn->arsenal.weapons)
    named.insert(named.end(), {w.fire_sound, w.land_sound, w.blast_sound});
  for (const pf::ArmourRow& a : worn->arsenal.armour) named.push_back(a.sound);
  for (const pf::ItemLook& l : worn->arsenal.looks) named.push_back(l.sound);
  named.push_back(worn->arsenal.pickup_sound);
  for (const std::string_view n : named) {
    if (n.empty() || n == "-") continue;
    const std::string_view heard = pf::heard_as(n, "q1");
    CHECK(!heard.empty() && heard != n);  // Quake's own shot
    CHECK(pf::heard_as(n, "q2") == n);    // cr_sounds q2: Quake II's as worn
  }
  const pf::RemixLooks* looks = pf::remix_looks("q2", "q1");
  CHECK(looks != nullptr);
  for (const pf::Look& l : looks->looks) {
    if (!l.to.ends_with(".wav")) continue;
    CHECK(pf::heard_as(l.to, "q2") != l.to);  // Quake II's own again
  }

  ge::console::Cvars cvars;
  cl::RemixSettings s;
  cl::bind_remix(cvars, &s);
  cl::RemixState state;
  CHECK(cl::heard_game(s, state, "q1").empty());  // off: each as named
  state.on = true;
  cvars.set("cr_items", "q2", ge::console::Source::local);
  CHECK(cl::heard_game(s, state, "q1") == "q1");
  cl::RemixFiles r;
  r.played = "q1";
  r.state = state;
  CHECK(cl::remix_dress(r, s, "q1") && r.worn != nullptr && r.heard == "q1");
  CHECK(cl::worn_line(r).find("The sounds") == std::string::npos);  // the game's own
  cvars.set("cr_items", "", ge::console::Source::local);
  cvars.set("cr_sounds", "q2", ge::console::Source::local);
  CHECK(cl::remix_dress(r, s, "q1") && r.worn == nullptr && r.heard == "q2");
  CHECK(!r.wearing() && r.heard_from_files());  // Quake II's files, for its sounds
  CHECK(cl::worn_line(r).ends_with(
      " The sounds are " + std::string(ge::classicenhance::find_profile("q2")->what) + "'s."));
  CHECK(!cl::remix_dress(r, s, "q1"));
}

// `cr_menu` and `cr_charset`: each names its own game, apart from the other and from the items; a
// glyph sheet is taken into a set at a density it divides, never one it does not.
TEST(cr_menu_and_cr_charset_name_the_menus_and_the_letters_games) {
  ge::console::Cvars cvars;
  cl::RemixSettings s;
  cl::bind_remix(cvars, &s);
  cl::RemixFiles r;
  r.played = "q1";
  r.state.on = true;
  cvars.set("cr_menu", "q2", ge::console::Source::local);
  CHECK(cl::remix_dress(r, s, "q1") && r.menu == "q2" && r.charset == "q1");
  CHECK(!r.wearing() && !r.from_files() && r.heard == "q1");  // nothing else swapped
  cvars.set("cr_charset", "q2", ge::console::Source::local);
  cvars.set("cr_menu", "", ge::console::Source::local);
  CHECK(cl::remix_dress(r, s, "q1") && r.menu == "q1" && r.charset == "q2");
  CHECK(!cl::remix_dress(r, s, "q1"));
  CHECK(cl::remix_parts(s)[6].empty() && cl::remix_parts(s)[7] == "q2");
  r.state.on = false;
  CHECK(cl::remix_dress(r, s, "q1") && r.menu.empty() && r.charset.empty());

  ge::ui::Image sheet;
  sheet.width = sheet.height = 128;
  sheet.rgba.assign(size_t{128} * 128 * 4, 7);
  ge::ui::Image into;
  CHECK(cl::take_glyphs(into, 4, sheet, 1) && into.width == 512 && into.height == 512);
  ge::ui::Image kept = into;
  CHECK(!cl::take_glyphs(kept, 1, into, 4) && kept.width == 512);   // never shrunk
  CHECK(!cl::take_glyphs(kept, 3, sheet, 2) && kept.width == 512);  // no whole step
  CHECK(cl::take_glyphs(kept, 1, sheet, 1) && kept.width == 128);

  // Quake II's status bar writes its words in the swapped sheet, and in its own when none is.
  cl::Q2Bar bar;
  cl::ClassicPictures pics;
  pics.menu.glyphs = into;
  pics.menu.density = 4;
  cl::bar_pictures(bar, pics);
  CHECK(bar.pictures.empty());  // its own, read from the packs when drawn
  pics.letters_swapped = true;
  cl::bar_pictures(bar, pics);
  const auto letters = bar.pictures.find("pics/conchars.pcx");
  CHECK(letters != bar.pictures.end() && letters->second.k == 4 &&
        letters->second.image.width == 512 && letters->second.width() == 128);
}

// `cr_over_mods`: a mod's own rules keep their physics unless it is on; then `cr_physics`'s lines
// are set over the mod's (`wear_lines`' `over_mod`), while the weapons stay the mod's.
TEST(cr_over_mods_sets_cr_physics_over_a_mods_rules) {
  ge::console::Cvars cvars;
  cl::RemixSettings s;
  cl::bind_remix(cvars, &s);
  cvars.set("cr_physics", "q1", ge::console::Source::local);
  cvars.set("cr_items", "q1", ge::console::Source::local);
  cl::RemixFiles r;
  r.state.on = true;
  r.mod_rules = true;
  cl::remix_dress(r, s, "q2");
  CHECK(r.physics == nullptr && r.looks == nullptr);  // the mod's own
  cl::ListenOptions opt;
  cl::wear_lines(opt, r, {});
  CHECK(opt.over_mod.empty());
  cvars.set("cr_over_mods", "1", ge::console::Source::local);
  CHECK(s.over_mods && cl::remix_dress(r, s, "q2"));
  CHECK(r.physics != nullptr && r.looks == nullptr);  // the physics over it; its weapons its own
  cl::wear_lines(opt, r, {});
  CHECK(opt.over_mod.size() == 1 && opt.over_mod[0] == "set sv_physics \"q1\"");
  r.mod_rules = false;  // no mod: the lines go with the window's own, none over a mod
  cl::wear_lines(opt, r, {});
  CHECK(opt.over_mod.empty());
}

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;

// The other game's hall: a floor of brushes walled round, a door-sized block of its own model
// (`*1`) ahead, the start facing it (+y), and Quake II's rail lying on the way.
std::vector<uint8_t> q2_hall() {
  br::BrushWorld world;
  world.models.resize(2);
  const auto box = [&](size_t model, float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[model].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(0, -512, -512, -16, 512, 512, 0);
  box(0, -528, -512, -16, -512, 512, 256);
  box(0, 512, -512, -16, 528, 512, 256);
  box(0, -528, 512, -16, 528, 528, 256);
  box(0, -528, -528, -16, 528, -512, 256);
  box(1, -64, 300, 0, 64, 316, 128);  // the block across the way
  const auto entity = [](std::initializer_list<std::pair<std::string, std::string>> pairs) {
    br::Entity e;
    for (const auto& p : pairs) e.pairs.push_back(p);
    return e;
  };
  world.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}, {"angle", "90"}}));
  world.entities.push_back(entity({{"classname", "weapon_railgun"}, {"origin", "-16 100 16"}}));
  world.entities.push_back(entity({{"classname", "item_armor_shard"}, {"origin", "200 0 16"}}));
  world.entities.push_back(entity({{"classname", "func_wall"}, {"model", "*1"}}));
  std::vector<uint8_t> bsp;
  std::string error;
  CHECK_MSG(br::write_brush_world(world, &bsp, &error), error);
  return bsp;
}

std::vector<uint8_t> pack(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files) {
  std::vector<uint8_t> out;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(files, &out, &error), error);
  return out;
}

}  // namespace

// Quake II's map in Quake (M18u2): found in the other game's packs only with the
// remix on, its items made Quake's, and played on Quake's program: the
// player stands on its brushes, walks over the rail that is now the beam weapon and takes it, and
// is stopped by the block of the map's own model, traced through its brushes.
TEST(q2_map_plays_on_the_classic_rules_with_the_classic_items) {
  cl::SearchPath mine;  // Quake's own files: no such map
  cl::RemixFiles remix;
  remix.played = "q1";
  std::string error;
  CHECK(cl::add_archive(remix.files, "pak0.pak", pack({{"maps/hall.bsp", q2_hall()}}), error));
  CHECK(cl::load_remixed_map(mine, "hall", &remix, error) == nullptr);  // the remix off
  remix.state.on = true;
  remix.state.other = "q2";
  std::string said;
  std::unique_ptr<cl::LoadedMap> map = cl::load_remixed_map(mine, "hall", &remix, error, &said);
  CHECK_MSG(map != nullptr, error);
  if (!map) return;
  CHECK(map->from_game == "q2" && map->program_rules && map->spec.brushes != nullptr);
  CHECK(map->spec.entities.find("weapon_lightning") != std::string::npos);
  CHECK(map->spec.entities.find("weapon_railgun") == std::string::npos);
  CHECK(map->spec.entities.find("item_armor_shard") == std::string::npos);
  CHECK_MSG(said.find("the map's 2 items") != std::string::npos, said);

  static const ge::progs::Progs progs = [] {
    ge::progs::Progs out;
    std::string why;
    CHECK_MSG(cl::baseline_progs(out, why), why);
    return out;
  }();
  cl::Listen l;
  CHECK_MSG(cl::listen_start(l, progs, std::move(map), shooters_edge::net::listen_options(), error),
            error);
  if (l.rules == nullptr) return;
  shooters_edge::server::QcServer* qc = shooters_edge::server::qc_of(*l.rules);
  CHECK(qc != nullptr && qc->tracer != nullptr);
  if (qc == nullptr) return;
  const auto me = [&] { return l.server->sv.edicts[1]; };
  for (int i = 0; i < 120; i++) cl::listen_frame(l, {});  // spawned, fallen onto the floor
  const sh::Vec3f stood = me().origin;
  CHECK_MSG(std::fabs(stood.z - 24) < 1.0f, "stood at z " + std::to_string(stood.z));
  CHECK((me().flags & sh::kFlagOnGround) != 0);
  sh::MoveCmd walk;
  walk.angles = {0, 90, 0};
  walk.forward = 320;
  for (int i = 0; i < 300; i++) cl::listen_frame(l, walk);
  const sh::Vec3f at = me().origin;
  CHECK_MSG(at.y > 200 && at.y < 300 - 16 + 1, "walked to y " + std::to_string(at.y));
  CHECK(std::fabs(at.z - stood.z) < 1.0f);
  const float items =
      std::bit_cast<float>(*ge::progs::entity_field(qc->q.vm, 1, ge::progs::kFItems));
  CHECK_MSG((static_cast<int>(items) & 64) != 0, "items " + std::to_string(items));
}

// Quake's rules wearing Quake II's things (M18u4, `cr_items q2`): only with the remix
// on and another game named; the rules keep every number and the map its class names, while the
// server's weapon and armour rows name Quake II's models and sounds, so the beam weapon lies on
// the floor as Quake II's railgun and the hand holds Quake II's view models.
TEST(the_classic_rules_wear_q2_weapons_and_armour_with_cr_items) {
  ge::console::Cvars cvars;
  cl::RemixSettings s;
  cl::bind_remix(cvars, &s);
  cl::RemixState state;
  cvars.set("cr_items", "q2", ge::console::Source::local);
  CHECK(cl::worn_arsenal(s, state, "q1") == nullptr);  // the remix off
  state.on = true;
  state.other = "q2";
  const pf::RemixArsenal* worn = cl::worn_arsenal(s, state, "q1");
  CHECK(worn != nullptr && worn->items == "q2");
  if (worn == nullptr) return;
  cvars.set("cr_items", "q1", ge::console::Source::local);
  CHECK(cl::worn_arsenal(s, state, "q1") == nullptr);  // the game's own
  // The numbers are Quake's; every name is Quake II's.
  CHECK(worn->arsenal.weapons.size() == std::size(pf::kQ1Weapons));
  CHECK(worn->held.size() == worn->arsenal.weapons.size());
  for (size_t i = 0; i < worn->arsenal.weapons.size(); i++) {
    const pf::WeaponRow& w = worn->arsenal.weapons[i];
    const pf::WeaponRow& c = pf::kQ1Weapons[i];
    CHECK(w.slot == c.slot && w.fire == c.fire && w.ammo == c.ammo && w.cost == c.cost);
    CHECK(w.refire == c.refire && w.damage == c.damage && w.reach == c.reach);
    CHECK(w.splash == c.splash && w.pickup_ammo == c.pickup_ammo);
    CHECK(worn->held[i] == w.view);
    for (const std::string_view n : {w.view, w.fire_sound, w.missile, w.pickup})
      CHECK_MSG(n == "-" || !n.starts_with("progs/"), std::string(n));
    const pf::HeldFrames* f = pf::held_frames(w.view);
    CHECK_MSG(f != nullptr && f->fire_first <= f->fire_last && f->rest > f->fire_last,
              std::string(w.view));
  }
  for (size_t i = 0; i < worn->arsenal.armour.size(); i++)
    CHECK(worn->arsenal.armour[i].absorb == pf::kQ1Armour[i].absorb &&
          worn->arsenal.armour[i].model.starts_with("models/items/armor/"));
  CHECK(pf::held_frames("progs/v_shot.mdl") == nullptr);
  // The ammunition and health in Quake II's boxes, the weapons picked up with its sound.
  const std::vector<std::string> lines = pf::arsenal_lines(worn->arsenal);
  const auto said = [&](std::string_view line) {
    for (const std::string& l : lines)
      if (l == line) return true;
    return false;
  };
  CHECK(said("set arsenal_ammo1 \"models/items/ammo/shells/medium/tris.md2 misc/am_pkup.wav 15\""));
  CHECK(said("set arsenal_health3 \"models/items/mega_h/tris.md2 items/m_health.wav 15\""));
  CHECK(said("set arsenal_pickup \"misc/w_pkup.wav\""));
  for (const std::string& l : pf::arsenal_lines(pf::kQ1Arsenal))
    CHECK_MSG(l.find("arsenal_ammo") == std::string::npos &&
                  l.find("arsenal_pickup") == std::string::npos,
              l);

  // On the server: the rows given as its settings, the rail lying where the beam weapon was.
  cl::SearchPath mine;
  cl::RemixFiles remix;
  remix.played = "q1";
  remix.state = state;
  std::string error;
  CHECK(cl::add_archive(remix.files, "pak0.pak", pack({{"maps/hall.bsp", q2_hall()}}), error));
  std::unique_ptr<cl::LoadedMap> map = cl::load_remixed_map(mine, "hall", &remix, error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return;
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs out;
    std::string why;
    CHECK_MSG(cl::baseline_progs(out, why), why);
    return out;
  }();
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.arsenal = pf::arsenal_lines(worn->arsenal);
  cl::Listen l;
  CHECK_MSG(cl::listen_start(l, progs, std::move(map), opt, error), error);
  if (l.server == nullptr) return;
  const auto listed = [](const std::vector<std::string>& names, std::string_view name) {
    for (const std::string& n : names)
      if (n == name) return true;
    return false;
  };
  for (const std::string_view m :
       {"models/weapons/g_rail/tris.md2", "models/weapons/v_rail/tris.md2",
        "models/weapons/v_blast/tris.md2", "models/objects/rocket/tris.md2"})
    CHECK_MSG(listed(l.server->models, m), std::string(m));
  CHECK(!listed(l.server->models, "progs/g_light.mdl"));
  CHECK(listed(l.server->sounds, "weapons/railgf1a.wav"));
  CHECK(listed(l.server->sounds, "weapons/rocklx1a.wav"));
}

// Quake II's bar over the classic status (M18u4b): the classic numbers in Quake II's stats,
// each picture by Quake II's own name, and the program run into pictures where Quake II's
// own bar puts them.
TEST(q2_bar_shows_the_classic_status_while_its_weapons_are_worn) {
  namespace hud = shooters_edge::hud;
  hud::Status st;
  st.t = 10;
  st.health = 73;
  st.armour = 150;
  st.armour_class = hud::ArmourClass::medium;
  st.weapon = hud::Weapon::nailgun;
  st.ammo[size_t(hud::Ammo::nails)] = 88;
  st.picked_at[size_t(hud::Weapon::beam_gun) - 1] = 9;  // the beam weapon a second ago
  st.power_s[size_t(hud::Power::damage)] = 12.5;
  st.frags = 7;
  const auto s = cl::worn_stats(st, true);
  CHECK(cl::worn_image(s[0]) == "i_health" && s[1] == 73);
  CHECK(cl::worn_image(s[2]) == "a_bullets" && s[3] == 88);  // the nailgun wears the machinegun
  CHECK(cl::worn_image(s[4]) == "i_combatarmor" && s[5] == 150);
  CHECK(cl::worn_image(s[7]) == "w_railgun" && s[8] == 1056 + 6);
  CHECK(cl::worn_image(s[9]) == "p_quad" && s[10] == 12);
  CHECK(s[14] == 7);
  st.weapon = hud::Weapon::melee;
  st.t = 20;  // the pickup long gone
  const auto m = cl::worn_stats(st, false);
  CHECK(m[2] == 0 && m[3] == 0 && m[7] == 0 && m[14] == 0);

  cl::Q2Bar bar;
  cl::run_worn_bar(bar, st, true, 0, 640, 480);
  const auto drawn = [&](std::string_view name) {
    for (const hud::LayoutOp& op : bar.ops)
      if (op.kind == hud::LayoutOp::Kind::picture && op.name == name) return true;
    return false;
  };
  CHECK(drawn("i_health") && drawn("i_combatarmor") && drawn("p_quad") && drawn("num_7"));
  CHECK(!drawn("a_bullets"));  // nothing to fire with the melee weapon
}

// Quake's map in Quake II (M18u3): its point tree's cells made brushes so the
// Quake II's rules trace it, its items Quake II's: the player stands on its floor, walks through a
// door and stops at the far wall.
TEST(the_classic_games_map_plays_on_q2_rules_with_q2_items) {
  br::Fixture f = br::sample_fixture();
  br::Entity health;
  health.pairs = {{"classname", "item_health"}, {"origin", "300 100 24"}};
  br::Entity lg;
  lg.pairs = {{"classname", "weapon_lightning"}, {"origin", "300 300 24"}};
  f.entities = {health, lg};
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(f, br::Family::v29, &bytes, &error), error);
  cl::SearchPath mine;  // Quake II's own files: no such map
  cl::RemixFiles remix;
  remix.played = "q2";
  remix.state.on = true;
  remix.state.other = "q1";
  CHECK(cl::add_archive(remix.files, "pak0.pak", pack({{"maps/halls.bsp", bytes}}), error));
  std::unique_ptr<cl::LoadedMap> map = cl::load_remixed_map(mine, "halls", &remix, error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return;
  CHECK(map->from_game == "q1" && !map->program_rules && map->spec.brushes != nullptr);
  CHECK(!map->spec.brushes->brushes.empty());
  CHECK(map->spec.entities.find("weapon_railgun") != std::string::npos);
  CHECK(map->spec.entities.find("item_health_large") != std::string::npos);
  bool rail = false;
  for (const br::Entity& e : map->spec.brushes->entities)
    rail = rail || e.get("classname") == "weapon_railgun";
  CHECK(rail);  // the rules read the rewritten blocks

  static const ge::progs::Progs progs = [] {
    ge::progs::Progs out;
    std::string why;
    CHECK_MSG(cl::baseline_progs(out, why), why);
    return out;
  }();
  cl::Listen l;
  CHECK_MSG(cl::listen_start(l, progs, std::move(map), shooters_edge::net::listen_options(), error),
            error);
  if (l.rules == nullptr) return;
  shooters_edge::server::Q2Server* sq = shooters_edge::server::q2_of(*l.rules);
  CHECK(sq != nullptr);
  if (sq == nullptr) return;
  int shown = 0;
  for (const auto& e : sq->world.entities)
    if (!e.free && e.modelindex > 0 && size_t(e.modelindex) < sq->world.models.size()) {
      std::printf("  shown: %s at %.0f %.0f %.0f\n", sq->world.models[size_t(e.modelindex)].c_str(),
                  double(e.origin.x), double(e.origin.y), double(e.origin.z));
      ++shown;
    }
  std::printf("  %d shown\n", shown);
  const auto me = [&] { return l.server->sv.edicts[1]; };
  for (int i = 0; i < 120; i++) cl::listen_frame(l, {});
  const sh::Vec3f stood = me().origin;
  CHECK_MSG(stood.z > 23 && stood.z < 25.5f, "stood at z " + std::to_string(stood.z));
  sh::MoveCmd walk;
  walk.angles = {0, 0, 0};  // east, along +x
  walk.forward = 400;
  for (int i = 0; i < 400; i++) cl::listen_frame(l, walk);
  const sh::Vec3f at = me().origin;
  // From the start (256 192) east through the hall's door into the side room, stopped by its
  // east wall at x 864 less the box's half width.
  CHECK_MSG(at.x > 800 && at.x < 864 - 16 + 1,
            "walked to x " + std::to_string(at.x) + " y " + std::to_string(at.y));
  CHECK(std::fabs(at.z - stood.z) < 1.0f);
}

namespace {

// Quake II's hall with its things lying about: the super shotgun, combat armour, shells and the
// large health, and the start.
std::shared_ptr<const ge::brush::Map> things_hall() {
  br::BrushWorld world;
  world.models.resize(1);
  const float lo[3] = {-512, -512, -16}, hi[3] = {512, 512, 0};
  world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  const auto entity = [](std::initializer_list<std::pair<std::string, std::string>> pairs) {
    br::Entity e;
    for (const auto& p : pairs) e.pairs.push_back(p);
    return e;
  };
  world.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}, {"angle", "90"}}));
  world.entities.push_back(entity({{"classname", "weapon_supershotgun"}, {"origin", "100 0 16"}}));
  world.entities.push_back(entity({{"classname", "item_armor_combat"}, {"origin", "200 0 16"}}));
  world.entities.push_back(entity({{"classname", "ammo_shells"}, {"origin", "300 0 16"}}));
  world.entities.push_back(entity({{"classname", "item_health_large"}, {"origin", "400 0 16"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(world, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

const shooters_edge::server::WorldEntity* row_at(const shooters_edge::server::RulesWorld& w,
                                                 float x) {
  for (const auto& e : w.entities)
    if (!e.free && e.modelindex > 0 && std::fabs(e.origin.x - x) < 0.5f) return &e;
  return nullptr;
}

bool listed(const std::vector<std::string>& names, std::string_view name) {
  return std::find(names.begin(), names.end(), name) != names.end();
}

}  // namespace

// The players' bodies (M18u6): `cr_physics` gives the played game's move the other game's physics
// by server lines from the next map: the classic rules' settings Quake II's run, friction, air,
// landing, the stair jump and water (each line one the settings take) and the arena's harm from a
// fall, Quake II's `sv_physics q1`.
TEST(cr_physics_sets_the_other_games_physics_by_server_lines) {
  namespace sv = shooters_edge::server;
  ge::console::Cvars cvars;
  cl::RemixSettings s;
  cl::bind_remix(cvars, &s);
  cl::RemixState state;
  state.on = true;
  CHECK(cl::worn_physics(s, state, "q1") == nullptr);
  cvars.set("cr_physics", "q2", ge::console::Source::local);
  cl::RemixFiles r;
  r.state = state;
  CHECK(cl::remix_dress(r, s, "q1") && r.physics != nullptr && !r.wearing());
  const std::vector<std::string> lines = cl::worn_lines(r, {"deathmatch 1"});
  CHECK(lines.front() == "deathmatch 1" && lines.size() == 1 + r.physics->lines.size());
  sh::ServerSettings set;
  std::string error;
  for (size_t i = 1; i < lines.size(); i++) {
    if (lines[i].starts_with("set ")) continue;  // the rules' own: `set sv_falldamage 1`
    const size_t sp = lines[i].find(' ');
    CHECK_MSG(sh::set_setting(set, lines[i].substr(0, sp), lines[i].substr(sp + 1), error), error);
  }
  CHECK(set.move.maxspeed == 300 && set.move.friction == 6);
  CHECK(set.move.air_accel == 1 && set.move.air_wish_cap == 0 && set.move.landpause);
  CHECK(set.move.swimscale == 0.5 && set.move.waterfriction == 1 && set.move.waterjump == 350);
  CHECK(set.move.airclimb);  // the stair's double jump (the se-remix-physics lab's stair table)
  CHECK(std::find(lines.begin(), lines.end(), "set sv_falldamage 1") != lines.end());
  CHECK(cl::worn_line(r).ends_with(
      "move as " + std::string(ge::classicenhance::find_profile("q2")->what) + "'s."));
  // Quake II's rules take Quake's from the next map.
  cvars.set("cr_physics", "q1", ge::console::Source::local);
  cl::RemixFiles q;
  q.state = state;
  CHECK(cl::remix_dress(q, s, "q2") && q.physics != nullptr);
  sv::RulesRunner run = sv::q2_runner(sv::make_q2_server({}));
  for (const std::string& line : cl::worn_lines(q, {})) run.console(line);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = things_hall();
  CHECK_MSG(run.start(map, error), error);
  CHECK(sv::q2_of(run)->q1_physics);
  CHECK(!shooters_edge::profiles::kQ1Feel.air_step);  // no stair's double jump
}

// Quake II's rules in Quake's things (M18u4c): `cr_items q1` in Quake II sets the
// server's `sv_wear`, which renames each model and sound as the players are told it; an item lies
// where Quake stands it (on the floor, a box from its corner), the armour by its skin,
// a box never turning; the hand's frames fall on the classic model's rest and firing frames.
TEST(q2_rules_wear_the_classic_games_things_with_cr_items) {
  namespace sv = shooters_edge::server;
  ge::console::Cvars cvars;
  cl::RemixSettings s;
  cl::bind_remix(cvars, &s);
  cl::RemixState state;
  state.on = true;
  state.other = "q1";
  cvars.set("cr_items", "q1", ge::console::Source::local);
  cl::RemixFiles r;
  r.looks = cl::worn_looks(s, state, "q2");
  CHECK(r.looks != nullptr && r.wearing());
  CHECK(cl::worn_arsenal(s, state, "q2") == nullptr);  // no arsenal: the rules name in code
  const std::vector<std::string> lines = cl::worn_lines(r, {"set deathmatch 1"});
  CHECK(lines.size() == 2 && lines[1] == "set sv_wear \"q1\"");
  CHECK(cl::worn_line(r).find("From the next map") == 0);

  const auto start = [&](bool wear) {
    sv::RulesRunner run = sv::q2_runner(sv::make_q2_server({}));
    if (wear)
      for (const std::string& line : lines) run.console(line);
    sv::MapSpec map;
    map.name = "hall";
    map.brushes = things_hall();
    std::string error;
    CHECK_MSG(run.start(map, error), error);
    return run;
  };
  const sv::RulesRunner own = start(false);
  const sv::RulesWorld& o = sv::q2_of(own)->world;
  CHECK(listed(o.models, "models/weapons/g_shotg2/tris.md2"));
  CHECK(listed(o.sounds, "weapons/sshotf1b.wav"));
  const sv::RulesRunner worn = start(true);
  const sv::RulesWorld& w = sv::q2_of(worn)->world;
  for (const char* name : {"progs/g_shot.mdl", "progs/armor.mdl", "maps/b_shell0.bsp",
                           "maps/b_bh25.bsp", "progs/v_shot2.mdl", "progs/missile.mdl"})
    CHECK_MSG(listed(w.models, name), name);
  CHECK(!listed(w.models, "models/weapons/g_shotg2/tris.md2"));
  CHECK(listed(w.sounds, "weapons/shotgn2.wav") && !listed(w.sounds, "weapons/sshotf1b.wav"));
  CHECK(listed(w.models, "players/male/tris.md2"));  // the player stays Quake II's

  // Each item's row: the same place, less Quake II's lift; the boxes from their corner.
  const sv::WorldEntity* gun = row_at(w, 100);
  const sv::WorldEntity* armour = row_at(w, 200);
  const sv::WorldEntity* shells = row_at(w, 300 - 16);
  const sv::WorldEntity* health = row_at(w, 400 - 16);
  const sv::WorldEntity* own_gun = row_at(o, 100);
  CHECK(gun != nullptr && armour != nullptr && shells != nullptr && health != nullptr &&
        own_gun != nullptr);
  if (gun == nullptr || armour == nullptr || shells == nullptr || health == nullptr ||
      own_gun == nullptr)
    return;
  CHECK(w.models[size_t(gun->modelindex)] == "progs/g_shot.mdl");
  CHECK(gun->origin.z == own_gun->origin.z - float(pf::kQ2Lift));
  CHECK(gun->turns);
  CHECK(w.models[size_t(armour->modelindex)] == "progs/armor.mdl" && armour->skin == 1);
  CHECK(w.models[size_t(shells->modelindex)] == "maps/b_shell0.bsp" && !shells->turns);
  CHECK(shells->origin.y == -16);
  CHECK(w.models[size_t(health->modelindex)] == "maps/b_bh25.bsp");
}

// The classic status bar over Quake II's player while it wears Quake's things
// (M18u4d): each weapon as the classic gun it wears, the counters by the gun in hand, the armour
// by class, a power-up's seconds and the frags.
TEST(the_classic_bar_shows_q2_player_in_the_classic_things) {
  namespace sv = shooters_edge::server;
  namespace hud = shooters_edge::hud;
  sv::RulesRunner run = sv::q2_runner(sv::make_q2_server({}));
  run.console("set sv_wear q1");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = things_hall();
  std::string error;
  CHECK_MSG(run.start(map, error), error);
  run.connect(0, "Ana", false, false);
  sv::Q2Server& sq = *sv::q2_of(run);
  sv::Q2Player& p = sq.players[0];
  p.body.ammo[int(sv::SqAmmo::shells)] = 20;
  p.body.ammo[int(sv::SqAmmo::bullets)] = 50;
  p.body.ammo[int(sv::SqAmmo::slugs)] = 7;
  p.body.ammo[int(sv::SqAmmo::cells)] = 90;
  p.body.armour = sv::SqArmour::combat;
  p.body.armour_count = 50;
  p.body.counts[sv::sq_item("weapon_railgun")] = 1;
  p.hand.weapon = 8;  // the railgun: the beam, its slugs in the cells' counter
  p.quad_until = sq.level_time + 12.5;
  const hud::Status s = cl::worn_status(sq, 1);
  CHECK(s.weapon == hud::Weapon::beam_gun);
  CHECK(s.has(hud::Weapon::beam_gun) && s.has(hud::Weapon::shotgun));  // the blaster's
  CHECK(s.ammo_for(hud::Weapon::beam_gun) == 7);
  CHECK(s.ammo[0] == 20 && s.ammo[1] == 50);
  CHECK(s.armour == 50 && s.armour_class == hud::ArmourClass::medium);
  CHECK(s.powered(hud::Power::damage) && s.power_s[size_t(hud::Power::damage)] > 12);
}

TEST(a_joined_q2_server_s_status_carries_its_own_weapon_keys) {
  namespace hud = shooters_edge::hud;
  cl::DemoState st;
  st.models = {"", "maps/q2dm1.bsp", "models/weapons/v_rail/tris.md2"};
  st.stats[cl::kStatWeapon] = 2;  // the railgun's view model in hand
  st.stats[cl::kStatHealth] = 80;
  // No inventory yet: every weapon taken as carried and ready (the server refuses the rest).
  hud::Status s = cl::q2_joined_status(st, 1);
  CHECK(s.keyed.set && s.keyed.in_hand == 9);  // the railgun is weapon 8, its key 9
  CHECK(s.keyed.carried == 0xFFEu && s.keyed.ready == 0xFFEu);
  CHECK(s.health == 80);
  // The inventory the server sent: the blaster, the railgun and its 5 slugs, a shotgun with none.
  st.item_names = {"", "Blaster", "Shotgun", "Railgun", "Shells", "Slugs"};
  st.inventory = {0, 1, 1, 1, 0, 5};
  s = cl::q2_joined_status(st, 1);
  CHECK(s.keyed.carried == ((1u << 1) | (1u << 2) | (1u << 9)));
  CHECK(s.keyed.ready == ((1u << 1) | (1u << 9)));  // the shotgun has no shells
  CHECK(s.keyed.ammo[9] == 5 && s.keyed.ammo[2] == 0 && s.keyed.ammo[1] == -1);
}

// cr_items changed mid-map: the HUD and the held weapons wait for the next map, so they switch
// with the map's items; at the title, or with a map asked for, the change is taken at once.
TEST(a_remix_change_mid_map_waits_for_the_next_map) {
  CHECK(shooters_edge::client::remix_waits(true, false));
  CHECK(!shooters_edge::client::remix_waits(true, true));
  CHECK(!shooters_edge::client::remix_waits(false, false));
  CHECK(!shooters_edge::client::remix_waits(false, true));
}
