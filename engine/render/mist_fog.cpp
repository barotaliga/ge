// Fog from the air, drawn: a Mist as a bank or steam's wisps over a place, and a valley's level
// sea.
//
// ge:summary Definitions for fog from the air, drawn: mist_volume and mist_valley
// ge:keywords render fog mist mist_volume mist_valley steam-fog wisps valley-fog
// ge:skill render water
// ge:test tests/render_mist_fog_test.cpp
#include "render/mist_fog.hpp"

#include <algorithm>

#include "core/libm/libm.hpp"

namespace ge::render {

FogVolume mist_volume(const wind::Mist& mist, const MistPlace& place) {
  FogVolume v;
  v.shape = place.round ? FogVolume::Shape::ellipsoid : FogVolume::Shape::box;
  v.hug_ground = place.hug_ground;
  v.centre[0] = place.centre[0];
  v.centre[2] = place.centre[1];
  v.half[0] = std::max(place.half[0], 0.01);
  v.half[2] = std::max(place.half[1], 0.01);
  v.phase = 0.5;
  v.noise = mist.noise;
  v.wind[0] = place.drift[0];
  v.wind[2] = place.drift[1];
  const double plan = std::min(v.half[0], v.half[2]);
  if (mist.kind == wind::MistKind::none) {
    v.density = 0;
    v.half[1] = 0.5;
  } else if (mist.kind == wind::MistKind::steam) {
    // The water's breath: thickest on the surface, gone within its depth, small wisps climbing.
    const double depth = std::max(mist.depth, 0.5);
    v.half[1] = depth / 2;
    // The mist's extinction is what one sees through inside it, its mean over its depth: the
    // floor's is k / (1 - e^-k) of it for k e-folds.
    v.density = mist.density * kSteamFalloff / (1 - core::libm::exp(-kSteamFalloff));
    v.falloff = kSteamFalloff / depth;
    v.edge = std::min({0.5 * v.half[1], 2.0, plan});
    v.noise_size = std::clamp(2 * depth, 2.0, 12.0);
    v.wind[1] = mist.rise;
  } else {
    // A bank as deep as the air laid it, even to its top.
    v.half[1] = std::max(mist.depth, 4.0) / 2;
    v.density = mist.density;
    v.edge = std::min({v.half[1], 15.0, plan});
    v.noise_size = kBankNoiseSize;
  }
  if (place.edge >= 0) v.edge = std::min({place.edge, v.half[1], plan});
  // On the ground under each point the floor is the ground's and the centre's height unused.
  v.centre[1] = place.hug_ground ? 0 : place.floor_y + v.half[1];
  return v;
}

ValleyFog mist_valley(const wind::Mist& mist, double floor_y, const double drift[2]) {
  ValleyFog f;
  if (mist.kind != wind::MistKind::valley && mist.kind != wind::MistKind::radiation) return f;
  f.top_m = floor_y + mist.depth;
  f.density = mist.density;
  f.drift[0] = drift[0];
  f.drift[1] = drift[1];
  return f;
}

}  // namespace ge::render
