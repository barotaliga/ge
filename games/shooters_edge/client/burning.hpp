// WHO IS BURNING: every drawn body's scorch (render/body_burn.hpp) kept by entity across frames and
// stepped by how deep it stands in the map's lava or slime (the contents at its feet, its waist
// and its eyes, the map's own leaves), under ce_burning in the modern look. Each frame it says
// what the world frame (world_draw.hpp) draws for them: the char, embers and etch on each body's
// skin (`burn_skin_of`), the smoke, flames, embers and fumes they throw (`births`), the plumes
// of shimmer and gas over the liquid round them (`stirs`) and where one has just fallen into lava
// and broken its crust (`fell_in`). Presentation only: it reads the bodies' places and writes
// nothing back. A body that has been out long enough to show nothing is dropped; a jump back in
// time (a demo's seek) cools them all.
//
// ge:summary Every drawn body's burn by lava or slime, kept by entity and stepped each frame
// ge:keywords scorches scorch burning charred smoke flames fumes acid lava slime ce_burning
// ge:keywords step_burning burn_skin_of body-in-lava stirs fell-in
// ge:skill shooter render
#pragma once

#include <span>
#include <unordered_map>
#include <vector>

#include "brush/map.hpp"
#include "render/body_burn.hpp"
#include "render/liquid.hpp"
#include "render/morph.hpp"
#include "render/particles.hpp"
#include "shooters_edge/client/world_models.hpp"

namespace shooters_edge::client {

struct Burning {
  struct Body {
    ge::render::BodyBurn scorch;
    double feet[3] = {0, 0, 0};  // m, last frame's (its motion)
    bool seen = false;           // in this frame's list
    bool hot = false;            // in lava last frame
  };
  std::unordered_map<int, Body> bodies;  // by entity
  double last_s = -1;                    // the last frame's time
  // This frame's, filled by step_burning.
  std::vector<ge::render::Particle> births;
  std::vector<ge::render::LiquidStir> stirs;
  std::vector<ge::render::LiquidStir> fell_in;  // a body just in lava: where, its radius
};

// The level of the liquid a point is in (m), or a value far below when none is known: the world
// frame answers it from the drawn bodies.
using LevelAt = double (*)(const void* ctx, const double at[3]);

// One frame at `t_s`: every entity in `placed` (not statics) stepped by the liquid round it in
// `map`; `on` false (ce_burning off, the classic look) cools them all at once and throws nothing.
void step_burning(Burning& s, const ge::brush::Map& map, std::span<const Placed> placed, double t_s,
                  bool on, LevelAt level_at = nullptr, const void* ctx = nullptr);
// The burn entity `e` shows (none: all 0).
ge::render::MorphBurn burn_skin_of(const Burning& s, int entity);

}  // namespace shooters_edge::client
