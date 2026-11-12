// Tests: the bots' map nav (games/shooters_edge/bots/map_nav.hpp) built on a version 38 map run by
// the sequel's rules. A hall split by a wall with a doorway, a plat up to a ledge, and a teleporter
// pad with its destination: the world is the brushes kind, the doorway joins both halves, the pad
// links to its destination, and the plat is a ridden mover traced through the nav's own tracer.
// The sequel's arsenal: its rows, its items and a player's numbers read through it (M18v8b).
//
// ge:summary Test: the bots' nav on a version 38 map under the sequel's rules (M18v8)
// ge:keywords test map-nav sequel version-38 brushes plat teleporter-pad either-family
// ge:skill bots shooter testing
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/bots/bot_arsenal.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/map_nav.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
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
  sv::RulesRunner r = sv::sequel_runner(sv::make_sequel_server({}));
  r.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = hall();
  std::string error;
  CHECK_MSG(r.start(spec, error), error);
  auto nav = std::make_unique<bt::MapNav>();
  CHECK_MSG(bt::build_map_nav(*r, spec, "info_player_deathmatch", *nav, error), error);
  CHECK(nav->world.solids[0].kind == sh::SolidKind::brushes);
  CHECK(nav->tracer != nullptr);
  CHECK(nav->graph.nodes.size() > 100);
  // The doorway joins the halves: both spawns are in the one main part.
  const int32_t west = sh::nav_nearest(nav->graph, {-300, 0, 24});
  const int32_t east = sh::nav_nearest(nav->graph, {200, 0, 24});
  CHECK(west >= 0 && east >= 0);
  CHECK(nav->main.reached(west) && nav->main.reached(east));
  // The pad links to its destination.
  CHECK(nav->teleport_links > 0);
  // The plat: a ridden mover on the nav's tracer, and the ledge reached by riding it.
  bool plat = false;
  for (const sh::NavMover& m : nav->movers)
    if (m.tracer == nav->tracer.get() && m.brush_model == 1) plat = true;
  CHECK(plat);
  int rides = 0;
  for (const sh::NavNode& n : nav->graph.nodes) rides += n.ride >= 0 ? 1 : 0;
  CHECK(rides > 0);
  const int32_t ledge = sh::nav_nearest(nav->graph, {448, 0, 96 + 24});
  CHECK(ledge >= 0);
  CHECK(nav->graph.nodes[static_cast<size_t>(ledge)].origin.z > 96);
  CHECK(nav->main.reached(ledge));
}

TEST(the_sequel_arsenal_reads_a_players_numbers_and_the_maps_items) {
  const bt::BotArsenal& a = bt::sequel_arsenal();
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
  // A player's numbers off the sequel's server: the railgun taken with its slugs, then read.
  sv::RulesRunner r = sv::sequel_runner(sv::make_sequel_server({}));
  r.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = hall();
  std::string error;
  CHECK_MSG(r.start(spec, error), error);
  r.connect(0, "one", false, false);
  sv::SequelServer* s = sv::sequel_of(r);
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
