// THE WATER a real face's map draws, as the `real-hints` lab bakes it into the face's hints
// (real_hints.hpp). The LAKES are every standing water (a lake, a reservoir, a pond; a river's or
// a canal's area is flowing water and is not one), each its outer shore: a closed way, or a
// multipolygon's outer members joined into rings, clipped to the map's square, thinned to the
// bends that keep it within `kShoreStray` m, and kept where `kSmallestLake` m² or more of it lies
// on the map, the largest first, `kMostLakes` at most. The STREAMS are every brook and river the
// map draws as a line (`waterway` stream or river; a culvert's or a tunnel's length, a canal and
// a ditch are not), each its longest run on the map, turned to run top first by the face's
// heights, thinned to the bends that keep it within `kStreamStray` m and kept where it runs
// `kShortestStream` m or more, the longest first, `kMostStreams` at most.
//
// The map data: © OpenStreetMap contributors, under the Open Database Licence (ODbL 1.0).
//
// ge:summary Lab tooling: a real face's lakes and streams read off the open map data
// ge:keywords osm-water lakes-of streams-of waterway reservoir shore openstreetmap
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#pragma once

#include <map>
#include <string>
#include <vector>

#include "mountain/labs/osm_data.hpp"
#include "mountain/mapgen/real_face.hpp"
#include "mountain/mapgen/real_hints.hpp"

namespace ge::mountain::labs {

// The lakes: how far a shore may stray, m; the least of one on the map, m²; the most kept a face.
inline constexpr double kShoreStray = 4;
inline constexpr double kSmallestLake = 2500;
inline constexpr size_t kMostLakes = 24;
// The streams: how far a line may stray, m; the shortest kept, m; the most kept a face.
inline constexpr double kStreamStray = 4;
inline constexpr double kShortestStream = 150;
inline constexpr size_t kMostStreams = 48;

// Whether a way's or a relation's tags draw standing water.
bool is_lake(const std::map<std::string, std::string>& tags);

// Every lake of a face's map data (the module comment); a multipolygon whose members the boxes
// left out is read whole, `missing` counting those that could not be.
std::vector<mapgen::HintLake> lakes_of(const mapgen::FaceCrop& f, const OsmData& d, OsmCache& osm,
                                       bool fetch, int& missing, std::string& error);

// Every stream and river of a face's map data, top first on its heights (the module comment).
std::vector<mapgen::HintStream> streams_of(const mapgen::FaceCrop& f, const OsmData& d,
                                           const mapgen::RealFace& face);

}  // namespace ge::mountain::labs
