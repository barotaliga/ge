// THE MAP SEEN AGAIN FOR ITS WATER (the v2 BSP renderer, bsp_draw.hpp; render/water.hpp's
// `WaterViews`): a classic map's pools are level and lie in rooms, so the room they mirror is the
// map itself drawn once more, reflected in the water's plane: a PLANAR MIRROR, exact for a still
// pool, its walls and lamps where they are, never the day outside where the screen holds nothing.
//
// - THE MIRROR: the frame's own camera turned over about the level (its view_proj times a flip of
//   the height, its eye mirrored), at half the render's size, every face drawn (no cull) and those
//   on the water's far side dropped by the frame's sixth plane (bsp.wgsl's `second_view`); the
//   sky faces left with an alpha of 0, for the water to fill with the sky's model. With the eye
//   under the level the mirror is of the water below instead: what the surface's total internal
//   reflection shows from under it.
// - THE VIEW UP (the eye under the level): the map over the water drawn from the surface straight
//   over the eye, looking straight up across 150 degrees, so the world bent through Snell's window
//   (the whole sky squeezed into a cone 97 degrees across) can be read along each pixel's refracted
//   ray, far past the frame's own edges.
//
// One body a frame: the one the eye is under, else the nearest seen from above. The passes are
// "bsp.views", timed as one; the models (the players, the items) are not in them.
//
// ge:summary The map drawn again for its water: the planar mirror in a pool's plane, the view up
// ge:keywords bsp water planar-mirror planar-reflection mirror view-up snell-window bsp-views
// ge:keywords create_bsp_views prepare_bsp_views encode_bsp_views water_views clip-plane
// ge:skill brush render water
#pragma once

#include <cstdint>

#include "brush/app/bsp_draw.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/sky.hpp"
#include "render/water.hpp"

namespace ge::brush::app {

inline constexpr double kUpFov = 150;  // degrees across the view up, square

struct BspView {
  uint32_t width = 0, height = 0;
  wgpu::Texture colour, motion, mask, depth;
  wgpu::TextureView colour_view, motion_view, mask_view, depth_view;
  wgpu::Buffer uniforms;
  wgpu::BindGroup frame;
  render::FrameUniforms staged{};
  bool on = false;  // this frame's: drawn
};

struct BspViews {
  BspView mirror, up;
  int body = -1;       // the water body they are of this frame; -1 none
  bool below = false;  // the eye is under its level
  double level = 0;
};

// The views' frame blocks and groups (beside the frame's own, over the same sky).
void create_bsp_views(gpu::Device& device, BspViews& views, const wgpu::BindGroupLayout& layout,
                      const render::Sky& sky);
// Their targets for a render of `width` x `height` (again when it changes).
void create_bsp_view_targets(gpu::Device& device, BspViews& views, uint32_t width, uint32_t height);
// This frame's: the mirror in the plane y = `level` m of body `body` (-1: no view this frame), of
// the water under it when `below`, and then the view up too; `main` the frame's own uniforms as
// filled for this frame.
void prepare_bsp_views(gpu::Device& device, BspViews& views, const render::FrameUniforms& main,
                       int body, double level, bool below);
// "bsp.views": the map into the views prepared (an empty row when none is).
void encode_bsp_views(const BspDraw& draw, const BspViews& views, const gpu::PipelineSet& pipelines,
                      render::FrameRecorder& rec, wgpu::CommandEncoder& encoder);
// What the water reads of them (render/water.hpp's set_water_views), for this frame's body.
render::WaterViews water_views(const BspViews& views);

// The matrices (column-major, camera-relative world to clip and back): the frame's own with the
// height turned over (for an eye moved to its mirror image in the plane), and the view straight up
// across `fov_deg` from `near_m` (reversed depth, no far plane).
void mirror_matrices(const float view_proj[16], const float inv_view_proj[16], float out[16],
                     float inv_out[16]);
void up_matrices(double fov_deg, double near_m, float out[16], float inv_out[16]);

}  // namespace ge::brush::app
