// Quake II's misc_actor and target_actor (porting/shooters-edge/q2-actor.md): a scripted figure on
// the monster rules (its row in q2_monster_kinds.hpp, a good guy) that stands until used, then
// walks from target_actor to target_actor. At each point it says the point's message to everyone
// as `<name>: <message>`, jumps (1), does nothing (2), attacks what the point's pathtarget names
// (4, holding its ground with 16), or fires the pathtarget; then on to the point's target, or it
// stands. Shot by a player it taunts him 40% of the time (`<name>: <word>!`, to him alone) instead
// of a pain, and it never fights players. Its name is one of eight, by its entity number.
//
// ge:summary Quake II's misc_actor and target_actor: the scripted figure, its points, its taunts
// ge:keywords q2 misc_actor target_actor actor scripted-figure taunt flipoff path-point jump
// ge:skill shooter
// ge:test tests/shooters_edge_q2_actor_test.cpp
#pragma once

#include <string>

namespace shooters_edge::server {

struct Q2Server;
struct Q2Monster;

// The actor's name (Hellrot, Tokay, ...) for entity row `e`, by its entity number on the wire.
std::string actor_name(const Q2Server& s, int e);

// Set up after the monsters spawn: an actor's `health` key, if set, is its health.
void actor_spawn(Q2Server& s);

// Actor `m` used: on to the target_actor its target names, or the game's line and it stands.
bool actor_use(Q2Server& s, Q2Monster& m);

// Actor `m` at the point it walked to (`m.corner`): the point's message, action and next point.
void actor_arrive(Q2Server& s, Q2Monster& m);

// Actor `m` hurt by `attacker` while its pain is not `waiting`: true when it taunted a player
// in place of a pain (the pain table picks one otherwise).
bool actor_pain(Q2Server& s, Q2Monster& m, int attacker, bool waiting);

}  // namespace shooters_edge::server
