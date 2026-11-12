// A classic map's liquids as ours (bsp_liquids.hpp).
//
// ge:summary A classic map's water, lava and slime tops made bodies of our water and liquids
// ge:keywords bsp brush liquids water lava slime body volume hull v2 add_bsp_liquids
// ge:keywords pool-site wind-exposure shelter still-pool eye_water_body
// ge:skill brush water render
#include "brush/app/bsp_liquids.hpp"

#include <algorithm>
#include <cmath>

#include "brush/light_probe.hpp"
#include "water/liquids.hpp"

namespace ge::brush::app {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kEmptyLeaf = -1, kSkyLeaf = -6, kSolidLeaf = -2;  // the leaves' contents (map.hpp)

// The leaf at an engine point (x, up, z in metres).
const Leaf& leaf_of(const Map& map, double x, double y, double z) {
  const float f[3] = {float(x / kUnitM), float(z / kUnitM), float(y / kUnitM)};
  return map.leaves[leaf_at(map, f)];
}

bool liquid(const Leaf& l) { return !l.solid && l.contents <= -3 && l.contents >= -5; }

// Whether a ray from (x, y, z) along the unit (dx, dy, dz) reaches the sky within `reach` m before
// a wall: marched through the leaves a quarter metre a step.
bool reaches_sky(const Map& map, double x, double y, double z, double dx, double dy, double dz,
                 double reach) {
  const int steps = static_cast<int>(reach * 4);
  for (int k = 1; k <= steps; ++k) {
    const double s = 0.25 * k;
    const Leaf& l = leaf_of(map, x + dx * s, y + dy * s, z + dz * s);
    if (l.contents == kSkyLeaf) return true;
    if (l.solid || l.contents == kSolidLeaf) return false;
  }
  return false;
}

}  // namespace

double open_sky_of(const Map& map, const LiquidPatch& p) {
  int seen = 0, open = 0;
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      const double x = p.box[0] + (i + 0.5) / 8 * (p.box[2] - p.box[0]);
      const double z = p.box[1] + (j + 0.5) / 8 * (p.box[3] - p.box[1]);
      for (int k = 0; k < 600; ++k) {
        const double y = p.level + 0.1 + 0.5 * k;
        const Leaf& l = leaf_of(map, x, y, z);
        if (k == 0 && l.solid) break;  // the point is in a wall, not over the pool
        if (l.contents == kSkyLeaf) {
          ++seen;
          ++open;
          break;
        }
        if (l.solid || l.contents == kSolidLeaf) {
          ++seen;
          break;
        }
      }
    }
  }
  return seen > 0 ? double(open) / seen : 1.0;
}

