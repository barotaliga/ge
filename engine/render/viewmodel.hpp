// THE VIEW-MODEL PASS (docs/rendering.md § The view model): the first-person held weapon
// (engine/viewmodel's `HeldModel`) drawn on the GPU after the world, through its own lens and in
// its own depth range, lit per pixel by the same light as `viewmodel::shade_held`, the CPU
// reference it is held to.
//
// - THE MODELS are uploaded once (`create_viewmodel`): every frame of every held model in one
//   storage buffer (a vertex two vec4: the position and the normal, the held frame), the
//   triangles, the texture coordinates or the colours, each triangle's flags (the model's own
//   muzzle flash, `HeldModel::flame`, 1; the hands holding it, `HeldModel::hand`, 2), and the skins
//   packed into one atlas, each texel's glow in its alpha. A frame writes its draws alone.
// - THE DRAW (`HeldDraw`): the model, its two frames and the share between them, its PLACEMENT
//   (`viewmodel::Placement`, the held frame into the view's) now and a frame ago (the motion the
//   TAA reads comes from there), the gun's own vertical lens, and its LIGHT: a
//   `viewmodel::Lighting` in the view's frame (the probe the game sampled at the player, the
//   lights it kept with `light_held`, the floor, the gloss), the share of the frame's sun and sky
//   it stands in (`outdoor`: the sun through the sun's shadow maps, render/shadows.hpp, at the
//   gun's place), and the frame's own LAMPS (render/lights.hpp's clusters, read at the gun's
//   pixel) when `lamps` is set.
// - THE UNITS: the held light's 1 is a white surface lit white. On the frame's HDR target that is
//   `white_lux` on a white face (the frame's lux, as the sun's 128 000), or, at 0, the white the
//   frame's eye adapted to (`white_luminance` in wgsl/frame.wgsl), so a gun in a dim corridor is
//   seen as dim as the corridor. A lamp's candela and a held light's intensity are one light
//   when `held_candela` says so.
// - THE LENS AND THE DEPTH: the gun is projected by its own field of view (the view's when 0) at
//   the frame's aspect and jitter, and its depth is squeezed into [`held_depth_floor`, 1] of the
//   reversed-z buffer: nearer than anything of the world farther than `guard_m`, so it is never cut
//   by a wall the player stands against (the world's near plane and a player's own size keep the
//   world farther than that), and the passes after it (an effect, the fog, the TAA) find it in the
//   depth. Back faces are culled as the CPU reference culls them.
// - THE PASS ("viewmodel") draws into the HDR and motion targets with the frame's depth, after
//   the world and its transparent effects, before the TAA and the post pass; the HUD draws over
//   the post pass. `encode_viewmodel_resolved` draws the same into an RGBA8 target already
//   resolved as `viewmodel::resolve` does (exposure 1, the same roll-off, sRGB): the CPU
//   reference's twin, for the `gpu-viewmodel` lab and its comparison.
//
// Every pipeline and buffer is made by `create_viewmodel`; a frame makes nothing.
//
// ge:summary The view-model pass: the held weapon on the GPU, its own lens and depth, its light
// ge:keywords render viewmodel view-model held-model first-person weapon gun pass own-lens
// ge:keywords depth-range guard reversed-z shade_held lamps clustered-lights sun-shadow outdoor
// ge:keywords white-lux held_candela held_depth_floor create_viewmodel prepare_viewmodel
// ge:keywords encode_viewmodel encode_viewmodel_resolved resolved cpu-reference flame-hidden
// ge:skill render
// ge:test tests/render_viewmodel_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/lights.hpp"
#include "render/morph.hpp"
#include "render/view.hpp"
#include "viewmodel/held.hpp"
#include "viewmodel/light.hpp"
#include "viewmodel/place.hpp"

