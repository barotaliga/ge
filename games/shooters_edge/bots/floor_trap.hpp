// THE FLOOR TRAP (`floortrap`): a floor over lava or slime that a touch button slides away (dm2's
// big room: the button by each yellow armour fires the floor's doors, which slide out for a few
// seconds and come back). A person who knows the map never brushes the button by accident, and
// presses it on purpose only when an enemy stands on the floor and no mate does; then he stays
// where the button is (the floor is gone between him and the room) until it comes back, or rocket
// jumps out.
// - THE MAP'S TRAPS (`floor_traps_init`): a touch button (`ButtonSpot`, not shot) whose doors
//   include a floor door (`Fighter::Bridge::floor`, map_nav.hpp) with lava or slime under it; the
//   floor's box (its doors' boxes at rest, and the floor doors linked to them) and the button's.
// - NEVER BRUSHED (`floor_trap_cmd`): no node stands near enough to touch the button (nodes keep
//   off walls); a walk between them does. Not pressing, the keys keep the box `kTrapKeep` clear
//   of it: the wish's share toward it let go, a run into it braked.
// - THE PRESS (`floor_trap_think`): an enemy seen or heard within `kTrapFresh_s` over the floor
//   (inside its box, his feet within `kTrapAbove` of its top), no mate over it as far as it knows
//   (each mate's last place seen, sent or not, and a team line's lately), the floor at rest, this
//   fighter on the button's level within `kTrapReach` of it, the straight way to it off the floor: it
//   goes to the button's node and walks into the button (the keys toward it under the view, the
//   aim kept), for at most `kTrapPress_s`.
// - THE STAY: the presser's goal is the button's node until the floor is back (`trap_stay`), and
//   while the floor is away its nodes are shut (`watch_bridges`), so no route crosses the gap.
//   The teleporters that land on the floor are shut with it (`floor_trap_shut`), for every bot,
//   and the keys keep `kTeleKeep` off where they start (a fight's footwork steps in otherwise).
// - THE CALL: a floor known only by sight is believed there by a mate who has not seen it go (he
//   walks or teleports in). The presser says `trap` (talk.hpp) as it goes; a mate who reads it
//   takes the floor, and its teleporters, as away for `kTrapTold_s` (`Fighter::trap_told`).
// - THE FLIGHT OFF IT (`trapflee`, `floor_exit`): a player standing on the floor as it goes (it
//   slides from under him at ~100 units/s: ~3 s to the drop) runs straight off it before any goal,
//   to the nearest node off the floor's box, up to a jump higher (dm2's floor lies 20 under the
//   strips beside it), that a straight run reaches (no wall, no open lava under the line), a way
//   against the slide of the door under him costing double; the keys run at it and jump onto a
//   ledge (the route's floor nodes stay where the floor was). dm2: 47 of 82 trap deaths stood on
//   the floor as it went and fell ~3 s later.
//
// ge:summary The floor trap: a floor over lava a touch button slides away, never brushed by accident,
// ge:summary pressed only when an enemy stands on it, the presser staying put till it returns
// ge:keywords floor-trap floortrap trap-button dm2 big-room lava floor-door touch-button press
// ge:keywords floor_traps_init floor_trap_think floor_trap_cmd
// ge:skill bots shooter
// ge:test tests/shooters_edge_footwork_test.cpp
#pragma once

#include "shooter/nav.hpp"
#include "shooter/usercmd.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::bots {

constexpr double kTrapFresh_s = 0.5;  // an enemy seen or heard this lately over the floor
constexpr float kTrapAbove = 80;      // his feet within this over the floor's top, units
constexpr float kTrapReach = 250;     // the fighter this near the button (across), units
constexpr double kTrapPress_s = 3;    // a press given up after this long
constexpr float kTrapKeep = 24;       // not pressing: the box kept this clear of the button, units
constexpr float kTrapKeep_s = 0.2f;   // and a run braked that would close the rest in this long, s
constexpr double kTrapTold_s = 4;     // a mate's `trap` call read: the floor taken as away this long
constexpr double kTrapMateFresh_s = 5;  // a mate a team line put over the floor this lately
constexpr float kTrapLevel = 64;        // the presser's feet within this of the button's bottom
constexpr float kFleeStep = 18;    // `trapflee`: a step (lower or higher: walked onto)
constexpr float kFleeJump = 40;    // a way off the floor up to this higher (jumped onto), units
constexpr float kFleeUp = 48;      // the straight run's sweep this high (over a ledge), units
constexpr float kFleeJumpAt = 100;  // a higher way jumped at from this near, units
constexpr float kFleeReach = 640;  // and this near across, units
constexpr int kFleeTries = 24;     // the nearest this many tried by the trace
constexpr float kTeleKeep = 48;  // the floor away: kept this far off its teleporters' link starts

// The map's floor traps off its buttons and bridges (after `buttons_init` and the bridges).
void floor_traps_init(Fighter& f, const ge::shooter::NavGraph& g, const server::RulesCommon& s);
// Each think: whether to press a trap's button now (`Fighter::trap_press`, the goal set by the
// caller), the press ended when the floor goes or the time is up.
void floor_trap_think(Fighter& f, const server::RulesCommon& s, int me, double t);
// Every command: while pressing, near the button, the keys toward it under the view (the aim
// kept); else the keys kept off every trap's button (the box `kTrapKeep` clear of it).
void floor_trap_cmd(Fighter& f, const server::RulesCommon& s, int me, ge::shooter::MoveCmd& cmd);
// The teleporters landing on a trap's floor while it is away (`watch_bridges` adds them to the
// shut links, with the bridges').
void floor_trap_shut(const Fighter& f, std::vector<ge::shooter::NavExtraLink>& shut);

}  // namespace shooters_edge::bots
