// Tests: the sequel's other single-player entities (games/shooters_edge/server/sequel_targets.cpp)
// in a hall built here: a key lock asks for its key and then spends it, a counter counts down to
// its targets, a barrel shot blows a moment later and hurts who stands by, a laser burns who
// stands in its beam, a killbox kills who is in it, a light ramp steps a light's letter, a string
// sets the frames of its characters, a door of water toggles between its ends, a ship flies its
// path once used, its bomb falls and blows, a falling object crushes who is under it, and a
// spawner brings out its monsters until it has made all it can.
//
// ge:summary Tests: the sequel's keys, counters, barrels, lasers, killboxes, ramps, strings, water
// ge:keywords test sequel trigger-key trigger-counter explobox target-laser killbox lightramp
// ge:keywords target-string func-water misc-viper viper-bomb func-object target-spawner
// ge:skill shooter testing
// ge:test tests/shooters_edge_sequel_targets_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

using Pairs = std::vector<std::pair<std::string, std::string>>;

br::Entity entity(Pairs pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A floored hall with a start at the west end, a killbox's box round it (model 1), a door of
// water's block at x 300 (model 2), an object's block high over the start (model 3), and `extra`
// entities.
std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& extra) {
  br::BrushWorld w;
  w.models.resize(4);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  const float klo[3] = {-640, -40, 0}, khi[3] = {-560, 40, 80};
  w.models[1].push_back(br::box_brush(klo, khi, sh::kSqSolid, "world/trigger"));
  const float wlo[3] = {280, -32, 0}, whi[3] = {344, 32, 64};
  w.models[2].push_back(br::box_brush(wlo, whi, sh::kSqWater, "world/water"));
  const float olo[3] = {-632, -24, 200}, ohi[3] = {-568, 24, 232};
  w.models[3].push_back(br::box_brush(olo, ohi, sh::kSqSolid, "world/crate"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  for (const Pairs& m : extra) w.entities.push_back(entity(m));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Game {
  sv::RulesRunner r;
  sv::MapSpec map;
  uint32_t sequence = 0;
  sv::SequelServer& s() const { return *sv::sequel_of(r); }
  sv::SequelPlayer& p() const { return s().players[0]; }
  int find(const char* classname) const {
    for (size_t i = 0; i < s().entities.size(); i++)
      if (s().entities[i].classname == classname) return static_cast<int>(i);
    return -1;
  }
  int named(const char* name) const {
    for (size_t i = 0; i < s().entities.size(); i++)
      if (s().entities[i].targetname == name) return static_cast<int>(i);
    return -1;
  }
  sv::SequelEntity& at(int e) const { return s().entities[size_t(e)]; }
};

std::unique_ptr<Game> open_game(const std::vector<Pairs>& extra) {
  auto g = std::make_unique<Game>();
  g->r = sv::sequel_runner(sv::make_sequel_server({}));
  g->r.console("deathmatch 0");
  g->map.name = "hall";
  g->map.brushes = hall(extra);
  std::string error;
  CHECK_MSG(g->r.start(g->map, error), error);
  g->r.connect(0, "Ana", false, false);
  return g;
}

void run(Game& g, int ticks) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++g.sequence;
    sh::MoveCmd c;
    c.msec = 13;
    pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
    g.r.frame(&pk, 1);
  }
}

constexpr int kSecond = 77;  // host ticks of 13 ms

Pairs relay(const char* name, const char* target) {
  return {{"classname", "trigger_relay"}, {"targetname", name}, {"target", target}};
}
Pairs help(const char* name, const char* message) {
  return {{"classname", "target_help"}, {"targetname", name}, {"message", message}};
}

bool printed(const Game& g, const std::string& words) {
  for (const sv::NetEvent& ev : g.s().out.events)
    if (ev.kind == sv::EventKind::centerprint && ev.text.find(words) != std::string::npos)
      return true;
  return false;
}

}  // namespace