PoolSite pool_site(const Map& map, const LiquidPatch& p, double from) {
  PoolSite site;
  site.light[0] = -1;
  // THE DEPTH: down from the top through the map's liquid leaves, on a grid over the box.
  for (int i = 0; i < 6; ++i) {
    for (int j = 0; j < 6; ++j) {
      const double x = p.box[0] + (i + 0.5) / 6 * (p.box[2] - p.box[0]);
      const double z = p.box[1] + (j + 0.5) / 6 * (p.box[3] - p.box[1]);
      int k = 0;  // tenths of a metre down from 5 cm under the top
      while (k < 640 && liquid(leaf_of(map, x, p.level - 0.05 - 0.1 * k, z))) ++k;
      if (k > 0) site.depth = std::max(site.depth, 0.1 * k);
    }
  }
  // THE AIR UNDER IT: on a finer grid (a shaft a player drops through may be a few metres across),
  // down from the top to the liquid's end, and air there rather than a bed.
  for (int i = 0; i < 16 && !site.open_below; ++i) {
    for (int j = 0; j < 16 && !site.open_below; ++j) {
      const double x = p.box[0] + (i + 0.5) / 16 * (p.box[2] - p.box[0]);
      const double z = p.box[1] + (j + 0.5) / 16 * (p.box[3] - p.box[1]);
      int k = 0;
      while (k < 640 && liquid(leaf_of(map, x, p.level - 0.05 - 0.1 * k, z))) ++k;
      if (k == 0) continue;
      const Leaf& under = leaf_of(map, x, p.level - 0.05 - 0.1 * k, z);
      site.open_below = !under.solid && under.contents == kEmptyLeaf;
    }
  }
  // THE AIR: rays from points over the pool (3 x 3, at a hand and at a man's height over the
  // water), sixteen ways round at four heights and one straight up, out to 48 m.
  constexpr int kWays = 16;
  constexpr double kElevations[4] = {0, 0.35, 0.8, 1.2};  // rad: level, 20, 45 and 70 degrees
  double sky = 0, sky_weight = 0, up = 0, up_weight = 0;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      const double x = p.box[0] + (i + 0.5) / 3 * (p.box[2] - p.box[0]);
      const double z = p.box[1] + (j + 0.5) / 3 * (p.box[3] - p.box[1]);
      for (const double h : {0.3, 1.5}) {
        const double y = p.level + h;
        const Leaf& here = leaf_of(map, x, y, z);
        if (here.solid || here.contents == kSolidLeaf) continue;  // in a wall over the pool
        for (int e = 0; e < 4; ++e) {
          const double ce = std::cos(kElevations[e]), se = std::sin(kElevations[e]);
          for (int w = 0; w < kWays; ++w) {
            const double b = 2 * kPi * w / kWays;  // the bearing the ray points to
            const bool open = reaches_sky(map, x, y, z, std::sin(b) * ce, se, std::cos(b) * ce, 48);
            if (e < 2) {
              // A low ray pointing the way the wind comes from: a window it blows in by.
              const double toward = std::max(0.0, std::cos(b - from));
              up += toward * toward * (open ? 1 : 0);
              up_weight += toward * toward;
            } else {
              sky += se * (open ? 1 : 0);
              sky_weight += se;
            }
          }
        }
        sky += kWays * (reaches_sky(map, x, y, z, 0, 1, 0, 48) ? 1 : 0);
        sky_weight += kWays;
      }
    }
  }
  site.sky = sky_weight > 0 ? sky / sky_weight : 0;
  site.upwind = up_weight > 0 ? up / up_weight : 0;
  // The wind in by the windows nearly whole, a courtyard's walls keeping most of what blows over
  // them off its floor.
  site.exposure = std::clamp(0.8 * site.upwind + 0.25 * site.sky * site.sky, 0.0, 1.0);
  return site;
}

double pool_sunlit(const Map& map, const LiquidPatch& p, const double sun[3]) {
  if (sun[1] <= 0.02) return 0;
  int seen = 0, lit = 0;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      const double x = p.box[0] + (i + 0.5) / 4 * (p.box[2] - p.box[0]);
      const double z = p.box[1] + (j + 0.5) / 4 * (p.box[3] - p.box[1]);
      const double y = p.level + 0.05;
      const Leaf& here = leaf_of(map, x, y, z);
      if (here.solid || here.contents == kSolidLeaf) continue;  // in a wall over the pool
      ++seen;
      lit += reaches_sky(map, x, y, z, sun[0], sun[1], sun[2], 256) ? 1 : 0;
    }
  }
  return seen > 0 ? double(lit) / seen : 0.0;
}

void light_pools_by_sun(render::Water& water, const Map& map, LiquidWorld& lw,
                        const double sun[3]) {
  const double turned = lw.sun[0] * sun[0] + lw.sun[1] * sun[1] + lw.sun[2] * sun[2];
  if (turned > 0.99996) return;  // within half a degree of the last
  for (int k = 0; k < 3; ++k) lw.sun[k] = sun[k];
  int slot = 0;
  for (size_t i = 0; i < lw.kinds.size(); ++i) {
    if (lw.kinds[i] != kWater) continue;
    if (slot >= int(water.bodies.size())) break;
    render::set_water_sunlit(water, slot++, pool_sunlit(map, lw.patches[i], sun));
  }
}

