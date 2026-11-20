// Quake II's game library hosted unchanged (server/q2_native.hpp), held to the interface by
// a library of our own built as a mod's is (q2_native_fixture_game.cpp): found, loaded and
// started, its lists and links, a player connected, moved and blocked by an entity, its messages,
// a refusal, its commands, a level change asked for, an `error` that stops the map, a spark's way
// by the table of directions, and a turned brush entity traced in its own frame.
//
// ge:summary Tests for hosting Quake II's game library: load, spawn, move, messages, error
// ge:keywords game-library native-host fixture m18v10
// ge:skill testing shooter
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/protocol/directions.hpp"
#include "shooter/client.hpp"
#include "shooters_edge/server/q2_native.hpp"
#include "shooters_edge/server/q2_wire.hpp"

namespace {

namespace sv = shooters_edge::server;
namespace sh = ge::shooter;
namespace br = ge::brush;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi);
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A walled hall with a floor at 0, a platform (`*1`) and one deathmatch spot.
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
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "-200 0 24"}, {"angle", "0"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

#if defined(GE_NATIVE_ROOT)
const std::filesystem::path kRoot = GE_NATIVE_ROOT;
#else
const std::filesystem::path kRoot;
#endif

sv::RulesRunner start() {
  std::filesystem::path lib;
  CHECK(sv::find_native_library(kRoot, "fixturemod", "base", lib));
  sv::NativeQ2Options o;
  o.library = lib;
  o.max_clients = 4;
  o.game = "fixturemod";
  std::string error;
  std::unique_ptr<sv::NativeQ2> s = sv::load_native_q2(o, error);
  CHECK_MSG(s != nullptr, error);
  if (!s) return {};
  s->map_checksum = 1234;
  sv::RulesRunner r = sv::native_q2_runner(std::move(s));
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  CHECK_MSG(r.start(map, error), error);
  return r;
}

sh::MoveCmd cmd(int16_t forward, uint8_t buttons = 0) {
  sh::MoveCmd c;
  c.msec = 13;
  c.forward = forward;
  c.buttons = buttons;
  return c;
}

void ticks(sv::RulesRunner& r, int n, int16_t forward, uint8_t buttons = 0) {
  static uint32_t sequence = 0;
  for (int i = 0; i < n; i++) {
    sh::Packet p;
    p.client = 0;
    p.sequence = ++sequence;
    p.cmds[0] = p.cmds[1] = p.cmds[2] = cmd(forward, buttons);
    r.frame(&p, 1);
  }
}

bool holds(const std::vector<uint8_t>& b, const std::string& text) {
  return std::search(b.begin(), b.end(), text.begin(), text.end()) != b.end();
}

float x_of(const sv::NativeQ2& s) { return s.raw.players[0].origin[0] * 0.125f; }
float z_of(const sv::NativeQ2& s) { return s.raw.players[0].origin[2] * 0.125f; }

TEST(the_library_is_found_in_the_mod_folder_then_the_base) {
  std::filesystem::path lib;
  CHECK(sv::find_native_library(kRoot, "fixturemod", "base", lib));
  CHECK(lib.parent_path().filename() == "fixturemod");
  CHECK(sv::find_native_library(kRoot, "nomod", "fixturemod", lib));  // the base folder's
  CHECK(!sv::find_native_library(kRoot, "nomod", "nobase", lib));
}

TEST(a_missing_library_is_refused_with_its_file_named) {
  sv::NativeQ2Options o;
  o.library = kRoot / "nowhere" / "game.so";
  std::string error;
  CHECK(sv::load_native_q2(o, error) == nullptr);
  CHECK(error.find("nowhere") != std::string::npos);
  CHECK(error.find("did not load") != std::string::npos);
}

TEST(the_level_spawns_with_the_games_lists_and_the_maps_own) {
  sv::RulesRunner r = start();
  sv::NativeQ2* s = sv::native_q2_of(r);
  CHECK(s != nullptr);
  if (s == nullptr) return;
  CHECK(std::string(r.calls->form) == "native");
  const std::vector<std::string>& cs = s->raw.config;
  CHECK(cs[2] == "sky1");                  // the game's
  CHECK(cs[31] == "1234");                 // the map's checksum
  CHECK(cs[33] == "maps/hall.bsp");        // the map itself
  CHECK(cs[34] == "*1");                   // its brush model
  CHECK(cs[288 + 1] == "misc/spark.wav");  // a sound the game listed
  // The brush entity and the box are in the baselines, numbered after the four players.
  CHECK(s->raw.baselines[5].present && s->raw.baselines[5].model[0] == 2);
  CHECK(s->raw.baselines[6].present && s->raw.baselines[6].solid != 0);
  sv::Q2Wire w;
  sv::Q2Signon on;
  sv::q2_signon_raw(w, s->raw, 0, {}, on);
  CHECK(on.baselines.size() == 2);
  bool sky = false;
  for (const auto& [i, b] : on.config_strings) sky = sky || (i == 2 && holds(b, "sky1"));
  CHECK(sky);
}

TEST(a_player_joins_moves_on_the_floor_and_is_stopped_by_the_box) {
  sv::RulesRunner r = start();
  sv::NativeQ2* s = sv::native_q2_of(r);
  if (s == nullptr) return;
  r->clients[0].userinfo = {{"name", "tester"}, {"skin", "male/grunt"}};
  r.connect(0, "tester", false, false, nullptr);
  CHECK(holds(s->raw.reliable[0], "player 1 entered the game"));
  CHECK(s->raw.config[1312] == "tester\\male/grunt");
  const int64_t before = s->frame_number;
  ticks(r, 77, 0);  // a second of host ticks: ten of the game's frames
  CHECK(s->frame_number - before >= 9 && s->frame_number - before <= 11);
  CHECK(s->raw.players[0].stats[1] == 100);
  CHECK_MSG(z_of(*s) > 23 && z_of(*s) < 25, "on the floor: z " + std::to_string(z_of(*s)));
  ticks(r, 154, 400);  // forward (+x) into the box 96 units ahead
  const float x = x_of(*s);
  CHECK_MSG(x > -200 && x < -200 + 96 - 31, "stopped short of the box: x " + std::to_string(x));
  // A frame for the player writes his state and the entities.
  sv::Q2Wire w;
  sv::Q2Signon on;
  sv::q2_signon_raw(w, s->raw, 0, {}, on);
  std::vector<uint8_t> block, rel;
  sv::q2_frame_raw(w, s->raw, static_cast<int32_t>(s->frame_number), block, rel);
  CHECK(!block.empty() && block[0] == 20);  // the frame message
}

// Classic remix's `cr_physics 1` over a mod (`cr_over_mods`): the window's line `set sv_physics
// "q1"` makes the engine's move, which the game library asks for, run Quake's
// ground and air: Quake's running speed (320) outruns Quake II's (300) from the same start.
TEST(sv_physics_q1_moves_a_mods_players_as_quake) {
  float x[2] = {0, 0};
  for (int classic = 0; classic < 2; classic++) {
    sv::RulesRunner r = start();
    sv::NativeQ2* s = sv::native_q2_of(r);
    if (s == nullptr) return;
    if (classic == 1) CHECK(r.console("set sv_physics \"q1\""));
    r->clients[0].userinfo = {{"name", "tester"}, {"skin", "male/grunt"}};
    r.connect(0, "tester", false, false, nullptr);
    ticks(r, 77, 0);
    const float from = x_of(*s);
    ticks(r, 12, 400);
    x[classic] = x_of(*s) - from;
  }
  CHECK_MSG(x[1] > x[0] + 1,
            "classic " + std::to_string(x[1]) + ", Quake II " + std::to_string(x[0]));
}

TEST(attack_sends_a_spark_and_a_sound_to_the_player) {
  sv::RulesRunner r = start();
  sv::NativeQ2* s = sv::native_q2_of(r);
  if (s == nullptr) return;
  r.connect(0, "tester", false, false, nullptr);
  s->raw.datagram[0].clear();
  ticks(r, 1, 0, 1);
  const std::vector<uint8_t>& d = s->raw.datagram[0];
  CHECK(d.size() > 10);
  CHECK(!d.empty() && d[0] == 3);                               // the temporary entity first
  CHECK(std::find(d.begin(), d.end(), uint8_t{9}) != d.end());  // then the sound
  // The spark's way (straight up) as its index into the protocol's table of directions.
  CHECK(d.size() > 8 && ge::classic::protocol::directions()[d[8]][2] == 1.0f);
}

// A brush entity turned by its angles is traced and tested in its own frame: the platform at
// x, y 256..320 turned half round about the world's origin stands at -320..-256.
TEST(a_turned_brush_entity_stops_traces_where_it_has_turned_to) {
  sv::RulesRunner r = start();
  sv::NativeQ2* s = sv::native_q2_of(r);
  if (s == nullptr) return;
  r.connect(0, "tester", false, false, nullptr);
  const auto probe = [&](const char* line) {
    s->raw.reliable[0].clear();
    r.client_command(0, line);
    const std::vector<uint8_t>& b = s->raw.reliable[0];
    const auto at = std::search(b.begin(), b.end(), std::begin("probe"), std::end("probe") - 1);
    return at == b.end() ? std::string() : std::string(at, std::find(at, b.end(), uint8_t{0}));
  };
  CHECK(probe("probe 288 288") == "probe z 8 up 100 inside 1\n");
  CHECK(probe("probe -288 -288") == "probe z 4 up 0 inside 0\n");
  r.client_command(0, "turn 180");
  CHECK(probe("probe 288 288") == "probe z 4 up 0 inside 0\n");
  CHECK(probe("probe -288 -288") == "probe z 8 up 100 inside 1\n");
}

TEST(a_player_the_game_refuses_is_told_why) {
  sv::RulesRunner r = start();
  sv::NativeQ2* s = sv::native_q2_of(r);
  if (s == nullptr) return;
  r->clients[1].userinfo = {{"name", "refuse"}};
  r.connect(1, "refuse", false, false, nullptr);
  CHECK(!r->clients[1].active);
  CHECK(holds(s->raw.reliable[1], "Go away"));
  CHECK(holds(s->raw.reliable[1], "disconnect"));
}

TEST(commands_reach_the_game_and_a_level_change_is_asked_for) {
  sv::RulesRunner r = start();
  sv::NativeQ2* s = sv::native_q2_of(r);
  if (s == nullptr) return;
  r.connect(0, "tester", false, false, nullptr);
  r.client_command(0, "score now please");
  CHECK(holds(s->raw.reliable[0], "your score: 0 (now please)"));
  CHECK(r.console("sv hello there"));
  CHECK(std::find(s->console.begin(), s->console.end(), "hello from the game (3)\n") !=
        s->console.end());
  r.client_command(0, "next");
  CHECK(s->pending_map == "hall2");
  // What the window's game draws: the game's bar program and the layout it sent the player.
  r.client_command(0, "layout");
  ticks(r, 10, 0);
  CHECK(s->world.status_bar == "yb -24 xv 0 hnum");
  CHECK(s->world.clients[0].layout == "xv 32 yv 8 string \"fixture scores\"");
}

TEST(the_games_error_stops_the_map_and_nothing_more_is_called) {
  sv::RulesRunner r = start();
  sv::NativeQ2* s = sv::native_q2_of(r);
  if (s == nullptr) return;
  r.connect(0, "tester", false, false, nullptr);
  r.client_command(0, "crash");
  CHECK(s->map_stopped);
  CHECK(s->stop_reason.find("asked to stop by crash") != std::string::npos);
  const int64_t frame = s->frame_number;
  ticks(r, 20, 0);
  CHECK(s->frame_number == frame);
}

}  // namespace
