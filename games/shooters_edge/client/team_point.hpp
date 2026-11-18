// WHAT THE PLAYER POINTS AT, for team chat's `%t`, `%x`, `%y`, `%j` and `tp_msgpoint`
// (client/team_chat.hpp): the thing nearest the view's line that the player can see, named in the
// team's words (`tp_name_*`), with the place it is in.
//
// - **The scene** (`PointScene`): the eye and the view's angles, and every thing in sight this
//   frame: items by their model's file (a thing whose model is not in the pointing table is never
//   pointed at) and players by slot, each with its side. `point_scene` reads it from a server
//   joined or a demo played (the protocol's entities and players) and from the window's own game.
// - **The choice** (`point_at`): a thing at least 10 units ahead of the eye; its miss from the
//   view's line at most 300 units and at most 1.7 times its distance ahead; in sight of the eye
//   (its middle, or a point a radius out from it on any axis: a player 27 units round a middle 30
//   above his feet, an item its own size, anything glowing or lit 200). The least miss wins,
//   weighted by (distance × 0.0016 + 0.3) closer than 375 units, so a near thing a little off the
//   line beats a far one on it.
// - **What counts** (the default pointing set): powerups, armour, the big weapons, the big health,
//   a pack, rockets, cells, runes, flags and players; deathmatch 4 leaves the ammunition out, a
//   deathmatch other than 1 the weapons. Dead players, the gibbed head and spectators never count.
// - **The words**: an item its `tp_name_*` word (armour by its skin: 0 light, 1 medium, else
//   heavy); a cloaked player `tp_name_eyes`; a teammate `tp_name_teammate`, or their own name when
//   that is empty; an enemy `tp_name_enemy`. A damage boost's or a shield's glow puts
//   `tp_name_quaded` and `tp_name_pented` in front. A spectator pointing reads players' names.
//
// ge:summary What the player points at for team chat: the scene, the choice, the team's word
// ge:keywords point pointing tp_msgpoint percent-codes pointing-codes tp_pointtimeout
// ge:keywords tp_name_enemy tp_name_teammate tp_name_eyes tp_name_quaded tp_name_pented
// ge:keywords point_at point_scene line-of-sight team-chat
// ge:skill shooter console
// ge:test tests/shooters_edge_team_point_test.cpp
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "console/console.hpp"
#include "shooter/vec.hpp"

namespace shooters_edge::client {

struct DemoState;
struct Listen;

// One thing in sight this frame.
struct PointThing {
  std::string model;  // an item's model file ("progs/armor.mdl"); a player's figure's
  int skin = 0;
  bool quaded = false, pented = false;  // the damage boost's glow, the shield's
  bool lit = false;                     // it carries a light (a held power's dim light)
  ge::shooter::Vec3f origin;
  bool player = false;
  bool teammate = false;  // a player on the viewer's team (teamplay on)
  std::string name;       // a player's name
};

struct PointScene {
  bool on = false;            // a game is playing or watched
  ge::shooter::Vec3f eye;     // the view's eye, units
  ge::shooter::Vec3f angles;  // the view's angles, degrees
  int deathmatch = 1;         // the server's rule (4: no ammunition; not 1: no weapons)
  bool spectator = false;     // the viewer watches (players read by name)
  std::vector<PointThing> things;
};

// What the player points at.
struct Pointed {
  bool any = false;
  std::string thing;  // its words ("quaded enemy", "ra", a teammate's name)
  ge::shooter::Vec3f at;
  std::string item;  // an item's word setting ("tp_name_ra"); "" a player
  bool enemy = false, teammate = false, eyes = false;
  bool quaded = false, pented = false;
};

// Whether nothing solid stands between two points (null: everything is in sight).
using ClearLine = std::function<bool(ge::shooter::Vec3f, ge::shooter::Vec3f)>;

// The thing the player points at in `scene`, named in `con`'s words.
Pointed point_at(const ge::console::Console& con, const PointScene& scene, const ClearLine& clear);

// The scene of a server joined or a demo played (`ds`: its view, entities and players).
PointScene point_scene(const DemoState& ds);
// The scene of the window's own game (its first player's view); `on` false when none is up.
PointScene point_scene(const Listen& l);

}  // namespace shooters_edge::client
