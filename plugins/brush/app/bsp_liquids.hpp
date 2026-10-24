// A CLASSIC MAP'S LIQUIDS AS OURS (the v2 BSP renderer, bsp_draw.hpp): each group of a liquid's
// top surfaces (draw_data.hpp's `LiquidPatch`) made a body of engine/water (a volume from its
// level down, over its box), drawn by our water, lava or slime, each kind by its named row.
//
// ge:summary A classic map's water, lava and slime tops made bodies of our water and liquids
// ge:keywords bsp brush liquids water lava slime body volume hull v2 add_bsp_liquids
// ge:skill brush water render
#pragma once

#include <string>
#include <vector>

#include "brush/app/draw_data.hpp"
#include "gpu/device.hpp"
#include "render/liquid.hpp"
#include "render/water.hpp"
#include "water/world.hpp"

namespace ge::brush::app {

struct LiquidRows {
  std::string water = "water", water_look = "nordic-brown";  // the liquid's row, the water's look
  std::string lava = "lava", slime = "slime";                // rows of water/liquids.hpp
  double depth = 2;                                          // m under the top the volume reaches
  double lava_cover = 0.3;  // the share of the lava crusted over (the classic's lava is molten)
  double lava_plate = 0.8;  // m: a crust plate's size
  // The slime drawn murky (a dark green ooze, scummed, a faint glow) like the classic maps', not
  // the toxic row's bright opaque green.
  bool murky_slime = true;
  double room = 0.3;  // the light of a room round a roofed pool, a share of a white face's by day
};

struct LiquidWorld {
  water::World world;
  std::vector<Kind> kinds;   // a body's: kWater, kSlime or kLava
  std::vector<double> open;  // the share of its surface the sky is over (open_sky_of)
};

// The share of the points over a pool's top from which straight up is the sky (a leaf of sky
// before any wall), on a grid of up to 8 x 8 over its box.
double open_sky_of(const Map& map, const LiquidPatch& patch);

// The bodies (at most render::kWaterBodies of water, the largest), under `wind` m/s from `from`
// (the bearing it blows from).
LiquidWorld liquid_world(const Scene& scene, const LiquidRows& rows, double wind = 3,
                         double from = 4.7);
// Every body into the pass that draws it.
void add_bsp_liquids(gpu::Device& device, render::Water& water, render::Liquids& liquids,
                     const LiquidWorld& world, const LiquidRows& rows);

}  // namespace ge::brush::app
