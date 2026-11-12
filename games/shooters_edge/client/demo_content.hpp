// THE FILES A DEMO DRAWS FROM (docs/hub.md § Watching in a browser): what the hub's content pack
// for a demo holds (engine/hub/content.hpp, `HubSpec::demo_files`). The demo is read through once:
// every level's map (`maps/<map>.bsp`, and its coloured light `maps/<map>.lit` when a pack has
// one), every model and sound its precache lists name (the map's own inline models left out), and
// what any picture of this game needs whatever the demo holds: the palette and colour map, the
// player's head and gibs, the thrown and fired models the effects draw, and the particle effects.
//
// ge:summary The content paths a demo needs: its maps, precached models and sounds, the basics
// ge:keywords demo content files precache models sounds map hub content-pack demo_files
// ge:skill shooter net
// ge:test tests/shooters_edge_demo_content_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace shooters_edge::client {

// `demo` (a recording, packed or a replay file) read into the names it needs, lower case and
// sorted. False with the reason when it does not open.
bool demo_files(std::span<const uint8_t> demo, std::vector<std::string>& names, std::string& error);

}  // namespace shooters_edge::client
