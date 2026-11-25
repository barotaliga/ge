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
// - THE MISSION PACKS' (measured off each pack's own rules, se-weapon-predict's launch table):
//   Scourge of Armagon's laser bolt (progs/lasrspik.mdl) leaves 12 ahead and 1.8 up at 1000, the
//   twin bolts 4.2 to each side; its proximity grenade (progs/proxbomb.mdl) is thrown as a grenade
//   but spins 100, 600, 100 degrees/s and sticks where it lands. Dissolution of Eternity's lava
//   nails (progs/lspike.mdl, the lava super nailgun's too) leave as the nails do; its multi
//   grenade (progs/mervup.mdl) is thrown as a grenade; its multi rockets (progs/rockup.mdl) leave
//   four at once as a rocket does, turned -6.6, -3.3, 3.3 and 6.6 degrees in a deathmatch, or in
//   single play with 80 and 40 units/s to each side; its plasma ball (progs/plasma.mdl) waits 0.1
//   s 24 ahead and 16 up, then flies at 1250 along the aim.
// - QUAKE II'S (off the rules our server runs, server/q2_combat.hpp's kSqWeapons and q2_fight's
//   launch): a shot leaves from 8 under the eye, 24 ahead and 8 to the side for a blaster or a
//   hyperblaster bolt, 8 and 8 for a grenade, a rocket or the BFG's ball (the side by the `hand`
//   the player holds his gun in: right, left, or none when centred); a bolt flies at 1000, a rocket
//   at 650, the ball at 400; a grenade at 600 along the view and 200 along its up, falls under the
//   player's gravity, bounces keeping half its speed into a wall and rests on a floor once slower
//   than 60. Its effect bits (the trail and the light: a bolt's 8, the hyperblaster's 64 on its
//   glowing frames, a grenade's 32, a rocket's 16, the ball's 128) ride on it as the server's copy
//   carries them. The server moves nothing ahead, so a stand-in shows at once and its copy, a round
//   trip and up to a frame (0.1 s) behind, takes it on from its path.
// - A WALL stops a nail, a bolt, a ball or a rocket: the stand-in goes (the server's hit is the
//   one drawn).
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
// ge:keywords grenade handover stand-in weapon-prediction q2-own-missiles
// ge:skill net shooter
// ge:test tests/shooters_edge_own_missiles_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "brush/trace.hpp"
#include "classic/weapon_predict.hpp"
#include "shooter/hull.hpp"
#include "shooter/vec.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

inline constexpr int kOwnMissilesMost = 96;
inline constexpr float kNailSpeedU = 1000, kRocketSpeedU = 1000, kMidairRocketSpeedU = 2000;
inline constexpr float kLaserAheadU = 12, kLaserUpU = 1.8f, kLaserSideU = 4.2f;
inline constexpr float kPlasmaAheadU = 24, kPlasmaSpeedU = 1250;
inline constexpr double kPlasmaWaitS = 0.1;
inline constexpr float kProxSpinDeg[3] = {100, 600, 100};
// The multi rockets' turn (degrees, a deathmatch) and side speed (units/s, single play), by rocket.
inline constexpr float kMultiRocketYaw[4] = {-6.6f, -3.3f, 3.3f, 6.6f};
inline constexpr float kMultiRocketSide[4] = {80, 40, -40, -80};
inline constexpr float kGrenadeAheadU = 600, kGrenadeUpU = 200, kGrenadeSpinDeg = 300;
inline constexpr float kQ2BoltSpeedU = 1000, kQ2RocketSpeedU = 650, kQ2BfgSpeedU = 400;
inline constexpr float kQ2BoltAheadU = 24, kQ2MissileAheadU = 8, kQ2MissileSideU = 8;
inline constexpr float kQ2EyeDropU = 8;          // the shot's start under the eye
inline constexpr float kQ2RestSpeedU = 60;       // a grenade slower than this off a floor rests
inline constexpr double kQ2FrameS = 0.1;         // the frame a Quake II server's copy may wait for
inline constexpr float kGrenadeBounce = 1.5f;    // the share of its speed into a wall taken off
inline constexpr float kOwnMissileHandU = 24;    // a server copy this near its path adopts one
inline constexpr double kOwnMissileWaitS = 0.1;  // an unadopted one waits this past its copy's due
inline constexpr double kOwnMissileLeadS = 0.013;
inline constexpr int kOwnMissilePath = 32;         // places kept of its path, ...
inline constexpr double kOwnMissilePathS = 0.015;  // ... this far apart
// The window's entity numbers for the stand-ins (above any server's).
inline constexpr int kOwnMissileEntity = 1 << 24;

