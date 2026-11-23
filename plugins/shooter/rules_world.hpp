// THE RULES' WORLD: what a running shooter game's rules show everyone else, in one shape whatever
// game's rules they are and whatever runs them. A window, the bots, the wire, the scoreboard and
// the recorder read it; only the runner writes it. Each game's runner fills it: Shooter's Edge's QC
// server copies its lists at the map's start and its entities and clients at the end of each
// frame (games/shooters_edge/server/rules_world.hpp), Quake II's own rules fill the same rows,
// and a later game's rules fill them too, so nothing that reads it names a game.
//
// The rows are sized once at the map's start and rewritten in place each frame: the frame's copy
// allocates nothing and calls nothing virtual. An entity row is the entity's slot in the pool
// (row 0 the world, rows 1..32 the players); a free slot keeps its row with `free` set.
//
// ge:summary The rules' world every shooter game's rules fill: lists, clients, a row an entity
// ge:keywords rules-world world-view entity-rows client-rows player-numbers world-shot
// ge:keywords shot-in-flight lightstyles serverinfo statics layout inventory stats
// ge:skill shooter
// ge:test tests/shooters_edge_rules_world_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "shooter/client.hpp"

namespace ge::shooter {

constexpr int kWorldLightStyles = 64;

// What a shot in flight is, for whoever draws its trail, its light and its blast: a rocket, a
// grenade (it bounces and smokes), a nail, a glowing bolt or the big energy ball; none for anything
// else. Every runner names its own shots by these.
enum class WorldShot : uint8_t { none, rocket, grenade, nail, bolt, ball };

// One entity as the world shows it.
struct WorldEntity {
  bool free = true;
  Vec3f origin, angles, velocity;
  Vec3f mins, maxs;
  Vec3f absmin, absmax;  // the box in the world, as the world links it
  Vec3f view_angles;     // a player's view, degrees
  int flags = 0;         // the loop's flags (on the ground, in water, ...)
  int waterlevel = 0;    // 0 dry .. 3 under
  int modelindex = 0;    // the model list's index; 0 none
  int frame = 0, skin = 0, colormap = 0, effects = 0;
  // It turns about its yaw on the viewer's own clock, 100° a second (an item lying about), its own
  // angles set aside: what a runner whose `effects` are its own game's bits says to every reader.
  bool turns = false;
  // Quake II's render bits (its items' glow); 0 under other rules.
  int render_fx = 0;
  // A beam's far end (Quake II's beam bit in `render_fx`, a laser's): sent as its old origin, the
  // beam drawn from `origin` to it.
  Vec3f beam_end;
  // Quake II's one-frame entity event (1 an item back, 6 a player arrived); 0 none. Set only on
  // the frame it happens: the wire sends it once, and a viewer throws its burst once.
  int event = 0;
  // Quake II's loop the entity carries (a rocket's roar, a door grinding, a pad's hum): the sound
  // list's index, heard every frame it is set; 0 none.
  int loop = 0;
  // A second model drawn with it, at its place and on its frame (a player's carried weapon): the
  // model list's index; 0 none.
  int carried_model = 0;
  // A box that stops moves (Quake II's teleporter discs): `mins`..`maxs` is sent as its solid,
  // so a client predicts against it.
  bool solid_box = false;
  // An item taken and waiting to come back: the seconds until it shows again and the model it will
  // show (`back_in` -1 and `back_model` 0: none, or the rules cannot say). Read by a viewer that
  // draws it coming back ahead of time (classicenhance/hologram.hpp).
  float back_in = -1;
  int back_model = 0;
  float alpha = 1;                // 1 opaque (the extension field; 1 when the program has none)
  float scale = 1;                // the model's size (the extension field; 0 in it reads 1)
  float colormod[3] = {1, 1, 1};  // the tint (the extension field; '0 0 0' in it reads 1 1 1)
  float health = 0;
  WorldShot shot = WorldShot::none;  // a shot in flight, by its kind
};

// What a player carries, by meaning, as the rules hold it (a program without one leaves it 0). A
// reader that shows a whole number truncates it (`static_cast<int>`), as the wire does.
struct PlayerNumbers {
  float health = 0, armour = 0;
  float armour_type = 0;  // the share of a blow the armour takes, 0..1
  float ammo[4] = {};     // shells, nails, rockets, cells
  float current_ammo = 0;
  float weapon = 0;      // the program's weapon bit
  int weapon_model = 0;  // the model list's index of the held weapon; 0 none
  float weapon_frame = 0;
  // The weapon's clock for a client that predicts it (QuakeWorld's weapon prediction), on the
  // server's clock (`RulesWorld::time`): the refire's end, the player's next think (0 none) and
  // the impulse still pending.
  float attack_finished = 0, next_think = 0;
  int impulse = 0;
  float items = 0;   // the program's item bits
  float view_z = 0;  // the eye above the origin
  // The view as the rules work it out (Quake II's: its bob, tilts and kicks), when `view_set`:
  // the eye's offset from the origin (units) and the angles added to the view (degrees).
  bool view_set = false;
  float view_offset[3] = {}, kick[3] = {};
  float gun_offset[3] = {}, gun_angles[3] = {};  // the held weapon's own shift and sway
  float blend[4] = {};  // the tint over his view the rules lay (Quake II's), r g b a in 0..1
  // The same without the damage flash, and the flash alone, for a window that draws the flash
  // apart (as a frame round the view's edge); a rules that keeps none apart leaves both 0.
  float blend_apart[4] = {}, damage_blend[4] = {};
  float fov = 0;  // the field of view the rules hold him to, degrees; 0 his own
  float frags = 0;
  // A runner's own weapons and ammunition, for the bots (shooterbots/bot_arsenal.hpp), where
  // Quake's bits above do not fit (Quake II's): the weapon held and those carried as bits of the
  // runner's own weapon index, and every ammunition by its own index.
  int32_t held = 0, carried = 0;
  float stock[8] = {};
  // A layout game's 32 stats, the numbers its status bar program reads (Quake II's: health,
  // ammo, armour, their icons as indices into `RulesWorld::images`, the pickup, the timer, ...).
  int16_t stats[32] = {};
};

// One client slot (the player is entity slot + 1).
struct WorldClient {
  bool active = false, spectator = false, bot = false;
  int userid = 0;
  std::string name;                                           // the user info's name
  std::vector<std::pair<std::string, std::string>> userinfo;  // in the order set
  int ping_ms = 0;
  double entered_at = 0;  // the server's time it connected, s
  PlayerNumbers numbers;  // zero while the slot is empty
  // A layout game's last layout program sent this client (the scoreboard, the help computer),
  // drawn over its bar while its stat 13 has bit 1; "" none.
  std::string layout;
  // Its last inventory message: 256 counts by item number (Quake II's item list), drawn while
  // stat 13 has bit 2; empty none.
  std::vector<int16_t> inventory;
};

struct WorldStatic {
  int modelindex = 0;
  int frame = 0, colormap = 0, skin = 0;
  Vec3f origin, angles;
  float alpha = 1;
  float colormod[3] = {1, 1, 1};
};

struct RulesWorld {
  // Set at the map's start.
  std::string map_name;
  std::string map_title;  // the map's own title (the world's message); "" when it has none
  std::string cd_track;   // the music's track as text (Quake II's world `sounds`; "0" stopped)
  std::vector<std::string> models;  // [0] ""
  std::vector<std::string> sounds;  // [0] ""
  std::vector<std::string> vwep;    // the visible-weapon list
  std::vector<WorldStatic> statics;
  int max_clients = kMaxClients;
  bool weapon_frames = false;  // the rules animate the held weapon (`weapon_frame` is live)
  // A layout game's status bar: its program (the layout language a client runs every frame), the
  // pictures its stats name by index ([0] "": none) and the item names they name (the pickup's).
  std::string status_bar;
  std::vector<std::string> images;
  std::vector<std::string> item_names;

  // Rewritten when they change (a string is copied only when it differs).
  std::string lightstyles[kWorldLightStyles];
  std::vector<std::pair<std::string, std::string>> serverinfo;

  // Rewritten each frame.
  double time = 0;        // the server's time, s
  float serverflags = 0;  // the flags the rules carry from map to map (a campaign's runes)
  bool paused = false;
  WorldClient clients[kMaxClients];
  std::vector<WorldEntity> entities;  // by slot; sized at the map's start
  int entity_count = 0;               // slots in use or freed so far (the pool's high mark)
};

}  // namespace ge::shooter
