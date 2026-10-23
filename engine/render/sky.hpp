// THE SKY: a physically based atmosphere from lookup tables built by compute (the scheme of
// Hillaire, 2020; docs/rendering.md § Sky, atmosphere and clouds), the sky pass that draws it
// behind the opaque passes, and group 0 of every draw (the frame block and these tables:
// render/wgsl/frame.wgsl).
//
// WHAT IS REDRAWN WHEN (prepare decides, encode obeys):
// - the transmittance and multiple-scattering tables: once, the first frame;
// - the sky-view table and the sky's light: when the real sun's direction has turned more than
//   kSunTurn or the eye's altitude has moved more than kEyeMove since they were built;
// - at night (the moon's light set: render/night.hpp) the MOON's sky-view table and light, the
//   same way when the moon has; by day never, so the day's work is the same;
// - the aerial-perspective volume: every frame (it is the camera's).
// The shaders are render/sky_tables.wgsl and render/sky.wgsl; the tables' sizes are stated in
// render/wgsl/atmosphere.wgsl and restated here for the textures (`kSkyViewSize`, …).
//
// ge:summary The sky: atmosphere tables by compute, aerial perspective, the sky pass, group 0
// ge:keywords render sky atmosphere hillaire transmittance multiple-scattering sky-view
// ge:keywords aerial-perspective haze froxel sun-disc sky-light irradiance lookup-table lut
// ge:keywords frame-layout frame_group create_sky prepare_sky encode_sky_tables draw_sky
// ge:keywords read_sky_light sky-light-readback moon-sky moon_view two-skies
// ge:skill render
// ge:test tests/render_sky_test.cpp
#pragma once

#include <cstdint>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

// render/wgsl/atmosphere.wgsl's table sizes.
inline constexpr uint32_t kTransmittanceW = 256, kTransmittanceH = 64;
inline constexpr uint32_t kMultiscatterW = 32, kMultiscatterH = 32;
inline constexpr uint32_t kSkyViewW = 192, kSkyViewH = 108;
inline constexpr uint32_t kAerialW = 32, kAerialH = 32, kAerialD = 32;
// Rebuild thresholds: the sun's turn (the cosine of the angle) and the eye's move, m.
inline constexpr double kSunTurnCos = 0.99996;  // about half a degree
inline constexpr double kEyeMove = 50.0;

// Group 0 of every draw (frame.wgsl's bindings): made once a device.
wgpu::BindGroupLayout frame_layout(gpu::Device& device);

struct Sky {
  wgpu::Texture transmittance, multiscatter, sky_view, moon_view, aerial, blank;
  wgpu::TextureView transmittance_view, multiscatter_view, sky_view_view, moon_view_view,
      aerial_view, blank_view;
  wgpu::Buffer light;  // frame.wgsl's SkyLight
  wgpu::Sampler sampler;
  wgpu::BindGroupLayout in_layout, out_2d, out_3d, out_buffer;
  uint32_t p_transmittance = 0, p_multiscatter = 0, p_sky_view = 0, p_aerial = 0, p_light = 0;
  uint32_t p_moon_view = 0, p_moon_light = 0, p_draw = 0;
  wgpu::BindGroup g_transmittance[2], g_multiscatter[2], g_sky_view[2], g_aerial[2], g_light[2];
  wgpu::BindGroup g_moon_view[2], g_moon_light[2];
  // What prepare decided for this frame.
  bool build_tables = true;
  bool build_view = true;
  bool build_moon = false;
  bool built = false;
  double built_sun[3] = {0, 0, 0};
  double built_eye_m = 0;
  double built_moon[3] = {0, 0, 0};
  double built_moon_eye_m = 0;
  bool moon_built = false;
};

// Make the tables and add the sky's pipelines to the manifest (the draw for an HDR target with
// the frame's depth). `uniforms` is the frame block's buffer.
void create_sky(gpu::Device& device, gpu::PipelineSet& pipelines, Sky& sky,
                const wgpu::Buffer& uniforms, const wgpu::BindGroupLayout& frame);
// Group 0 of every draw, made once the sky's tables exist.
wgpu::BindGroup frame_group(gpu::Device& device, const wgpu::BindGroupLayout& layout,
                            const wgpu::Buffer& uniforms, const Sky& sky);

// Decide which tables this frame rebuilds.
void prepare_sky(Sky& sky, const FrameView& view);
// The tables' compute pass ("sky.tables"): what prepare decided, then the aerial volume.
void encode_sky_tables(Sky& sky, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                       wgpu::CommandEncoder& encoder);
// What the sky's light buffer holds after the last tables' pass (frame.wgsl's SkyLight), read
// back blocking: for tests and labs, never in a frame.
struct SkyLightReading {
  double irradiance_up[3];  // the sun's sky's irradiance on an up-facing surface, per unit sun
  double sun_at_eye[3];     // the sun's transmittance to the eye
  double moon_up[3];        // the moon's sky's, per unit of its light
  double moon_at_eye[3];    // the moon's transmittance to the eye
};
SkyLightReading read_sky_light(gpu::Device& device, const Sky& sky);

// The sky pass ("sky"): into the frame's HDR target over its depth (both loaded).
void encode_sky(const Sky& sky, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                const wgpu::BindGroup& frame);

}  // namespace ge::render
