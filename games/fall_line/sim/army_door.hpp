// THE DOOR GUNNERS: the men on the army's helicopters' door guns (`WeaponKind::door_gun`, fired
// through gunfire.hpp), the roster's `gunners` spread two to a machine in the order called. Its
// numbers are army_heli_rules.hpp's.
//
// - MANNED: the gunners are shared out two to each HUNTING machine in the order called (never one
//   about a landing, where the men go out of those doors), a lone gunner in the inward door (the
//   side its circle turns toward). The doors are manned while the army fires (`ArmyState::fire`)
//   and the rules of fire the stars set (`WantedRoster::fire`, made warning fire where the army
//   does not shoot to kill) are more than `hold`. A manned machine slides its doors open with the
//   man in sight within `kOpenM`; one flying echelon is crabbed side-on to him (`kCrab`) so the
//   inward door has the line. The pylon turn has him square out of it already.
// - THE LAY: each manned gun is swung onto him at `kTraverse` where its arc lets it (`kArc` either
//   side of square out of the door, `kDepress` under the horizon to `kElevate` over it), and back
//   to its rest where it does not; it fires a burst once laid on him, its door open, the man in
//   its gunner's sight from its pivot (`sees_quarry`: the woods' crowns and his cover counted) and
//   inside the gun's effective range.
// - The guns stand at their pivots' muzzles and move with the machine every step, so a burst's
//   rounds leave from where the gun is when each is fired, at the machine's own velocity; a door
//   no longer manned has its gun put away.
//
// Nothing here draws a number (the spreads are gunfire's) or allocates in a step.
//
// ge:summary The army's door gunners: the helicopters' door guns manned, their arcs, their bursts
// ge:keywords door-gun door-gunner gunners pintle arc beam crab side-on army helicopter gunfire
// ge:keywords step_army_doors doors_manned lay traverse
// ge:skill sim
// ge:test tests/fall_line_army_door_test.cpp
#pragma once

#include <cstdint>

#include "fall_line/sim/army_state.hpp"
#include "fall_line/sim/state.hpp"
#include "fall_line/sim/wanted.hpp"

namespace fall_line::sim {

// How many of the roster's gunners machine `h` carries (0, 1 or 2): the first machine out the
// first two.
int32_t gunners_of(const GameState& state, const ArmyHeli& h);

// Whether machine `h`'s doors are manned this step: it hunts, it carries a gunner, the army fires
// and the rules of fire are more than `hold`.
bool doors_manned(const GameState& state, const ArmyHeli& h);

// THE DOOR GUNNERS' STEP, after the machines have flown: every manned door's gun moved to its
// pintle and, with the man in its arc and sight, a burst fired at him; every other door's gun put
// away. `q` is where he is.
void step_army_doors(GameState& state, Spot q);

}  // namespace fall_line::sim
