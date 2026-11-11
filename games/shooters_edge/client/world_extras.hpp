// WHAT THE MODERN LOOK ADDS TO A MAP, EACH BEHIND ITS SWITCH: the passes the world frame
// (world_draw.hpp) draws over the BSP renderer's walls in the modern look: the water (reflecting,
// refracting, rippled), the lava and the slime (render/liquid.hpp), the torches' burning gas and
// the braziers' bonfires throwing sparks into the wind (brush/app/bsp_flames.hpp, ce_bonfires) and
// the weather that falls through the open sky (brush/app/bsp_weather.hpp). Each reads the player's
// switch (plugins/classicenhance's `Switches`, the `ce_` cvars) every frame, so `ce_water 0` in the
// console shows the pack's own warped water on the next frame; the classic look draws none of them.
//
// The lava's heat haze and the gas over lava and slime are the liquids' own (ce_heathaze,
// ce_fumes), stirred into plumes round the bodies burning or fuming in them (`stir_extras`).
//
// The switches that change the BSP renderer's own options (the teleporters' and the pools' light)
// are applied by `dress_extras`, with the weather's wet, puddles and fog. THE WATER'S STOP
// (app/look.hpp's Stops, read every frame): low a still surface lit by the room (no chop, no
// reflection, no wet band at the waterline, no sun under it), medium the chop and the screen
// marched for its reflection, high the map drawn again in the pool's plane for its mirror (the
// dearest part: the whole map's faces again) and the sun's caustics and shafts. A liquid
// switched off is drawn by the BSP renderer as the pack's warped surface. Under the water the view
// is the engine's (render/underwater.hpp), and the room over the surface is drawn without the
// map's visibility, which the map's tools often worked out with the liquids opaque.
//
// The water mirrors the room it lies in (the map drawn again in its plane,
// brush/app/bsp_views.hpp), its waves grown by the weather's wind (sv_wind, sv_winddir) as far as
// the pool is open to it (brush/app/bsp_liquids.hpp's pool site; ce_wind 0: still air), and the eye
// is in it when the map says so (the leaf at the eye), at any depth and in a flooded passage beyond
// the top.
//
// ge:summary The modern look's water, lava, slime, flames and weather in the window, by ce_ switch
// ge:keywords world-extras water lava slime liquids flames torches weather rain snow precip
// ge:keywords underwater warped-liquids planar-mirror bsp-views pool-wind
// ge:keywords wade_extras wading ce_wade plunge splash fireball lava-ball
// ge:keywords classicenhance switches ce_water ce_lava ce_slime ce_flames ce_weather ce_shadows
// ge:keywords bonfires ce_bonfires sparks extras_births ce_water_quality water-stop
// ge:keywords ce_heathaze ce_fumes stir_extras extras_level_at heat-haze fumes
// ge:skill render shooter classicenhance
#pragma once

#include <memory>
#include <span>

#include "brush/app/bsp_draw.hpp"
#include "brush/app/bsp_weather.hpp"
#include "brush/app/draw_data.hpp"
#include "classicenhance/switches.hpp"
#include "gpu/device.hpp"
#include "render/frame.hpp"
#include "render/liquid.hpp"
#include "render/particles.hpp"
#include "render/sky.hpp"
#include "render/view.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/world_models.hpp"

