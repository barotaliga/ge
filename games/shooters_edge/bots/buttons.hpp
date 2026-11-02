// THE MAP'S BUTTONS: which doors each opens and which items those doors keep, read off the map's
// own entities (a `func_button`'s `target` names the `func_door`s it opens; one with `health`
// over 0 is opened by a shot, the rest by a touch), and how each kind of player uses them:
// - an AVERAGE or EXPERT player knows them: an item behind a shut door is gone for through its
//   button (`ItemSpot::button_*`), from the nearest place that reaches it (a touch) or sees it (a
//   shot);
// - a NOVICE knows none of it: now and then he shoots at a button he sees, or walks into it, to
//   see what it does (`button_poke`).
//
// An item is a door's when it stands inside the door's box grown by `kDoorKeeps`; read once a map,
// off the hot path.
//
// ge:summary The map's buttons: the doors each opens, the items they keep, a novice's random pokes
// ge:keywords button func_button shootable-button door func_door secret armour-door buttons_init
// ge:keywords button_poke button_node poke novice
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include "shooter/usercmd.hpp"
#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

constexpr float kDoorKeeps = 160;  // an item this near a door's box is behind it

// The buttons off the map's entity text, linked to the server's entities, and the items behind
// their doors (a novice's items get no link: he knows no button).
void buttons_init(Fighter& f, const ge::shooter::NavGraph& g, const server::QcServer& s,
                  const char* entities);

// A novice's whim, at a think: a button in sight now and then becomes where he goes and what he
// shoots at. True while he is on one.
bool button_poke(Fighter& f, const server::QcServer& s, int me, double t);

// The poke itself, once there: the view on the button and the trigger (or the walk into it).
void button_poke_cmd(Fighter& f, const server::QcServer& s, int me, double t,
                     ge::shooter::MoveCmd& cmd);

}  // namespace shooters_edge::bots
