// THE VALLEY'S RESERVOIR: a lake a game lays on a dealt map behind an earth dam across one of the
// mountain's valleys (mapgen/types.hpp's `MapLake` and `MapDam`, `Level::lakes`, `Level::dams`),
// iced by the map's day as a tarn is (tarn.hpp). The generator lays no water, so no dealt map
// moves; Fall Line's free ride lays one after its tarn (fall_line/sim/free.hpp's `free_map`).
//
// - THE SITE (`find_reservoir`): every `kDamGrid` m of the mountain's lower part (up to
//   `kDamHigh` of the way from its base to its summit, above the hub by `kDamClear` m) is tried as
//   the middle of a dam across the ground's fall there, at each height of `kDamHeights` over that
//   ground: the CREST runs both ways along the contour until the valley's sides rise to it, no
//   longer than `kDamLongest` m in all (a slope with no valley to close never closes). The water
//   stands `kFreeboard` m under the crest, and the ground under it is FLOODED from just upstream
//   of the dam on a grid `kFloodCell` m apart: it must not run round the dam's ends, off the map,
//   or within `kDamClear` m of a run, a ski route, the piste, a lift's line, the hub or a lake
//   already laid, and it must cover `kLeastReservoir` m² and no more than `kMostReservoir`. The
//   site kept floods the most ground (the first so found, in the grid's order, on a tie).
// - THE SHORE: the flooded cells' outline (the longest ring of their outer edges), thinned to the
//   bends that keep it within `kShoreThin` m. Its middle is the shore's centroid and its `radius`
//   the circle's of its area, as a mapped lake's (mapped.hpp).
// - THE DAM (`lay_reservoir`): an earth bank along the crest, `kCrestWidth` m across its top and
//   its two faces falling `kDamFace` (rise over run) to the ground, run on `kDamTie` m into the
//   valley's sides; the trees on it felled.
// - THE WATER: the ground inside the shore already lies under it (it was flooded); ice that BEARS
//   a skier fills the shore flat at the surface, as a frozen tarn's; the trees inside the shore and
//   within 4 m of it are felled.
//
// ge:summary The valley's reservoir on a dealt map: the dam site, the flood, the shore, the dam
// ge:keywords reservoir dam earth-dam find_reservoir lay_reservoir map-dam flood valley crest
// ge:keywords freeboard dealt-map
// ge:skill mountain
// ge:test tests/mountain_reservoir_test.cpp
#pragma once

#include <array>
#include <optional>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::tarn {

inline constexpr double kDamGrid = 32;   // m between the dam sites tried
inline constexpr double kDamHigh = 0.6;  // of the base-to-summit height, the highest site
inline constexpr std::array<double, 5> kDamHeights{8, 12, 18, 25, 35};  // m over the valley
inline constexpr double kDamLongest = 420;                              // m of crest at most
inline constexpr double kFreeboard = 2;           // m from the water up to the crest
inline constexpr double kFloodCell = 8;           // m between the flood's samples
inline constexpr double kDamClear = 30;           // m from every run, lift, hub and lake
inline constexpr double kLeastReservoir = 30000;  // m²: the least water kept (3 ha)
inline constexpr double kMostReservoir = 600000;  // m²: the most (60 ha)
inline constexpr double kShoreThin = 5;           // m a shore may stray from the flooded cells
inline constexpr double kCrestWidth = 8;          // m across the dam's top
inline constexpr double kDamFace = 0.5;           // the dam's faces, rise over run (1 in 2)
inline constexpr double kDamTie = 10;             // m past each end the bank runs into the side

// A reservoir found: the lake (its ice not yet set) and the dam that holds it.
struct Reservoir {
  mapgen::MapLake lake;
  mapgen::MapDam dam;
};

// The best reservoir on `level` (the module comment), or none where no valley closes clear of the
// ski area.
std::optional<Reservoir> find_reservoir(const mapgen::Level& level);
// Lay it: the dam built, the ice by the map's day, the water filled flat where the ice bears, the
// trees felled; the lake added to `level.lakes` and the dam to `level.dams`.
void lay_reservoir(mapgen::Level& level, Reservoir reservoir);

}  // namespace ge::mountain::tarn
