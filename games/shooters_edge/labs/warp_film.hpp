// A PLAYER'S WARP FILMED for the labs: a figure blown in as ash or crumbling into it (client/
// world_fx.hpp's player dust, app/effects.hpp's dust_in and dust_off, the morph pass's warp) drawn
// as the window draws it, a thirtieth of a second a frame, a picture at each moment asked; or the
// original (ce_warps off: the teleport's classic specks, the figure there or gone at once). Seen
// from aside (another player arriving or leaving) or from his own eyes (`first_person`: his own
// arrival, the ash streaming at the eyes and the weapon in hand assembling).
//
// ge:summary Labs' helper: a player's warp in or out filmed through the window's world draw
// ge:keywords lab warp film dust ash spawn teleport first-person held-warp film_warp warp_dust
// ge:skill shooter labs
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/warps.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"

namespace shooters_edge::labs {

struct WarpFilm {
  std::vector<ge::lab::Picture> pics;
  std::vector<std::string> names;
};

struct WarpTake {
  client::Placed figure;  // who warps, where and how turned (map units, degrees)
  bool arriving = true;
  bool first_person = false;  // his own arrival through his eyes: the figure is not drawn
  bool enhanced = true;      // the dust (ce_warps); false: the original, fog and the figure at once
  effects::Palette palette;  // the pack's (gfx/palette.lmp) for the original's specks; empty: greys
  std::vector<double> moments;  // s into the warp, rising
  uint32_t seed = 1;
};

// `take` drawn over `base` (the frame's other models and effects) from `view`, with `hand` in
// hand (null: none), into `film`.
inline void film_warp(ge::gpu::Device& d, client::WorldDraw& world, const ge::gpu::Target& out,
                      const ge::render::FrameView& view, const client::FxFrame& base,
                      const view::HandFrame* hand, const WarpTake& take, WarpFilm& film) {
  namespace fx = shooters_edge::effects;
  const client::Warp warp{take.figure.entity, take.figure.origin, 0, take.arriving};
  fx::Dust dust = client::player_dust(take.figure.origin, take.figure.angles, take.figure.entity);
  std::vector<float> skin;
  if (client::body_triangles(client::world_models(world), take.figure, view.time_s, skin))
    dust.skin = skin;
  if (take.first_person) {
    dust.keep_off_eye = true;
    for (int k = 0; k < 3; ++k) dust.eye[k] = view.eye[k];
  }
  client::FxFrame f = base;
  f.births.clear();
  std::erase_if(f.models, [&](const client::Placed& m) { return m.entity == take.figure.entity; });
  constexpr double kFrame = 1.0 / 30;
  // Each frame's stage at `age` s: the figure at its share, the weapon's, this frame's ash.
  const auto stage = [&](double age, double was, bool births) {
    client::FxFrame s = f;
    if (!take.enhanced) {
      // The original: the fog at his feet once, the figure there (arriving) or gone (leaving).
      if (take.arriving && !take.first_person) s.models.push_back(take.figure);
      if (births && was < 0) {
        const ge::camera::Vec3 o = client::engine_point(take.figure.origin);
        const double p[3] = {o.x, o.y, o.z};
        fx::teleport_splash(p, take.seed, take.palette, &s.births);
      }
      return s;
    }
    const double shown = client::warp_shown(warp, age);
    if (!take.first_person) {
      client::Placed m = take.figure;
      m.warp = dust.warp;
      m.warp.shown = float(shown);
      s.models.push_back(m);
    } else {
      s.held_shown = float(shown);
    }
    if (births)
      client::warp_dust(warp, dust, was, age, take.seed + uint32_t(age * 1000), &s.births);
    return s;
  };
  double age = -1;  // before the first frame
  for (const double moment : take.moments) {
    while (age < moment - 1e-9) {
      const double was = age;
      const int step = age < 0 ? 0 : int((age + 1e-9) / kFrame) + 1;
      age = std::min(moment, step * kFrame);
      ge::render::FrameView fv = view;
      fv.time_s += age;
      const client::FxFrame s = stage(age, was, true);
      client::draw_world(d, world, fv, s, hand, was < 0 ? 0 : age - was, {}, out.view);
    }
    // The moment held a frame (the particles' step 0), so the picture is the moment's.
    ge::render::FrameView fv = view;
    fv.time_s += age;
    client::draw_world(d, world, fv, stage(age, age, false), hand, 0, {}, out.view);
    const ge::gpu::Image img = ge::gpu::read_now(d, out);
    film.pics.push_back(
        ge::lab::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba});
    film.names.push_back(std::string(take.arriving ? "ARRIVING " : "LEAVING ") +
                         std::to_string(std::lround(moment * 1000)) + " MS");
  }
  // Every flake out before the next take: three seconds flown with nothing new.
  client::FxFrame quiet = f;
  for (int i = 0; i < 30; i++) client::draw_world(d, world, view, quiet, hand, 0.1, {}, out.view);
}

}  // namespace shooters_edge::labs
