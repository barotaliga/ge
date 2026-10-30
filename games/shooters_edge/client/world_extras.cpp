// The modern look's extra passes in the window (world_extras.hpp).
//
// ge:summary The window's water, lava, slime, flames and weather passes, each behind its switch
// ge:keywords world-extras water liquids flames weather precip switches create_extras
// ge:skill render shooter classicenhance
#include "shooters_edge/client/world_extras.hpp"

#include <vector>

#include "brush/app/bsp_flames.hpp"
#include "brush/app/bsp_liquids.hpp"
#include "render/flame.hpp"
#include "render/liquid.hpp"
#include "render/precip.hpp"
#include "render/underwater.hpp"
#include "render/water.hpp"

namespace shooters_edge::client {

namespace ba = ge::brush::app;
namespace gr = ge::render;

struct WorldExtras {
  const ba::Scene* scene = nullptr;
  const ge::classicenhance::Switches* on = nullptr;
  const ba::WeatherCvars* weather = nullptr;
  ba::LiquidRows rows;
  ba::LiquidWorld world;
  gr::Water water;
  gr::Underwater under;  // the view from under the water's level
  uint32_t height = 1;   // the targets' rows
  gr::Liquids liquids;
  std::vector<ba::Kind> liquid_kinds;  // each liquid slot's: kLava or kSlime
  gr::Flames flames;
  std::vector<gr::Flame> fires;
  gr::Precip precip;
  ba::WeatherState state;
  ba::WeatherFrame now;
  bool raining = false;  // this frame's weather is on and switched on
};

void WorldExtrasDeleter::operator()(WorldExtras* x) const { delete x; }

WorldExtrasPtr create_extras(ge::gpu::Device& d, ge::gpu::PipelineSet& pipelines,
                             const wgpu::BindGroupLayout& frame, const ba::Scene& scene,
                             const ge::classicenhance::Switches& switches,
                             const ba::WeatherCvars& weather) {
  WorldExtrasPtr x(new WorldExtras);
  x->scene = &scene;
  x->on = &switches;
  x->weather = &weather;
  gr::create_water(d, pipelines, x->water, frame);
  gr::create_underwater(d, pipelines, x->under, frame);
  // The pools are clear enough to see across: a third of the water's own murk.
  x->under.look.fog = 1.0 / 3;
  gr::create_liquid(d, pipelines, x->liquids, frame);
  gr::create_flames(d, pipelines, x->flames, frame);
  gr::PrecipSettings precip;
  precip.particles = ba::weather_particles(precip, weather.density);
  gr::create_precip(d, pipelines, x->precip, frame, precip);
  x->world = ba::liquid_world(scene, x->rows);
  ba::add_bsp_liquids(d, x->water, x->liquids, x->world, x->rows);
  // The liquid slots in the order add_bsp_liquids made them: every body that is not water and
  // whose row is known.
  for (const ba::Kind k : x->world.kinds) {
    if (k == ba::kWater) continue;
    if (gr::find_liquid_look(k == ba::kLava ? x->rows.lava : x->rows.slime))
      x->liquid_kinds.push_back(k);
  }
  return x;
}

void bind_extras_targets(ge::gpu::Device& d, WorldExtras& x, const gr::FrameTargets& targets) {
  gr::create_water_targets(d, x.water, targets);
  gr::bind_underwater(d, x.under, x.water);
  x.height = targets.height;
  gr::create_liquid_targets(d, x.liquids, targets);
  gr::bind_flame_targets(d, x.flames, targets);
  gr::bind_precip_targets(d, x.precip, targets);
}

void dress_extras(WorldExtras& x, gr::FrameView& view, ba::Options& options, double dt) {
  const ge::classicenhance::Switches& on = *x.on;
  const bool v2 = options.mode == ba::Mode::v2;
  if (!on.shadows) options.shadowed = 0;
  options.portal_lights = options.portal_lights && on.teleports;
  if (!on.lava) options.lava_glow = 0;
  if (!on.slime) options.slime_glow = 0;
  // A liquid switched off is the pack's own warped surface, as the classic draws it.
  options.warped_water = !on.water;
  options.warped_slime = !on.slime;
  options.warped_lava = !on.lava;
  // Under the water (last frame's eye) the room over the surface must be drawn for the view up
  // through it, and a map's visibility is often worked out with the liquids opaque.
  if (v2 && on.water && x.water.eye_under >= 0) options.pvs = false;
  x.raining = v2 && on.weather && x.weather->on;
  if (!x.raining) return;
  x.now = ba::step_weather(*x.weather, x.state, view.eye, view.time_s, 0.2);
  ba::apply_weather(x.now, *x.weather, view, options, on.fog);
  x.now.draw.dt = dt;
  x.now.draw.near_m = view.near_m;
}

void prepare_extras(ge::gpu::Device& d, WorldExtras& x, const ba::BspDraw& bsp,
                    const gr::FrameView& view, const ba::Options& options) {
  const ge::classicenhance::Switches& on = *x.on;
  const bool v2 = options.mode == ba::Mode::v2;
  for (gr::Water::Slot& s : x.water.bodies) s.hidden = !v2 || !on.water;
  for (size_t i = 0; i < x.liquids.bodies.size() && i < x.liquid_kinds.size(); ++i) {
    const bool shown = x.liquid_kinds[i] == ba::kLava ? on.lava : on.slime;
    x.liquids.bodies[i].hidden = !v2 || !shown;
  }
  ba::light_rooms(x.water, x.liquids, x.rows, options.minlight, options.brightness);
  gr::prepare_water(d, x.water, view);
  gr::prepare_underwater(d, x.under, x.water, view, x.height);
  gr::prepare_liquid(d, x.liquids, view);
  x.fires.clear();
  if (v2 && on.flames) ba::bsp_fires(*x.scene, view.time_s, options.brightness, x.fires);
  gr::prepare_flames(d, x.flames, x.fires, view);
  const gr::PrecipShelter shelter = ba::bsp_shelter(bsp);
  gr::bind_precip_shelter(d, x.precip, x.raining ? &shelter : nullptr);
  gr::PrecipFrame fall;  // nothing falls: the passes keep their rows, empty
  if (x.raining && x.weather->draw) fall = x.now.draw;
  fall.near_m = view.near_m;
  gr::prepare_precip(d, x.precip, fall);
}

void encode_extras_sim(const WorldExtras& x, const ge::gpu::PipelineSet& pipelines,
                       gr::FrameRecorder& rec, wgpu::CommandEncoder& e) {
  gr::encode_precip_sim(x.precip, pipelines, rec, e);
}

void encode_extras(const WorldExtras& x, const ge::gpu::PipelineSet& pipelines,
                   gr::FrameRecorder& rec, wgpu::CommandEncoder& e, const gr::FrameTargets& targets,
                   const wgpu::BindGroup& frame) {
  gr::encode_flames(x.flames, pipelines, rec, e, targets, frame);
  gr::encode_water(x.water, pipelines, rec, e, targets, frame);
  gr::encode_underwater(x.under, pipelines, rec, e, targets, frame);
  gr::encode_liquid(x.liquids, pipelines, rec, e, targets, frame);
  gr::encode_precip(x.precip, pipelines, rec, e, targets, frame);
}

}  // namespace shooters_edge::client
