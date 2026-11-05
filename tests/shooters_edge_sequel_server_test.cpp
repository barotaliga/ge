// Tests: the sequel's rules as a runner (games/shooters_edge/server/sequel_server.hpp) over a
// brush world written as a version 38 map. A map without brushes is refused with the fix; the
// model list is the brush models and the player's; a player spawns 10 above a spot facing its
// yaw, settles an eighth above the floor and walks where it looks; the rules' frame runs every
// 100 ms of host time; a lost packet's commands run from the next one; `kill` is refused for
// 5 s; a dead player respawns by attack after 1 s, held still by the teleport hold; a second
// player avoids the spots nearest the first; the runner is found behind its handle; and a
// scripted match holds golden (tests/golden/shooters_edge_sequel_server.txt; rewrite it with
// GE_GOLDEN_WRITE=1 when the rules deliberately change, and say why).
//
// ge:summary Tests: the sequel's runner: map start, spawns, walking, the 10 Hz frame, kill, golden
// ge:keywords test sequel sequel-server sequel-runner spawn-point respawn kill frame-100ms golden
// ge:skill shooter testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "golden.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

constexpr const char* kGolden = "tests/golden/shooters_edge_sequel_server.txt";

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A walled hall 1024 square with a floor at 0, a platform (`*1`), four deathmatch spots near the
// corners and a start in the middle.
std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(2);
  auto& m = w.models[0];
  m.push_back(box(-512, -512, -16, 512, 512, 0));
  m.push_back(box(-528, -512, -16, -512, 512, 256));
  m.push_back(box(512, -512, -16, 528, 512, 256));
  m.push_back(box(-528, 512, -16, 528, 528, 256));
  m.push_back(box(-528, -528, -16, 528, -512, 256));
  w.models[1].push_back(box(256, 256, 0, 320, 320, 8));
  w.entities.push_back(entity({{"classname", "func_plat"}, {"model", "*1"}}));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "45"}}));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "400 -400 24"}, {"angle", "135"}}));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "400 400 24"}, {"angle", "225"}}));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "-400 400 24"}, {"angle", "315"}}));
  w.entities.push_back(entity({{"classname", "info_player_start"}, {"origin", "0 0 24"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  // The writer leaves the world's own block bare: give it a title.
  if (!map->entities.empty()) map->entities[0].pairs.emplace_back("message", "The Hall");
  return map;
}

sv::RulesRunner start(uint32_t seed = 7, const char* deathmatch = "1") {
  sv::SequelServerOptions o;
  o.seed = seed;
  sv::RulesRunner r = sv::sequel_runner(sv::make_sequel_server(o));
  r.console(std::string("deathmatch ") + deathmatch);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(r.start(map, error), error);
  return r;
}

sv::SequelServer& server(const sv::RulesRunner& r) { return *sv::sequel_of(r); }

// One host tick: each slot's packet carries `c` as its newest command.
struct Driver {
  sv::RulesRunner& r;
  uint32_t sequence[sh::kMaxClients] = {};
  void tick(const std::vector<std::pair<int, sh::MoveCmd>>& cmds) {
    std::vector<sh::Packet> packets;
    for (const auto& [slot, c] : cmds) {
      sh::Packet p;
      p.client = slot;
      p.sequence = ++sequence[slot];
      p.cmds[0] = p.cmds[1] = p.cmds[2] = c;
      packets.push_back(p);
    }
    r.frame(packets.data(), static_cast<int>(packets.size()));
  }
};

sh::MoveCmd cmd(int16_t forward = 0, float yaw = 0, uint8_t buttons = 0) {
  sh::MoveCmd c;
  c.msec = 13;
  c.forward = forward;
  c.angles = {0, yaw, 0};
  c.buttons = buttons;
  return c;
}

float z_of(const sv::SequelPlayer& p) { return float(p.move.origin[2]) * 0.125f; }
float x_of(const sv::SequelPlayer& p) { return float(p.move.origin[0]) * 0.125f; }

TEST(a_map_without_brushes_is_refused_with_the_fix) {
  sv::RulesRunner r = sv::sequel_runner(sv::make_sequel_server({}));
  sv::MapSpec map;
  map.name = "bare";
  std::string error;
  CHECK(!r.start(map, error));
  CHECK(error.find("bare") != std::string::npos);
  CHECK(error.find("read_bsp") != std::string::npos);
}

TEST(the_world_lists_the_brush_models_and_the_player) {
  sv::RulesRunner r = start();
  const sv::RulesWorld& w = r->world;
  CHECK(std::string(r.calls->form) == "sequel");
  CHECK(w.map_name == "hall" && w.map_title == "The Hall");
  CHECK(w.models.size() == 3 && w.models[0].empty() && w.models[1] == "*1");
  CHECK(w.models[2] == sv::kSequelPlayerModel);
  // Slot 0 the world, then the players, then the map's six entities.
  CHECK(w.entities.size() == size_t(1 + sh::kMaxClients + 6));
  CHECK(w.entities[1 + sh::kMaxClients].modelindex == 1);
  CHECK(w.entities[2 + sh::kMaxClients].origin.x == -400);
}

TEST(a_player_spawns_ten_above_a_spot_facing_its_yaw_and_settles) {
  sv::RulesRunner r = start();
  r.connect(0, "one", false, false);
  const sv::SequelPlayer& p = server(r).players[0];
  CHECK(p.alive && p.health == 100);
  CHECK(z_of(p) == 34);
  CHECK(r->world.clients[0].active && r->world.clients[0].name == "one");
  CHECK(!r->world.entities[1].free && r->world.entities[1].modelindex == 2);
  Driver d{r};
  for (int i = 0; i < 60; i++) d.tick({{0, cmd()}});
  CHECK(z_of(p) == 24.125f);
  CHECK(p.on_ground && (r->world.entities[1].flags & sh::kFlagOnGround));
  // The spot's yaw is a multiple of 45: the view looks along it with the command's yaw 0.
  const float yaw = p.view_angles.y;
  CHECK(yaw == 45 || yaw == 135 || yaw == -135 || yaw == -45 || yaw == 225 || yaw == 315);
}

TEST(a_player_walks_where_it_looks) {
  sv::RulesRunner r = start(7, "0");  // not deathmatch: the start in the middle, yaw 0
  r.connect(0, "one", false, false);
  Driver d{r};
  for (int i = 0; i < 20; i++) d.tick({{0, cmd()}});
  const float x0 = x_of(server(r).players[0]);
  for (int i = 0; i < 40; i++) d.tick({{0, cmd(400)}});
  const sv::SequelPlayer& p = server(r).players[0];
  CHECK(x_of(p) > x0 + 100);
  CHECK(float(p.move.velocity[0]) * 0.125f <= 300);
  CHECK(p.move.origin[1] == 0);
}

TEST(the_rules_frame_runs_every_100_ms_of_host_time) {
  sv::RulesRunner r = start();
  Driver d{r};
  for (int i = 0; i < 100; i++) d.tick({});  // 1.3 s of host time at 13 ms a tick
  CHECK(server(r).frame_number == 13);
  CHECK(r->world.time == 13 * 0.1);
}

TEST(a_lost_packets_commands_run_from_the_next) {
  sv::RulesRunner a = start(7, "0"), b = start(7, "0");
  a.connect(0, "one", false, false);
  b.connect(0, "one", false, false);
  // `a` gets every packet; `b` loses every other one, whose command the next packet carries.
  uint32_t seq = 0;
  for (int i = 0; i < 40; i++) {
    sh::Packet p;
    p.client = 0;
    p.sequence = ++seq;
    p.cmds[0] = cmd(i >= 2 ? 400 : 0);
    p.cmds[1] = cmd(i >= 1 ? 400 : 0);
    p.cmds[2] = cmd(400);
    a.frame(&p, 1);
    if (i % 2 == 1 || i == 0)
      b.frame(&p, 1);
    else
      b.frame(nullptr, 0);
  }
  CHECK(server(a).players[0].move == server(b).players[0].move);
}

TEST(kill_is_refused_for_five_seconds_then_a_dead_player_respawns_by_attack) {
  sv::RulesRunner r = start();
  r.connect(0, "one", false, false);
  Driver d{r};
  sv::SequelPlayer& p = server(r).players[0];
  for (int i = 0; i < 100; i++) d.tick({{0, cmd()}});  // 1.3 s
  CHECK(r.client_command(0, "kill") == sv::CommandResult::mod);
  CHECK(p.alive && p.frags == 0);
  while (server(r).level_time < 5.0) d.tick({{0, cmd()}});
  r.client_command(0, "kill");
  CHECK(!p.alive && p.frags == -1 && r->world.clients[0].numbers.frags == -1);
  const double died = p.died_at;
  // Attack held at once does nothing for a second.
  while (server(r).level_time < died + 0.85) d.tick({{0, cmd(0, 0, sh::kButtonAttack)}});
  CHECK(!p.alive);
  while (server(r).level_time < died + 1.05) d.tick({{0, cmd(0, 0, sh::kButtonAttack)}});
  CHECK(p.alive && p.health == 100);
  CHECK((p.move.flags & sh::kSqTimeTeleport) != 0);
  CHECK(r.client_command(0, "fly") == sv::CommandResult::bad);
}

TEST(a_second_player_avoids_the_spots_nearest_the_first) {
  for (uint32_t seed = 1; seed <= 20; seed++) {
    sv::RulesRunner r = start(seed);
    r.connect(0, "one", false, false);
    const sh::Vec3f first = r->world.entities[1].origin;
    r.connect(1, "two", false, false);
    const sh::Vec3f second = r->world.entities[2].origin;
    // The nearest spot is the first player's own; the next two are the neighbouring corners, one
    // of which is excluded: the second spawns across the hall or beside it, never on the first.
    CHECK(!(first.x == second.x && first.y == second.y));
  }
}

TEST(the_runner_is_found_behind_its_handle) {
  sv::RulesRunner r = start();
  CHECK(sv::sequel_of(r) != nullptr && sv::qc_of(r) == nullptr);
  sv::RulesRunner moved = std::move(r);
  CHECK(sv::sequel_of(moved) != nullptr);
  CHECK(moved.console("sv_gravity 400"));
  CHECK(moved->cvars.find("sv_gravity")->number == 400);
  CHECK(!moved.console("no_such_setting 1"));
  CHECK(moved.console("map other") && moved->pending_map == "other");
  CHECK(moved.pause(0, true) && !moved.pause(0, true) && moved->world.paused);
}

TEST(a_scripted_match_holds_golden) {
  sv::RulesRunner r = start(38);
  r.connect(0, "one", false, false);
  r.connect(1, "two", false, true);
  Driver d{r};
  std::string table =
      "# two players' origin velocity flags per 0.5 s (shooters_edge_sequel_server_test)\n";
  std::vector<std::vector<uint64_t>> rows;
  for (int i = 0; i < 1200; i++) {
    const float yaw0 = float((i * 7) % 360), yaw1 = float(360 - (i * 5) % 360);
    const auto f0 = int16_t(i % 300 < 200 ? 400 : -200), f1 = int16_t(i % 170 < 120 ? 300 : 0);
    const uint8_t b0 = i % 150 == 0 ? sh::kButtonAttack : 0;
    d.tick({{0, cmd(f0, yaw0, b0)}, {1, cmd(f1, yaw1, sh::kButtonAttack)}});
    if (i == 500) r.client_command(0, "kill");
    if (i % 38 == 37) {
      std::vector<uint64_t> row;
      row.reserve(14);
      for (int slot = 0; slot < 2; slot++) {
        const sh::SequelState& s = server(r).players[slot].move;
        for (int k = 0; k < 3; ++k) row.push_back(uint16_t(s.origin[k]));
        for (int k = 0; k < 3; ++k) row.push_back(uint16_t(s.velocity[k]));
        row.push_back(s.flags);
      }
      std::string line;
      char word[8];
      for (const uint64_t v : row) {
        std::snprintf(word, sizeof word, "%04llx ", static_cast<unsigned long long>(v));
        line += word;
      }
      line.back() = '\n';
      table += line;
      rows.push_back(row);
    }
  }
  CHECK(server(r).players[0].frags == -1);
  const char* write = std::getenv("GE_GOLDEN_WRITE");
  if (write && *write) {
    std::ofstream(ge::test::root_path(kGolden)) << table;
    std::printf("  wrote %s\n", kGolden);
    return;
  }
  const auto golden = ge::test::golden_rows(kGolden);
  CHECK(golden.size() == rows.size());
  for (size_t k = 0; k < golden.size() && k < rows.size(); ++k) CHECK(golden[k] == rows[k]);
}

}  // namespace
