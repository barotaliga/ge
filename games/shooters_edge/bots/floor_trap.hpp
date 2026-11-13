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
//   (inside its box, his feet within `kTrapAbove` of its top), no mate over it, the floor at rest,
//   this fighter within `kTrapReach` of the button and the straight way to it off the floor: it
//   goes to the button's node and walks into the button (the keys toward it under the view, the
//   aim kept), for at most `kTrapPress_s`.
// - THE STAY: while the floor is away its nodes are shut (`watch_bridges`), so no route crosses
//   the gap: the presser holds where it is unless a rocket jump (when it can afford one) leads out.
//   The teleporters that land on the floor are shut with it (`floor_trap_shut`), for every bot.
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
