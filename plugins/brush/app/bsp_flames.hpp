// THE MAP'S FIRES (docs/rendering.md § Flames): every light entity that stands for a fire (a wall
// torch, a large flame) drawn as burning gas (render/flame.hpp) where its model's flame is, in
// place of the model. The gas keeps step with the light the map casts for it: its level is the
// light's `flicker_at`, so the fire and the wall it lights waver as one.
//
// ge:summary A map's torches and large flames as render::Flame gas, synced to their lights
// ge:keywords brush bsp torch flame fire wall-torch light-entity flicker-sync bsp_fires
// ge:skill brush render
// ge:test tests/brush_bsp_flames_test.cpp
#pragma once

#include <vector>

#include "brush/app/draw_data.hpp"
#include "render/flame.hpp"

namespace ge::brush::app {

// A model's flame over its entity's origin, in map units (1 unit is kUnitM): where its gas leaves
// the fuel and how tall it stands. A wall torch's stick reaches about 6 units up and its flame
// to 29; a large flame's bowl about 2 and its flame to 43.
inline constexpr double kTorchBase = 6, kTorchHeight = 23;
inline constexpr double kLargeBase = 2, kLargeHeight = 41;

// How far above its entity's origin a fire's light shines from, m: from the middle of its gas
// (where render::flame_light puts a flame's), 0 for a light that is no fire.
double fire_light_lift(const PointLight& light);
// A fire's light softened over its flame's height, m (0 for a light that is no fire).
double fire_light_size(const PointLight& light);

// How bright a fire's core is against a lightmap's 1 (a white face as the map's light lights it).
inline constexpr double kFireGlow = 30;

// The scene's fires at `t_s` (into `out`, cleared first), each seeded by its light; `brightness`
// is the renderer's (Options::brightness), what a lightmap's 1 is against the open day's white.
void bsp_fires(const Scene& scene, double t_s, double brightness, std::vector<render::Flame>& out);

}  // namespace ge::brush::app
