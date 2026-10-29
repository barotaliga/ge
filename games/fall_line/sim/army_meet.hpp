// HIM AGAINST THE ARMY ON THE SNOW: the skier the army is after meeting its machines and its men
// where they stand, after they have moved (`step_army`). Their numbers are army_sled_rules.hpp's
// and army_rules.hpp's.
//
// - THE HULL: every army snowmobile's plan is the capsule its trunks are held out of, and he is
//   held out of it too: pushed apart by their masses (a machine with its crew, or empty), the
//   closing speed traded back at `army_sled::kRestitution`. Clear over its seat he passes over.
// - ITS CREW KNOCKED OFF: a crewed machine skied into at `army_sled::kKnockOffSpeed` or more of
//   his own puts its driver and gunner over beside it (`knock_off`: soldiers lying where they
//   fell, the machine left standing empty to be stolen) and is a `hit_soldier` offence.
// - THROWN: a closing speed of `army_sled::kThrowSpeed` or more puts him down too (a wipeout, as
//   into a trunk), whether he skied into it or it drove into him.
// - A MAN ON FOOT: each soldier on the snow is a circle he is held out of the same way; skied into
//   at `army::kRunDownSpeed` or more he goes down (`SoldierMode::down`), an `army` event
//   (`run_down`) and a `hit_soldier` offence.
// - A VEHICLE (army_vehicle.hpp): each hull (a carrier's two units) is a box of steel he is held
//   out of, giving nothing; met at `army_sled::kThrowSpeed` he goes down, and where the army shoots
//   to kill a front driven into him faster than `army_vehicle::kStrikeClosing` knocks him down
//   (`army_vehicle` event, `strike`).
//
// Only the player (racer 0), on his skis, off any lift and any machine, still in the run; nothing
// while the army has never been sent. Draws nothing and allocates nothing.
//
// ge:summary The skier meeting the army: its machines' hulls, a crew knocked off, a man run down
// ge:keywords army meet_army hull knock-off knocked run-down hit_soldier crew push-apart thrown
// ge:keywords skier-against-machine soldier-down
// ge:skill sim
// ge:test tests/fall_line_army_meet_test.cpp
#pragma once

#include "fall_line/sim/state.hpp"

namespace fall_line::sim {

// The player against every army machine and every soldier on the snow, after the army moved.
// Writes `wipeout`, `army` and `army_sled` events and raises the heat for what he knocks over.
void meet_army(GameState& state);

}  // namespace fall_line::sim
