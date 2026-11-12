// Tests: the sequel's items on its runner (games/shooters_edge/server/sequel_server.hpp): an item
// is dropped onto the floor at the map's start, one that starts in a solid is removed, one flagged
// out of deathmatch is not spawned there; a player walking into items takes them by their rule
// (the HUD's numbers follow), a taken item hides and shows again after its respawn time in
// deathmatch, and outside deathmatch it is gone for good.
//
// ge:summary Tests: the sequel's items on the runner: dropped, left out, taken, hidden, respawned
// ge:keywords test sequel items pickup respawn drop-to-floor spawnflags deathmatch sequel-server
// ge:skill shooter testing
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A floored hall; a start at -200 facing +x; rockets in the air ahead (they fall to the floor), a
// railgun past them, a jacket only outside deathmatch, a health sunk in the floor.
std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-512, -512, -16}, hi[3] = {512, 512, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "-200 0 24"}, {"angle", "0"}}));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-200 0 24"}, {"angle", "0"}}));
  w.entities.push_back(entity({{"classname", "ammo_rockets"}, {"origin", "-100 0 60"}}));
  w.entities.push_back(entity({{"classname", "weapon_railgun"}, {"origin", "0 0 24"}}));
  w.entities.push_back(
      entity({{"classname", "item_armor_jacket"}, {"origin", "100 0 24"}, {"spawnflags", "2048"}}));
  w.entities.push_back(entity({{"classname", "item_health"}, {"origin", "0 200 -8"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

sv::RulesRunner start(const char* deathmatch) {
  sv::RulesRunner r = sv::sequel_runner(sv::make_sequel_server({}));
  r.console(std::string("deathmatch ") + deathmatch);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(r.start(map, error), error);
  r.connect(0, "player", false, false);
  return r;
}

// Host ticks of 13 ms with `forward` along +x.
void run(const sv::RulesRunner& r, uint32_t& sequence, int ticks, int16_t forward) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet p;
    p.client = 0;
    p.sequence = ++sequence;
    sh::MoveCmd c;
    c.msec = 13;
    c.forward = forward;
    p.cmds[0] = p.cmds[1] = p.cmds[2] = c;
    r.frame(&p, 1);
  }
}

const sv::SequelEntity* named(const sv::SequelServer& s, const char* classname) {
  for (const sv::SequelEntity& e : s.entities)
    if (e.classname == classname) return &e;
  return nullptr;
}

}  // namespace

TEST(items_drop_to_the_floor_and_the_ones_in_a_solid_or_left_out_are_not_there) {
  sv::RulesRunner r = start("1");
  const sv::SequelServer& s = *sv::sequel_of(r);
  const sv::SequelEntity* rockets = named(s, "ammo_rockets");
  CHECK(rockets != nullptr && !rockets->gone);
  if (rockets) CHECK(rockets->origin.z > 15 && rockets->origin.z < 15.2f);  // resting on z 0
  CHECK(named(s, "item_health")->gone);                                     // started in the floor
  CHECK(named(s, "item_armor_jacket")->gone);  // flagged out of deathmatch
  const sv::RulesRunner single = start("0");
  CHECK(!named(*sv::sequel_of(single), "item_armor_jacket")->gone);
  // The items' models are in the list before the player's, which stays last.
  const std::vector<std::string>& models = r->world.models;
  CHECK(models.back() == sv::kSequelPlayerModel);
  CHECK(models[size_t(rockets->modelindex)] == "models/items/ammo/rockets/medium/tris.md2");
}

TEST(a_player_walking_into_items_takes_them_and_they_respawn_in_deathmatch) {
  sv::RulesRunner r = start("1");
  const sv::SequelServer& s = *sv::sequel_of(r);
  uint32_t sequence = 0;
  run(r, sequence, 80, 0);     // landed at the start
  run(r, sequence, 120, 300);  // through the rockets and onto the railgun
  const sv::SequelPlayer& me = s.players[0];
  CHECK(me.taken.size() == 2);
  CHECK(me.body.ammo[int(sv::SqAmmo::rockets)] == 5 && me.body.ammo[int(sv::SqAmmo::slugs)] == 10);
  CHECK(r->world.clients[0].numbers.ammo[2] == 5);  // the HUD's rockets
  const sv::SequelEntity* rockets = named(s, "ammo_rockets");
  CHECK(rockets->hidden && !rockets->gone);
  const size_t row = 1 + sh::kMaxClients + size_t(rockets - s.entities.data());
  CHECK(r->world.entities[row].free);
  run(r, sequence, 2400, 0);  // 31 s
  CHECK(!rockets->hidden && !r->world.entities[row].free);
}

TEST(outside_deathmatch_a_taken_item_is_gone) {
  sv::RulesRunner r = start("0");
  const sv::SequelServer& s = *sv::sequel_of(r);
  uint32_t sequence = 0;
  run(r, sequence, 80, 0);
  run(r, sequence, 120, 300);
  CHECK(named(s, "ammo_rockets")->gone);
}
