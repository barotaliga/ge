// The modern look's extra passes in the window (world_extras.hpp).
//
// ge:summary The window's water, lava, slime, flames and weather passes, each behind its switch
// ge:keywords world-extras water liquids flames weather precip switches create_extras
// ge:skill render shooter classicenhance
#include "shooters_edge/client/world_extras.hpp"

#include <cmath>
#include <vector>

#include "brush/app/bsp_flames.hpp"
#include "brush/app/bsp_liquids.hpp"
#include "brush/app/bsp_views.hpp"
#include "render/flame.hpp"
#include "render/liquid.hpp"
#include "render/precip.hpp"
#include "render/underwater.hpp"
#include "render/water.hpp"

namespace shooters_edge::client {

namespace ba = ge::brush::app;
namespace gr = ge::render;

constexpr double kDegree = 3.14159265358979323846 / 180;

struct WorldExtras {
  const ba::Scene* scene = nullptr;
  const ge::classicenhance::Switches* on = nullptr;
  const ba::WeatherCvars* weather = nullptr;
  ba::LiquidRows rows;
  ba::LiquidWorld world;
  gr::Water water;
  gr::Underwater under;              // the view from under the water's level
  ba::BspViews views;                // the map drawn again for the water: its mirror, the view up
  gr::WaterViews handed;             // what the water was last handed of them
  const ba::BspDraw* bsp = nullptr;  // this frame's (prepare_extras), for the views' pass
  double wind = -1, from = 0;        // the air the waves were last laid under (m/s, rad)
  uint32_t height = 1;               // the targets' rows
  gr::Liquids liquids;
  std::vector<ba::Kind> liquid_kinds;  // each liquid slot's: kLava or kSlime
  gr::Flames flames;
  std::vector<gr::Flame> fires;
  ba::FireLook fire_look;  // this frame's: bonfires or not, the wind
  double sparks_t = -1;    // the time the last sparks were thrown to (< 0: none yet)
  gr::Precip precip;
  ba::WeatherState state;
  ba::WeatherFrame now;
  bool raining = false;  // this frame's weather is on and switched on
};

void WorldExtrasDeleter::operator()(WorldExtras* x) const { delete x; }

WorldExtrasPtr create_extras(ge::gpu::Device& d, ge::gpu::PipelineSet& pipelines,
                             const wgpu::BindGroupLayout& frame, const ba::Scene& scene,
                             const ge::classicenhance::Switches& switches,
                             const ba::WeatherCvars& weather, const gr::Sky& sky) {
  WorldExtrasPtr x(new WorldExtras);
  x->scene = &scene;
  x->on = &switches;
  x->weather = &weather;
  gr::create_water(d, pipelines, x->water, frame);
  gr::create_underwater(d, pipelines, x->under, frame);
  // The maps' pools are indoors: the water lit by the room, rippled, caustics on its walls and
  // the ceiling over it.
  gr::underwater_look("pool", x->under.look);
  gr::create_liquid(d, pipelines, x->liquids, frame);
  // A player dropped in the slime sees a few metres round them, not the two centimetres a toxic
  // ooze lets through.
  x->liquids.under_reach = kUnderReach;
  gr::create_flames(d, pipelines, x->flames, frame);
  gr::PrecipSettings precip;
  precip.particles = ba::weather_particles(precip, weather.density);
  gr::create_precip(d, pipelines, x->precip, frame, precip);
  // The waves under the weather's wind as far as each pool is open to it (re-laid when it changes).
  x->wind = switches.wind ? weather.wind : 0;
  x->from = weather.wind_from * kDegree;
  x->world = ba::liquid_world(scene, x->rows, x->wind, x->from);
  ba::add_bsp_liquids(d, x->water, x->liquids, x->world, x->rows);
  ba::create_bsp_views(d, x->views, frame, sky);
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
  ba::create_bsp_view_targets(d, x.views, targets.width, targets.height);
  x.handed = {};
  gr::set_water_views(d, x.water, x.handed);
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
  options.bonfires = v2 && on.flames && on.bonfires;
  x.fire_look.bonfires = options.bonfires;
  for (double& a : x.fire_look.air) a = 0;
  if (on.wind) ba::wind_air(*x.weather, view.time_s, x.fire_look.air);
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

// The body the water's views are drawn for this frame: the one the eye is in, else the nearest
// over whose level the eye stands (-1: none drawn, or none within reach).
int view_body(const WorldExtras& x, const gr::FrameView& view, int eye_in) {
  if (eye_in >= 0) return eye_in;
  int best = -1;
  double best_d = 60 * 60;  // m^2: past 60 m a pool's reflection is a few pixels
  for (size_t i = 0; i < x.water.bodies.size(); ++i) {
    const gr::Water::Slot& s = x.water.bodies[i];
    if (s.hidden || view.eye[1] <= s.staged.info[0]) continue;
    const double dx = std::max({s.staged.box[0] - view.eye[0], 0.0, view.eye[0] - s.staged.box[2]});
    const double dz = std::max({s.staged.box[1] - view.eye[2], 0.0, view.eye[2] - s.staged.box[3]});
    const double dy = view.eye[1] - s.staged.info[0];
    const double d = dx * dx + dz * dz + dy * dy;
    if (d < best_d) {
      best_d = d;
      best = int(i);
    }
  }
  return best;
}

void prepare_extras(ge::gpu::Device& d, WorldExtras& x, const ba::BspDraw& bsp,
                    const gr::FrameView& view, const ba::Options& options,
                    const gr::FrameUniforms& uniforms) {
  const ge::classicenhance::Switches& on = *x.on;
  const bool v2 = options.mode == ba::Mode::v2;
  x.bsp = &bsp;
  for (gr::Water::Slot& s : x.water.bodies) s.hidden = !v2 || !on.water;
  for (size_t i = 0; i < x.liquids.bodies.size() && i < x.liquid_kinds.size(); ++i) {
    const bool shown = x.liquid_kinds[i] == ba::kLava ? on.lava : on.slime;
    x.liquids.bodies[i].hidden = !v2 || !shown;
  }
  // The weather's wind changed (sv_wind, sv_winddir, ce_wind): the waves laid again under it.
  const double wind = on.wind ? x.weather->wind : 0, from = x.weather->wind_from * kDegree;
  if (wind != x.wind || from != x.from) {
    x.wind = wind;
    x.from = from;
    ba::relay_liquid_wind(d, x.water, x.world, wind, from);
  }
  ba::light_rooms(x.water, x.liquids, x.world, x.rows, options.minlight, options.brightness);
  // The eye in the water when the map's leaf there is water: at any depth, in any passage.
  const int eye_in = ba::eye_water_body(x.scene->map, x.world, view.eye);
  const bool water_on = v2 && on.water;
  const int body = water_on ? view_body(x, view, eye_in) : -1;
  const double level = body >= 0 ? x.water.bodies[size_t(body)].staged.info[0] : 0;
  ba::prepare_bsp_views(d, x.views, uniforms, body, level, body >= 0 && eye_in == body);
  const gr::WaterViews handed = ba::water_views(x.views);
  if (handed.body != x.handed.body || handed.below != x.handed.below ||
      handed.has_up != x.handed.has_up || handed.level != x.handed.level) {
    x.handed = handed;
    gr::set_water_views(d, x.water, x.handed);
  }
  gr::prepare_water(d, x.water, view, eye_in);
  gr::prepare_underwater(d, x.under, x.water, view, x.height);
  gr::prepare_liquid(d, x.liquids, view);
  x.fires.clear();
  if (v2 && on.flames)
    ba::bsp_fires(*x.scene, view.time_s, options.brightness, x.fires, x.fire_look);
  gr::prepare_flames(d, x.flames, x.fires, view);
  const gr::PrecipShelter shelter = ba::bsp_shelter(bsp);
  gr::bind_precip_shelter(d, x.precip, x.raining ? &shelter : nullptr);
  gr::PrecipFrame fall;  // nothing falls: the passes keep their rows, empty
  if (x.raining && x.weather->draw) fall = x.now.draw;
  fall.near_m = view.near_m;
  gr::prepare_precip(d, x.precip, fall);
}

void extras_births(WorldExtras& x, double t_s, std::vector<gr::Particle>& out) {
  // A jump back in time (a replay's seek, a new map's clock) or a first frame throws none: the
  // next frame takes up from here.
  const double t0 = x.sparks_t;
  x.sparks_t = t_s;
  if (t0 < 0 || t_s <= t0 || t_s - t0 > 1) return;
  ba::bonfire_sparks(*x.scene, t0, t_s, x.fire_look, out);
}

bool liquid_round_eye(const WorldExtras& x) { return x.liquids.eye_under >= 0; }

void encode_extras_sim(const WorldExtras& x, const ge::gpu::PipelineSet& pipelines,
                       gr::FrameRecorder& rec, wgpu::CommandEncoder& e) {
  gr::encode_precip_sim(x.precip, pipelines, rec, e);
}

void encode_extras(const WorldExtras& x, const ge::gpu::PipelineSet& pipelines,
                   gr::FrameRecorder& rec, wgpu::CommandEncoder& e, const gr::FrameTargets& targets,
                   const wgpu::BindGroup& frame) {
  if (x.bsp) ba::encode_bsp_views(*x.bsp, x.views, pipelines, rec, e);
  gr::encode_flames(x.flames, pipelines, rec, e, targets, frame);
  gr::encode_water(x.water, pipelines, rec, e, targets, frame);
  gr::encode_underwater(x.under, pipelines, rec, e, targets, frame);
  gr::encode_liquid(x.liquids, pipelines, rec, e, targets, frame);
  gr::encode_precip(x.precip, pipelines, rec, e, targets, frame);
}

}  // namespace shooters_edge::client
