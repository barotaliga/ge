// A REAL FACE'S LAKES: the standing water the open map data draws on a real face (the face's
// hints, mapgen/real_hints.hpp's `HintLake`), laid on a map built off that face as a tarn is laid
// (tarn.hpp): `Level::lakes`, its ice by the map's day. The generator lays no water, so no map
// moves; a game lays them after it (fall_line/sim/free.hpp's `free_map` on a face).
//
// - THE LAKE (`mapped_lakes`): its shore as mapped, its middle the shore's centroid, its `radius`
//   the circle's of the same area (what a view frames it by), its surface the ground's `kSurface`
//   share inside the shore: the elevation model reads a lake flat, but the generator lays its own
//   mountain over the face and the ground it leaves inside a shore can rise tens of metres across
//   it, so the surface is set low in that ground and the rest cut down to it. A lake less than
//   `kLeastLake` m² is a pond the map's grid cannot hold and is left dry.
// - THE ICE: the tarn's (`tarn_ice`, `thaw_of`) at the lake's own height, so a valley lake opens
//   while a high one is still frozen to bear.
// - THE BASIN (`lay_mapped`): ice that bears a skier fills the shore flat at the surface; else the
//   shore is dug into a bowl `depth_of` m deep (a tenth of its `radius`, between
//   `kMappedShallow` and `kMappedDeep`), shelving up over the outer `kShelf` m (or less, on a
//   narrow lake) inside the shore. THE BANKS: the ground outside stands just over the water out to
//   `kLip` m, then rises or falls to the ground round it no steeper than `kBankSlope` (out to
//   `kBankReach` m past the lip): the water stands in the shore, and a lake set lower than its
//   ground is cut into it. The trees inside the shore and within 4 m of it are felled.
//
// ge:summary A real face's mapped lakes laid on its map: the shore, the basin, the ice by the day
// ge:keywords mapped-lake real-lake lay_mapped_lakes mapped_lakes hint-lake shore basin lake-ice
// ge:keywords banks lip bank-slope
// ge:skill mountain
// ge:test tests/mountain_mapped_test.cpp
#pragma once

#include <vector>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::tarn {

inline constexpr double kLeastLake = 2500;   // m²: the smallest lake laid
inline constexpr double kMappedShallow = 2;  // m: the shallowest lake's deepest water
inline constexpr double kMappedDeep = 12;    // m: the deepest's
inline constexpr double kShelf = 40;         // m inside the shore the bed shelves down over
inline constexpr double kSurface = 0.3;      // the share of the ground inside under the surface
inline constexpr double kLip = 8;            // m outside the shore the bank stands over the water
inline constexpr double kBankSlope = 0.6;    // the banks' steepest, rise over run
inline constexpr double kBankReach = 80;     // m past the lip the banks reach at most

// The lakes of the real face `level` is built off (none on a dealt map, or a face with none), on
// its ground, their ice not yet set.
std::vector<mapgen::MapLake> mapped_lakes(const mapgen::Level& level);
// The deepest water a mapped lake is dug to, m.
double depth_of(const mapgen::MapLake& lake);
// Lay one mapped lake: its ice by the map's day, its basin dug or filled, the trees in it felled,
// added to `level.lakes`.
void lay_mapped(mapgen::Level& level, mapgen::MapLake lake);
// Every mapped lake of the face laid (`mapped_lakes`, then `lay_mapped`).
void lay_mapped_lakes(mapgen::Level& level);

}  // namespace ge::mountain::tarn
