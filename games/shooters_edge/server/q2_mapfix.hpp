// QUAKE II'S MAPS MENDED AS A SERVER LOADS THEM (docs/servers.md § The settings):
//
// - THE VISIBILITY PATCH (`sv_vispatch`, 1 by default): four stock deathmatch maps' compiled
//   visibility leaves out clusters a player at a few spots can plainly see, so players and items
//   there vanish or pop in. Each map is known by its file's checksum (configstring 31's, never its
//   name), and the visible and hearable rows of the clusters at those spots gain the missing
//   clusters; nothing is ever taken away. q2dm1 (80717714): cluster 395 sees 176 and 183. q2dm3
//   (509300933): 345 and 384 see 466, 484 and 692. q2dm8 (742046128): 629, 631, 633 and 639 see
//   908, 909, 910, 915, 923, 924, 927, 930, 938, 939 and 947. mgu6m2 (515801089): every cluster
//   sees 213, 214 and 217. A client is told the same checksum as before.
// - ENTITY FILES (`sv_entpath`, a folder in the game's files; empty, the default: none): a map's
//   entities replaced by `<folder>/<map>@<crc>.ent` (the crc four lower-case hex digits of the
//   CRC-16 of the map's own entity text, so the file fits that build of the map alone), else by
//   `<folder>/<map>.ent`. A file under 2 bytes is refused and the map's own entities kept. Clients
//   see nothing different: the checksum and the name stay the map's.
//
// ge:summary Quake II's maps mended at load: four stock maps' visibility, entities from a file
// ge:keywords q2 vis-patch sv_vispatch map_visibility_patch entity-file sv_entpath ent-file
// ge:keywords map_override_path q2dm1 q2dm3 q2dm8 mgu6m2 q2_vis_patch q2_entity_file_name
// ge:skill shooter brush
// ge:test tests/shooters_edge_q2_mapfix_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "brush/map.hpp"

namespace shooters_edge::server {

// The visible and hearable rows `map` gains when its file's checksum is one of the four above;
// false (the map unchanged) for any other.
bool q2_vis_patch(ge::brush::Map& map, int32_t checksum);
// The entity file named for this build of the map: `<map>@<crc>.ent`.
std::string q2_entity_file_name(const std::string& map, const std::string& entity_text);

}  // namespace shooters_edge::server
