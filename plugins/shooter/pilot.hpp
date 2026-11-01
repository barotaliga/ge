// THE PILOT: a route on the nav graph (nav.hpp) played as a player plays it, one command at a time,
// through the same move. It reads only what a client knows of its own player (where it stands,
// how fast it moves, whether it is on the ground) and of the movers it can see (their boxes now),
// and writes only a command: the view's yaw, the forward and side axes, the jump. So it can do
// nothing a player cannot, and whatever the move does to a person it does to the pilot.
//
// Each command (`pilot_step`), the link it is on (from `route[k]` to `route[k + 1]`) decides:
//   walk, drop  pure pursuit: the view turned at the point `look` units along the route (carried
//               through the level walks that follow, so no stop at each node), steered from where
//               the player will be `lead_s` ahead (late hands); forward pressed by how much the
//               view already faces the way, and slowed into the goal. A node is passed within
//               `arrive` units, or within `corner` of the next link's line on its height.
//   jump        a jump link is proven from its start moving toward the target at its own `speed`.
//               The walk into the start runs at the start on the jump's line, slowing to that
//               speed before the run-up; the jump is pressed at the start, on the line, moving
//               0.9 to 1.15 of it toward the target. Arriving off the line or past the start
//               without the speed, it goes back to the RUN-UP point (`run_up` units behind the
//               start), stands there turned down the jump, and runs again. In the air the view
//               turns at the target with forward full, as the proof did.
//   ride        stand still on the mover's top until it carries the player within 2 units of the
//               top node's height (wherever on the top it stands), then on.
//   a mover     a link onto a ridden mover waits until the mover is back at rest; a link a door's
//               box now meets waits. The pilot waits stepped clear of the mover's box and of its
//               travel, out of its nearer side (a plat stopped on a player stands still). A wait
//               at a door longer than `wait_s` bars that door and plans again round it.
//   strafe      a stretch of walks on level ground in a near-straight line longer than
//               `strafe_min` units is run as strafe jumps: the jump pressed on every landing, in
//               the air the view held along the velocity and the side axis pressed to the full
//               (the air's 30 unit/s cap adds the most speed with the wish at right angles to the
//               velocity), its sign the side the target is on; left `strafe_end` units before the
//               stretch ends.
//   hop         with `hop` set (an expert) the strafe gives way to the BUNNY HOP, as a recorded
//               pro moves (docs/movement.md § The bunny hop): any stretch of walks and drops with
//               no mover or jump, `hop_min` units or more, curves and stairs too. In the air the
//               side key alone and the view led on the velocity by the move's most turn (the
//               most gain); the side the route `hop_look_s` ahead lies on, switched past
//               `hop_hold` after `hop_side_s` (at once past `hop_swing`); the back key when the
//               aim is `hop_brake` off. The jump is let go in the air and pressed on the
//               landing's command (no friction taken). With the map's solids in the sense, a
//               side whose arc meets a wall within `hop_see_s` gives way to the other. Left
//               `hop_end` units before the stretch ends, or landing under `hop_slow` x maxspeed.
// STUCK: no nearer the target for `stuck_s` on the ground plans again from the node nearest where
// it stands; LOST (more than `lost` units from the link, on the ground) the same. Every decision
// is recorded on the `ai::Why` handed in (`link`: the mode chosen, with its facts), and the intent
// before the hands (`hands`: yaw, forward, side).
//
// THE HANDS (`ai::Hands`): yaw, forward and side pass through them, the jump untouched.
// `none` presses what the pilot wants; `human` reacts about 0.15 s late and moves as a hand does
// (it cannot hold a hop's speed); `practised` (an expert's movement) has no reaction to wait for
// and no settle, as a pro's view stays within a few degrees of his velocity while it turns.
//
// Allocates only when it plans (the route's vector); deterministic to the bit (floats in the
// move's order, angles through core's libm). Held by tests/shooter_pilot_test.cpp, drawn by the
// `se-pilot` lab.
//
// ge:summary The pilot: a nav route played one command at a time through the player move
// ge:keywords pilot bot route follow usercmd command jump run-up ride plat door wait strafe-jump
// ge:keywords bunny-hop air-strafe stuck replan lost hands why pilot_step pilot_go pilot_source
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ai/hands.hpp"
#include "ai/path.hpp"
#include "ai/why.hpp"
#include "shooter/cmd_source.hpp"
#include "shooter/nav.hpp"

