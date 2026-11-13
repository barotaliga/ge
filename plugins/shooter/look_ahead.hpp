// THE BOX'S LOOK AHEAD: how a moving player's box will meet the room, asked before it presses. The
// hop's arc (how long the velocity, turned at the air's most, stays clear of walls), the walk's
// straight line (whether the box reaches a point straight), and THE WALK FORESEEN: the keys a
// walker would press flown through the move itself (player_move over the map's solids) for the
// hands' delay and a little on, the keys already pressed first (the hands have not caught up), then
// each way of pressing (the pursuit, the turn braked, the next node, no keys); a way that bumps (a
// wall or a corner takes `kWalkBump` u/s or more in a command) is put after every way that does
// not, and among those the one that ends nearest the aim wins, the pursuit first on a tie. A person
// knows how their body meets the room; the pilot asks the move (pilot.hpp `ahead`). OTHER
// PLAYERS too (`OtherBox`, pilot.hpp `ahead_bodies`): each one near put in the flight as a box
// moved along his velocity, so a way that runs into a mate or an enemy is a bump as a wall's is.
//
// ge:summary The box's look ahead: the hop's arc, the walk's straight line, the walk's keys flown
// ge:summary through the move before a bump
// ge:keywords look-ahead foresee foresee_walk walk_clear arc_clear hop_turn_rate bumps corner
// ge:keywords player-move collision walk-ahead brake-the-turn other-players bodies player-bumps
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "ai/why.hpp"
#include "shooter/pmove.hpp"
#include "shooter/vec.hpp"
#include "shooter/world.hpp"

namespace ge::shooter {

// The most the velocity turns a second in the air with the wish across it (hops.hpp's rule:
// 30 units/s across it a command), degrees/s.
float hop_turn_rate(float speed, float dt);

// How long the hop's arc on `side` (-1 left, 1 right) stays clear of walls, up to `horizon`: the
// velocity turned at the move's most each command, the player's box traced along it a step up
// (so a stair's riser is not a wall). Floors and ramps it meets do not count.
float arc_clear(const MoveWorld& world, Vec3f o, Vec3f v, int side, float dt, float horizon);

// Whether the player's box goes from `o` to `to` straight, both a step up (a stair's risers are
// climbed, not walls): no wall on the way (floors, ramps and ceilings met do not count).
bool walk_clear(const MoveWorld& world, Vec3f o, Vec3f to);

// THE WALK FORESEEN.
enum class WalkWay : uint8_t { pursue, brake, near, ease };
constexpr int kWalkWays = 4;
constexpr int kWalkMs = 25;      // a flight's command, ms
constexpr float kWalkBump = 40;  // the speed a wall takes in one command to count a bump, u/s
const char* walk_way_name(WalkWay way);
// The pilot's `because` for a walk pressed the way `way` (not the pursuit), round a bump or
// (`fall`) away from a fall into lava or slime the pursuit's flight foresaw.
const char* walk_way_because(WalkWay way, bool fall = false);

// Another player as the walk foresees him: his box where he stands now, moved along `velocity`
// (flat) through the flight. The pilot is handed at most `kWalkOthers`, the nearest.
struct OtherBox {
  Vec3f origin, velocity;
};
constexpr int kWalkOthers = 8;

struct WalkKeys {
  float yaw = 0, forward = 0, side = 0;  // degrees; shares of maxspeed
};

struct WalkStart {
  Vec3f origin, velocity;
  bool onground = true;
  int waterlevel = 0;
  float maxspeed = 320;
  WalkKeys pressed;                    // the keys the hands press now (flown for `delay`)
  float delay = 0;                     // the hands' delay, s
  float horizon = 0;                   // the flight's length, s (the delay included)
  std::span<const OtherBox> others;    // the players near, as boxes in the flight (empty: none)
  const MoveSettings* move = nullptr;  // the move flown (null: the default MoveSettings)
  // A FALL INTO HARM foreseen too (`fallseen`): a flight whose box leaves the floor over lava or
  // slime (the floor under it, traced down, in either) falls, and is put after every way that
  // does not, bumps or none; the straight look along the velocity then looks for such a floor too.
  bool harm = false;
};

struct WalkSeen {
  float bump = 0;    // the speed walls took in the flight, u/s summed
  float nearer = 0;  // how much nearer the aim it ends, units
  bool falls = false;  // (`WalkStart::harm`) its box left the floor over lava or slime
};

// The way to press from `st` toward `aim` (`near`: the route's next node, the `near` way's aim),
// its keys in `keys` (the pursuit's when it is chosen) and every way's flight in `seen`. Nothing is
// flown (`flown` false, the pursuit) unless the box swept straight along the velocity meets a wall
// within the horizon, or another player's box comes within touch of it on both their velocities;
// the other ways are flown only when the pursuit's bumps.
WalkWay foresee_walk(const MoveWorld& world, const WalkStart& st, Vec3f aim, Vec3f near,
                     WalkKeys& keys, WalkSeen seen[kWalkWays], bool& flown);

// A BODY ON THE WAY: the nearest of `others` standing (under `kBodyStill` u/s) whose box the
// walker's, walked straight from `o` to `to`, runs into within `kBodyLook`. True when one does,
// `round` then the point beside and just past him to walk to first: on the side the line already
// passes him, else the other, both legs clear of walls (`walk_clear`); `o` itself when neither
// side is clear (wait for him to move). A person walks round a mate standing in a doorway.
constexpr float kBodyStill = 60;  // u/s: a player slower than this stands
constexpr float kBodyLook = 192;  // units: a body further along the line is not in the way yet
bool body_on_way(const MoveWorld& world, Vec3f o, Vec3f to, std::span<const OtherBox> others,
                 Vec3f& round);

// The flights as the `foresee` decision on `why` (null: nothing): each way's score its nearness
// less its bumps, the chosen one and why.
void record_walk(ai::Why* why, float speed, WalkWay way, const WalkSeen seen[kWalkWays]);

}  // namespace ge::shooter
