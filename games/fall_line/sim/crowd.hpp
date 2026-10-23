// THE CROWD: the amateurs out on the ski area on a free ride, hundreds of people skiing its runs
// top to bottom, riding the lifts back up, falling over, stopping where they should not and
// getting in the player's way. Who they are and how they come is crowd_rules.hpp.
//
// ROUGH AND CHEAP ON PURPOSE. An amateur is not a skier on the snow's physics: he is a point
// sliding along his run's line, kept as how far down it he is (`s`) and how far right of it (`d`),
// with a speed the fall along the run's pitch feeds and the snow, the air and his skis turned off
// the line scrub, held to the pace he means (`cap`). His turns are a wave across the piste in the
// run's own arc, round or sharp, wide or narrow by his style, so he steers for a lateral, never for
// a point, and a ski school's children set to the instructor's wave ski in his very track. He
// DECIDES only every `kThinkS` seconds, each at his own phase of it (a fall, a stop, room from the
// one ahead or the player, keeping up with his group); between decisions a step is a handful of
// multiplies and one ground sample.
//
// THE NETWORK he skis is the free ride's runs: reaching a run's end he carries on down the one it
// merges into, keeping where he is across the snow, or at its foot joins the QUEUE of the lift
// there (a ticket; his place is his ticket less the ones taken up, in rows of a carrier's seats
// behind the load point), is taken up by the carrier that passes the load point while he stands at
// the front (`lift_loop.hpp`'s clock, as the player and the army are), let go at its top beside
// his seat, and skates down the ramp of the run his group chose there. A run with no lift at its
// foot sends him up one unseen until his group is all down and the ride (`kUnseenLift`) is over.
//
// THE PLAYER MEETS THEM (`meet_crowd`) as a racer meets another (field.hpp): his circle against
// each amateur's, pushed apart by their masses and the closing speed traded at `kRestitution`, a
// `bump` event past `kBumpEvent`. Hard enough (`kKnock`, by the amateur's skill) and the amateur
// goes down; the player is shouldered, never thrown. Amateurs dodge the one ahead of them on their
// run and the player when he is ahead: nobody looks uphill.
//
// THE STREAM is the crowd's own (`CrowdState::rng`, seeded off the map's seed with a salt), so a
// crowd draws nothing from any other and a run dealt none is the run it always was. A step
// allocates nothing: every buffer is sized at the deal.
//
// ge:summary The crowd: amateurs dealt in groups, skiing the runs, riding the lifts, met by the
// player ge:keywords crowd amateur amateurs deal_crowd step_crowd meet_crowd crowd_hash group queue
// ge:keywords ticket ride skate unseen lift free-ride bump dodge room regroup stop fall
// ge:skill sim
// ge:test tests/fall_line_crowd_test.cpp
#pragma once

#include <cstdint>

#include "fall_line/sim/crowd_rules.hpp"
#include "fall_line/sim/crowd_state.hpp"
#include "fall_line/sim/state.hpp"

namespace fall_line::sim {

// DEAL THE CROWD onto a free ride: `count` amateurs (at most `crowd::kMost`) in their groups, most
// of them on the runs and a share in the lifts' queues. Once, after `create_free_ride`; a race or a
// map with no run deals none.
void deal_crowd(GameState& state, int32_t count = crowd::kCount);

// Step the crowd one step: the groups up a lift unseen, the lifts' queues, carriers and tops,
// every decision due and every amateur's way down his run.
void step_crowd(GameState& state);

// Every racer against the crowd, after the crowd has moved. Writes `bump` events.
void meet_crowd(GameState& state);

// FNV-1a 64 over every amateur and the crowd's stream, folded into `h`.
uint64_t crowd_hash(const CrowdState& crowd, uint64_t h);

}  // namespace fall_line::sim
