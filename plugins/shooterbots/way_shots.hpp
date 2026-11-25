// The way through's shot buttons on a bridge (way_through.hpp): a shot button whose node lies on
// a bridge still in, shot from off it at its face.
//
// ge:summary A shot button whose node is on a bridge still in: shot at its face from off it
// ge:keywords way_face_shot_buttons shot-button button-face bridge func_button health r1m4
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#pragma once

#include "shooterbots/bot_view.hpp"
#include "shooterbots/fighter.hpp"
#include "shooterbots/map_nav.hpp"

namespace ge::shooterbots {

// (`complete`) A shot button given a node on a bridge still in (where nothing stands until the
// button runs it out) is shot instead from the nearest node within kWayShotAcross at its face
// (the other side from its `push`; no more than 60 degrees off straight out), seeing it, off any
// mover and any such bridge. r1m4's, across the lava from the start, slides out the bridge it was
// given a node on: he stood at the start for the whole map. Any other node is kept (one he does
// not reach is sought as the plan goes: `shotplace`, way_gates.hpp).
void way_face_shot_buttons(Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s);

}  // namespace ge::shooterbots