LiquidWorld liquid_world(const Scene& scene, const LiquidRows& rows, double wind, double from) {
  ge::wind::Field f;
  f.mean = wind;
  f.from = from;
  LiquidWorld out{water::World(f), {}, {}, {}, {}, {0, 0, 0}, wind, from};
  water::World& world = out.world;
  const LightProbe probe = make_light_probe(scene.map);
  int waters = 0;
  for (const LiquidPatch& p : scene.liquids) {
    if (p.kind == kWater && waters >= render::kWaterBodies) continue;
    water::Stuff stuff = water::liquid(p.kind == kLava    ? rows.lava
                                       : p.kind == kSlime ? rows.slime
                                                          : rows.water);
    if (p.kind == kWater) stuff.look = rows.water_look;
    PoolSite site = pool_site(scene.map, p, from);
    // The bed's light: the lightmaps under a grid of points just over the top.
    double sum[3] = {0, 0, 0};
    int lit = 0;
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        const double x = p.box[0] + (i + 0.5) / 3 * (p.box[2] - p.box[0]);
        const double z = p.box[1] + (j + 0.5) / 3 * (p.box[3] - p.box[1]);
        const float at[3] = {float(x / kUnitM), float(z / kUnitM), float((p.level + 0.2) / kUnitM)};
        float rgb[3];
        if (!light_below(probe, scene.map, at, rgb, float((site.depth + 1) / kUnitM))) continue;
        for (int k = 0; k < 3; ++k) sum[k] += rgb[k];
        ++lit;
      }
    }
    if (lit > 0)
      for (int k = 0; k < 3; ++k) site.light[k] = sum[k] / lit;
    const double depth = std::max(site.depth, rows.least_depth);
    water::Hull h;
    h.planes = {{0, 1, 0, p.level},  {0, -1, 0, -(p.level - depth)},
                {1, 0, 0, p.box[2]}, {-1, 0, 0, -p.box[0]},
                {0, 0, 1, p.box[3]}, {0, 0, -1, -p.box[1]}};
    const water::Hull hulls[1] = {h};
    const int i = water::add_volume(world, hulls, stuff);
    water::Body& b = world.bodies[size_t(i)];
    b.level = p.level;
    for (int k = 0; k < 4; ++k) b.box[k] = p.box[k];
    water::lay_waves(world, i, std::max(p.box[2] - p.box[0], p.box[3] - p.box[1]),
                     wind * site.exposure, from);
    waters += p.kind == kWater;
    out.kinds.push_back(p.kind);
    out.open.push_back(open_sky_of(scene.map, p));
    out.sites.push_back(site);
    out.patches.push_back(p);
  }
  return out;
}

void relay_liquid_wind(gpu::Device& device, render::Water& water, LiquidWorld& lw, double wind,
                       double from) {
  lw.wind = wind;
  lw.from = from;
  lw.world.wind.mean = wind;
  lw.world.wind.from = from;
  int slot = 0;
  for (size_t i = 0; i < lw.world.bodies.size(); ++i) {
    if (lw.kinds[i] != kWater) continue;
    water::Body& b = lw.world.bodies[i];
    water::lay_waves(lw.world, int(i), std::max(b.box[2] - b.box[0], b.box[3] - b.box[1]),
                     wind * lw.sites[i].exposure, from);
    if (slot < int(water.bodies.size())) render::set_water_body(device, water, slot, b);
    ++slot;
  }
}

void relay_liquid_pace(gpu::Device& device, render::Water& water, const LiquidWorld& lw,
                       double pace) {
  int slot = 0;
  for (size_t i = 0; i < lw.world.bodies.size(); ++i) {
    if (lw.kinds[i] != kWater) continue;
    if (slot < int(water.bodies.size()))
      render::set_water_pace(device, water, slot, lw.world.bodies[i], pace);
    ++slot;
  }
}