namespace ge::shooter {

struct PilotSettings {
  float arrive = 12;       // a node is reached within this across, units
  float corner = 24;       // a walk turns at the next node's target within this of its own
  float stuck_s = 1;       // no nearer the target for this long on the ground: plan again
  float lost = 96;         // this far off the link on the ground: plan again
  float wait_s = 5;        // a mover in the way this long is barred and the way planned round
  bool strafe = true;      // run long straight stretches as strafe jumps
  float strafe_min = 448;  // the shortest stretch strafed, units
  float strafe_end = 160;  // strafing stops this far before the stretch's end
  float line = 20;         // a stretch's nodes lie within this of its straight line, units
  float look = 64;         // a walk aims this far along the route, units
  float lead_s = 0;        // and steers from where it will be this long ahead (late hands), s
  // THE HOP (an expert's bunny hop, off by default; replaces the strafe when on):
  bool hop = false;         // hop the route's walks and drops, round its curves too
  float hop_min = 256;      // the shortest stretch hopped, along the route, units
  float hop_end = 96;       // hopping stops this far along the route before the stretch's end
  float hop_look_s = 0.3f;  // the hop aims at the route this long ahead at its speed, s
  float hop_hold = 12;      // the side holds until the aim is this far off the other way, deg
  float hop_side_s = 0.2f;  // and for at least this long (a hand's rhythm), unless the aim is
  float hop_swing = 45;     // this far off: then the side it is on at once, deg
  float hop_brake = 70;     // the aim this far off the velocity: the back key pressed too, deg
  float hop_slow = 0.75f;   // landed under this share of maxspeed: the hop ends (run instead)
  float hop_see_s = 0.12f;  // the arc ahead checked for a wall this long, s (with a world)
};

enum class PilotMode : uint8_t { idle, walk, run_up, jump, air, wait, ride, strafe, arrived, hop };
const char* pilot_mode_name(PilotMode m);

enum class PilotHands : uint8_t { none, human, practised };
// The hands' axes: yaw (degrees), forward and side (shares of maxspeed), jump.
std::vector<ai::AxisHands> pilot_hands(PilotHands profile);
// How far ahead a pilot with these hands steers from (their delay), s.
float pilot_lead_s(PilotHands profile);

// What the pilot reads before each command: its own player and the movers' boxes now.
struct PilotSense {
  Vec3f origin, velocity;
  bool onground = false;
  float maxspeed = 320;
  std::span<const NavMover> movers;  // each mover's entity and box as it stands now
  // The map's solids, when the pilot may look ahead through them (the hop checks its arc for a
  // wall, as a player sees the room); null: the hop flies blind.
  const MoveWorld* world = nullptr;
};

struct PilotStats {
  int32_t plans = 0, stuck = 0, lost = 0, waits = 0, barred = 0, jumps = 0, run_ups = 0;
  int32_t rides = 0, strafes = 0;
  int32_t hops = 0, hop_jumps = 0;  // hop stretches entered, jumps pressed on a landing in them
  double time_s = 0;                // the commands' time since the last pilot_go
};

struct Pilot {
  const NavGraph* g = nullptr;
  PilotSettings set;
  std::vector<int32_t> route;  // the nodes; empty: none
  size_t k = 0;                // on the link route[k] -> route[k + 1]
  int32_t goal = -1;
  PilotMode mode = PilotMode::idle;
  float yaw = 0;                 // the view's yaw last pressed, degrees, unwrapped
  float best = 0, best_s = 0;    // the nearest the target has been and when (the stuck check)
  float wait_s = 0;              // waiting at a mover this long
  int32_t strafe_end_node = -1;  // the route index a strafe or hop stretch ends at
  int8_t strafe_side = 0;        // the side axis's sign in the air, -1 left, 1 right
  float side_s = 0;              // the hop's side held this long, s
  bool jump_pressed = false;     // the jump button in the last command
  bool airborne = false;         // left the ground on the current jump link
  int32_t barred[4] = {-1, -1, -1, -1};    // doors found shut: planned round
  double wanted[3] = {}, pressed[3] = {};  // the last command's yaw, forward, side
  ai::Hands hands;
  ai::Search search;
  PilotStats stats;
};

// A pilot on `g` (kept alive and unmoved) with `hands` (from pilot_hands), its wander off `seed`.
void pilot_init(Pilot& p, const NavGraph& g, const PilotSettings& set,
                std::span<const ai::AxisHands> hands, double hz, uint32_t seed);
// The view turned to `yaw` (degrees) with the hands at rest there: a pilot taking over a player
// already moving (a run started from a recorded player's place and velocity).
void pilot_face(Pilot& p, float yaw);
// Plans the way to node `goal` from where the pilot stands; false when there is none.
bool pilot_go(Pilot& p, int32_t goal, const PilotSense& sense);
// The next command, `msec` long.
MoveCmd pilot_step(Pilot& p, const PilotSense& sense, int msec, ai::Why* why = nullptr);

// A pilot filling a server's slot: it reads the slot's player and the movers' edicts. Kept alive
// and unmoved while the slot is filled. `goals` are visited in turn, round and round; a goal with
// no way to it is skipped, and the doors barred are forgotten at each new goal.
struct PilotBot {
  Pilot pilot;
  std::vector<int32_t> goals;
  size_t next_goal = 0;
  std::vector<NavMover> now;   // the movers as they stand (reused)
  MoveWorld* world = nullptr;  // the map's solids the hop looks ahead through (null: none)
  ai::Why* why = nullptr;
  int64_t command = 0;
  int32_t arrivals = 0;
};
CmdSource pilot_source(PilotBot& b);

}  // namespace ge::shooter
