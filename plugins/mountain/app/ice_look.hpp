// THE ICE ON A LAKE AS DRAWN (mountain/air/marks.hpp's `lake_ice`), for a game to draw as one
// instance family over a lake whose water it stops drawing while the ice holds.
//
// THE SHEET (`ice_mesh`): a flat grid of `kIceCell` m cells laid `kIceLift` m over the lake's
// surface, every cell with a corner inside the shore, CLIPPED TO IT: a corner outside is drawn in
// to the nearest point of the shore, so the sheet's edge is the shore's own line and never a
// stair of cells. Thawing, the sheet stands `inset` m off the shore (the MOAT, open water round
// it, where the warming ground melts it first). Each cell is one of two parts: SNOW lying on the
// ice or, where the wind swept it, BARE ICE, the swept cells in patches tens of metres across (a
// value noise of the cell, off `seed`, over `kSwept`). On ice that lies on ground filled flat (a
// tarn frozen to bear a skier, mountain/tarn/tarn.hpp) the ground's own snow is the snow on it:
// only the bare cells are laid, finer (`kIceCellFlat`) and more of them (over `kSweptFlat`).
//
// THE LOOK BY THICKNESS (`ice_surfaces`): young ice under `kIceYoung` m is clear and nearly black
// (the deep water seen through it), glossy, and takes no snow; as it thickens past `kIceSnowed` m
// the snow lies on it everywhere but the swept patches, which stay a grey-blue glass. Thinner than
// `kIceShows` the lake is open water. THAWING (`thaw` 0..1, the spring's share gone, as
// mountain/tarn/tarn.hpp's `MapLake::thaw`): the snow on it goes to grey wet slush with a sheen
// (rougher than the moat's water, so the two read apart) and the glass to dark rotten ice, glossy
// with the meltwater lying on it.
//
// ge:summary A frozen lake as drawn: the ice sheet's mesh and its look by thickness
// ge:keywords lake-ice ice-sheet ice_mesh ice_surfaces frozen-lake black-ice swept-ice snow-on-ice
// ge:keywords thaw rotten-ice moat slush clipped-to-shore
// ge:skill mountain render
// ge:test tests/mountain_ice_look_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "render/mesh.hpp"
#include "render/surface.hpp"
#include "water/fetch.hpp"

namespace ge::mountain::app {

enum IcePart : uint8_t { kIceSnow, kIceBare };
inline constexpr int kIceParts = 2;
inline constexpr double kIceCell = 2;      // m
inline constexpr double kIceCellFlat = 1;  // m: the swept patches alone, at a skier's eye
inline constexpr double kIceLift = 0.04;   // m over the water's surface
inline constexpr double kSwept = 0.62;     // the noise over which a cell is swept bare
inline constexpr double kSweptFlat = 0.5;  // the same on ground filled flat, where the bare
                                           // patches are all that tell the ice from the field
inline constexpr double kIceShows = 0.01;  // m: thinner, open water
inline constexpr double kIceYoung = 0.04;  // m: clear black ice to here
inline constexpr double kIceSnowed = 0.1;  // m: snow lies on it from here

// The sheet over a lake inside `shore` (world m; the instance stands at the origin) whose surface
// is at `level` m; `swept_only`: the bare cells alone, over `kSweptFlat` (ice on ground filled
// flat, whose own snow is the snow on the ice); `inset`: m of open water between it and the shore.
render::MeshData ice_mesh(std::span<const water::Point> shore, double level, uint32_t seed,
                          bool swept_only = false, double inset = 0);
// The two parts' surfaces on ice `h` m thick, `thaw` (0..1) of the way through the spring's melt.
std::vector<render::Surface> ice_surfaces(double h, double thaw = 0);

}  // namespace ge::mountain::app
