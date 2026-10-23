// THE GUNFIRE: the one layer every shooter the army sends fires through (the soldiers on skis, the
// snowmobile gunners, and the door gunners, the paramotor riflemen as their rows land). Its
// numbers are `weapons_rules.hpp`'s.
//
// A SHOOTER is a GUN its owner arms (`arm_gun`), puts at the muzzle and moves with the platform
// every step, and fires with `fire_at`: a BURST at the hunted man or at a point. The burst is aimed
// once: the LEAD on the target's way over the round's time of flight, relative to the gun's own,
// and the DROP over it; then the shooter's error, drawn once a burst (his hold, widened early in a
// hunt; the share of the lead he misjudges). Its rounds leave at the cyclic rate, each with the
// weapon's own dispersion and the muzzle's climb, and are FLOWN (slowed by the air as v' = -k v^2
// and pulled down by gravity) and tested along every step's path against the snow (found by
// halving), the trunks (upright cylinders) and his body (gunfire_body.hpp).
//
// A ROUND IN HIM is a penetrating wound on the part it struck, its dose the round's energy there
// (`Mech::pierce`, the wounds in `skier/anatomy.hpp`), judged by the body (`judge_body` on the
// racer's `body`): the injuries, the severity score and a mortal wound are the body's like any
// other's, so enough hits kill, and death ends the hunt `killed` (wanted.hpp).
//
// THE RULES OF FIRE come off the wanted level's roster (`hold`, `warn`, `aim`), and where the army
// does not shoot to kill (`WantedState::lethal` false) every burst is warning fire: put down in
// the snow to a side of him or ahead and never tested against his body. The army arrests him
// instead.
//
// Every spread and burst is drawn off the gunfire's OWN stream (`GunfireState::stream`, seeded off
// the map), never `state.rng`; and `GunfireState` stays inactive until a gun is armed, so a run
// that never arms one moves no digest. Nothing here allocates once armed: the guns and the rounds
// are reserved at the first arm.
//
// ge:summary The gunfire: guns armed, bursts aimed and led, rounds flown, struck and wounding
// ge:keywords gunfire gun arm_gun fire_at step_gunfire burst round aim lead drop flight_time
// ge:keywords speed_at energy_at warning-fire rules-of-fire wound pierce hit snap tracer
// ge:skill sim
// ge:test tests/fall_line_gunfire_test.cpp
#pragma once

#include <cstdint>

#include "fall_line/sim/state.hpp"

namespace fall_line::sim {

// ARM A GUN: gun `id` with `weapon` (the gunfire made active on the first), its muzzle at
// (x, y, z). Its owner moves it every step after. Null when every gun's place is taken.
Gun* arm_gun(GameState& state, int32_t id, WeaponKind weapon, double x, double y, double z);
// Put gun `id` away; its rounds in flight fly on.
void disarm_gun(GameState& state, int32_t id);
// Whether a gun is ready for a burst: not firing one, not waiting.
bool gun_ready(const Gun& gun);
// THE RULES OF FIRE a burst is fired under: `asked`, never `aim` where the army does not shoot to
// kill.
FireRule rules_of(const GameState& state, FireRule asked);
// PULL THE TRIGGER: a burst from `gun` at `target` under the rules asked. False when it is not
// fired: the gun busy or waiting, the rules `hold`, the target past its reach.
bool fire_at(GameState& state, Gun& gun, const AimTarget& target, FireRule asked);

// THE GUNFIRE'S STEP, after the army's: every burst's rounds fired at the cyclic rate, every round
// flown and struck, the wounds judged on his body. Allocates nothing.
void step_gunfire(GameState& state);

// THE TIME a round takes to fly `d` m, s (t = (e^(k d) - 1) / (k v0)).
double flight_time(const GunDef& def, double d);
// THE SPEED a round still has `d` m out, m/s, and the energy it carries there, J.
double speed_at(const GunDef& def, double d);
double energy_at(const GunDef& def, double d);

// A gunfire event's phase and, on a hit, the part.
GunfirePhase gunfire_phase(const GameEvent& e);
ge::skier::Part gunfire_part(const GameEvent& e);
const char* gunfire_phase_name(GunfirePhase p);
const char* weapon_name(WeaponKind k);

// FNV-1a 64 over the gunfire, folded into `h` (nothing while inactive).
uint64_t gunfire_hash(const GunfireState& g, uint64_t h);

}  // namespace fall_line::sim
