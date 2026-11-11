// THE SEQUEL'S RULES AS A RUNNER: the second program form behind `RulesRunner`
// (rules_runner.hpp), a C++ server for the sequel's maps (version 38, brushes). It keeps the parts
// every runner keeps (`RulesCommon`) and its own: the map's brushes and their trace, the entities
// the map's text spawns, and each player's packed move state (plugins/shooter/sequel_move.hpp).
//
// - **Time**: the rules run a frame every 100 ms of level time, the frame number × 0.1. The host
//   calls `frame` at its own tick (`sv.set.mintic` of host time a call); a rules frame runs each
//   time the host's clock reaches the next 100 ms. Commands run at once, as they arrive: a
//   packet's newest command, and the commands before it that a lost packet left unrun (up to the
//   two it carries).
// - **Players** spawn 9 above a spawn point and 1 higher (deathmatch: an `info_player_deathmatch`
//   that is not one of the two nearest to a living player, all allowed with two or fewer; else
//   the first `info_player_start`), facing its yaw through the delta angles, with health 100. A
//   deathmatch respawn holds the player still for 14 × 8 ms. `kill` is refused within 5 s of
//   spawning; a dead player may respawn 1 s after dying by pressing attack.
// - **The world** (rules_world.hpp) is filled at the start and after every call: entity slot 0
//   the world, slots 1..max clients the players, then the map's entities; the model list is the
//   map's brush models (`*1` ...) and the player's model.
//
// Items, weapons, damage, movers and monsters are the next rows (docs/goals/M18t.md, M18t4); the
// rules' numbers are in the clean-room notes. Held by tests/shooters_edge_sequel_server_test.cpp.
//
// ge:summary The sequel's rules as a second runner: its maps' brushes, spawns and the 10 Hz frame
// ge:keywords sequel sequel-server sequel-runner runner-calls program-form spawn-point respawn
// ge:keywords deathmatch level-time frame-100ms brush-map sequel_runner sequel_of
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_server_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/prng.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace ge::brush {
struct Map;
class BrushTracer;
}  // namespace ge::brush

namespace shooters_edge::server {

// One of the map's entities, as its text placed it.
struct SequelEntity {
  std::string classname, targetname, target, model;
  sh::Vec3f origin, angles;
  int modelindex = 0;  // `*n` brush models
};

struct SequelPlayer {
  sh::SequelState move;
  uint32_t last_sequence = 0;
  bool alive = false;
  float health = 0;
  float frags = 0;
  double spawned_at = 0, died_at = 0;  // level time, s
  uint8_t buttons = 0;                 // the last command's
  int16_t cmd_angles[3] = {};          // the last command's view, 65536ths of a turn
  bool changed = false;                // placed by the rules since its last move (a spawn)
  // The view the rules turned him to (a spawn), 65536ths of a turn, until his client sends it back
  // as its own (with `SequelServerOptions::clients_take_view`): the delta angles that turned him
  // are then spent (zeroed), so the turn is not added twice.
  bool forced = false;
  int16_t forced_angles[3] = {};
  sh::Vec3f view_angles;
  sh::Vec3f mins, maxs;
  float view_height = 0;
  int water_level = 0;
  bool on_ground = false;
};

struct SequelServerOptions {
  uint32_t seed = 1;
  // The clients take the server's view as their own when it turns them (the classic protocol's
  // clients: the window's listen server and the classic endpoint), so a turn's delta angles are
  // spent once a client sends the turned view back (`SequelPlayer::forced`). False: the sequel's
  // own model, where a client keeps its angles and the delta stays.
  bool clients_take_view = false;
};

// The player's model, last in the model list.
inline constexpr const char* kSequelPlayerModel = "players/male/tris.md2";

struct SequelServer : RulesCommon {
  SequelServerOptions opt;
  std::shared_ptr<const ge::brush::Map> map;
  std::unique_ptr<ge::brush::BrushTracer> tracer;
  std::vector<SequelEntity> entities;
  SequelPlayer players[sh::kMaxClients];
  ge::core::Rng rng = ge::core::make_rng(1);
  int64_t frame_number = 0;
  double level_time = 0;  // frame_number × 0.1, s
  double host_time = 0;   // the host's clock, s
  int next_userid = 1;

  SequelServer();
  ~SequelServer();
};

std::unique_ptr<SequelServer> make_sequel_server(const SequelServerOptions& opt);

// The sequel's server as a runner, owning it (form "sequel").
RulesRunner sequel_runner(std::unique_ptr<SequelServer> s);
// The sequel's server behind a runner, or null for another form.
SequelServer* sequel_of(const RulesRunner& r);

}  // namespace shooters_edge::server
