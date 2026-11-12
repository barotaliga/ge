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
// - **Items** (sequel_items.hpp): spawned from their class names (an entity flagged 0x800 is not
//   spawned in deathmatch; outside it 0x100, 0x200 and 0x400 leave it out at skill 0, 1 and 2-3),
//   dropped up to 128 units to the floor (one that starts in a solid is removed), taken when a
//   living player's box touches theirs (−15..15), hidden in deathmatch until their respawn time and
//   removed otherwise. A mega health counts its taker down a point a second above his maximum,
//   from 5 s after, and shows again 20 s after he is back to it.
//
// - **Weapons and damage** (sequel_combat.hpp, sequel_fight.cpp): each player holds a weapon whose
//   frames step once a rules frame; a number key (impulse 1..10) or `use <name>` chooses one, and
//   a newly owned weapon is taken up at once when the blaster is in hand (always outside
//   deathmatch). Bullets, pellets and the rail are lines; bolts, grenades, rockets and the BFG's
//   ball fly, moved each rules frame. A blow pushes (the rocket jump), is shared with the armour
//   and takes the rest from health; at 0 the player dies and the obituary scores (+1 the killer,
//   −1 a suicide or the world's kill) and is printed to everyone.
//
// - **The rest of the fight**: power armour (the screen guards hits in front, a third of a blow at
//   a cell a point; the shield all round, two thirds at two points a cell), the powerups used by
//   `use <name>` (or at once with `dmflags` 16): quad damage ×4, invulnerability, the silencer,
//   the rebreather and the suit; drowning after 12 s under, lava and slime each frame, falling
//   from the change of the fall speed; `fraglimit` and `timelimit` end the level.
//
// The hand grenade, dropping at death, movers and monsters are the next rows
// (docs/goals/M18v.md, M18v7); the rules' numbers are in the clean-room notes. Held by
// tests/shooters_edge_sequel_server_test.cpp.
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
#include "shooters_edge/server/sequel_combat.hpp"
#include "shooters_edge/server/sequel_items.hpp"

namespace ge::brush {
struct Map;
class BrushTracer;
}  // namespace ge::brush

namespace shooters_edge::server {

// One of the map's entities, as its text placed it.
struct SequelEntity {
  std::string classname, targetname, target, model;
  sh::Vec3f origin, angles;
  int modelindex = 0;  // `*n` brush models; an item's model in the model list
  int spawnflags = 0;
  // An item (sequel_items.hpp's row, -1 none): dropped to the floor at the start, taken by a
  // player's touch, hidden until its respawn in deathmatch, gone otherwise.
  int item = -1;
  bool hidden = false, gone = false;
  double respawn_at = 0;  // level time it shows again; 0: not waiting
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
  SqBody body;             // health, armour, ammo and what he carries (sequel_items.hpp)
  int mega = -1;           // the mega health's entity he took, while it counts down; -1 none
  double drained_at = 0;   // level time the mega's count-down last took a point
  std::vector<int> taken;  // the item rows he took, in order (the HUD's pickups, the tests)
  sh::Vec3f mins, maxs;
  float view_height = 0;
  int water_level = 0;
  int32_t water_type = 0;  // the liquid he is in (contents bits)
  bool on_ground = false;
  // The weapon in hand (sequel_combat.hpp), stepped once a rules frame: by the first command of the
  // frame that presses attack, else at the frame's end (`stepped` says which).
  SqHand hand;
  bool stepped = false;
  int killer = -1;  // the slot that killed him last; his own for a suicide, -1 the world
  // The powerups in use (level time each ends; 0 none), the silencer's shots left, power armour
  // switched on, and the body's state the frame reads: his air, the next drowning blow and its
  // size, and his fall speed at the last frame's end.
  double quad_until = 0, invulnerable_until = 0, breather_until = 0, suit_until = 0;
  int silencer_shots = 0;
  bool power_on = false;
  double air_until = 0, drown_at = 0;
  int drown_damage = 2;
  float old_fall = 0;
};

// A shot in flight (a bolt, a grenade, a rocket, the BFG's ball), moved each rules frame.
struct SequelMissile {
  bool live = false;
  SqShot kind = SqShot::bolt;
  int owner = -1;  // the shooter's slot
  int weapon = 0;  // the weapon that fired it (the obituary's words)
  sh::Vec3f origin, velocity;
  double dies_at = 0;  // level time it goes (a grenade's fuse explodes it)
  int damage = 0;      // a direct hit's
  int radius_damage = 0;
  float radius = 0;
  bool resting = false;  // a grenade come to rest on a floor
};
inline constexpr int kSequelMissiles = 64;  // the rows the world keeps for shots in flight

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
  SequelMissile missiles[kSequelMissiles];
  int weapon_models[kSqWeaponCount] = {};  // each weapon's view model in the model list
  int missile_models[7] = {};              // by `SqShot`
  std::vector<std::string> obituaries;     // every death's line, in order (the tests, the lab)

  SequelServer();
  ~SequelServer();
};

std::unique_ptr<SequelServer> make_sequel_server(const SequelServerOptions& opt);

// The sequel's server as a runner, owning it (form "sequel").
RulesRunner sequel_runner(std::unique_ptr<SequelServer> s);
// The sequel's server behind a runner, or null for another form.
SequelServer* sequel_of(const RulesRunner& r);

}  // namespace shooters_edge::server
