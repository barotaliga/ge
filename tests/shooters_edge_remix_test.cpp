// Classic remix (games/shooters_edge/client/remix.hpp, profiles/remix.hpp): the table swaps each
// game's items by role and variant, keeps what is not an item and the skill and mode bits, drops
// an item with no match; the check wants two full games; cr_items names whose items a map gets.
//
// ge:summary Tests classic remix: the item table, the entity rewrite, the check, cr_items
// ge:keywords test remix classic-remix item-table remix_entities remix_check cr_items cr_enabled
// ge:skill shooter testing
#include <bit>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "console/cvar.hpp"
#include "progs/entity_text.hpp"
#include "progs/layout.hpp"
#include "progs/vm.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/remix.hpp"
#include "shooters_edge/client/remix_bar.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/remix.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/sequel_server.hpp"
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
  // The middle of its floor kept: the classic box's corner moved to the sequel's middle, 15 up
  // to its middle and 1 off the floor.
  CHECK(value(b[2], "origin") == "24 24 16");
  CHECK(value(b[4], "origin") == "128 0 16");  // a classic weapon stands on its middle already
  CHECK(value(b[3], "classname") == "ammo_rockets" && value(b[3], "spawnflags") == "0");
  CHECK(value(b[4], "classname") == "weapon_railgun" && value(b[4], "spawnflags").empty());
  CHECK(rep.items == 4 && rep.swapped == 3 && rep.dropped == 1);
  CHECK(rep.swaps.size() == 4 && rep.swaps[3].from == "item_key1" && rep.swaps[3].to.empty());
  CHECK(cl::remix_line(rep, "sequel").find("the map's 4 items") != std::string::npos);

  std::string same;
  CHECK(cl::remix_entities(text, "classic", "classic", same, &rep, error) && same == text);
  CHECK(rep.items == 0);
}

TEST(the_sequels_small_pickups_are_gathered_into_the_classic_games_sizes) {
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
  CHECK(cl::remix_entities(text + shards, "sequel", "classic", out, &rep, error));
  const auto b = blocks_of(out);
  CHECK_MSG(b.size() == 6, std::to_string(b.size()) + " blocks");
  if (b.size() != 6) return;
  CHECK(value(b[0], "classname") == "item_rockets" && value(b[0], "spawnflags") == "1");
  CHECK(value(b[1], "classname") == "item_health" && value(b[1], "spawnflags") == "1");
  // On the pool's first spot, the sequel's middle moved to the classic box's corner, 1 up.
  CHECK(value(b[1], "origin") == "984 -16 -14");
  for (int i = 2; i <= 4; i++)
    CHECK(value(b[i], "classname") == "item_health" && value(b[i], "spawnflags").empty());
  CHECK(value(b[5], "classname") == "item_armor1");
  CHECK(rep.pooled == 19 && rep.laid == 5);
  CHECK(cl::remix_line(rep, "classic").find("19 small pickups gathered into 5") !=
        std::string::npos);
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
                         "classic", "sequel", out, &rep, error));
  auto b = blocks_of(out);
  CHECK(rep.weapons == 3 && rep.strong_by_role == 0 && rep.strong == 1);
  // The best weak one (the chaingun the super nailgun became) raised to the rocket launcher.
  CHECK(b.size() == 3 && value(b[2], "classname") == "weapon_rocketlauncher");
  CHECK(rep.notes.size() == 1);
  CHECK(cl::remix_entities(weapons({"weapon_rocketlauncher", "weapon_rocketlauncher",
                                    "weapon_lightning", "weapon_lightning", "weapon_nailgun"}),
                           "classic", "sequel", out, &rep, error));
  b = blocks_of(out);
  CHECK(rep.strong_by_role == 4 && rep.strong == 2);  // two of five at most
  CHECK(b.size() == 5 && value(b[0], "classname") == "weapon_rocketlauncher" &&
        value(b[3], "classname") == "weapon_chaingun" &&
        value(b[2], "classname") == "weapon_grenadelauncher");  // each to a weak one it lacks
  shooters_edge::profiles::RemixBalance free;
  free.strong_min = 0;
  free.strong_share = 1;
  CHECK(cl::remix_entities(weapons({"weapon_supershotgun", "weapon_nailgun"}), "classic", "sequel",
                           out, &rep, error, free));
  CHECK(rep.strong == 0 && rep.notes.empty());  // the settings turn the balance off
}