enum class OwnMissileKind : uint8_t {
  nail,
  super_nail,
  rocket,
  grenade,
  laser,          // Scourge of Armagon's laser bolt
  prox,           // its proximity grenade
  lava_nail,      // Dissolution of Eternity's lava nail (and the lava super nailgun's)
  multi_grenade,  // its multi grenade
  multi_rocket,   // one of its four multi rockets
  plasma,         // its plasma ball
  q2_bolt,        // Quake II's blaster and hyperblaster bolt
  q2_grenade,     // its grenade launcher's grenade
  q2_rocket,      // its rocket
  q2_bfg          // its BFG's ball
};

// What a shot needs: its kind, the player's origin and view on its command, the nail's or the
// twin laser bolts' side (1 right, -1 left, 0 middle), the midair rule, how long ago its command
// was (s), and a multi rocket's place among its four (0..3) and whether a deathmatch turns them.
struct OwnShot {
  OwnMissileKind kind = OwnMissileKind::nail;
  sh::Vec3f origin, angles;  // units; degrees, pitch down positive (the command's)
  int side = 0;
  bool midair = false;
  double age_s = 0;
  int spread = 0;
  bool deathmatch = true;
  // Quake II's: the eye's height over the origin, and the effect bits it flies with.
  float view_height = 0;
  int effects = 0;
};

struct OwnMissile {
  OwnMissileKind kind = OwnMissileKind::nail;
  sh::Vec3f origin, velocity, angles;
  sh::Vec3f aim;       // the plasma ball's way once it goes
  double go_s = -1;    // the plasma ball waits still until then
  bool stuck = false;  // a proximity grenade landed (or Quake II's came to rest)
  int effects = 0;     // Quake II's effect bits, drawn as its copy's
  double at_s = 0;     // the time `origin` is for
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

// What a predicted shot throws (net/qw_weapon.hpp's `fired`): the shot's sound, its copies, the
// weapon in hand and the rules give the stand-ins' kind and how many (the laser cannon's twin
// bolts 2, the multi rockets 4); 0 for a shot that throws none.
int own_shot_kind(ge::classic::WeaponSound sound, int copies, int weapon,
                  ge::classic::WeaponRules rules, OwnMissileKind& kind);

// A shot thrown at `now_s`: `latency_s` the round trip, `ping_ms` the server's measure of it (0:
// none thrown); `player` the player's place now, for the replacement. False when none was thrown.
bool own_missile_fire(OwnMissiles& m, const OwnShot& shot, double now_s, double latency_s,
                      int ping_ms, const sh::Vec3f& player);

// The stand-ins flown to `now_s` through `world` (hull 0 of the level's world; null: nothing
// stops them) under `gravity` (units/s²); those past their end dropped (an adopted one's copy
// hidden on, when it hit a wall first). Quake II's level is its brushes (`brushes`: solid and
// window contents stop a missile; null: nothing does).
void own_missiles_step(OwnMissiles& m, const sh::Hull* world, float gravity, double now_s);
void own_missiles_step_brushes(OwnMissiles& m, ge::brush::BrushTracer* brushes, float gravity,
                               double now_s);

// The server's missiles drawn in `s` adopt the stand-ins whose path they are on, and are hidden
// while the stand-in flies (above); a stand-in whose copy is gone goes too.
void own_missiles_handover(OwnMissiles& m, DemoState& s);

// The stand-ins showing at `now_s` onto `s.own_missiles` (their models from `s.models`: one the
// server did not list is not drawn).
void own_missiles_draw(const OwnMissiles& m, DemoState& s, double now_s);

// Everything thrown forgotten (a new level, a new server).
void own_missiles_clear(OwnMissiles& m);

}  // namespace shooters_edge::client
