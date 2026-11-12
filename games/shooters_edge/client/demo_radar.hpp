// A MULTIVIEW RECORDING'S RADAR (engine/replay/radar.hpp, docs/hub.md § The live map): every
// player's place, the way they face and whether they are dead, four times a second, read off the
// recording with the demo reader (demo.hpp), so a hub draws the match as an overhead map. The
// map's plane is the level's floor: x east, y north (the classic y), a heading from the view's yaw
// (the classic yaw turns anticlockwise from east; the radar's clockwise from north). A roster
// chunk opens the track and comes again whenever a name or a team changes.
//
// THE OUTLINE, when the level's map is at hand: the map sliced across at the height the players
// stood at most (the median of their origins), and the edges between open and solid cells drawn as
// lines, runs along a row or a column joined into one. A level of several floors shows the floor
// most played on.
//
// `add_radar` packs on top: a version 2 file `pack_demo` wrote gets the radar track (id 9, codec
// zstd) and, where the header has none, the hub's words for the match (map, mode, every player and
// team). Only a multiview recording has every player's place; any other is refused.
//
// ge:summary A multiview recording's radar track for the hub's map, and its pack with the radar
// ge:keywords demo radar track hub map positions heading roster add_radar demo_radar multiview
// ge:keywords map_outline slice outline
// ge:skill replay shooter
// ge:test tests/shooters_edge_demo_radar_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "replay/radar.hpp"
#include "replay/stream.hpp"

namespace shooters_edge::client {

inline constexpr uint32_t kRadarTrack = 9;
inline constexpr int kRadarHz = 4;

// The radar chunks of `recording` (a plain multiview recording) for track `kRadarTrack`, and the
// hub's meta words it finds (map, player, team); false with `error` for another format or a
// recording that does not read.
bool demo_radar(std::span<const uint8_t> recording, std::vector<ge::replay::Chunk>& chunks,
                ge::replay::Meta& meta, std::string& error);
// The outline of the map file `bsp` at height `z` (units); empty when it does not read.
std::vector<ge::replay::RadarLine> map_outline(std::span<const uint8_t> bsp, double z);
// `file` (`pack_demo`'s output for `recording`) with the radar track and the hub's words added,
// and the outline when `bsp` (the level's map file) is given.
bool add_radar(std::span<const uint8_t> recording, std::span<const uint8_t> file,
               std::vector<uint8_t>& out, std::string& error, std::span<const uint8_t> bsp = {});

}  // namespace shooters_edge::client
