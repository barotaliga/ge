// THE RULES' WORLD: what a running game's rules show everyone else, in one shape whatever runs
// them (docs/shooters-edge.md § The rules runner). The host, the bots, the wire, the scoreboard and
// the recorder read it; only the runner writes it. The QC server (qc_server.hpp) is the first
// runner: `fill_world` copies its lists at the map's start and its entities and clients at the end
// of each frame. A later program form fills the same struct, so nothing that reads it names QC.
//
// The rows are sized once at the map's start and rewritten in place each frame: the frame's copy
// allocates nothing and calls nothing virtual. An entity row is the entity's slot in the pool
// (row 0 the world, rows 1..32 the players); a free slot keeps its row with `free` set.
//
// ge:summary The rules' world: the lists, the clients and one row an entity, filled by the runner
// ge:keywords rules-world rules-runner runner world-view entity-rows client-rows player-numbers
// ge:keywords fill_world model-list sound-list vwep statics lightstyles serverinfo frags stats
// ge:keywords model-shown taken-item hidden-item model-text
// ge:skill progs shooter
// ge:test tests/shooters_edge_rules_world_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "shooter/client.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;

constexpr int kWorldLightStyles = 64;

// One entity as the world shows it.
struct WorldEntity {
  bool free = true;
  sh::Vec3f origin, angles, velocity;
  sh::Vec3f mins, maxs;
  sh::Vec3f absmin, absmax;  // the box in the world, as the world links it
  sh::Vec3f view_angles;     // a player's view, degrees
  int flags = 0;             // the loop's flags (on the ground, in water, ...)
  int waterlevel = 0;        // 0 dry .. 3 under
  int modelindex = 0;        // the model list's index; 0 none
  int frame = 0, skin = 0, colormap = 0, effects = 0;
  // It turns about its yaw on the viewer's own clock, 100° a second (an item lying about), its own
  // angles set aside: what a runner whose `effects` are its own game's bits says to every reader.
  bool turns = false;
  float alpha = 1;                // 1 opaque (the extension field; 1 when the program has none)
  float scale = 1;                // the model's size (the extension field; 0 in it reads 1)
  float colormod[3] = {1, 1, 1};  // the tint (the extension field; '0 0 0' in it reads 1 1 1)
  float health = 0;
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
  float items = 0;   // the program's item bits
  float view_z = 0;  // the eye above the origin
  float fov = 0;     // the field of view the rules hold him to, degrees; 0 his own
  float frags = 0;
  // A runner's own weapons and ammunition, for the bots (bots/bot_arsenal.hpp), where the first
  // game's bits above do not fit (the sequel's): the weapon held and those carried as bits of the
  // runner's own weapon index, and every ammunition by its own index.
  int32_t held = 0, carried = 0;
  float stock[8] = {};
  // A layout game's 32 stats, the numbers its status bar program reads (the sequel's: health,
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
};

struct WorldStatic {
  int modelindex = 0;
  int frame = 0, colormap = 0, skin = 0;
  sh::Vec3f origin, angles;
  float alpha = 1;
  float colormod[3] = {1, 1, 1};
};

struct RulesWorld {
  // Set at the map's start.
  std::string map_name;
  std::string map_title;  // the map's own title (the world's message); "" when it has none
  std::vector<std::string> models;  // [0] ""
  std::vector<std::string> sounds;  // [0] ""
  std::vector<std::string> vwep;    // the visible-weapon list
  std::vector<WorldStatic> statics;
  int max_clients = sh::kMaxClients;
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
  WorldClient clients[sh::kMaxClients];
  std::vector<WorldEntity> entities;  // by slot; sized at the map's start
  int entity_count = 0;               // slots in use or freed so far (the pool's high mark)
};

struct QcServer;
struct StaticEntity;

// A static entity as the world shows it: its see-through share and tint from the extras.
WorldStatic world_static(const StaticEntity& st);

// True when entity `e` is shown: drawn by a window and sent to clients. It needs a model index
// AND a model text: a program hides a taken item by emptying its `model` (keeping the index, so
// setting the text again brings it back), and such an entity is drawn and sent by no one.
bool model_shown(const QcServer& s, int e);

// How much of entity `e` shows, 0..1 (1 solid): the mod's `alpha` field when it sets one above 0
// (DP_ENT_ALPHA), else the map's `alpha` key, else 1.
float entity_alpha(const QcServer& s, int e);
// Entity `e`'s size: the mod's `scale` field, 1 when it is 0 or the program has none.
float entity_scale(const QcServer& s, int e);
// Entity `e`'s tint: the mod's `colormod` field when not '0 0 0', else the map's key, else 1 1 1.
void entity_colormod(const QcServer& s, int e, float out[3]);

// The runner's half for the QC server: the lists, then the clients and every entity row. The lists
// are copied only at the map's start (`full`); a frame's fill rewrites the rest in place.
void fill_world(const QcServer& s, RulesWorld& w, bool full);

}  // namespace shooters_edge::server
