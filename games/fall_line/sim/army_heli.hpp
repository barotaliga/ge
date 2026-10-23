// THE ARMY'S HELICOPTERS: the machines the wanted level's roster asks for (`WantedRoster::
// helicopters`), called in, flown after him, and the soldiers they carry (`WantedRoster::
// soldiers`) put down near him. Its numbers are army_heli_rules.hpp, where it sets down
// army_land.hpp.
//
// - THE CALL: when the roster asks for more machines than are out, each is called; after the
//   crew's scramble (`kRespondS`, the next `kStaggerS` after) it comes in at the MAP'S EDGE on a
//   bearing from the map's middle toward where he was when the first was called, each machine
//   called with it spread either side, `kEnterM` over the snow, at its cruise, a section aboard.
//   Machines past what is asked go home (one on a landing lifts off first).
// - THE FLIGHT is a point flown with a pilot's limits: the velocity it wants, the acceleration
//   that asks for (no more than the disc leant `kTilt` gives, smoothed over `kLagS` so the bank
//   rolls in), its attitude read off that acceleration as a helicopter's is: leant into it, the
//   nose hung down at speed. Its height is held `kClearM` over the snow it reads ahead and never
//   let under the floor over the snow and the crowns under its disc (`army_floor`). Two machines
//   keep `kApartM` apart.
// - THE HUNT: where he is seen, a PYLON TURN round him (`kOrbitM` out, `kOrbitUpM` up, flown at
//   `kOrbitSpeed`), its nose along the circle so he is square out of the inward door; a man who
//   runs faster than the circle can hold is flown in ECHELON, behind him and to the side at his
//   speed. Out of sight they SEARCH a wide, high, slow circle round where he was last seen, and
//   until the first machine has reached that search its clock does not run (the stars flash, but
//   are not lost to a hunt nobody has yet flown). Each machine's choice is a decision recorded on
//   `GameState::why` (agent `kAgentBase` + its id).
// - THE LANDINGS, asked once a second while he is on the snow: a machine on station with men
//   aboard and the roster short of soldiers is given a DROP near him (army_land.hpp); it flies in,
//   flares, sinks onto the spot (or holds the rope's hover over it), its doors slide open, and a
//   man comes out every `kEveryS` from each door in turn, runs out to his side and kneels facing
//   out until it has lifted, or slides down the rope; then they chase him as every soldier does
//   (army.hpp). With more than one machine out the first stays up as the OVERWATCH and never lands.
//   A drop he has skied `kStaleM` from before a man is out is given up. THE LEAPFROG: a squad left
//   far behind is PICKED UP (its machine lands by them, they ski to its doors) and put down again
//   ahead of him.
// - HOME: the hunt over, every machine turns for the map's edge, climbing, and is gone past it.
// - Every machine out is written into the wanted level's pursuers (`kPursuerBase` + its id), so its
//   crew's eyes keep the stars up.
//
// The door guns are manned and fired by army_door.hpp, through gunfire. `step_army` steps the
// machines after the men and their snowmobiles. Nothing here draws a random number or allocates in
// a step (a machine's way round its circle is its id's own), and `ArmyState::helis` stays empty on
// every run whose hunt never reached a star.
//
// ge:summary The army's helicopters: the call, the pylon turn, the search, the landings, the rope
// ge:keywords army helicopter army-heli step_army_helis call pylon-turn orbit echelon search
// ge:keywords landing drop pick-up leapfrog fast-rope overwatch dismount kneel board leave
// ge:keywords pursuers army_helis_hash step_heli_soldier why decision
// ge:skill sim
// ge:test tests/fall_line_army_heli_test.cpp
#pragma once

#include <cstdint>

#include "fall_line/sim/army_state.hpp"
#include "fall_line/sim/state.hpp"
#include "fall_line/sim/wanted.hpp"

namespace fall_line::sim {

// THE HELICOPTERS' STEP: the call for the roster's machines, every machine flown, the landings
// asked for and worked, and the crews written into the pursuers. `on` is whether a hunt is running
// with a star; `q` where he is.
void step_army_helis(GameState& state, bool on, Spot q);

// ONE STEP OF A SOLDIER A HELICOPTER PUT DOWN OR COMES FOR (`out`, `rope`, `board`); returns
// whether he means to be moving. One done with it goes on to the chase (or, aboard, is gone).
bool step_heli_soldier(GameState& state, Soldier& s);

// Whether the machine is about a landing: on its way down to it, on the snow, hovering for the
// rope, or climbing back off.
bool landing(const ArmyHeli& h);

// FNV-1a 64 over every machine, folded into `h`.
uint64_t army_helis_hash(const ArmyState& a, uint64_t h);

const char* army_heli_mode_name(ArmyHeliMode m);
const char* army_heli_change_name(ArmyHeliChange c);

}  // namespace fall_line::sim
