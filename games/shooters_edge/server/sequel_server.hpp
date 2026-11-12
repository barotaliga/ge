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
// - **The hand grenade, drops and the level's end**: the hand grenade is lit at frame 11, cooked
//   while attack is held and thrown faster the longer it was held (exploding in the hand at the
//   fuse's end); at death in deathmatch the weapon in hand and (`dmflags` 16384) the quad are
//   dropped, and `drop <name>` throws a weapon, ammo or a powerup, the dropper's again only after
//   1 s, gone after 30 s in deathmatch; a body under −40 is thrown apart; a level's end starts the
//   intermission (everyone frozen at its spot, a button after 5 s leaves for the next map); the
//   `dmflags` bits 1, 2, 2048 and 8192 leave items out, 4 keeps weapons, 32 repeats the level, 512
//   spawns farthest, 1024 respawns at once.
// - **Teams, bodies and scores**: with `dmflags` 64 (by skin) or 128 (by model), or in `coop`,
//   teammates' blows do nothing with 256 but push, and a team kill scores −1; `fov` is held to
//   1..160 (90 with 32768); a respawn leaves the body, up to 8, thrown apart by a blast under −40;
//   `score` lists the scores, which the intermission prints to everyone.
// - **The map's movers and triggers**: sequel_map.hpp. Monsters are the next row (M18v7d).
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

  // The map's movers and triggers (sequel_map.hpp).
  int block = -1;  // its block in the map's entity text; -1 one the rules made (a door's field)
  int owner = -1;  // a field's or a trigger's mover
  std::string message, killtarget, team, pathtarget, noise, map;
  bool solid = false;      // a brush that stops moves and shots (a mover, a wall present)
  bool trigger = false;    // touched when a living player's box overlaps it
  sh::Vec3f mins, maxs;    // its box about its origin (a brush model's bounds, a field's box)
  sh::Vec3f dir;           // a mover's or a push's way
  sh::Vec3f closed, open;  // a door's or button's two places (a plat's top and bottom)
  float speed = 0, accel = 0, decel = 0, wait = 0, lip = 0, delay = 0, random = 0;
  int dmg = 0, health = 0, max_health = 0, sounds = 0;
  int state = 0;             // a mover's: 0 at rest closed, 1 opening, 2 open, 3 closing
  int team_master = -1;      // the first of its team; -1 none
  double think_at = 0;       // level time its next action is due; 0 none
  int think = 0;             // which action (sequel_map.cpp)
  double touched_at = -100;  // the last touch that counted (a field's once a second)
  bool moved = false;        // a plat: it has had a move (its use is spent)
  int activator = -1;        // the slot that set it going (a train's, a delayed use's)
  // The move in progress: its velocity (units/s), the place it is going to and what follows.
  sh::Vec3f velocity, dest;
  int frames = 0;     // the plain move's whole frames left
  float rest = 0;     // the plain move's remainder; the accelerated move's distance left
  bool last = false;  // the next push is the move's last
  bool accelerated = false;
  float now_speed = 0, next_speed = 0, top = 0, decel_span = 0;  // per frame
  int arrive = 0;  // what arriving does (sequel_map.cpp)
  // A turning mover (a rotating door, a rotator): its turn in progress (degrees/s), the angles it
  // is turning to, a door's two angles, and the axis it turns about (a unit vector of angles).
  bool turns = false;
  sh::Vec3f avelocity, adest, closed_angles, open_angles, axis;
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
  bool gibbed = false;      // died under −40: his body is gone (no model)
  int ground = -1;          // the world row he stands on (0 the world, a mover's row), -1 none
  double windfly_at = -10;  // the last push's sound
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

// An item a player dropped (by `drop`, or at death): thrown, falling to rest, taken on touch.
struct SequelDrop {
  bool live = false;
  int item = -1;   // sequel_items.hpp's row
  int count = 0;   // ammo's: how much it gives (0 the item's own amount)
  int owner = -1;  // the dropper's slot, who may take it back only after `takeable_at`
  sh::Vec3f origin, velocity;
  double takeable_at = 0;
  double dies_at = 0;    // level time it vanishes; 0 never
  double quad_left = 0;  // a dropped quad's time left, s
  bool resting = false;  // come to rest on a floor
};
inline constexpr int kSequelDrops = 32;  // the rows the world keeps for dropped items

// A body left where a player died, kept when he respawns: up to 8 stay, the oldest reused. A blow
// that takes it under −40 throws it apart.
struct SequelBody {
  bool live = false;
  sh::Vec3f origin;
  float yaw = 0;
  float health = 0;
};
inline constexpr int kSequelBodies = 8;

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
  int missile_models[kSqShotKinds] = {};   // by `SqShot`
  std::vector<std::string> obituaries;     // every death's line, in order (the tests, the lab)
  SequelDrop drops[kSequelDrops];
  SequelBody bodies[kSequelBodies];
  int next_body = 0;             // the body slot the next respawn fills
  std::vector<int> item_models;  // each item row's world model in the model list (the drops')
  // The intermission: level time it began (0 none) and the map it leads to; everyone stands frozen
  // at its spot, and after 5 s any button leaves.
  double intermission_at = 0;
  std::string next_map;
  // A use put off by its `delay`: the entity whose targets fire, at a level time, by a slot.
  struct Delayed {
    int entity;
    double at;
    int activator;
  };
  std::vector<Delayed> delayed;
  int secrets = 0, secrets_found = 0, goals = 0, goals_found = 0;
  bool exit_now = false;  // an exit was used: the intermission starts at the frame's end
  // The area portals by number, open or shut (a door opens those its target names); what is sent
  // to a client is cut at a shut one once the map's areas are read.
  std::vector<uint8_t> portals;

  SequelServer();
  ~SequelServer();
};

std::unique_ptr<SequelServer> make_sequel_server(const SequelServerOptions& opt);

// The sequel's server as a runner, owning it (form "sequel").
RulesRunner sequel_runner(std::unique_ptr<SequelServer> s);
// The sequel's server behind a runner, or null for another form.
SequelServer* sequel_of(const RulesRunner& r);

}  // namespace shooters_edge::server
