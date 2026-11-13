// THE FILES A DEMO DRAWS FROM (docs/hub.md § Watching in a browser): what the hub's content pack
// for a demo holds (engine/hub/content.hpp, `HubSpec::demo_files`). The demo is read through once:
// every level's map (`maps/<map>.bsp`, and its coloured light `maps/<map>.lit` when a pack has
// one), every model and sound its precache lists name (the map's own inline models left out), and
// what any picture of this game needs whatever the demo holds: the palette and colour map, the
// player's head and gibs, the thrown and fired models the effects draw, and the particle effects.
//
// THE SEQUEL'S (demo.hpp § SEQUEL): its maps, the folder of each model it names (the model, its
// skins), each player's model folder (`players/<model>/`: the model, skins and own sounds), the
// sounds it names (a player's own, `*name`, are in that folder), its pictures (`pics/<name>.pcx`)
// and the palette (`pics/colormap.pcx`). Its maps keep their textures in files of their own, so
// `content_needs` names a map's textures (`textures/<name>.wal`) when the hub finds the map; and
// for a map of either game, its own sky box's six faces (`env/<sky><side>`).
//
// ge:summary The content paths a demo needs: its maps, precached models and sounds, the basics
// ge:keywords demo content files precache models sounds map hub content-pack demo_files
// ge:keywords sequel dm2 wal textures content_needs file_needs
// ge:skill shooter net
// ge:test tests/shooters_edge_demo_content_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace shooters_edge::client {

// `demo` (a recording, packed or a replay file) read into the names it needs, lower case and
// sorted. False with the reason when it does not open.
bool demo_files(std::span<const uint8_t> demo, std::vector<std::string>& names, std::string& error);

// The files `name` (its `bytes`) draws from in turn, appended to `more` (`HubSpec::file_needs`): a
// map that keeps its textures outside names each one's file, and a map with a sky box its faces.
void content_needs(std::string_view name, std::span<const uint8_t> bytes,
                   std::vector<std::string>& more);

}  // namespace shooters_edge::client
