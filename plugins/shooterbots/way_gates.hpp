// The way through's waking triggers (way_through.hpp): a trigger that waits to be made live, the
// names that wake it, the doors such a live trigger opens by touch, and the side of its doors it
// is walked into from, and the place a shot button is shot from.
//
// ge:summary Triggers a button wakes: the names that wake one, touch doors, the near side's node
// ge:keywords way_gate_names way_wake_gates way_touch_opened way_trigger_sides triggered gate
// ge:keywords trigger-multiple touch-door wake bunk1 way_shot_sides shot-button force-field
// ge:keywords way_key_monsters way_key_prey trigger_counter monster-target hip1m2
// ge:keywords way_lit_bridge force-bridge togglewall lit-bridge
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "shooter/entity.hpp"
#include "shooterbots/entities.hpp"
#include "shooterbots/map_nav.hpp"
#include "shooterbots/nav.hpp"
#include "shooterbots/way_through.hpp"

namespace ge::shooterbots {

constexpr int kTriggerWaits = 4;  // a trigger's flag: it waits to be made live (Quake II's)

// Whether trigger block `b` waits to be made live (spawnflags kTriggerWaits).
bool trigger_waits(const EntityBlock& b);

// Step `st` (a trigger named `name` that waits) gated on its name and every name that fires it in
// turn (a trigger the button fires: bunk1's t97 wakes t98's strip through a trigger_once), and on
// the monsters of `s` whose death fires one (`WayStep::gate_dead`), left for good until
// way_wake_gates opens it.
void way_gate_names(WayStep& st, const char* name, const std::vector<EntityBlock>& blocks,
                    const BotView& s);

// A trigger made live by a step done (the button that wakes it pressed) or by a monster's death
// (one of `gate_dead` gone from `s`) is walked into from now.
void way_wake_gates(WayThrough& w, const BotView& s);

// Whether locked door `e` (its edict `de`) is one a live trigger_multiple reaching across it opens
// each time he walks up (bunk1's pairs by the strips their buttons wake): a door he opens by
// touch, not a barrier.
bool way_touch_opened(const WayThrough& w, const ge::shooter::Edict& de, int32_t e);

// A trigger reaching across the doors it opens is walked into on his side of them (a node in its
// box he reaches), not at the node first chosen beyond them.
void way_trigger_sides(WayThrough& w, const NavGraph& g);

// A shot button whose node he does not reach (the one nearest it that sees it, in a room he has
// not got into) is shot from the nearest node he reaches that sees it within kShotFrom (bunk1's
// switch behind its force field, shot from the corridor through the room's far door).
void way_shot_sides(WayThrough& w, Fighter& f, const NavGraph& g, const BotView& s, double t);

// THE MONSTERS A DOOR WAITS ON: those whose `target` opens one of the doors only something else
// opens (`WayThrough::locked`), straight or through a counter or a relay (hip1m2's four soldiers
// on counter t125, the door *98 to the rest of the map). Even on the way through alone
// (`complete` off), with nothing else he reaches to do, the nearest of them he reaches and that
// is alive is hunted (`way_key_prey`): with none hunted he explored the same rooms for 800 s.
void way_key_monsters(WayThrough& w, const BotView& s, const std::vector<EntityBlock>& blocks);
// The nearest such monster alive whose node he reaches (`w.search`): its edict, its node in
// `node` and the way's cost in `cost`; -1 when none.
int way_key_prey(const WayThrough& w, const BotView& s, const NavGraph& g, int32_t& node,
                 double& cost);
// A LIT BRIDGE (map_nav.hpp `NavBridge::lit`): wall `e`, off at the start, as a floor shut to him
// till it is solid (`kLockedLit`), its nodes shut with it (hip1m2's force bridge to the silver
// keycard: left out of the nav, the keycard and all past it were out of his reach). False when `e`
// is no lit bridge (a force field instead).
bool way_lit_bridge(WayThrough& w, const MapNav& nav, int e);

}  // namespace ge::shooterbots
