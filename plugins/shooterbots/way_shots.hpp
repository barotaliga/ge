// The way through's shot buttons (way_through.hpp): the node each is shot from, at its face and
// on the map's own floor, and one he reaches when that node is not.
//
// ge:summary The way through's shot buttons: shot at their face, from a node he reaches
// ge:keywords way_face_shot_buttons way_reach_shot_buttons shot-button button-face bridge
// ge:keywords func_button health r1m4
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#pragma once

#include <vector>

#include "shooterbots/bot_view.hpp"
#include "shooterbots/entities.hpp"
#include "shooterbots/fighter.hpp"
#include "shooterbots/map_nav.hpp"
#include "shooterbots/nav.hpp"
#include "shooterbots/way_through.hpp"

namespace ge::shooterbots {

// (`complete`) Each shot button's face (the side its press pushes away from: `face`, from its
// angle), and a button shot from a node on a bridge still in (where nothing stands) shot from
// the nearest node within kWayShotAcross at its face instead: no more than 60 degrees off
// straight out, seeing it, off any mover and any such bridge. Other nodes are kept.
void way_face_shot_buttons(Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s,
                           const std::vector<EntityBlock>& blocks);

// (`complete`, after the plan's reach) A shot button whose node he does not reach is shot from the
// nearest node at its face he does: r1m4's, high on the wall across the lava from the start, was
// given a ledge behind it, out of reach till the bridge it slides out was there, and he stood at
// the start with nothing to do; now he shoots it from the end of the corridor across the lava.
// Looked for again each kWayRepick_s while unreached.
void way_reach_shot_buttons(WayThrough& w, Fighter& f, const NavGraph& g,
                            const ge::shooterbots::BotView& s, double t);

}  // namespace ge::shooterbots
