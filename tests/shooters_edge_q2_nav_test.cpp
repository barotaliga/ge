// Tests: the bots' map nav (plugins/shooterbots/map_nav.hpp) built on a version 38 map run by
// Quake II's rules. A hall split by a wall with a doorway, a plat up to a ledge, and a teleporter
// pad with its destination: the world is the brushes kind, the doorway joins both halves, the pad
// links to its destination, and the plat is a ridden mover traced through the nav's own tracer.
// Quake II's arsenal: its rows, its items and a player's numbers read through it (M18v8b).
//
// ge:summary Test: the bots' nav on a version 38 map under Quake II's rules (M18v8)
// ge:keywords test map-nav q2 version-38 brushes plat teleporter-pad either-family
// ge:skill bots shooter testing
#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_move.hpp"
#include "shooter/trace.hpp"
#include "shooterbots/bot_arsenal.hpp"
#include "shooterbots/fighter.hpp"
#include "shooterbots/knowledge.hpp"
#include "shooterbots/map_nav.hpp"
#include "shooters_edge/bots/arsenals.hpp"
#include "shooters_edge/bots/quake_maps.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sb = ge::shooterbots;
namespace sv = shooters_edge::server;
namespace bt = shooters_edge::bots;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// West and east halves joined by a doorway at x = 0; a ledge 96 high at the east end with a plat
// (model 1) beside it; a pad in the west half sending players to the east half.
std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(2);
  auto& m = w.models[0];
  m.push_back(box(-512, -256, -16, 512, 256, 0));    // floor
  m.push_back(box(-512, -256, 256, 512, 256, 272));  // ceiling
  m.push_back(box(-528, -256, 0, -512, 256, 256));   // walls round
  m.push_back(box(512, -256, 0, 528, 256, 256));
  m.push_back(box(-512, -272, 0, 512, -256, 256));
  m.push_back(box(-512, 256, 0, 512, 272, 256));
  m.push_back(box(-8, -256, 0, 8, -48, 256));  // the dividing wall, a doorway in it
  m.push_back(box(-8, 48, 0, 8, 256, 256));
  m.push_back(box(384, -256, 0, 512, 256, 96));          // the ledge
  w.models[1].push_back(box(304, -32, 0, 368, 32, 96));  // the plat, drawn raised
  w.entities.push_back(entity({{"classname", "info_player_deathmatch"}, {"origin", "-300 0 24"}}));
  w.entities.push_back(entity({{"classname", "info_player_deathmatch"}, {"origin", "200 0 24"}}));
  w.entities.push_back(entity({{"classname", "func_plat"}, {"model", "*1"}}));
  w.entities.push_back(
      entity({{"classname", "misc_teleporter"}, {"origin", "-400 -180 16"}, {"target", "t1"}}));
  w.entities.push_back(entity(
      {{"classname", "misc_teleporter_dest"}, {"origin", "200 180 16"}, {"targetname", "t1"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

}  // namespace

TEST(the_nav_builds_on_a_version_38_map) {
  sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
  r.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = hall();
  std::string error;
  CHECK_MSG(r.start(spec, error), error);
  auto nav = std::make_unique<bt::MapNav>();
  CHECK_MSG(bt::build_map_nav(*r, spec, bt::quake_maps(), "info_player_deathmatch", *nav, error),
            error);
  CHECK(nav->world.solids[0].kind == sh::SolidKind::brushes);
  CHECK(nav->tracer != nullptr);
  CHECK(nav->graph.nodes.size() > 100);
  // The doorway joins the halves: both spawns are in the one main part.
  const int32_t west = sb::nav_nearest(nav->graph, {-300, 0, 24});
  const int32_t east = sb::nav_nearest(nav->graph, {200, 0, 24});
  CHECK(west >= 0 && east >= 0);
  CHECK(nav->main.reached(west) && nav->main.reached(east));
  // The pad links to its destination.
  CHECK(nav->teleport_links > 0);
  // The plat: a ridden mover on the nav's tracer, and the ledge reached by riding it.
  bool plat = false;
  for (const sb::NavMover& m : nav->movers)
    if (m.tracer == nav->tracer.get() && m.brush_model == 1) plat = true;
  CHECK(plat);
  int rides = 0;
  for (const sb::NavNode& n : nav->graph.nodes) rides += n.ride >= 0 ? 1 : 0;
  CHECK(rides > 0);
  const int32_t ledge = sb::nav_nearest(nav->graph, {448, 0, 96 + 24});
  CHECK(ledge >= 0);
  CHECK(nav->graph.nodes[static_cast<size_t>(ledge)].origin.z > 96);
  CHECK(nav->main.reached(ledge));
}

TEST(q2_arsenal_reads_a_players_numbers_and_the_maps_items) {
  const bt::BotArsenal& a = bt::q2_arsenal();
  CHECK(a.weapons.size() == 10 && a.ammo_count == 6);
  // The rows: a weapon's bit is 1 << its index, the impulse that asks for it its index + 1.
  const bt::BotWeapon* rail = bt::weapon_row(a, 1 << 8);
  CHECK(rail != nullptr);
  if (rail == nullptr) return;
  CHECK(rail->number == 9 && rail->ammo == 5);
  CHECK(bt::role_of(a, a.launcher) == bt::WeaponRole::launcher && bt::launcher_number(a) == 7);
  CHECK(bt::role_of(a, a.lob) == bt::WeaponRole::lob &&
        bt::role_of(a, 1 << 8) == bt::WeaponRole::none);
  // The map's items: the launcher's part set, the slugs an ammunition the railgun spends.
  bt::ItemSpot rl, slugs, body;
  CHECK(bt::item_of("weapon_rocketlauncher", 0, rl, a));
  CHECK(rl.weapon == a.launcher && rl.role == bt::WeaponRole::launcher);
  CHECK(bt::item_of("ammo_slugs", 0, slugs, a) && a.item_ammo(slugs.kind) == rail->ammo);
  CHECK(bt::item_of("item_armor_body", 0, body, a) && body.kind == bt::ItemKind::armor3);
  CHECK(!bt::item_of("info_player_start", 0, body, a));
  // A player's numbers off Quake II's server: the railgun taken with its slugs, then read.
  sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
  r.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = hall();
  std::string error;
  CHECK_MSG(r.start(spec, error), error);
  r.connect(0, "one", false, false);
  sv::Q2Server* s = sv::q2_of(r);
  CHECK(s != nullptr);
  if (s == nullptr) return;
  CHECK(sv::sq_pick_up(sv::sq_item("weapon_railgun"), s->players[0].body, true, 1, 0));
  r.frame(nullptr, 0);
  const bt::Stats st = bt::read_stats(*r, 1, a);
  CHECK(st.arsenal == &a);
  CHECK(st.health == 100);
  CHECK((st.items & (1 << 8)) && (st.items & 1));  // the railgun and the blaster
  CHECK(st.weapon == 1);                           // the blaster in hand
  CHECK(st.ammo[5] == 10 && bt::loaded(st, *rail));
  CHECK(!bt::has_launcher(st) && bt::launcher_rounds(st) == 0);
}

TEST(ground_zeros_arsenal_aims_its_guns_and_counts_its_ammunition) {
  // Ground Zero's deathmatch: its five aimed guns join the base ten, its flechettes, prox mines
  // and disruptor rounds are ammunitions by their place in the rules, and double damage is sought
  // as the quad is. The tesla, thrown and never aimed, is not a row; nor are The Reckoning's guns.
  const bt::BotArsenal& a = bt::q2_arsenal(sv::Q2Pack::ground_zero);
  CHECK(a.weapons.size() == 15 && a.ammo_count == 12);
  CHECK(bt::weapon_row(a, 1 << 11) == nullptr);  // the ion ripper
  CHECK(bt::weapon_row(a, 1 << 19) == nullptr);  // the tesla
  const bt::BotWeapon* etf = bt::weapon_row(a, 1 << 15);
  const bt::BotWeapon* dis = bt::weapon_row(a, 1 << 18);
  const bt::BotWeapon* prox = bt::weapon_row(a, 1 << 16);
  CHECK(etf != nullptr && dis != nullptr && prox != nullptr);
  if (etf == nullptr || dis == nullptr || prox == nullptr) return;
  CHECK(etf->number == 16 && etf->ammo == 8 && dis->number == 19 && dis->ammo == 11);
  CHECK(prox->ammo == 9 && prox->blast);
  bt::ItemSpot spot;
  CHECK(bt::item_of("ammo_flechettes", 0, spot, a) && a.item_ammo(spot.kind) == etf->ammo);
  CHECK(bt::item_of("ammo_disruptor", 0, spot, a) && a.item_ammo(spot.kind) == dis->ammo);
  CHECK(bt::item_of("ammo_prox", 0, spot, a) && a.item_ammo(spot.kind) == prox->ammo);
  CHECK(bt::item_of("weapon_etf_rifle", 0, spot, a) && spot.weapon == etf->bit);
  CHECK(bt::item_of("item_double", 0, spot, a) && spot.kind == bt::ItemKind::quad);
  CHECK(!bt::item_of("ammo_tesla", 0, spot, a));
  CHECK(a.ammo_max[8] == 200 && a.ammo_max[9] == 50 && a.ammo_max[11] == 100);
  // The Reckoning's own stay as they were: its mag slugs the phalanx's.
  const bt::BotArsenal& x = bt::q2_arsenal(sv::Q2Pack::reckoning);
  CHECK(x.weapons.size() == 12 && x.ammo_count == 8);
  CHECK(bt::item_of("ammo_magslug", 0, spot, x) && x.item_ammo(spot.kind) == 6);
}

TEST(every_arsenals_weapon_numbers_fit_a_table_by_number) {
  // A fighter counts the commands holding each weapon by its number (fighter.hpp's
  // `weapon_commands`): Quake II's ten run to 10, past Quake's eight.
  int most = 0;
  for (const bt::BotArsenal* a : {&bt::first_arsenal(), &bt::q2_arsenal()})
    for (const bt::BotWeapon& w : a->weapons) {
      CHECK_MSG(w.number >= 0 && w.number <= bt::kMostWeaponNumber,
                std::string(w.name) + " answers to " + std::to_string(w.number));
      most = std::max(most, w.number);
    }
  CHECK(most == bt::kMostWeaponNumber);
  bt::FighterStats st;
  CHECK(std::size(st.weapon_commands) == static_cast<size_t>(bt::kMostWeaponNumber + 1));
}

TEST(the_pools_trace_stops_at_a_mover_where_the_rules_put_it) {
  sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
  r.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = hall();
  std::string error;
  CHECK_MSG(r.start(spec, error), error);
  r.frame(nullptr, 0);
  const sh::Server& pool = r->sv;
  CHECK(pool.world_trace != nullptr);
  // The plat lowered to the floor: a line just over the floor across its place meets it, and
  // the hit is the plat's row, not the world.
  const sh::HullTrace low = sh::trace_move(pool, {250, 0, 4}, {}, {}, {380, 0, 4}, 0, 0);
  CHECK(low.fraction < 1);
  CHECK(low.ent > 0);
  CHECK(low.endpos.x > 290 && low.endpos.x < 310);
  // Above it (the plat down), the line goes on to the ledge's face.
  const sh::HullTrace high = sh::trace_move(pool, {250, 0, 60}, {}, {}, {380, 0, 60}, 0, 0);
  CHECK(high.fraction == 1);
  // Through the dividing wall: the world.
  const sh::HullTrace wall = sh::trace_move(pool, {-100, 150, 40}, {}, {}, {100, 150, 40}, 0, 0);
  CHECK(wall.fraction < 1 && wall.ent == 0);
}
