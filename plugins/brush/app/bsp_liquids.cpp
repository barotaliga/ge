// A classic map's liquids as ours (bsp_liquids.hpp).
//
// ge:summary A classic map's water, lava and slime tops made bodies of our water and liquids
// ge:keywords bsp brush liquids water lava slime body volume hull v2 add_bsp_liquids
// ge:skill brush water render
#include "brush/app/bsp_liquids.hpp"

#include <algorithm>

#include "water/liquids.hpp"

namespace ge::brush::app {

double open_sky_of(const Map& map, const LiquidPatch& p) {
  int seen = 0, open = 0;
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      const double x = p.box[0] + (i + 0.5) / 8 * (p.box[2] - p.box[0]);
      const double z = p.box[1] + (j + 0.5) / 8 * (p.box[3] - p.box[1]);
      for (double y = p.level + 0.1; y < p.level + 300; y += 0.5) {
        const float f[3] = {float(x / kUnitM), float(z / kUnitM), float(y / kUnitM)};
        const Leaf& l = map.leaves[leaf_at(map, f)];
        if (y == p.level + 0.1 && l.solid) break;  // the point is in a wall, not over the pool
        if (l.contents == -6) {
          ++seen;
          ++open;
          break;
        }
        if (l.solid || l.contents == -2) {
          ++seen;
          break;
        }
      }
    }
  }
  return seen > 0 ? double(open) / seen : 1.0;
}

LiquidWorld liquid_world(const Scene& scene, const LiquidRows& rows, double wind, double from) {
  ge::wind::Field f;
  f.mean = wind;
  f.from = from;
  LiquidWorld out{water::World(f), {}, {}};
  water::World& world = out.world;
  int waters = 0;
  for (const LiquidPatch& p : scene.liquids) {
    if (p.kind == kWater && waters >= render::kWaterBodies) continue;
    water::Stuff stuff = water::liquid(p.kind == kLava    ? rows.lava
                                       : p.kind == kSlime ? rows.slime
                                                          : rows.water);
    if (p.kind == kWater) stuff.look = rows.water_look;
    water::Hull h;
    h.planes = {{0, 1, 0, p.level},  {0, -1, 0, -(p.level - rows.depth)},
                {1, 0, 0, p.box[2]}, {-1, 0, 0, -p.box[0]},
                {0, 0, 1, p.box[3]}, {0, 0, -1, -p.box[1]}};
    const water::Hull hulls[1] = {h};
    const int i = water::add_volume(world, hulls, stuff);
    water::Body& b = world.bodies[size_t(i)];
    b.level = p.level;
    for (int k = 0; k < 4; ++k) b.box[k] = p.box[k];
    water::lay_waves(world, i, std::max(p.box[2] - p.box[0], p.box[3] - p.box[1]));
    waters += p.kind == kWater;
    out.kinds.push_back(p.kind);
    out.open.push_back(open_sky_of(scene.map, p));
  }
  return out;
}

void add_bsp_liquids(gpu::Device& device, render::Water& water, render::Liquids& liquids,
                     const LiquidWorld& lw, const LiquidRows& rows) {
  const water::World& world = lw.world;
  for (size_t i = 0; i < world.bodies.size(); ++i) {
    const water::Body& b = world.bodies[i];
    if (lw.kinds[i] != kWater) {
      const render::LiquidLook* look =
          render::find_liquid_look(lw.kinds[i] == kLava ? rows.lava : rows.slime);
      if (!look) continue;
      render::LiquidLook here = *look;
      here.open_sky = lw.open[i];
      if (lw.kinds[i] == kSlime && rows.murky_slime) {
        render::SlimeLook& sl = here.slime;
        sl.absorb = {90, 22, 180};
        sl.scatter = {40, 40, 40};
        sl.rough = 0.2;
        sl.scum = 0.2;
        sl.scum_albedo = {0.14, 0.18, 0.07};
        sl.bubbles = 2;
        sl.bubble_size = 0.05;
        sl.glow = {2, 12, 1};
        sl.pulse = 0.2;
      }
      if (lw.kinds[i] == kLava) {
        here.lava.cover = rows.lava_cover;
        here.lava.plate = rows.lava_plate;
      }
      here.room = rows.room;
      render::add_liquid_body(device, liquids, b, here, world.wind.mean,
                              world.wind.from + 3.14159265358979,
                              std::max(b.box[2] - b.box[0], b.box[3] - b.box[1]));
    } else {
      render::WaterDraw draw;
      draw.open_sky = lw.open[i];
      draw.room = rows.room;
      render::add_water_body(device, water, b, draw);
    }
  }
}

}  // namespace ge::brush::app