TEST(a_key_lock_asks_for_its_key_then_spends_it) {
  auto g = open_game({relay("go", "lock"),
                      {{"classname", "trigger_key"},
                       {"targetname", "lock"},
                       {"item", "key_data_cd"},
                       {"target", "news"}},
                      help("news", "The lock is open.")});
  run(*g, 10);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(printed(*g, "You need the Data CD."));
  CHECK(g->s().help_changed == 0);
  const int cd = sv::sq_item("key_data_cd");
  g->p().body.counts[cd] = 1;
  sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(g->s().help_changed == 1 && g->s().help[1] == "The lock is open.");
  CHECK(g->p().body.counts[cd] == 0);
}

TEST(a_counter_counts_down_to_its_targets) {
  auto g = open_game({relay("go", "count"),
                      {{"classname", "trigger_counter"},
                       {"targetname", "count"},
                       {"count", "3"},
                       {"target", "news"}},
                      help("news", "All three.")});
  run(*g, 10);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(printed(*g, "Only 2 more to go..."));
  run(*g, 1);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(printed(*g, "Only one more to go..."));
  CHECK(g->s().help_changed == 0);
  run(*g, 1);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(printed(*g, "Sequence completed!"));
  CHECK(g->s().help_changed == 1);
}

TEST(a_barrel_shot_blows_a_moment_later_and_hurts_who_stands_by) {
  auto g = open_game({{{"classname", "misc_explobox"}, {"origin", "-500 0 0"}}});
  run(*g, 10);
  const int barrel = g->find("misc_explobox");
  CHECK(barrel >= 0 && !g->at(barrel).gone);
  const float health = g->p().health;
  CHECK(sv::map_damage(g->s(), barrel, 0, 5));
  CHECK(!g->at(barrel).gone);  // 10 health
  CHECK(sv::map_damage(g->s(), barrel, 0, 10));
  CHECK(!g->at(barrel).gone);  // the fuse
  run(*g, kSecond / 2);
  CHECK(g->at(barrel).gone);
  CHECK_MSG(g->p().health < health, std::to_string(g->p().health));
}

TEST(a_laser_burns_who_stands_in_its_beam) {
  auto g = open_game({{{"classname", "target_laser"},
                       {"origin", "-900 0 30"},
                       {"angle", "0"},
                       {"spawnflags", "1"},
                       {"dmg", "4"}}});
  run(*g, kSecond / 2);
  const float before = g->p().health;
  CHECK(before == 100);  // it sets itself up a second in
  run(*g, kSecond);
  CHECK_MSG(g->p().health < before, std::to_string(g->p().health));
}

TEST(a_killbox_kills_who_is_in_it) {
  auto g = open_game({relay("go", "box"),
                      {{"classname", "func_killbox"}, {"targetname", "box"}, {"model", "*1"}}});
  run(*g, 10);
  CHECK(g->p().alive);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(!g->p().alive || g->p().health <= 0);
}

TEST(a_light_ramp_steps_a_lights_letter) {
  auto g = open_game({relay("go", "ramp"),
                      {{"classname", "light"}, {"targetname", "lamp"}, {"style", "32"}},
                      {{"classname", "target_lightramp"},
                       {"targetname", "ramp"},
                       {"target", "lamp"},
                       {"message", "az"},
                       {"speed", "1"}}});
  run(*g, 10);
  CHECK(g->s().world.lightstyles[32] == "m");
  sv::map_use_targets(g->s(), g->named("go"), 0);
  run(*g, kSecond / 2);
  const std::string mid = g->s().world.lightstyles[32];
  CHECK_MSG(mid > "d" && mid < "w", mid);
  run(*g, kSecond);
  CHECK_MSG(g->s().world.lightstyles[32] == "z", g->s().world.lightstyles[32]);
}

TEST(a_string_sets_the_frames_of_its_characters) {
  auto g = open_game({relay("go", "text"),
                      {{"classname", "target_character"}, {"team", "clock"}, {"count", "1"}},
                      {{"classname", "target_character"}, {"team", "clock"}, {"count", "2"}},
                      {{"classname", "target_character"}, {"team", "clock"}, {"count", "3"}},
                      {{"classname", "target_string"},
                       {"targetname", "text"},
                       {"team", "clock"},
                       {"message", "4:"}}});
  run(*g, 10);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  std::vector<int> frames;
  for (const sv::SequelEntity& e : g->s().entities)
    if (e.classname == "target_character") frames.push_back(e.frame);
  CHECK(frames.size() == 3 && frames[0] == 4 && frames[1] == 11 && frames[2] == 12);
}

