// FOG FROM THE AIR, DRAWN (docs/rendering.md § Fog from the air): what engine/wind's `mist_at`
// says lies at a place (wind/mist.hpp's `Mist`) handed to the frame's fog, so any world that asks
// its air at the places that matter draws the answer the same way:
//
// - AN AREA (`mist_volume`): a bank of the mist over a plan box (or an ellipse) on a floor, as a
//   render/fog.hpp `FogVolume`. Its extinction at the floor is the mist's `density`, its height
//   the mist's `depth`, its carving the mist's `noise`, and it drifts on the wind it is handed.
//   STEAM FOG (sea smoke) is the water's own breath, not a bank: thickest on the surface and
//   gone within its depth (it thins e^-3 to its top; its mean over its depth is the mist's), carved
//   into small wisps (a tile of twice its depth, 2 to 12 m) that CLIMB at the mist's `rise`. The
//   other kinds are banks as deep as the air laid them (4 m at the least), even to their top,
//   carved on a 40 m tile.
// - A VALLEY'S FOG (`mist_valley`): a radiation or valley fog in a valley's cold pool laid LEVEL,
//   like water: its top is the floor's height plus the mist's depth (render/valley_fog.hpp's
//   `ValleyFog`), its extinction the mist's, drifting on the wind it is handed. Any other kind,
//   none (density 0).
//
// Pure functions of their arguments: nothing is stepped and nothing draws a random number.
//
// ge:summary Fog from the air, drawn: a Mist as an area of fog (steam's wisps rising) or a valley's
// ge:summary level sea
// ge:keywords render fog mist mist-volume mist_volume mist_valley steam-fog sea-smoke wisps
// ge:keywords radiation-fog valley-fog fog-from-air lake water-fog bank
// ge:skill render water
// ge:test tests/render_mist_fog_test.cpp
#pragma once

#include "render/fog.hpp"
#include "render/view.hpp"
#include "wind/mist.hpp"

namespace ge::render {

// Where a mist lies: its plan's middle and half sizes (world m, x and z), the floor it lies on
// (a lake's surface, a pool's), whether it lies on the ground under each point instead
// (`FogVolume::hug_ground`), whether it is round (an ellipsoid, thinning toward its rim and its
// top), the wind it drifts on (m/s, x and z) and its soft edge.
struct MistPlace {
  double centre[2] = {0, 0};
  double half[2] = {10, 10};
  double floor_y = 0;
  bool hug_ground = false;
  bool round = false;
  double drift[2] = {0, 0};
  // The soft margin inside its faces, m; negative: the kind's own (a bank's as deep as half of it,
  // to 15 m; steam's a quarter of its depth, to 2 m). A bank on a lake wants a thin one, so it
  // lies at its full density from the water up.
  double edge = -1;
};

// The tile of the banks' noise, m, and the share of a steam fog's density left at its top.
inline constexpr double kBankNoiseSize = 40;
inline constexpr double kSteamFalloff = 3;  // e-folds over its depth

// The mist at `place` as an area of fog; its density 0 when no mist lies (`MistKind::none`).
FogVolume mist_volume(const wind::Mist& mist, const MistPlace& place);

// A radiation or valley fog over a valley floor at `floor_y` as its level sea; none otherwise.
ValleyFog mist_valley(const wind::Mist& mist, double floor_y, const double drift[2]);

}  // namespace ge::render
