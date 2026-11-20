// Tests: Quake II's other single-player entities (games/shooters_edge/server/q2_targets.cpp)
// in a hall built here: a key lock asks for its key and then spends it, a counter counts down to
// its targets, a barrel shot blows a moment later and hurts who stands by, a laser burns who
// stands in its beam, a killbox kills who is in it, a light ramp steps a light's letter, a string
// sets the frames of its characters, a door of water toggles between its ends, a ship flies its
// path once used, its bomb falls and blows, a falling object crushes who is under it, and a
// spawner brings out its monsters until it has made all it can, a prisoner never fights back
// nor counts, a crucified one hangs where it is, and a turret's driver aims its gun and fires.
//
// ge:summary Tests: Quake II's keys, counters, barrels, lasers, killboxes, ramps, strings, water
// ge:keywords test q2 trigger-key trigger-counter explobox target-laser killbox lightramp
// ge:keywords target-string func-water misc-viper viper-bomb func-object target-spawner
// ge:keywords misc-insane turret-breach turret-driver
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_targets_test.cpp
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_move.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_wire.hpp"

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
// water's block at x 300 (model 2), an object's block high over the start (model 3), a turret's
// gun about the middle (model 4), and `extra` entities.
std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& extra) {
  br::BrushWorld w;
  w.models.resize(5);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  const float klo[3] = {-640, -40, 0}, khi[3] = {-560, 40, 80};
  w.models[1].push_back(br::box_brush(klo, khi, sh::kSqSolid, "world/trigger"));
  const float wlo[3] = {280, -32, 0}, whi[3] = {344, 32, 64};
  w.models[2].push_back(br::box_brush(wlo, whi, sh::kSqWater, "world/water"));
  const float olo[3] = {-632, -24, 200}, ohi[3] = {-568, 24, 232};
  w.models[3].push_back(br::box_brush(olo, ohi, sh::kSqSolid, "world/crate"));
  const float glo[3] = {-16, -8, 40}, ghi[3] = {16, 8, 56};
  w.models[4].push_back(br::box_brush(glo, ghi, sh::kSqSolid, "world/gun"));
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
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Player& p() const { return s().players[0]; }
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
  sv::Q2Entity& at(int e) const { return s().entities[size_t(e)]; }
};

std::unique_ptr<Game> open_game(const std::vector<Pairs>& extra) {
  auto g = std::make_unique<Game>();
  g->r = sv::q2_runner(sv::make_q2_server({}));
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
  CHECK(printed(*g, "You need the Data CD"));
  CHECK(!printed(*g, "Data CD."));  // no full stop
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
  CHECK(printed(*g, "2 more to go..."));
  CHECK(!printed(*g, "Only"));
  run(*g, 1);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(printed(*g, "1 more to go..."));
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
  int chunks = 0;  // 2 big, its 4 corners, 8 small
  for (const sv::Q2Gib& c : g->s().gibs) chunks += c.live && c.debris ? 1 : 0;
  CHECK_MSG(chunks == 14, std::to_string(chunks));
}