namespace ge::render {

inline constexpr uint32_t kMaxHeldDraws = 4;  // drawn a frame (a gun, a second hand)
inline constexpr double kHeldNearM = 0.01;    // the gun's near plane, m (the stage's)

struct HeldSettings {
  double guard_m = 0.25;  // the world nearer than this may cover the gun; farther never does
  bool resolved = false;  // also build the resolved pipeline (the reference's twin: a lab's)
};

struct HeldDraw {
  uint32_t model = 0;
  int frame_a = 0, frame_b = 0;
  float blend = 0;                 // 0: frame_a, 1: frame_b
  viewmodel::Placement now, last;  // `last`: a frame ago (a cut: the same as now)
  double fov_y = 0;                // the gun's lens, rad (0: the placement's, then the view's)
  viewmodel::Lighting light;       // in the view's frame, the held unit
  float outdoor = 0;               // the share of the frame's sun and sky
  bool lamps = true;               // the frame's local lights (render/lights.hpp)
  double white_lux = 0;            // lx on white that the held unit's 1 is (0: the eye's white)
  bool hide_flame = true;          // leave out the model's own flash (`HeldModel::flame`)
  bool hide_hands = false;         // leave out the hands holding it (`HeldModel::hand`)
};

// The reversed-z depth the gun's nearest point may take least: what the world at `guard_m` has
// under a near plane of `near_m` (near / guard, at most 0.9).
double held_depth_floor(double near_m, double guard_m);

// The candela a frame lamp has to light a white face as a held point light of `intensity` (the
// held unit at 1 m) does, where the held unit's 1 is `white_lux` on white: the same light in the
// renderer's units.
float held_candela(float intensity, double white_lux);
// A held point light (viewmodel/light.hpp) as a steady frame lamp of the same light.
LocalLight lamp_of(const viewmodel::PointLight& light, double white_lux);

// wgsl's HeldDraw, packed: what the shader reads of a draw (the tests read it back).
struct GpuHeldDraw {
  float now[12];     // the placement's three rows
  float last[12];    // a frame ago
  float lens[4];     // 1 / tan(x / 2), 1 / tan(y / 2), the gun's near plane (m), the depth floor
  uint32_t info[4];  // frame a, frame b, model, flags (1: lamps, 2: hide the flame, 4: the hands)
  float mix[4];      // the share between the frames, outdoor, white (lx; 0: the eye's), floor
  float ambient[4];  // rgb; gloss
  float direct[4];   // rgb; shininess
  float toward[4];   // view, unit; the held lights' count
  float right[4];    // the view's right in the world (camera-relative); the lens ratio
  float up[4];       // and its up
  float forward[4];  // and its forward
  float lights[viewmodel::kHeldLights][8];  // at (view, m), range; rgb, radius
};
static_assert(sizeof(GpuHeldDraw) == 496, "match viewmodel.wgsl's HeldDraw");

// One draw packed for a frame seen through `view` on a `width` x `height` render.
GpuHeldDraw pack_held_draw(const HeldDraw& d, const FrameView& view, uint32_t width,
                           uint32_t height, double guard_m);

struct HeldModels {
  wgpu::Buffer models, vertices, attrs, indices, flags, draws;
  wgpu::Texture atlas;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup group, shadows;
  uint32_t p_draw = 0, p_resolved = 0;
  bool has_resolved = false;
  HeldSettings settings;
  std::vector<uint32_t> index_counts;  // a model each
  std::vector<uint32_t> frame_counts;
  std::vector<GpuHeldDraw> staged;  // kMaxHeldDraws
  std::vector<uint32_t> staged_models;
  uint32_t drawn = 0;
  uint32_t dropped = 0;  // past kMaxHeldDraws this frame
  float depth_floor = 0.4f;
};

// Upload `models` once and build the pipelines. False when there is none or the skins do not fit
// the atlas.
bool create_viewmodel(gpu::Device& device, gpu::PipelineSet& pipelines, HeldModels& held,
                      const wgpu::BindGroupLayout& frame,
                      std::span<const viewmodel::HeldModel> models,
                      const HeldSettings& settings = {});
// The sun's shadows the gun receives (render/shadows.hpp's `SunShadows::group`); until bound, none.
void bind_viewmodel_shadows(HeldModels& held, const wgpu::BindGroup& shadows);
// This frame's draws, seen through `view` on the targets' size.
void prepare_viewmodel(gpu::Device& device, HeldModels& held, std::span<const HeldDraw> draws,
                       const FrameView& view, uint32_t width, uint32_t height);
// The pass ("viewmodel"): the draws over the HDR and motion targets and into the depth.
void encode_viewmodel(const HeldModels& held, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                      wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                      const wgpu::BindGroup& frame);
// The same resolved into `colour` (RGBA8Unorm) over `depth` (kDepthFormat), both cleared first
// ("viewmodel.resolved"); built only with `HeldSettings::resolved`.
void encode_viewmodel_resolved(const HeldModels& held, const gpu::PipelineSet& pipelines,
                               FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                               const wgpu::TextureView& colour, const wgpu::TextureView& depth,
                               uint32_t width, uint32_t height, const wgpu::BindGroup& frame);

}  // namespace ge::render
