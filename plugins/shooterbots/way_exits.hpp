// The way through's exits kept shut (way_through.hpp): a route that runs into a level change's box
// on its way somewhere else ends the map early (base3's exit back to base2 lies across a hall he
// crosses for its keys), so every link into an exit's box is shut to his routes until that exit is
// the step he is on.
//
// ge:summary Exits shut to his routes: links into a level change's box until it is the step
// ge:keywords way_exit_links way_shut_exits exit changelevel trigger crossing early-exit route
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#pragma once

#include <vector>

#include "shooterbots/nav.hpp"
#include "shooterbots/way_through.hpp"

namespace ge::shooterbots {

// The links whose line runs into an exit step's box from outside it (a standing player's box at
// each end and along the way), kept as `w.exit_links` (way_init, once the steps are made). A link
// out of a box (he starts in base2's car up to base1) is not one: leaving it changes nothing.
void way_exit_links(WayThrough& w, const NavGraph& g);

// The links into every exit but the step he is on (`w.step`), added to his routes' shut links.
void way_shut_exits(const WayThrough& w, const NavGraph& g, std::vector<NavExtraLink>& shut);

}  // namespace ge::shooterbots
