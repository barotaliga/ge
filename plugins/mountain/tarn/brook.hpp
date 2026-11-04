// A REAL FACE'S STREAMS: the brooks and rivers the open map data draws on a real face (the face's
// hints, mapgen/real_hints.hpp's `HintStream`), laid on a map built off that face as its lakes are
// (mapped.hpp): `Level::streams`, cut into the ground, iced by the map's day. The generator lays
// no water, so no map moves; a game lays them after it, after the lakes.
//
// - THE LINE (`mapped_streams`): the mapped line, top first, read every `kStreamStep` m. Its
//   SURFACE falls and never climbs: at each point the lower of the last point's surface and the
//   ground there less the stream's `sink` (a brook's `kBrookSink`, a river's `kRiverSink`), so the
//   water runs in a bed of its own below the banks. The generator lays its own mountain over the
//   face, and where its ground rises over the mapped line by more than `kDeepestCut` m above that
//   surface (a spur the real stream does not cross), or the line runs into a laid lake, the stream
//   is BURIED there: the line breaks, and each run of `kShortestRun` m or more between the breaks
//   is a stream of its own.
// - THE CHANNEL: a brook `kBrookWidth` m bank to bank and `kBrookDepth` m deep mid-stream, a river
//   `kRiverWidth` and `kRiverDepth`.
// - THE ICE (`stream_ice`): a still lake's at the stream's top (tarn.hpp's `tarn_ice`): a brook
//   with any ice on it is FROZEN, iced over and snowed in; a river only where that ice would bear
//   a skier, since running water holds open longer.
// - THE BED (`lay_stream`): open water is cut into the ground across its channel and half a
//   cell of the ground's grid past each edge (so the ground read between the nodes lies under the
//   water), shelving from the waterline to its depth mid-stream; THE BANKS beyond stand `kRim` m
//   over the water and rise or fall to the ground round them no steeper than `kStreamBank` (out
//   to `kStreamReach` m): the water stands in its channel. A frozen stream is a shallow
//   gully under the snow (`kGully` of its depth, over twice its width), no water drawn. The trees
//   in the channel and within `kFell` m of its edge are felled.
//
// ge:summary A real face's mapped streams laid on its map: the falling line, the channel, the ice
// ge:keywords mapped-stream brook river channel stream-bed lay_mapped_streams mapped_streams
// ge:keywords hint-stream buried frozen-stream gully stream_ice
// ge:skill mountain
// ge:test tests/mountain_brook_test.cpp
#pragma once

#include <vector>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::tarn {

inline constexpr double kStreamStep = 4;    // m between a stream's points
inline constexpr double kBrookWidth = 3;    // m bank to bank
inline constexpr double kBrookDepth = 0.5;  // m mid-stream
inline constexpr double kBrookSink = 0.8;   // m the surface stands under the ground on its line
inline constexpr double kRiverWidth = 12;   // m
inline constexpr double kRiverDepth = 1.5;  // m
inline constexpr double kRiverSink = 1.6;   // m
inline constexpr double kDeepestCut = 3;    // m of ground over the bank before the line breaks
inline constexpr double kShortestRun = 60;  // m: the shortest run kept between breaks
inline constexpr double kRim = 0.25;        // m the bank stands over the water at its edge
inline constexpr double kStreamBank = 0.8;  // the banks' steepest, rise over run
inline constexpr double kStreamReach = 12;  // m past the edge the banks reach at most
inline constexpr double kGully = 0.6;       // a frozen stream's gully, a share of its depth
inline constexpr double kFell = 1.5;        // m past the edge the trees are felled

// The streams of the real face `level` is built off (none on a dealt map, or a face with none),
// on its ground and round its laid lakes, their ice not yet set.
std::vector<mapgen::MapStream> mapped_streams(const mapgen::Level& level);
// The ice a stilled stream would carry on the map's day, m, and whether it lies frozen.
double stream_ice(const mapgen::Level& level, const mapgen::MapStream& stream);
bool frozen(const mapgen::MapStream& stream);
// Lay one stream: its ice by the map's day, its bed cut (or its gully, frozen), the trees in it
// felled, added to `level.streams`.
void lay_stream(mapgen::Level& level, mapgen::MapStream stream);
// Every mapped stream of the face laid (`mapped_streams`, then `lay_stream`).
void lay_mapped_streams(mapgen::Level& level);

}  // namespace ge::mountain::tarn