void light_rooms(render::Water& water, render::Liquids& liquids, const LiquidWorld& lw,
                 const LiquidRows& rows, double minlight, double brightness) {
  const double m = minlight * brightness;
  const auto room_of = [&](size_t body) {
    const PoolSite& s = lw.sites[body];
    const double base =
        s.light[0] < 0
            ? rows.room
            : (0.2126 * s.light[0] + 0.7152 * s.light[1] + 0.0722 * s.light[2]) * brightness;
    return static_cast<float>(std::sqrt(base * base + m * m));
  };
  size_t w = 0, l = 0;
  for (size_t i = 0; i < lw.kinds.size(); ++i) {
    const float room = room_of(i);
    if (lw.kinds[i] == kWater) {
      if (w >= water.bodies.size()) continue;
      render::Water::Slot& s = water.bodies[w++];
      s.draw.room = room;
      s.staged.emission[3] = room;
    } else if (l < liquids.bodies.size()) {
      liquids.bodies[l++].staged.flow[3] = room;
    }
  }
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
        const LiquidRows::Grime& g = rows.slime_grime;
        const double k = 0.5 + g.murk;
        sl.absorb = {45 * k, 24 * k, 190 * k};
        sl.scatter = {40 * k, 40 * k, 40 * k};
        sl.rough = 0.2 + 0.25 * g.grime;
        sl.scum = g.scum;
        sl.scum_albedo = {0.09, 0.085, 0.035};
        sl.bubbles = g.bubbles;
        sl.bubble_size = 0.05;
        sl.glow = {2 * g.glow, 12 * g.glow, 1 * g.glow};
        sl.pulse = 0.2;
        sl.film = g.film;
        sl.grime = g.grime;
        sl.debris = g.debris;
      }
      if (lw.kinds[i] == kLava) {
        here.lava.cover = rows.lava_cover;
        here.lava.core = rows.lava_core;
        here.lava.plate = rows.lava_plate;
        here.lava.crack = rows.lava_crack;
        here.lava.ropes = rows.lava_ropes;
        here.lava.patch = rows.lava_patch;
        here.lava.crust_rough = rows.lava_rough;
      }
      here.room = rows.room;
      render::add_liquid_body(device, liquids, b, here, world.wind.mean * lw.sites[i].exposure,
                              world.wind.from + kPi,
                              std::max(b.box[2] - b.box[0], b.box[3] - b.box[1]));
    } else {
      render::WaterDraw draw;
      // The sky the surface mirrors where nothing else is reflected; the day lights the water
      // only through what of the dome is sky, and the walls' shade the map's own light keeps.
      draw.open_sky = lw.open[i];
      draw.daylight = lw.sites[i].sky;
      draw.sunlit = 0;  // none until light_pools_by_sun finds where the sun reaches
      draw.room = rows.room;
      draw.stir = rows.stir;
      draw.chop = rows.chop;
      draw.chop_shortest = rows.chop_shortest;
      draw.pace = kPoolPace;  // the game's own (Options::water_pace) by relay_liquid_pace
      draw.open_below = lw.sites[i].open_below;
      // The wind's gusts break the water into patches of ripples and calm as it reaches it.
      draw.gusts = lw.sites[i].exposure > 0 ? 1 : 0;
      render::add_water_body(device, water, b, draw);
    }
  }
}

int eye_water_body(const Map& map, const LiquidWorld& lw, const double eye[3]) {
  if (!liquid(leaf_of(map, eye[0], eye[1], eye[2]))) return -1;
  int best = -1, slot = 0;
  double best_d = 1e300;
  for (size_t i = 0; i < lw.world.bodies.size(); ++i) {
    if (lw.kinds[i] != kWater) continue;
    const water::Body& b = lw.world.bodies[i];
    if (slot >= render::kWaterBodies) break;
    if (b.level > eye[1]) {
      // How far the eye is outside the body's box in the plan, and how far under its level.
      const double dx = std::max({b.box[0] - eye[0], 0.0, eye[0] - b.box[2]});
      const double dz = std::max({b.box[1] - eye[2], 0.0, eye[2] - b.box[3]});
      const double d = dx * dx + dz * dz + 1e-3 * (b.level - eye[1]);
      if (d < best_d) {
        best_d = d;
        best = slot;
      }
    }
    ++slot;
  }
  return best;
}

}  // namespace ge::brush::app
