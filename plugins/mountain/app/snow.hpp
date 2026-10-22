// THE SNOW AS DRAWN: the material the terrain's colour pass shades a snow mountain with
// (render/terrain.hpp's `TerrainSource::shader`, over render/wgsl/terrain_draw.wgsl), and the map
// it reads. The game keeps its colours (the albedo it hands the terrain: snow, the packed piste,
// rock); this is what the snow DOES to the light: the micro-relief and the groomer's corduroy
// bent into the normal, the wrap's blue terminator, the glitter, the glare. snow.wgsl states
// every term. Nothing here moves the ground the simulation skis on.
//
// THE GROUND MAP (`bake_snow_ground`): one RGBA8 texel a heightfield sample, rows along z:
//   r  the packed share 0..1 (the map's R10 field)
//   g  the rock share 0..1 (`rock_share` of the ground's slope)
//   ba the piste's direction as the doubled angle, (cos 2h, sin 2h) / 2 + 1/2, scaled by how far
//      inside the piste the sample lies (nought, 128/255 in both, at its edge and off it)
//
// ge:summary The snow's material over the terrain draw: corduroy, relief, wrap, glitter, glare
// ge:keywords snow snow-material snow-look corduroy groomed piste lanes micro-relief glitter
// ge:keywords glints crystal wrap-lighting terminator glare sheen rock-share ground-map
// ge:keywords bake_snow_ground create_snow snow_shader rock_share snowlook
// ge:skill mountain render
// ge:test tests/mountain_snow_test.cpp
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "gpu/device.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::app {

// The share of a ground of `slope` (rise over run) that is bare rock: nought where snow holds
// (up to 45 degrees), one where it cannot (past about 58), smooth between.
double rock_share(double slope);

std::vector<uint8_t> bake_snow_ground(const mapgen::Level& level);

// The material's gains, each 1 for the tuned look and 0 for none of it.
struct SnowLook {
  float glitter = 1, corduroy = 1, relief = 1, wrap = 1, glare = 1, lanes = 1;
};

struct SnowMaterial {
  wgpu::BindGroupLayout layout;  // TerrainSource::material
  wgpu::BindGroup group;         // bind_terrain_material
  wgpu::Texture ground;
  wgpu::Buffer params;
};

// The material for `level`: its ground map uploaded, its gains, its layout and group.
void create_snow(gpu::Device& device, SnowMaterial& snow, const mapgen::Level& level,
                 const SnowLook& look = {});
// The gains changed (labs).
void set_snow_look(gpu::Device& device, SnowMaterial& snow, const SnowLook& look);

// The shader (TerrainSource::shader).
std::string_view snow_shader();

}  // namespace ge::mountain::app
