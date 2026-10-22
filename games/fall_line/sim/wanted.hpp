// THE WANTED LEVEL: the army hunting a skier who broke the law. Its numbers are wanted_rules.hpp,
// the state it moves `GameState::wanted` (wanted_state.hpp).
//
// Five stars. An OFFENCE raises the heat (`raise_heat`, THE hook: stealing a machine, downing a
// helicopter, running a soldier down) to at least its floor and its worth more than he had; from
// then on the hunt itself climbs it, a star every `wanted::kClimbS` while a pursuer has him in
// sight and at `kSearchClimb` of that pace while they only search, so the longer he survives the
// worse it gets. What each star sends is `wanted::kRoster` and `kWaves`; the parts that fly, ski
// and drive them (later rows) read those and write where each one is into `pursuers`.
//
// SEEN OR SEARCHING: every `kLookEveryS` the pursuers look, each within his kind's sight
// (`kSightM`), with no ground rising between his eyes and the quarry's chest, no woods between
// them from the snow, and no crown over his head from the air (`sees_quarry`). For `kFixS` after
// an offence they know where he is whatever they see. Seen, the stars are solid and the search
// centres on him; unseen, they flash, and after `kLoseS` out of sight the stars are dropped.
//
// THE SURVIVAL TIMER runs from the first raise to the hunt's end: the stars lost, or the ARREST
// (`arrest_quarry`) where the army does not shoot to kill, which ends the run.
//
// Nothing here draws a random number or allocates in a step, and a run that never raises the
// heat moves no digest (the state stays inactive and unhashed).
//
// ge:summary The wanted level: raise_heat, the climb, the look and the sight line, search, arrest
// ge:keywords wanted wanted-level stars heat raise_heat set_wanted step_wanted sees_quarry
// ge:keywords through_woods under_cover arrest_quarry survived_of roster_of ground_wave
// ge:keywords waves_sent quarry_of line-of-sight sight search survival-timer offence hunt
// ge:keywords pursuit police army wanted_hash wanted_change
// ge:skill sim
// ge:test tests/fall_line_wanted_test.cpp
#pragma once

#include <cstdint>
#include <optional>

#include "fall_line/sim/state.hpp"
#include "fall_line/sim/wanted_rules.hpp"
#include "fall_line/sim/wanted_state.hpp"
#include "mountain/mapgen/types.hpp"
#include "skier/trees.hpp"

namespace fall_line::sim {

// A point in the world frame, m.
struct Spot {
  double x = 0, y = 0, z = 0;
};

// Where the hunted man is: the player's skis under his boots. (A machine he rides carries him
// once the free ride's machines exist.)
Spot quarry_of(const GameState& state);

// RAISE THE HEAT for an offence: its worth more than he had, to at least its floor; they know
// where he is for `kFixS`. Begins a hunt on the run's first offence, or again after he escaped
// one. Returns the stars he carries now (0 once a hunt ended in his arrest: that run is over).
// Its `EventKind::wanted` events go into this step's `state.events`.
int32_t raise_heat(GameState& state, WantedCause cause);

// SET THE STARS to exactly `stars` (0..5), by hand (a lab, the developer's page); 0 drops them
// as a search run out would.
void set_wanted(GameState& state, int32_t stars);

// THE ARREST: a soldier reached him where the army does not shoot to kill. The hunt ends
// `arrested`, the survival timer stops. Refused (false) where the army shoots to kill, or when
// nobody hunts him.
bool arrest_quarry(GameState& state);

// THE TIME SURVIVED this hunt, s: still counting while it runs.
double survived_of(const WantedState& w, double t);

// What `stars` send (clamped to the table).
const WantedRoster& roster_of(int32_t stars);

// THE `n`th GROUND WAVE (0 the first) a hunt at `stars` sends, or none where that star sends
// none: each kind its base plus its growth for every wave before, rounded down, up to its cap.
std::optional<WaveMix> ground_wave(int32_t stars, int32_t n);

// How many ground waves the hunt has sent by run time `t`, counted from when it reached its star.
int32_t waves_sent(const WantedState& w, double t);

// WHETHER PURSUER `p` SEES THE MAN standing at `q`: within his kind's sight or near enough to
// hear; no ground within `kLineClearM` of the line to his chest; from the snow, not through the
// woods (`through_woods`); from the air, not `cover`ed past `kCoverNearM`.
bool sees_quarry(const ge::mountain::mapgen::Level& level, const ge::skier::TreeIndex& trees,
                 const Pursuer& p, Spot q, bool cover);

// WHETHER THE LINE from `a` to `b` passes through the crowns of `kWoods` trees or more.
bool through_woods(const ge::mountain::mapgen::Level& level, const ge::skier::TreeIndex& trees,
                   Spot a, Spot b);

// WHETHER HE IS UNDER COVER from the air: slower than `kCoverSpeed`, a crown over his head.
bool under_cover(const GameState& state);

// THE WANTED LEVEL'S STEP, on a run with a hunt running: the look, the climb, the search running
// out. `step` calls it after the racers (game.hpp has the order).
void step_wanted(GameState& state);

// What a wanted event says (`GameEvent::value`); its `gate` is the stars after it.
WantedChange wanted_change(const GameEvent& e);
const char* wanted_change_name(WantedChange c);
const char* wanted_cause_name(WantedCause c);
const char* wanted_end_name(WantedEnd e);

// FNV-1a 64 over the hunt, folded into `h`; nothing when it was never raised.
uint64_t wanted_hash(const WantedState& w, uint64_t h);

}  // namespace fall_line::sim
