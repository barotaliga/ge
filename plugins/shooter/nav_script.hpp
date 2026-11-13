// SCRIPTED WAYS: a short run of points a player takes in one go, where the proven graph has no
// way a person would take (a stair whose treads the grid misses, hopped up in one flow as people
// do; a key item's last steps). A game writes them per map (Shooter's Edge's `bots/item_routes`);
// the nav adds each as one `NavKind::script` link from the node at its start to the node at its
// end, so a route leads to the script and the script finishes it.
//
// THE KEYS (`script_keys`), the same for the proof and the pilot: the view on the step aimed at,
// forward held to the full, and the jump pressed on the ground when that step is a HOP and stands
// more than a step above the feet within `kHopReach`. A step is reached standing on the ground
// within a few units of its height and either near its point or past it toward the next step
// (a flowing hop lands beyond a narrow tread's middle); the last step within `arrive`. The entry
// is left only LINED UP (slow, and standing or running the script's way): a player carried in at
// a hop's speed or from the side brakes there first, as the proof starts standing.
//
// THE PROOF (`prove_script`): a player standing at the start, the view toward the first step,
// flown by `player_move` command by command under those keys until every step is reached, or he
// stands on the ground below the lowest step (fell), or `kScriptLimit_s` runs out. The link's time
// is the proof's; its box the player's sweep (a mover it meets is the link's: a shut door bars it).
//
// Deterministic: the view's yaw through core's libm, floats in the move's order. Held by
// tests/shooter_nav_test.cpp; drawn by the `se-spot` lab (`--route`) and tabled by `se-reach`.
//
// ge:summary Scripted ways: a run of walked and hopped points proven by the move, one nav link
// ge:keywords nav-script scripted-route script-link item-route stair-hop flowing-hop prove_script
// ge:keywords script_keys hop-reach scripted-way
// ge:skill shooter bots
// ge:test tests/shooter_nav_test.cpp
#pragma once

#include <cstddef>
#include <vector>

#include "shooter/nav.hpp"

namespace ge::shooter {

// `ScriptStep` and `NavScript` are nav.hpp's (the graph holds its scripts).

// A hop is pressed this near its step, across, units.
inline constexpr float kHopReach = 72;
// A script's proof gives up after this long, s.
inline constexpr float kScriptLimit_s = 6;

struct ScriptKeys {
  float yaw = 0;      // the view's heading, degrees
  float forward = 0;  // a share of maxspeed
  bool jump = false;
  bool done = false;  // the last step reached
};

// The keys for a player at `origin` moving at `velocity` along `sc`, aiming at step `i` (moved on
// as each is reached; the entry, step 0, only once he is lined up: under 330 units/s and standing
// or running within 30 degrees of the way to step 1, else braking there turned down the script).
// With `loose_entry` (`scriptentry`) the entry is had anywhere within `kSlowInto` of it once lined
// up, and short of that the keys let go, turned up the script: a player run in from the side
// circled the entry's point at ~200 units/s under a keyboard's taps, never on it slow and lined up.
ScriptKeys script_keys(const NavScript& sc, size_t& i, Vec3f origin, Vec3f velocity, bool onground,
                       float arrive, bool loose_entry = false);

// The script flown by the move from standing at `from`.
NavProof prove_script(const MoveWorld& world, const MoveSettings& move, Vec3f from,
                      const NavScript& sc, const NavSettings& set,
                      std::vector<Vec3f>* path = nullptr);

}  // namespace ge::shooter
