// WHAT THE MODERN LOOK ADDS TO A MAP, EACH BEHIND ITS SWITCH: the passes the world frame
// (world_draw.hpp) draws over the BSP renderer's walls in the modern look: the water (reflecting,
// refracting, rippled), the lava and the slime (render/liquid.hpp), the torches' burning gas and
// the braziers' bonfires throwing sparks into the wind (brush/app/bsp_flames.hpp, ce_bonfires) and
// the weather that falls through the open sky (brush/app/bsp_weather.hpp). Each reads the player's
// switch (plugins/classicenhance's `Switches`, the `ce_` cvars) every frame, so `ce_water 0` in the
// console shows the pack's own warped water on the next frame; the classic look draws none of them.
//
// The switches that change the BSP renderer's own options (the shadows, the teleporters' and the
// pools' light) are applied by `dress_extras`, with the weather's wet, puddles and fog. A liquid
// switched off is drawn by the BSP renderer as the pack's warped surface. Under the water the view
// is the engine's (render/underwater.hpp), and the room over the surface is drawn without the
// map's visibility, which the map's tools often worked out with the liquids opaque.
//
// ge:summary The modern look's water, lava, slime, flames and weather in the window, by ce_ switch
// ge:keywords world-extras water lava slime liquids flames torches weather rain snow precip
// ge:keywords underwater warped-liquids
// ge:keywords classicenhance switches ce_water ce_lava ce_slime ce_flames ce_weather ce_shadows
// ge:keywords bonfires ce_bonfires sparks extras_births
// ge:skill render shooter classicenhance
#pragma once

#include <memory>

#include "brush/app/bsp_draw.hpp"
#include "brush/app/bsp_weather.hpp"
#include "brush/app/draw_data.hpp"
#include "classicenhance/switches.hpp"
#include "gpu/device.hpp"
#include "render/frame.hpp"
#include "render/particles.hpp"
#include "render/view.hpp"

namespace shooters_edge::client {

struct WorldExtras;
struct WorldExtrasDeleter {
  void operator()(WorldExtras* x) const;
};
using WorldExtrasPtr = std::unique_ptr<WorldExtras, WorldExtrasDeleter>;

// The passes for `scene` (its liquids found and laid out), their pipelines into `pipelines` while
// the map loads. `switches` and `weather` are read every frame and must outlive the extras.
WorldExtrasPtr create_extras(ge::gpu::Device& device, ge::gpu::PipelineSet& pipelines,
                             const wgpu::BindGroupLayout& frame, const ge::brush::app::Scene& scene,
                             const ge::classicenhance::Switches& switches,
                             const ge::brush::app::WeatherCvars& weather);
// Again at every resize, after the frame's targets are made.
void bind_extras_targets(ge::gpu::Device& device, WorldExtras& x,
                         const ge::render::FrameTargets& targets);

// Before the frame's uniforms are filled: the weather stepped to the view's time and laid on the
// view (its wet, its fog, its flash), and the switches on the renderer's `options`.
void dress_extras(WorldExtras& x, ge::render::FrameView& view, ge::brush::app::Options& options,
                  double dt);
// After the BSP renderer is prepared: each pass's upload (hidden by its switch or by the classic
// look).
void prepare_extras(ge::gpu::Device& device, WorldExtras& x, const ge::brush::app::BspDraw& bsp,
                    const ge::render::FrameView& view, const ge::brush::app::Options& options);
// After prepare_extras: the bonfires' sparks since the last call, to `t_s`, appended to `out` for
// the frame's particle pool (none when ce_bonfires is off, or on the first call).
void extras_births(WorldExtras& x, double t_s, std::vector<ge::render::Particle>& out);
// After prepare_extras: whether the eye is under a lava's or a slime's level and the liquid is
// drawn round it (render/liquid.hpp's view from below), so the screen's blend leaves it out.
bool liquid_round_eye(const WorldExtras& x);
// m: the least an eye in a liquid sees (render/liquid.hpp's `under_reach`).
inline constexpr double kUnderReach = 3;
// Before the world: the fall's simulation.
void encode_extras_sim(const WorldExtras& x, const ge::gpu::PipelineSet& pipelines,
                       ge::render::FrameRecorder& rec, wgpu::CommandEncoder& encoder);
// After the world's veil: the flames, the water, the liquids and the fall.
void encode_extras(const WorldExtras& x, const ge::gpu::PipelineSet& pipelines,
                   ge::render::FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                   const ge::render::FrameTargets& targets, const wgpu::BindGroup& frame);

}  // namespace shooters_edge::client