TEST(a_third_copy_of_a_strong_weapon_is_lowered_the_one_nearest_a_strong_armour_first) {
  // The sequel's hyperblaster, railgun and BFG all become the lightning gun: three of a kind.
  const std::string map =
      "{\n\"classname\" \"weapon_hyperblaster\"\n\"origin\" \"0 0 0\"\n}\n"
      "{\n\"classname\" \"weapon_railgun\"\n\"origin\" \"1000 0 0\"\n}\n"
      "{\n\"classname\" \"weapon_bfg\"\n\"origin\" \"2000 0 0\"\n}\n"
      "{\n\"classname\" \"item_armor_body\"\n\"origin\" \"1100 0 0\"\n}\n";
  shooters_edge::profiles::RemixBalance keep_share;
  keep_share.strong_share = 1;  // only the copies' limit at work
  std::string out, error;
  cl::RemixReport rep;
  CHECK(cl::remix_entities(map, "sequel", "classic", out, &rep, error, keep_share));
  auto b = blocks_of(out);
  CHECK(rep.strong_by_role == 3 && rep.strong == 2);
  // The railgun's spot lies beside the body armour: its lightning gun is the one lowered.
  CHECK(b.size() == 4 && value(b[1], "classname") == "weapon_supernailgun");
  CHECK(value(b[0], "classname") == "weapon_lightning" &&
        value(b[2], "classname") == "weapon_lightning");
  CHECK(rep.notes.size() == 1 && rep.notes[0].find("strong armour") != std::string::npos);
  // With no strong pickup on the map, the last copy is the one lowered.
  const std::string bare = map.substr(0, map.rfind("{\n\"classname\" \"item_armor_body"));
  CHECK(cl::remix_entities(bare, "sequel", "classic", out, &rep, error, keep_share));
  b = blocks_of(out);
  CHECK(b.size() == 3 && value(b[2], "classname") == "weapon_supernailgun");
  keep_share.strong_same = 0;  // no limit
  CHECK(cl::remix_entities(bare, "sequel", "classic", out, &rep, error, keep_share));
  CHECK(rep.strong == 3);
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

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;

// The other game's hall: a floor of brushes walled round, a door-sized block of its own model
// (`*1`) ahead, the start facing it (+y), and the sequel's rail lying on the way.
std::vector<uint8_t> sequel_hall() {
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

// The sequel's map in the classic game (M18u2): found in the other game's packs only with the
// remix on, its items made the classic game's, and played on the classic game's program: the
// player stands on its brushes, walks over the rail that is now the beam weapon and takes it, and
// is stopped by the block of the map's own model, traced through its brushes.
TEST(the_sequels_map_plays_on_the_classic_rules_with_the_classic_items) {
  cl::SearchPath mine;  // the classic game's own files: no such map
  cl::RemixFiles remix;
  remix.played = "classic";
  std::string error;
  CHECK(cl::add_archive(remix.files, "pak0.pak", pack({{"maps/hall.bsp", sequel_hall()}}), error));
  CHECK(cl::load_remixed_map(mine, "hall", &remix, error) == nullptr);  // the remix off
  remix.state.on = true;
  remix.state.other = "sequel";
  std::string said;
  std::unique_ptr<cl::LoadedMap> map = cl::load_remixed_map(mine, "hall", &remix, error, &said);
  CHECK_MSG(map != nullptr, error);
  if (!map) return;
  CHECK(map->from_game == "sequel" && map->program_rules && map->spec.brushes != nullptr);
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

// The classic game's rules wearing the sequel's things (M18u4, `cr_items 2`): only with the remix
// on and another game named; the rules keep every number and the map its class names, while the
// server's weapon and armour rows name the sequel's models and sounds, so the beam weapon lies on
// the floor as the sequel's railgun and the hand holds the sequel's view models.
TEST(the_classic_rules_wear_the_sequels_weapons_and_armour_with_cr_items) {
  ge::console::Cvars cvars;
  cl::RemixSettings s;
  cl::bind_remix(cvars, &s);
  cl::RemixState state;
  cvars.set("cr_items", "2", ge::console::Source::local);
  CHECK(cl::worn_arsenal(s, state, "classic") == nullptr);  // the remix off
  state.on = true;
  state.other = "sequel";
  const pf::RemixArsenal* worn = cl::worn_arsenal(s, state, "classic");
  CHECK(worn != nullptr && worn->items == "sequel");
  if (worn == nullptr) return;
  cvars.set("cr_items", "1", ge::console::Source::local);
  CHECK(cl::worn_arsenal(s, state, "classic") == nullptr);  // the game's own
  // The numbers are the classic game's; every name is the sequel's.
  CHECK(worn->arsenal.weapons.size() == std::size(pf::kClassicWeapons));
  CHECK(worn->held.size() == worn->arsenal.weapons.size());
  for (size_t i = 0; i < worn->arsenal.weapons.size(); i++) {
    const pf::WeaponRow& w = worn->arsenal.weapons[i];
    const pf::WeaponRow& c = pf::kClassicWeapons[i];
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
    CHECK(worn->arsenal.armour[i].absorb == pf::kClassicArmour[i].absorb &&
          worn->arsenal.armour[i].model.starts_with("models/items/armor/"));
  CHECK(pf::held_frames("progs/v_shot.mdl") == nullptr);
  // The ammunition and health in the sequel's boxes, the weapons picked up with its sound.
  const std::vector<std::string> lines = pf::arsenal_lines(worn->arsenal);
  const auto said = [&](std::string_view line) {
    for (const std::string& l : lines)
      if (l == line) return true;
    return false;
  };
  CHECK(said("set arsenal_ammo1 \"models/items/ammo/shells/medium/tris.md2 misc/am_pkup.wav 15\""));
  CHECK(said("set arsenal_health3 \"models/items/mega_h/tris.md2 items/m_health.wav 15\""));
  CHECK(said("set arsenal_pickup \"misc/w_pkup.wav\""));
  for (const std::string& l : pf::arsenal_lines(pf::kClassicArsenal))
    CHECK_MSG(l.find("arsenal_ammo") == std::string::npos &&
                  l.find("arsenal_pickup") == std::string::npos,
              l);

  // On the server: the rows given as its settings, the rail lying where the beam weapon was.
  cl::SearchPath mine;
  cl::RemixFiles remix;
  remix.played = "classic";
  remix.state = state;
  std::string error;
  CHECK(cl::add_archive(remix.files, "pak0.pak", pack({{"maps/hall.bsp", sequel_hall()}}), error));
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

// The sequel's bar over the classic status (M18u4b): the classic numbers in the sequel's stats,
// each picture by the sequel's own name, and the program run into pictures where the sequel's
// own bar puts them.
TEST(the_sequels_bar_shows_the_classic_status_while_its_weapons_are_worn) {
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

  cl::SequelBar bar;
  cl::run_worn_bar(bar, st, true, 0, 640, 480);
  const auto drawn = [&](std::string_view name) {
    for (const hud::LayoutOp& op : bar.ops)
      if (op.kind == hud::LayoutOp::Kind::picture && op.name == name) return true;
    return false;
  };
  CHECK(drawn("i_health") && drawn("i_combatarmor") && drawn("p_quad") && drawn("num_7"));
  CHECK(!drawn("a_bullets"));  // nothing to fire with the melee weapon
}

// The classic game's map in the sequel (M18u3): its point tree's cells made brushes so the
// sequel's rules trace it, its items the sequel's: the player stands on its floor, walks through a
// door and stops at the far wall.
TEST(the_classic_games_map_plays_on_the_sequels_rules_with_the_sequels_items) {
  br::Fixture f = br::sample_fixture();
  br::Entity health;
  health.pairs = {{"classname", "item_health"}, {"origin", "300 100 24"}};
  br::Entity lg;
  lg.pairs = {{"classname", "weapon_lightning"}, {"origin", "300 300 24"}};
  f.entities = {health, lg};
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(f, br::Family::v29, &bytes, &error), error);
  cl::SearchPath mine;  // the sequel's own files: no such map
  cl::RemixFiles remix;
  remix.played = "sequel";
  remix.state.on = true;
  remix.state.other = "classic";
  CHECK(cl::add_archive(remix.files, "pak0.pak", pack({{"maps/halls.bsp", bytes}}), error));
  std::unique_ptr<cl::LoadedMap> map = cl::load_remixed_map(mine, "halls", &remix, error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return;
  CHECK(map->from_game == "classic" && !map->program_rules && map->spec.brushes != nullptr);
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
  shooters_edge::server::SequelServer* sq = shooters_edge::server::sequel_of(*l.rules);
  CHECK(sq != nullptr);
  if (sq == nullptr) return;
  int shown = 0;
  for (const auto& e : sq->world.entities)
    if (!e.free && e.modelindex > 0 && size_t(e.modelindex) < sq->world.models.size()) {
      std::printf("  shown: %s at %.0f %.0f %.0f\n", sq->world.models[size_t(e.modelindex)].c_str(),
                  double(e.origin.x), double(e.origin.y), double(e.origin.z));
      ++shown;
    }
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
