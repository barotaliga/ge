// BEVELLED EDGES IN THE PICTURE (docs/rendering.md § Bevels): a level built of sharp-edged blocks
// (a classic brush map) drawn as if its exposed edges were rounded off a few centimetres, so a
// step's nose, a pillar's arris and a door's frame catch a highlight instead of meeting in a line
// no real stone has. Done on the screen, after the level's faces and before anything soft is drawn
// over them: no map is re-cut, and any level of any format gets it.
//
// - "bevels" reads the frame's depth alone. Each pixel's surface (its place, and its plane's normal
//   from its neighbours) is compared with the surfaces `radius_m` and two thirds and a third of it
//   away along the screen's four ways: one that falls away BEHIND the pixel's plane by more than
//   a hair, facing another way by more than kBevelMinDeg, is the far side of a convex edge that
//   near. The pixel's normal is then bent toward that face's, by half at the edge itself and less
//   further in (a round's normal), and the change a light from above and the eye makes is
//   multiplied onto the colour: the top of a ledge brightens, its underside darkens. Concave
//   edges (a wall into a floor) are left as they are.
// - The radius is a length in the world, so the bevel keeps its size with the distance: a few
//   pixels near, nothing far off (it fades out under a pixel).
//
// ge:summary Bevelled edges on the screen: convex creases found in the depth, their normals bent
// ge:keywords render bevels bevel chamfer arris edge crease convex screen-space depth ce_bevels
// ge:keywords create_bevels create_bevel_targets encode_bevels highlight brush-edges
// ge:skill render
// ge:test tests/render_bevels_test.cpp
#pragma once

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

inline constexpr double kBevelRadiusM = 0.04;  // a bevel's width on each face
inline constexpr double kBevelMinDeg = 25;     // an edge turning less stays as it is
inline constexpr double kBevelStrength = 1;    // the light's change, as worked out

struct BevelSettings {
  double radius_m = kBevelRadiusM;
  double strength = kBevelStrength;
};

struct Bevels {
  wgpu::BindGroupLayout layout;
  uint32_t p_draw = 0;
  wgpu::Buffer settings;
  wgpu::BindGroup group;
};

// The pipeline into the manifest and the settings' buffer.
void create_bevels(gpu::Device& device, gpu::PipelineSet& pipelines, Bevels& bevels);
// The pass's group over the targets' depth (again when the targets change). `uniforms` is the
// frame block's buffer.
void create_bevel_targets(gpu::Device& device, Bevels& bevels, const FrameTargets& targets,
                          const wgpu::Buffer& uniforms);
void set_bevels(gpu::Device& device, Bevels& bevels, const BevelSettings& settings);
// "bevels": the depth read, the HDR target multiplied by each pixel's change of light.
void encode_bevels(const Bevels& bevels, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder, const FrameTargets& targets);

}  // namespace ge::render
