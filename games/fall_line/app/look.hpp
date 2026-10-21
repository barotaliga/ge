// FALL LINE'S GROUND, AS COLOUR: the albedo the terrain pass is handed, baked once a map off
// the map's own fields: untouched snow, the groomed piste where the map packs it (R10, a share
// 0..1 blended in), and rock where the ground is too steep to hold snow. One RGBA8 texel a
// heightfield sample, sRGB-encoded. The snow's own material (its age, glitter and grooves)
// replaces this with M8 (mountain/app).
//
// ge:summary Fall Line's ground albedo: snow, the packed piste and steep rock, baked per map
// ge:keywords fall-line look albedo snow piste packed rock steep bake ground-colour bake_albedo
// ge:skill render
#pragma once

#include <cstdint>
#include <vector>

#include "mountain/mapgen/types.hpp"

namespace fall_line::app {

std::vector<uint8_t> bake_albedo(const ge::mountain::mapgen::Level& level);

}  // namespace fall_line::app
