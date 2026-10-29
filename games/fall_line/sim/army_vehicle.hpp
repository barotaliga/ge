// THE GROUND WAVES' VEHICLES: the tracked carriers, the trucks and the tanks a ground wave's mix
// names (`ground_wave`'s `tracked`, `trucks` and `tanks`), driven up the mountain at the man the
// army has hunted long enough. Their numbers are army_vehicle_rules.hpp's; a run holds them in
// `ArmyState::vehicles`.
//
// - THE CALL (`call_vehicles`, from the wave's send): they set off from the way the wave climbs,
//   `kSetOffBackM` further down it than the column forms up (out of his sight), one after another
//   `kStaggerS` apart, and drive THE WAY UP (the runs and the cat tracks, as graded as anything on
//   a mountain) as far as it goes. A truck on a map whose village has a road out comes in along it
//   from the map's edge, and on up the way from the road's end. At most `kMost` out by stars.
// - THE DRIVE: four times a second each reads the snow ahead (army_drive.hpp, as the snowmobiles
//   do) and steers the cheapest heading toward the next point of its way (or, the way driven, at
//   him). The move itself is a ground vehicle's, kept simple on purpose: its pace gathered and
//   shed at its own rates, slowed up a rise in proportion to the steepest it climbs and stopped at
//   one steeper; its heading turned at its own rate (tracks pivot, a truck turns no tighter than
//   its wheels); set on the snow, its attitude read off the ground under its ends and sides. A
//   trunk (thicker than a sapling) at its front, the map's edge or another vehicle in its way
//   stops it (one on the move only when it was called first: two never wait on each other). Held
//   short with its way well off its nose, a truck backs with its wheels turned until it is round.
//   The trees the village felled for its streets are not in its way. A CARRIER'S REAR UNIT trails
//   the front about the steered hitch as a trailer does.
// - ITS WATCH: a window in which it meant to move and barely did swings it off to a side; a second
//   backs it down its own track; a third puts it back on its way at the point it was making for
//   (`ArmyState::vehicles_stuck`, the lab's count: always 0).
// - A CARRIER drives up the way and then at him; within `kDropCarrierM` with him in sight (or
//   `kDropBlindM` out of it), or backed out twice, it halts and its section gets out of the back
//   and goes after him as every soldier on the ground does (army.hpp). It stays where it put them
//   down (`park`), its commander watching him from the hatch.
// - A TRUCK drives the road and the way, then at him as far as its chains hold: within
//   `kDropTruckM` in sight (`kDropBlindM` out of it), or held by a rise it cannot take, its men get
//   out. Put down further than `kChaseM` from him they climb on with the wave up its way; nearer,
//   they go straight at him. Emptied, it drives back down.
// - A TANK drives up the way and then at him, standing off `kTankLethalM` (`kTankArrestM` where
//   the army does not shoot to kill), its turret turned on him; within `kRamM` with him in sight,
//   where the army shoots to kill, it runs him down. (Its main gun is a later row.)
// - Every vehicle but a tank running him down stops `kStopM` short of him; meeting him is
//   army_meet.hpp's (the steel he is held out of, and a moving front that knocks him down).
// - HOME: further than `kBehindM` from where he was last seen, or the hunt over and `kHomeM` from
//   him, it is taken off the map; a nearer one drives back down its way and is gone at its foot or
//   after `kLeaveS`.
// - Every crewed vehicle on the snow is one of the hunt's pursuers (`kPursuerBase` + its id).
//
// Nothing here draws a random number: where each comes from, its side and its pauses are its id's
// own. A vehicle's way is laid once, when it is called; nothing in a step allocates.
// `ArmyState::vehicles` stays empty, and nothing of it is hashed, on every run whose hunt never sent
// a wave that names one.
//
// ge:summary The army's ground vehicles: carriers, trucks and tanks called, driven, unloaded, home
// ge:keywords army vehicle carrier truck tank step_army_vehicles call_vehicles drive unload drop
// ge:keywords park stand-off ram hitch road-in watch stuck pursuers army_vehicles_hash
// ge:skill sim
// ge:test tests/fall_line_army_vehicle_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "fall_line/sim/army_state.hpp"
#include "fall_line/sim/army_vehicle_rules.hpp"
#include "fall_line/sim/state.hpp"
#include "fall_line/sim/wanted.hpp"

namespace fall_line::sim {

// THE VILLAGE'S ROAD IN, from the map's edge to its end (`points`, every `kRoadEvery`th of its
// street's).
struct ArmyRoad {
  std::vector<VehiclePoint> points;
};

// THE WAVE'S VEHICLES brought up the valley: `mix`'s carriers, trucks and tanks for wave number
// `wave`, setting off from way `way` (`GameState::ways`) at the arc `kSetOffBackM` below `s_head`
// (where the wave's column forms up), up it to `s_end` (the arc nearest him).
void call_vehicles(GameState& state, const WaveMix& mix, int32_t wave, int32_t way, double s_head,
                   double s_end);

// THE VEHICLES' STEP: every vehicle out driven, halted, emptied or sent home, and the crews
// written into the pursuers. `on` is whether a hunt is running with a star; `q` where he is.
void step_army_vehicles(GameState& state, bool on, Spot q);

// A kind's body and drive.
const army_vehicle::Spec& spec_of(ArmyVehicleKind kind);

// Where a carrier's rear unit's middle is, in the plan, m.
struct PlanSpot {
  double x = 0, z = 0;
};
PlanSpot rear_of(const ArmyVehicle& v);

// Whether a vehicle is on the snow with its crew in it (set off and not yet gone).
bool on_snow(const ArmyVehicle& v);

// FNV-1a 64 over every vehicle, folded into `h`.
uint64_t army_vehicles_hash(const ArmyState& a, uint64_t h);

const char* army_vehicle_kind_name(ArmyVehicleKind k);
const char* army_vehicle_mode_name(ArmyVehicleMode m);
const char* army_vehicle_change_name(ArmyVehicleChange c);

}  // namespace fall_line::sim
