// THE PLAYER'S OWN MISSILES THROWN AHEAD OF THE SERVER (cl_predict_projectiles): when the weapon
// prediction fires a nail, a rocket or a grenade, a stand-in of it flies at once from where the
// player was and along where he looked on that command, rather than a round trip later when the
// server's copy arrives. Any game whose prediction knows its shots can throw them (QuakeWorld's
// joined window today, net/qw_remote.cpp off net/qw_weapon.hpp's `fired`).
//
// - THE LAUNCH (`own_missile_fire`), by Quake's rules: a nail and a rocket leave 8 units ahead of
//   the player's origin along the view and 16 up; the nailgun's nail 4 more to the right or the
//   left by its barrel (the super nailgun's from the middle); both fly straight along the view, a
//   nail at 1000 units/s, a rocket at 1000 (2000 under the midair rule). A grenade leaves from the
//   origin at 600 along the view and 200 along the view's up, spins at 300 degrees/s on each axis,
//   falls under the server's gravity and bounces off the world keeping half its speed into it.
// - ITS AGE: a shot is thrown when its command is shown (the weapon's shown command), so it is
//   dated back by the commands sent since and placed where it has flown by now.
// - WHEN IT SHOWS: from the moment the server's own copy would have been fired for it (the round
//   trip less the server's measured ping and 0.013 s, never before now), taken back by its age.
// - THE SERVER'S COPY TAKES IT ON (`own_missiles_handover`): the server's copy, drawn as it was a
//   round trip ago, trails the stand-in along the same path, so drawing the copy in its place
//   would throw the missile back by a round trip's flight. Instead a server missile of the same
//   model within 24 units of where a stand-in was over its last 0.48 s adopts it: the copy is
//   hidden and the stand-in flies on in its place until the copy is gone (its hit or its
//   explosion, which the server draws). Unadopted, a stand-in ends when its copy should long
//   have come: the round trip plus 0.013 s plus 0.1 s.
// - A WALL stops a nail or a rocket: the stand-in goes (the server's hit is the one drawn).
// - AT MOST `kOwnMissilesMost` fly; a new one replaces the one farthest from the player.
// - NONE with the setting off, or when the server measures no ping (a game on this machine,
//   where the server's copy is not late).
//
// Drawn (`own_missiles_draw`) as entities on `DemoState::own_missiles` with each one's number, so
// the window draws them and their trails as it does the server's (demo_video.hpp's `demo_placed`,
// demo_fx.hpp's missiles). Map units and seconds throughout; no GPU.
//
// ge:summary The player's own nails, rockets and grenades thrown ahead of the server, no GPU
// ge:keywords own_missiles predicted-projectiles cl_predict_projectiles fake-missile nail rocket
// ge:keywords grenade handover stand-in weapon-prediction
// ge:skill net shooter
// ge:test tests/shooters_edge_own_missiles_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "shooter/hull.hpp"
#include "shooter/vec.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

inline constexpr int kOwnMissilesMost = 96;
inline constexpr float kNailSpeedU = 1000, kRocketSpeedU = 1000, kMidairRocketSpeedU = 2000;
inline constexpr float kGrenadeAheadU = 600, kGrenadeUpU = 200, kGrenadeSpinDeg = 300;
inline constexpr float kGrenadeBounce = 1.5f;    // the share of its speed into a wall taken off
inline constexpr float kOwnMissileHandU = 24;    // a server copy this near its path adopts one
inline constexpr double kOwnMissileWaitS = 0.1;  // an unadopted one waits this past its copy's due
inline constexpr double kOwnMissileLeadS = 0.013;
inline constexpr int kOwnMissilePath = 32;         // places kept of its path, ...
inline constexpr double kOwnMissilePathS = 0.015;  // ... this far apart
// The window's entity numbers for the stand-ins (above any server's).
inline constexpr int kOwnMissileEntity = 1 << 24;

enum class OwnMissileKind : uint8_t { nail, super_nail, rocket, grenade };

// What a shot needs: its kind, the player's origin and view on its command, the nail's barrel
// (1 right, -1 left, 0 middle), the midair rule, and how long ago its command was (s).
struct OwnShot {
  OwnMissileKind kind = OwnMissileKind::nail;
  sh::Vec3f origin, angles;  // units; degrees, pitch down positive (the command's)
  int side = 0;
  bool midair = false;
  double age_s = 0;
};

struct OwnMissile {
  OwnMissileKind kind = OwnMissileKind::nail;
  sh::Vec3f origin, velocity, angles;
  double at_s = 0;  // the time `origin` is for
  double show_s = 0, end_s = 0;
  int id = 0;
  int adopted = 0;  // the server's copy (its entity number) hidden while this flies; 0 none
  // Where it was over its last kOwnMissilePath * kOwnMissilePathS seconds (a ring).
  sh::Vec3f path[kOwnMissilePath];
  double path_s = -1;  // when the newest was kept
  int path_n = 0;      // kept so far
};

struct OwnMissiles {
  bool on = true;  // cl_predict_projectiles
  std::vector<OwnMissile> flying;
  // Server copies hidden after their stand-in hit a wall first, until the copy is gone.
  std::vector<int> hidden;
  int next_id = 1;
  // Counted since joined, for the labs: thrown, adopted by the server's copy, ended unadopted,
  // hit a wall.
  int thrown = 0, handed = 0, ended = 0, walled = 0;
};

// A shot thrown at `now_s`: `latency_s` the round trip, `ping_ms` the server's measure of it (0:
// none thrown); `player` the player's place now, for the replacement. False when none was thrown.
bool own_missile_fire(OwnMissiles& m, const OwnShot& shot, double now_s, double latency_s,
                      int ping_ms, const sh::Vec3f& player);

// The stand-ins flown to `now_s` through `world` (hull 0 of the level's world; null: nothing
// stops them) under `gravity` (units/s²); those past their end dropped (an adopted one's copy
// hidden on, when it hit a wall first).
void own_missiles_step(OwnMissiles& m, const sh::Hull* world, float gravity, double now_s);

// The server's missiles drawn in `s` adopt the stand-ins whose path they are on, and are hidden
// while the stand-in flies (above); a stand-in whose copy is gone goes too.
void own_missiles_handover(OwnMissiles& m, DemoState& s);

// The stand-ins showing at `now_s` onto `s.own_missiles` (their models from `s.models`: one the
// server did not list is not drawn).
void own_missiles_draw(const OwnMissiles& m, DemoState& s, double now_s);

// Everything thrown forgotten (a new level, a new server).
void own_missiles_clear(OwnMissiles& m);

}  // namespace shooters_edge::client
