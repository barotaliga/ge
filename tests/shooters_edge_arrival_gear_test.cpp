// Gear on arrival (bots/arrival_gear.hpp): the share of ammunition a map's monsters leave, Quake
// II's campaign order read off its level changes, Quake II's body after an earlier map, and Quake's
// leader arriving on a map with the coop hall's pickups carried by the program's level change.
//
// ge:summary Tests: gear on arrival: the kept share, Quake II's order and body, Quake's arrival
// ge:keywords test arrival-gear gear_kept_share q2_campaign_order q2_arrival_body q1 parms
// ge:skill shooter bots
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_move.hpp"
#include "shooters_edge/bots/arrival_gear.hpp"
#include "shooters_edge/bots/coop_episode.hpp"
#include "shooters_edge/bots/coop_run.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace bt = shooters_edge::bots;

using Pairs = std::vector<std::pair<std::string, std::string>>;
using Maps = std::map<std::string, std::shared_ptr<const br::Map>>;

// A closed room x -512..512, y -512..512 with `extra` entities.
std::shared_ptr<const br::Map> room(const std::vector<Pairs>& extra) {
  br::BrushWorld world;
  world.models.resize(1);
  const auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(-512, -512, -16, 512, 512, 0);
  box(-512, -512, 256, 512, 512, 272);
  box(-528, -512, 0, -512, 512, 256);
  box(512, -512, 0, 528, 512, 256);
  box(-528, -528, 0, 528, -512, 256);
  box(-528, 512, 0, 528, 528, 256);
  br::Entity start;
  start.pairs = {{"classname", "info_player_start"}, {"origin", "-400 0 24"}};
  world.entities.push_back(start);
  for (const Pairs& p : extra) {
    br::Entity e;
    e.pairs = p;
    world.entities.push_back(e);
  }
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(world, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

bool load(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  const Maps& maps = *static_cast<const Maps*>(ctx);
  const auto it = maps.find(name);
  if (it == maps.end()) {
    error = "no map " + name;
    return false;
  }
  out = sv::MapSpec();
  out.name = name;
  out.brushes = it->second;
  return true;
}

Pairs change(const char* map) { return {{"classname", "target_changelevel"}, {"map", map}}; }

}  // namespace

TEST(the_kept_share_pays_the_monsters_then_the_margin) {
  const int have[2] = {20, 10};
  const double damage[2] = {10, 20};  // 400 damage held
  CHECK(bt::gear_kept_share(have, damage, 0) == 1 - bt::kGearMargin);
  CHECK(bt::gear_kept_share(have, damage, 100) == 1 - 0.25 - bt::kGearMargin);
  CHECK(bt::gear_kept_share(have, damage, 400) == 0);
  CHECK(bt::gear_kept_share(have, damage, 1000) == 0);
  const int none[2] = {0, 0};
  CHECK(bt::gear_kept_share(none, damage, 50) == 0);
}

TEST(quake_ii_order_follows_the_unit_before_the_next_unit) {
  Maps maps;
  maps["a"] = room({change("b"), change("eou1_.cin+*d$start")});
  maps["b"] = room({change("c$back"), change("a")});
  maps["c"] = room({change("b")});
  maps["d"] = room({change("end.cin+victory.pcx"), change("missing")});
  const std::vector<std::string> order = bt::q2_campaign_order(&load, &maps, "a");
  CHECK(order == (std::vector<std::string>{"a", "b", "c", "d"}));
}

TEST(quake_ii_arrives_with_the_earlier_maps_guns_keys_and_what_their_monsters_left) {
  Maps maps;
  maps["a"] = room({{{"classname", "weapon_shotgun"}, {"origin", "0 100 16"}},
                    {{"classname", "ammo_shells"}, {"origin", "0 -100 16"}},
                    {{"classname", "item_health"}, {"origin", "100 0 16"}},
                    {{"classname", "monster_soldier_light"}, {"origin", "300 0 24"}},
                    {{"classname", "key_blue_key"}, {"origin", "-100 100 16"}},
                    {{"classname", "key_red_key"}, {"origin", "-100 -100 16"}},
                    {{"classname", "trigger_key"},
                     {"targetname", "lock"},
                     {"item", "key_blue_key"},
                     {"target", "gate"}}});
  maps["b"] = room({});
  sv::SqBody body;
  std::vector<bt::GearLeg> legs;
  std::string error;
  CHECK_MSG(bt::q2_arrival_body(&load, &maps, sv::Q2Pack::base, 1, {"a"}, body, legs, error),
            error);
  CHECK(legs.size() == 1);
  if (legs.size() != 1) return;
  const int shotgun = sv::sq_item("weapon_shotgun");
  CHECK(body.counts[shotgun] == 1);
  CHECK_MSG(legs[0].pickups == 4, "the gun, the shells and the two keys; never the health");
  CHECK_MSG(body.counts[sv::sq_item("key_blue_key")] == 0, "the map's lock took its blue key");
  CHECK_MSG(body.counts[sv::sq_item("key_red_key")] == 1, "the red key carries on");
  CHECK(legs[0].monster_health > 0);
  const int shells = body.ammo[static_cast<int>(sv::SqAmmo::shells)];
  const int held = 2 * ge::classic::kQ2AmmoShells;  // the gun's and the box's, as both give
  std::printf("shells %d of %d, kept %.3f, monsters' health %.0f, %s\n", shells, held, legs[0].kept,
              legs[0].monster_health, legs[0].after.c_str());
  CHECK(shells == static_cast<int>(held * legs[0].kept));
  CHECK(shells > 0 && shells < held);
  CHECK(body.health == body.max_health);
  // Nothing before: a new game's body.
  sv::SqBody fresh;
  std::vector<bt::GearLeg> none;
  CHECK(bt::q2_arrival_body(&load, &maps, sv::Q2Pack::base, 1, {}, fresh, none, error));
  CHECK(none.empty() && fresh.counts[shotgun] == 0);
}

TEST(quake_arrives_with_the_halls_nailgun_carried_by_the_level_change) {
  shooters_edge::bots::EpisodeOptions o;
  o.root = GE_SOURCE_DIR;
  o.load = &bt::coop_hall_exit_map;
  o.first = "hall";
  o.before = {"hall"};
  o.maps = 1;
  o.map_seconds = 2;
  o.skills.clear();
  o.single = true;
  o.god = true;
  const bt::EpisodeReport r = bt::run_coop_episode(o);
  CHECK_MSG(r.ok, r.error);
  if (!r.ok) return;
  CHECK(r.gear.size() == 1);
  if (r.gear.size() != 1) return;
  std::printf("took %d, monsters' health %.0f, kept %.3f; arrived: %s\n", r.gear[0].pickups,
              r.gear[0].monster_health, r.gear[0].kept, r.arrived.c_str());
  CHECK_MSG(r.gear[0].pickups >= 3, "the shells, the nails and the nailgun");
  CHECK(r.gear[0].monster_health > 0);
  CHECK(r.gear[0].kept > 0 && r.gear[0].kept < 1 - bt::kGearMargin);
  CHECK_MSG(r.arrived.find("NG") != std::string::npos, "the nailgun came through: " + r.arrived);
  CHECK(r.maps.size() == 1);
}