namespace shooters_edge::client {

struct WorldExtras;
struct WorldExtrasDeleter {
  void operator()(WorldExtras* x) const;
};
using WorldExtrasPtr = std::unique_ptr<WorldExtras, WorldExtrasDeleter>;

// The passes for `scene` (its liquids found and laid out), their pipelines into `pipelines` while
// the map loads. `switches` (the switches as drawn), `weather` and `stops` are read every frame
// and must outlive the extras.
WorldExtrasPtr create_extras(ge::gpu::Device& device, ge::gpu::PipelineSet& pipelines,
                             const wgpu::BindGroupLayout& frame, const ge::brush::app::Scene& scene,
                             const ge::classicenhance::Switches& switches,
                             const ge::brush::app::WeatherCvars& weather,
                             const shooters_edge::look::Stops& stops, const ge::render::Sky& sky);
// The same in parts, so a window can make one a frame (each parses its shaders: 10 to 20 ms):
// `new_extras`, then every part from 0 to kExtrasParts - 1 in order (0 the water, 1 the lava and
// slime, 2 the flames, the weather and the map's bodies laid out).
inline constexpr int kExtrasParts = 3;
WorldExtrasPtr new_extras(const ge::brush::app::Scene& scene,
                          const ge::classicenhance::Switches& switches,
                          const ge::brush::app::WeatherCvars& weather,
                          const shooters_edge::look::Stops& stops);
void create_extras_part(ge::gpu::Device& device, ge::gpu::PipelineSet& pipelines,
                        const wgpu::BindGroupLayout& frame, const ge::render::Sky& sky,
                        WorldExtras& x, int part);
// Again at every resize, after the frame's targets are made.
void bind_extras_targets(ge::gpu::Device& device, WorldExtras& x,
                         const ge::render::FrameTargets& targets);

// Before the frame's uniforms are filled: the weather stepped to the view's time and laid on the
// view (its wet, its fog, its flash), and the switches on the renderer's `options`.
void dress_extras(WorldExtras& x, ge::render::FrameView& view, ge::brush::app::Options& options,
                  double dt);
// After the BSP renderer is prepared: each pass's upload (hidden by its switch or by the classic
// look); `uniforms` the frame's own block as filled for this frame (the water's mirror is its
// camera turned over).
void prepare_extras(ge::gpu::Device& device, WorldExtras& x, const ge::brush::app::BspDraw& bsp,
                    const ge::render::FrameView& view, const ge::brush::app::Options& options,
                    const ge::render::FrameUniforms& uniforms);
// Before prepare_extras: the players wading in lava or slime this frame (`models`' players and
// `self`, the local player, when not null) stamped onto the surface they stand in (ce_wade,
// render/liquid_wade.hpp), at the frame's time `t_s`; and every other thing in `models` that
// crossed a lava's or a slime's surface since the frame before (a ball of lava hurled out of it)
// a plunge: a heave going out, a crater and a splash coming down.
void wade_extras(WorldExtras& x, std::span<const Placed> models, const Placed* self, double t_s);
// Before prepare_extras: this frame's stirs over the liquids round the bodies burning or fuming in
// them (client/burning.hpp), and where one has just fallen into lava (its crust broken there at
// `t_s`).
void stir_extras(WorldExtras& x, std::span<const ge::render::LiquidStir> stirs,
                 std::span<const ge::render::LiquidStir> fell_in, double t_s);
// A bench's: the air over the liquids drawn `n` times a frame (render/liquid.hpp's air_draws).
void set_extras_air_draws(WorldExtras& x, int n);
// The level (m) of the drawn lava or slime body whose box holds `at` (m), or -1e9 when none does;
// `x` a WorldExtras (burning.hpp's LevelAt).
double extras_level_at(const void* x, const double at[3]);
// After prepare_extras: the bonfires' sparks since the last call, to `t_s`, appended to `out` for
// the frame's particle pool (none when ce_bonfires is off, or on the first call), and the drops
// of this frame's splashes (wade_extras).
void extras_births(WorldExtras& x, double t_s, std::vector<ge::render::Particle>& out);
// After prepare_extras: whether the eye is under a lava's or a slime's level and the liquid is
// drawn round it (render/liquid.hpp's view from below), so the screen's blend leaves it out.
bool liquid_round_eye(const WorldExtras& x);
// m: the least an eye in a liquid sees (render/liquid.hpp's `under_reach`).
inline constexpr double kUnderReach = 3;
// Before the world: the fall's simulation.
void encode_extras_sim(const WorldExtras& x, const ge::gpu::PipelineSet& pipelines,
                       ge::render::FrameRecorder& rec, wgpu::CommandEncoder& encoder);
// After the world's veil: the water's views of the map, the flames, the water, the liquids and the
// fall.
void encode_extras(const WorldExtras& x, const ge::gpu::PipelineSet& pipelines,
                   ge::render::FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                   const ge::render::FrameTargets& targets, const wgpu::BindGroup& frame);

}  // namespace shooters_edge::client
