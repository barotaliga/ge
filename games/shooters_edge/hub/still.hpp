// A MAP'S STILL FOR THE HUB (docs/hub.md § Match cards): the map drawn once from its intermission
// view (cards.hpp's `still_view`) by the window's own world draw (client/world_draw.hpp) in the
// game's CLASSIC look with classic enhance off (`r_look classic`, `ce_enabled 0`: the packs as
// they are, crisp texels, the lightmaps, the palette), no weapon in hand and nobody standing in
// it, on whatever GPU adapter the machine has (Mesa's lavapipe in software on a server with none).
//
// THE KEY the hub keeps it by (engine/hub/stills.hpp): a digest of the map's file, of the source
// of every directory whose code draws it (`kStillCodeStamp`, cmake/SourceStamp.cmake: the world
// draw, the brush renderer, the classic look, the engine's renderer and GPU layer and their
// shaders) and of the still's size, so a changed map or a new build of the renderer draws it
// again. Nothing drawn is committed: the pictures are made from each hub's own packs.
//
// ge:summary A map's still for the hub: its intermission view drawn in the classic look, its key
// ge:keywords hub still map-still intermission classic-look ce_enabled world-draw lavapipe key
// ge:keywords source-stamp cache-key still_key draw_still
// ge:skill net render shooter
// ge:test tests/shooters_edge_hub_cards_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace shooters_edge::hub {

// The cache key of `map` of `game` under `content` at `width` x `height`: 16 hex digits, "" when
// the content holds no such map.
std::string still_key(std::string_view content, std::string_view game, std::string_view map,
                      int width, int height);

// The still drawn into `rgba` (width * height * 4). False with the reason.
bool draw_still(std::string_view content, std::string_view game, std::string_view map, int width,
                int height, std::vector<uint8_t>& rgba, std::string& error);

}  // namespace shooters_edge::hub