TEST(a_door_of_water_toggles_between_its_ends) {
  auto g = open_game(
      {relay("go", "flood"),
       {{"classname", "func_water"}, {"targetname", "flood"}, {"model", "*2"}, {"angle", "-1"}}});
  run(*g, 10);
  const int water = g->find("func_water");
  CHECK(water >= 0 && g->at(water).origin.z == 0);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  run(*g, 4 * kSecond);  // 64 units at 25 a second
  CHECK_MSG(g->at(water).origin.z == 64, std::to_string(g->at(water).origin.z));
  sv::map_use_targets(g->s(), g->named("go"), 0);
  run(*g, 4 * kSecond);
  CHECK_MSG(g->at(water).origin.z == 0, std::to_string(g->at(water).origin.z));
}

TEST(a_ship_flies_its_path_once_used_and_its_bomb_falls_and_blows) {
  auto g = open_game({relay("go", "ship"),
                      relay("drop", "bomb"),
                      {{"classname", "path_corner"},
                       {"targetname", "c1"},
                       {"origin", "0 0 300"},
                       {"target", "c2"}},
                      {{"classname", "path_corner"}, {"targetname", "c2"}, {"origin", "900 0 300"}},
                      {{"classname", "misc_viper"},
                       {"targetname", "ship"},
                       {"target", "c1"},
                       {"origin", "0 0 300"}},
                      {{"classname", "misc_viper_bomb"},
                       {"targetname", "bomb"},
                       {"origin", "0 0 200"},
                       {"target", "news"}},
                      help("news", "The bomb fell.")});
  run(*g, kSecond);
  const int ship = g->find("misc_viper");
  CHECK(ship >= 0 && g->at(ship).hidden);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  run(*g, kSecond);
  CHECK(!g->at(ship).hidden);
  CHECK_MSG(g->at(ship).origin.x > 100, std::to_string(g->at(ship).origin.x));
  const int bomb = g->find("misc_viper_bomb");
  CHECK(bomb >= 0 && g->at(bomb).hidden);
  sv::map_use_targets(g->s(), g->named("drop"), 0);
  run(*g, 3 * kSecond);
  CHECK(g->at(bomb).gone);
  CHECK(g->s().help_changed == 1 && g->s().help[1] == "The bomb fell.");
}

TEST(a_falling_object_crushes_who_is_under_it) {
  auto g = open_game({relay("go", "crate"),
                      {{"classname", "func_object"},
                       {"targetname", "crate"},
                       {"spawnflags", "1"},
                       {"model", "*3"},
                       {"dmg", "40"}}});
  run(*g, kSecond);
  const int crate = g->find("func_object");
  CHECK(crate >= 0 && g->at(crate).hidden && g->at(crate).origin.z == 0);
  const float before = g->p().health;
  sv::map_use_targets(g->s(), g->named("go"), 0);
  run(*g, 3 * kSecond);
  CHECK(!g->at(crate).hidden && g->at(crate).origin.z < -100);
  CHECK_MSG(g->p().health == before - 40, std::to_string(g->p().health));
}

TEST(a_spawner_brings_out_its_monsters_until_it_has_made_all_it_can) {
  auto g = open_game({relay("go", "make"),
                      {{"classname", "target_spawner"},
                       {"targetname", "make"},
                       {"target", "monster_soldier_light"},
                       {"origin", "400 0 24"},
                       {"angle", "180"}}});
  run(*g, kSecond);
  CHECK(g->s().monsters_total == 0);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(g->s().monsters_total == 1);
  for (int n = 0; n < 4; n++) sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(g->s().monsters_total == 4);
  bool told = false;
  for (const std::string& line : g->s().console)
    if (line.find("has made all 4") != std::string::npos) told = true;
  CHECK(told);
}
