// THE ARMY'S SNOWMOBILES: the military over-snow machines the wanted level sends after a man on
// the snow, two up: a driver and a gunner on the pillion behind him. The machine is the free
// ride's own (`plugins/snowmobile`: `step_sled` on the same snow, the same trunks and the same
// edge) and the hands on it the bot's (`sled_pilot`). Its numbers are army_sled_rules.hpp.
//
// - THE CALL: the roster's `snowmobiles` at his stars, plus what the ground waves keep out on top
//   (`ArmyState::wave_sleds`). Each machine called waits for its crew to mount up (`kRespondS`,
//   the next `kStaggerS` after) and leaves the village from beside the free ride's snowmobile
//   spot, side by side with the ones before. Machines past what is asked go home. A map with no
//   free ride snowmobile spot sends none.
// - READING THE SNOW (army_drive.hpp): ten times a second each machine weighs a fan of headings
//   either side of the way to him and steers the cheapest, slowing among the trees. One that
//   barely moves in a window swings off to a side; one that still does not is dug out and stood
//   on open snow facing him. One lying on its side is stood back on its belt by its crew. One below
//   him on a face steeper than it climbs (the grade to him past `kRise`, the way straight at him
//   stopped within `kBlockedM`) waits where it is until he comes within its reach.
// - UP THE RUNS: further than `kDirectM` from him and him near a run (the ways the ground waves
//   climb), a machine below him on it climbs it, the open groomed snow, to the arc nearest him.
// - THE CHASE: at him (a second ahead of where he is going) while its crew see him (their eyes
//   low on the snow: the woods hide him from them), else at where he was last seen. Where the army
//   shoots to kill it keeps `kStandLethalM` off him and makes SHORT HALTS with him in sight and in
//   range: side-on to him, braked to a stop, the gunner on him a few seconds, then on again (the
//   bursts are gunfire's, a later row). Where it does not, it runs him down, and once he is slow or
//   down and it is close the crew get off and take him in on foot as soldiers (army.hpp), the
//   machine left standing empty.
// - HOME: the hunt over, it drives back to the village and is gone there. Empty machines are kept
//   up to `kMostEmpty`, and let go once the hunt is over and he is far from them.
// - Every machine with its crew on it is written into the wanted level's pursuers
//   (`army_sled::kPursuerBase` + its id), so its crew's eyes keep the stars up.
//
// `step_army` steps them after the soldiers. Nothing here draws a random number or allocates in a
// step (a machine's side and its pauses are its id's own), and `ArmyState::sleds` stays empty on
// every run whose hunt never reached a star that sends them.
//
// ge:summary The army's snowmobiles: the call, the drive over the snow, the short halt, the arrest
// ge:keywords army snowmobile sled army-sled step_army_sleds call drive halt short-halt stand-off
// ge:keywords dismount empty home dug righted pursuers sled_pad army_sled_hash
// ge:skill sim snowmobile
// ge:test tests/fall_line_army_sled_test.cpp
#pragma once

#include <cstdint>

#include "fall_line/sim/army_state.hpp"
#include "fall_line/sim/state.hpp"
#include "fall_line/sim/wanted.hpp"

namespace fall_line::sim {

// Where the `k`th machine called stands in the village: beside the snowmobile spot, out to
// alternate sides of it `kApartM` a place, on the first snow clear of trunks; facing the way the
// free ride's is parked.
struct SledPad {
  double x = 0, z = 0, heading = 0;
};
SledPad sled_pad(const GameState& state, int32_t k);

// THE SNOWMOBILES' STEP: the call for `want` machines, every machine driven, halted, emptied or
// sent home, and the crews written into the pursuers. `on` is whether a hunt is running with a
// star; `q` where he is; `fire` the rules of fire at his stars.
void step_army_sleds(GameState& state, bool on, Spot q, FireRule fire);

// Whether a machine carries its crew: called, on its way, halted or going home.
bool crewed(const ArmySled& m);

// FNV-1a 64 over every machine, folded into `h`.
uint64_t army_sleds_hash(const ArmyState& a, uint64_t h);

const char* army_sled_mode_name(ArmySledMode m);
const char* army_sled_change_name(ArmySledChange c);

}  // namespace fall_line::sim
