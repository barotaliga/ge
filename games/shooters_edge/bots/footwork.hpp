// A FIGHTER'S FOOTWORK past its route: what its feet do that the pilot's way does not say.
// - THE DODGE: in a fight with the one it aims at in sight, the side key across the line to him
//   (the view is on him, so the side is across), from shooter/dodge.hpp at the fighter's own
//   `dodge` trait; not on a jump the route proved, a lift, a door, in water, on a button's errand
//   or as a co-op companion (who keeps its place by the leader).
// - THE LANDING: just climbed out of the water (shooter/swim.hpp's exit), straight onto the exit
//   node before the route from the nearest node takes over: that node may stand in the water,
//   and the route from it led a bot straight back in.
//
// ge:summary A fighter's footwork past its route: the dodge in a fight, the landing out of water
// ge:keywords footwork dodge side-step landing swim-exit fighter_dodge fighter_landing
// ge:skill bots
// ge:test tests/shooters_edge_fight_test.cpp
#pragma once

#include "ai/why.hpp"
#include "shooter/usercmd.hpp"
#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

// The dodge on `cmd` for player `me` at server time `t` (the side key replaced while it steps).
void fighter_dodge(Fighter& f, const server::RulesCommon& s, int me, double t, ge::ai::Why* why,
                   sh::MoveCmd& cmd);

// The landing on `cmd`: true while it holds (out of the water, `f.landing` set and not yet
// reached or timed out), the axes set straight at the exit node; reaching it ends the landing.
bool fighter_landing(Fighter& f, const server::RulesCommon& s, int me, double t, sh::MoveCmd& cmd);

}  // namespace shooters_edge::bots