TEST(an_explosive_wall_shot_breaks_into_chunks_by_its_mass) {
  auto g = open_game({{{"classname", "func_explosive"}, {"model", "*4"}, {"mass", "200"}}});
  run(*g, 10);
  const int wall = g->find("func_explosive");
  CHECK(sv::map_damage(g->s(), wall, 0, 200));
  CHECK(g->at(wall).gone);
  int chunks = 0;  // 2 big and 8 small
  for (const sv::Q2Gib& c : g->s().gibs) chunks += c.live && c.debris ? 1 : 0;
  CHECK_MSG(chunks == 10, std::to_string(chunks));
  run(*g, kSecond * 11);
  chunks = 0;
  for (const sv::Q2Gib& c : g->s().gibs) chunks += c.live ? 1 : 0;
  CHECK(chunks == 0);  // gone after 5 to 10 s
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

TEST(a_laser_is_a_beam_in_its_colour_through_monsters_with_sparks_when_turned_on) {
  auto g = open_game({relay("go", "beam"),
                      {{"classname", "target_laser"},
                       {"targetname", "beam"},
                       {"origin", "-900 0 30"},
                       {"angle", "0"},
                       {"spawnflags", "2"},  // red, off
                       {"dmg", "4"}},
                      {{"classname", "monster_infantry"}, {"origin", "-760 0 24"}}});
  run(*g, kSecond + kSecond / 2);
  const int laser = g->named("beam");
  const sv::WorldEntity& row = g->s().world.entities[size_t(sv::map_row(laser))];
  CHECK(row.free);  // off: not sent
  const int monster = g->s().entities[size_t(g->s().monsters[0].entity)].health;
  const float player = g->p().health;
  sv::map_use_targets(g->s(), g->named("go"), 0);
  int sparks = 0;
  for (const sv::NetEvent& ev : g->s().out.events)
    if (ev.text == "te_laser_sparks") {
      sparks++;
      CHECK_MSG(ev.values[3] == 8, std::to_string(ev.values[3]));  // 8 when it comes on
      CHECK_MSG(ev.level == 240, std::to_string(ev.level));        // red's low byte
    }
  CHECK(sparks == 1);
  run(*g, kSecond / 2);
  const sv::WorldEntity& on = g->s().world.entities[size_t(sv::map_row(laser))];
  CHECK(!on.free);
  CHECK((on.render_fx & 128) != 0 && (on.render_fx & 32) != 0);
  CHECK(on.frame == 4);
  CHECK(static_cast<uint32_t>(on.skin) == 0xF2F2F0F0u);
  CHECK_MSG(on.beam_end.x > -600, std::to_string(on.beam_end.x));  // on past both
  CHECK_MSG(g->s().entities[size_t(g->s().monsters[0].entity)].health < monster,
            std::to_string(g->s().entities[size_t(g->s().monsters[0].entity)].health));
  CHECK_MSG(g->p().health < player, std::to_string(g->p().health));
  const sv::Q2EntityState st = sv::q2_entity_state(g->s(), sv::map_row(laser));
  CHECK(st.present && st.model[0] == 1 && st.solid == 0);
  CHECK_MSG(std::abs(float(st.old_origin[0]) - on.beam_end.x * 8) <= 1,
            std::to_string(st.old_origin[0]));
}

TEST(a_clock_counts_down_on_its_string_then_fires_its_path_target) {
  auto g = open_game({{{"classname", "func_clock"},
                       {"target", "text"},
                       {"pathtarget", "lamp"},
                       {"count", "2"},
                       {"spawnflags", "2"},  // down
                       {"style", "1"}},
                      {{"classname", "target_string"}, {"targetname", "text"}},
                      {{"classname", "light"}, {"targetname", "lamp"}, {"style", "33"}}});
  const int text = g->named("text");
  run(*g, kSecond / 2);
  CHECK(g->at(text).message.empty());  // it starts a second in
  run(*g, kSecond);
  CHECK_MSG(g->at(text).message == " 0:02", g->at(text).message);
  run(*g, kSecond);
  CHECK_MSG(g->at(text).message == " 0:01", g->at(text).message);
  CHECK(g->s().world.lightstyles[33] == "m");
  run(*g, kSecond);
  CHECK_MSG(g->at(text).message == " 0:00", g->at(text).message);
  CHECK(g->s().world.lightstyles[33] == "a");  // fired with the last text
  run(*g, kSecond * 2);
  CHECK(g->at(text).message == " 0:00");  // and it stopped
}

TEST(an_elevator_sends_its_train_to_the_callers_corner_and_waits_for_it) {
  auto g =
      open_game({{{"classname", "trigger_relay"},
                  {"targetname", "up"},
                  {"target", "lift"},
                  {"pathtarget", "floor2"}},
                 {{"classname", "trigger_relay"},
                  {"targetname", "back"},
                  {"target", "lift"},
                  {"pathtarget", "floor1"}},
                 {{"classname", "trigger_elevator"}, {"targetname", "lift"}, {"target", "car"}},
                 {{"classname", "func_train"},
                  {"targetname", "car"},
                  {"model", "*4"},
                  {"target", "floor1"},
                  {"speed", "200"},
                  {"wait", "-1"}},
                 {{"classname", "path_corner"},
                  {"targetname", "floor1"},
                  {"origin", "0 0 0"},
                  {"target", "floor2"},
                  {"wait", "-1"}},
                 {{"classname", "path_corner"},
                  {"targetname", "floor2"},
                  {"origin", "0 100 0"},
                  {"target", "floor1"},
                  {"wait", "-1"}}});
  run(*g, kSecond / 2);
  const int car = g->named("car");
  CHECK_MSG(g->at(car).origin.y == 8, std::to_string(g->at(car).origin.y));  // at floor 1
  sv::map_use_targets(g->s(), g->named("up"), 0);
  sv::map_use_targets(g->s(), g->named("back"), 0);  // busy: not heard
  run(*g, kSecond * 2);
  CHECK_MSG(g->at(car).origin.y == 108, std::to_string(g->at(car).origin.y));
  sv::map_use_targets(g->s(), g->named("back"), 0);
  run(*g, kSecond * 2);
  CHECK_MSG(g->at(car).origin.y == 8, std::to_string(g->at(car).origin.y));
}

TEST(a_monster_sets_off_a_trigger_meant_for_monsters_and_a_player_does_not) {
  auto g = open_game({{{"classname", "trigger_multiple"},
                       {"model", "*2"},
                       {"spawnflags", "3"},  // MONSTER, NOT_PLAYER
                       {"target", "lamp"}},
                      {{"classname", "light"}, {"targetname", "lamp"}, {"style", "34"}},
                      {{"classname", "monster_soldier_light"}, {"origin", "312 0 24"}}});
  run(*g, kSecond);
  CHECK(g->s().world.lightstyles[34] == "a");  // the monster stood in it
}

TEST(a_power_cube_hidden_until_used_then_never_taken_and_other_items_lose_their_flags) {
  auto g =
      open_game({relay("go", "cube"),
                 {{"classname", "key_power_cube"},
                  {"targetname", "cube"},
                  {"origin", "-560 0 16"},
                  {"spawnflags", "3"}},
                 {{"classname", "item_health"}, {"origin", "-900 0 16"}, {"spawnflags", "2"}}});
  const int cube = g->find("key_power_cube");
  CHECK(g->at(cube).hidden);
  CHECK((g->at(g->find("item_health")).spawnflags & 2) == 0);
  bool told = false;
  for (const std::string& line : g->s().console)
    if (line.find("only a power cube") != std::string::npos) told = true;
  CHECK(told);
  run(*g, 10);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(!g->at(cube).hidden);
  g->s().players[0].move.origin[0] = -560 * 8;  // onto it
  run(*g, kSecond);
  CHECK(!g->at(cube).hidden && !g->at(cube).gone);  // lying there, untouchable
}

TEST(a_train_stopped_at_a_wait_minus_one_corner_never_moves_again) {
  auto g = open_game(
      {relay("go", "car"),
       {{"classname", "func_train"},
        {"targetname", "car"},
        {"model", "*4"},
        {"target", "c1"},
        {"speed", "200"}},
       {{"classname", "path_corner"}, {"targetname", "c1"}, {"origin", "0 0 0"}, {"target", "c2"}},
       {{"classname", "path_corner"},
        {"targetname", "c2"},
        {"origin", "0 100 0"},
        {"target", "c1"},
        {"wait", "-1"}}});
  run(*g, kSecond / 2);
  const int car = g->named("car");
  sv::map_use_targets(g->s(), g->named("go"), 0);
  run(*g, kSecond * 2);
  CHECK_MSG(g->at(car).origin.y == 108, std::to_string(g->at(car).origin.y));
  sv::map_use_targets(g->s(), g->named("go"), 0);
  run(*g, kSecond * 2);
  CHECK_MSG(g->at(car).origin.y == 108, std::to_string(g->at(car).origin.y));
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
  for (const sv::Q2Entity& e : g->s().entities)
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
  for (int n = 0; n < 8; n++) sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(g->s().monsters_total == 8);
  bool told = false;
  for (const std::string& line : g->s().console)
    if (line.find("has made all 8") != std::string::npos) told = true;
  CHECK(told);
}

TEST(a_spawner_with_a_speed_throws_along_its_angles_facing_north_and_throws_body_parts) {
  auto g = open_game({relay("go", "make"),
                      relay("arm", "limb"),
                      {{"classname", "target_spawner"},
                       {"targetname", "make"},
                       {"target", "monster_soldier_light"},
                       {"origin", "400 0 24"},
                       {"angle", "-1"},  // up
                       {"speed", "300"}},
                      {{"classname", "target_spawner"},
                       {"targetname", "limb"},
                       {"target", "misc_gib_arm"},
                       {"origin", "300 0 100"},
                       {"angle", "90"},
                       {"speed", "100"}}});
  run(*g, kSecond);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  const sv::Q2Monster& m = g->s().monsters[0];
  CHECK_MSG(m.velocity.z == 300 && m.velocity.x == 0, std::to_string(m.velocity.z));
  CHECK(g->at(m.entity).angles.y == 0);
  sv::map_use_targets(g->s(), g->named("arm"), 0);
  const sv::Q2Gib* arm = nullptr;
  for (const sv::Q2Gib& c : g->s().gibs)
    if (c.live) arm = &c;
  CHECK(arm != nullptr);
  CHECK(g->s().models[size_t(arm->model)] == "models/objects/gibs/arm/tris.md2");
  CHECK_MSG(arm->velocity.y > 99 && arm->velocity.y < 101, std::to_string(arm->velocity.y));
}

TEST(a_prisoner_never_fights_back_nor_counts_and_a_crucified_one_hangs) {
  auto g = open_game({{{"classname", "misc_insane"}, {"origin", "-400 0 24"}, {"angle", "180"}},
                      {{"classname", "misc_insane"},
                       {"origin", "-300 0 100"},
                       {"spawnflags", "8"},
                       {"angle", "180"}}});
  run(*g, 3 * kSecond);
  CHECK(g->s().monsters_total == 0);
  CHECK(g->p().health == 100);
  int walker = -1, hung = -1;
  for (size_t i = 0; i < g->s().entities.size(); i++)
    if (g->s().entities[i].classname == "misc_insane")
      (walker < 0 ? walker : hung) = static_cast<int>(i);
  CHECK_MSG(g->at(hung).origin.z == 100, std::to_string(g->at(hung).origin.z));
  CHECK(sv::monster_damage(g->s(), walker, 0, 60, {1, 0, 0}));
  run(*g, kSecond);
  CHECK(g->p().health == 100);  // hurt, and still no fight in it
  CHECK(sv::monster_damage(g->s(), walker, 0, 200, {1, 0, 0}));
  CHECK(g->s().monsters_killed == 0);
}

TEST(a_turret_driver_aims_its_gun_and_fires) {
  auto g =
      open_game({{{"classname", "turret_breach"},
                  {"targetname", "gun"},
                  {"target", "muzzle"},
                  {"model", "*4"},
                  {"angle", "140"}},
                 {{"classname", "info_notnull"}, {"targetname", "muzzle"}, {"origin", "24 0 48"}},
                 {{"classname", "turret_driver"},
                  {"target", "gun"},
                  {"origin", "40 48 24"},
                  {"angle", "180"}}});
  CHECK(g->s().monsters_total == 1);
  const int gun = g->find("turret_breach");
  run(*g, 2 * kSecond);
  // Turned from 140 toward the player in the west, at up to 50 degrees a second.
  CHECK_MSG(g->at(gun).angles.y > 160, std::to_string(g->at(gun).angles.y));
  run(*g, 6 * kSecond);
  CHECK_MSG(g->at(gun).angles.y > 175 && g->at(gun).angles.y < 185,
            std::to_string(g->at(gun).angles.y));
  CHECK_MSG(g->p().health < 100, std::to_string(g->p().health));
}

TEST(a_target_explosion_waits_its_delay_then_blasts_and_fires_its_targets_together) {
  auto g = open_game({{{"classname", "trigger_always"}, {"target", "boom"}},
                      {{"classname", "target_explosion"},
                       {"targetname", "boom"},
                       {"origin", "0 0 24"},
                       {"delay", "1"},
                       {"target", "won"}},
                      {{"classname", "target_goal"}, {"targetname", "won"}}});
  run(*g, kSecond / 2);
  CHECK(g->s().goals_found == 0);
  bool flash = false;
  for (int i = 0; i < kSecond && !flash; i++) {
    g->s().out.events.clear();
    run(*g, 1);
    for (const sv::NetEvent& ev : g->s().out.events)
      if (ev.text == "te_explosion") flash = true;
    if (flash) CHECK_MSG(g->s().goals_found == 1, "the targets fire with the blast");
  }
  CHECK(flash && g->s().goals_found == 1);
}

TEST(a_maps_backslash_n_is_a_new_line_and_a_speaker_keeps_its_volume_and_reach) {
  auto g = open_game({{{"classname", "trigger_always"}, {"target", "say"}},
                      help("say", "first\\nsecond"),
                      {{"classname", "trigger_always"}, {"target", "horn"}},
                      {{"classname", "target_speaker"},
                       {"targetname", "horn"},
                       {"origin", "0 0 24"},
                       {"noise", "world/klaxon2.wav"},
                       {"attenuation", "-1"},
                       {"volume", "0.5"}}});
  bool heard = false;
  for (int i = 0; i < kSecond && !heard; i++) {
    g->s().out.events.clear();
    run(*g, 1);
    for (const sv::NetEvent& ev : g->s().out.events)
      if (ev.kind == sv::EventKind::sound && ev.text == "world/klaxon2.wav") {
        heard = true;
        CHECK_MSG(ev.values[0] == 127.5f && ev.values[1] == 0, "half volume, heard everywhere");
      }
  }
  CHECK(heard);
  CHECK_MSG(g->s().help[1] == "first\nsecond", g->s().help[1]);
}

TEST(an_items_first_touch_fires_its_targets_once) {
  auto g =
      open_game({{{"classname", "item_armor_shard"}, {"origin", "-600 0 24"}, {"target", "won"}},
                 {{"classname", "target_goal"}, {"targetname", "won"}}});
  run(*g, kSecond);
  CHECK_MSG(g->s().goals_found == 1, "the shard under Ana's start fired its target");
}

TEST(placed_decorations_banner_waves_the_dish_turns_once_per_use_and_a_placed_gib_falls) {
  auto g = open_game(
      {relay("go", "dish"),
       {{"classname", "misc_banner"}, {"origin", "-300 100 24"}},
       {{"classname", "misc_satellite_dish"}, {"targetname", "dish"}, {"origin", "300 100 24"}},
       {{"classname", "light_mine2"}, {"origin", "0 200 80"}},
       {{"classname", "misc_gib_head"}, {"origin", "100 -100 100"}}});
  run(*g, 10);
  const auto row = [&](const char* classname) -> const sv::WorldEntity& {
    return g->s().world.entities[size_t(sv::map_row(g->find(classname)))];
  };
  const auto model = [&](const char* classname) {
    return g->s().models[size_t(row(classname).modelindex)];
  };
  CHECK(model("misc_banner") == "models/objects/banner/tris.md2");
  CHECK(model("light_mine2") == "models/objects/minelite/light2/tris.md2");
  CHECK(!g->at(g->find("misc_banner")).box && g->at(g->find("light_mine2")).box);
  const int wave = row("misc_banner").frame;
  run(*g, 8);  // a tenth: the next frame of 16
  CHECK(row("misc_banner").frame == (wave + 1) % 16 || row("misc_banner").frame == (wave + 2) % 16);
  CHECK(row("misc_satellite_dish").frame == 0);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  run(*g, 2 * kSecond);
  CHECK(row("misc_satellite_dish").frame > 10 && row("misc_satellite_dish").frame < 38);
  run(*g, 3 * kSecond);
  CHECK(row("misc_satellite_dish").frame == 38);  // and there it stays
  const sv::Q2Gib* head = nullptr;
  for (const sv::Q2Gib& c : g->s().gibs)
    if (c.live) head = &c;
  CHECK(head != nullptr &&
        g->s().models[size_t(head->model)] == "models/objects/gibs/head/tris.md2");
  CHECK(g->at(g->find("misc_gib_head")).gone);
}
