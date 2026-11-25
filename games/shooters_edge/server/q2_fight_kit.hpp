// The fight's pieces a pack's weapons are built from (q2_gz.hpp): the shot's line through walls,
// players and the map's entities, the sounds, the muzzle flash, a temp entity drawn along a line,
// a row for a shot in flight, the aim from the gun onto the eye's point, and the damage multiplier
// the quad and a pack's double damage give. The base game's own weapons use them through
// q2_fight.cpp; nothing here changes how they play.
//
// ge:summary Quake II's fight pieces a pack's weapons use: the line, sounds, flash, aim, multiplier
// ge:keywords q2 fight-kit shot-line muzzle-flash multiplier double-damage quad mission-pack
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_test.cpp
#pragma once

#include <cstdint>

#include "shooter/vec.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;
struct Q2Server;
struct Q2Player;
struct Q2Missile;

// What a line met first: the world (a wall, the sky) or a player.
struct Line {
  float fraction = 1;
  sh::Vec3f end;
  sh::Vec3f normal;
  bool sky = false;
  int player = -1;
  int mover = -1;  // the map's entity it met (a door, a plat, a monster), when it met one
};

// A line from `from` to `to` through the walls and the living players (not those in `skip`, a
// bit per slot; not map entity `skip_entity`).
Line fight_line(Q2Server& s, sh::Vec3f from, sh::Vec3f to, uint32_t skip, int skip_entity = -1);
// A sound on the weapon's channel of entity `ent` at `at`, heard by everyone.
void fight_sound(Q2Server& s, int ent, sh::Vec3f at, const char* sample, float volume = 1);
// A sound on player `slot`'s item channel.
void fight_item_sound(Q2Server& s, int slot, const char* sample);
// Player `slot`'s muzzle flash `value` (q2_flash.hpp) at his origin.
void fight_flash(Q2Server& s, int slot, int value);
// Temp entity `name` drawn from `from` to `to`, entity `ent`'s (a beam).
void fight_line_effect(Q2Server& s, const char* name, sh::Vec3f from, sh::Vec3f to, int ent);
// A free row for a shot in flight, or nullptr (every row in flight: the shot is lost).
Q2Missile* fight_new_missile(Q2Server& s);
// The entity number the clients know shot `m` by.
int fight_missile_entity(const Q2Server& s, const Q2Missile& m);
// The aim from the gun at `from` onto the point the eye looks at (`aimfix`), for `slot`.
sh::Vec3f fight_aim(Q2Server& s, int slot, sh::Vec3f from, sh::Vec3f forward);
// A draw in -1..1 from the rules' stream.
float fight_crandom(Q2Server& s);
// The damage multiplier `p` fires with: 4 with the quad, and a pack's double damage 2 more on top
// (×8) in deathmatch, or alone (×2) when the quad is not running; dmflags 262144 keeps the quad's
// alone while both run.
int fight_multiplier(const Q2Server& s, const Q2Player& p);
// The sound a shot of multiplier `m` makes beside its own: the quad's at 4 or more, double
// damage's at 2, none at 1.
const char* fight_multiplier_sound(int m);

// While a monster's blow is dealt, which monster deals it (`Q2Server::blow_from`, a map entity;
// -1 none): a hurt player's defender sphere turns on it.
struct FightBlowFrom {
  Q2Server& s;
  int was;
  FightBlowFrom(Q2Server& server, int from);
  ~FightBlowFrom();
  FightBlowFrom(const FightBlowFrom&) = delete;
  FightBlowFrom& operator=(const FightBlowFrom&) = delete;
};

}  // namespace shooters_edge::server
