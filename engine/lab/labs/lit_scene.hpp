// THE LIT LABS' SCENE (shared by `gpu-lights` and `gpu-flames`): flagstones at y = 0 over 256 m,
// a family of props (boxes, spheres and cylinders in the lab's own surfaces), the sky, and the
// local lights and flames a frame hands it, drawn through the whole frame (the terrain, the
// instances, the sky, the flames, post) on a device of its own and read back as a picture.
//
// ge:summary The lit labs' scene: flagstones, props in their surfaces, lamps and flames, read back
// ge:keywords lab lit-scene courtyard flagstones props lamps flames make_scene draw view_at
// ge:skill render labs
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "render/flame.hpp"
#include "render/frame.hpp"
#include "render/hiz.hpp"
#include "render/instances.hpp"
#include "render/lights.hpp"
#include "render/mesh.hpp"
#include "render/night.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/surface.hpp"
#include "render/terrain.hpp"

namespace ge::lab::lit_scene {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace libm = ge::core::libm;

// The flagstones' middle (the field runs 0..256 on x and z).
inline constexpr double kCx = 128, kCz = 128;

inline gr::Instance at(double x, double y, double z, float sx, float sy, float sz, uint32_t mesh,
                       uint32_t material) {
  gr::Instance in;
  in.position[0] = x;
  in.position[1] = y;
  in.position[2] = z;
  in.scale[0] = sx;
  in.scale[1] = sy;
  in.scale[2] = sz;
  in.mesh = mesh;
  in.material = material;
  return in;
}

// Meshes: 0 a unit box on its base, 1 a unit sphere, 2 a unit cylinder on its base.
inline std::vector<gr::MeshSource> meshes() {
  std::vector<gr::MeshSource> m(3);
  gr::MeshData box, ball, post;
  const float base[3] = {0, 0.5f, 0}, half[3] = {0.5f, 0.5f, 0.5f};
  gr::add_box(box, base, half, 0);
  const float zero[3] = {0, 0, 0}, one[3] = {1, 1, 1};
  gr::add_ellipsoid(ball, zero, one, 64, 128, 0);  // fine: the mesh is flat-shaded
  gr::add_cylinder(post, 0, 1, 0.5f, 0.5f, 24, 0);
  m[0].lods = {{box, 0}};
  m[1].lods = {{ball, 0}};
  m[2].lods = {{post, 0}};
  return m;
}

inline float flat_ground(double x, double z) {
  // Flagstones' slight unevenness, a few millimetres.
  return static_cast<float>(0.004 * libm::sin(x * 2.1) * libm::cos(z * 1.7) - 0.05);
}

struct Scene {
  ge::gpu::PipelineSet pipelines;
  gr::FrameTargets targets;
  wgpu::Buffer uniforms;
  gr::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_group, light;
  gr::Sky sky;
  gr::Terrain terrain;
  gr::Hiz hiz;
  gr::InstancePipelines ip;
  gr::InstanceFamily props;
  gr::Lights lights;
  gr::Flames flames;
  gr::Post post;
  gr::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
  ge::gpu::Target out;
  uint32_t w = 0, h = 0;
};

// `more` adds a lab's own pipelines before the load closes (false: it failed); `caster_maps` > 0
// gives the props that many shadow maps' caster slots (render/shadows.hpp).
inline bool make_scene(ge::gpu::Device& d, Scene& s, std::span<const gr::Surface> surfaces,
                       const std::vector<gr::Instance>& props, uint32_t w, uint32_t h,
                       const std::string& label, const std::function<bool()>& more = {},
                       int caster_maps = 0) {
  s.w = w;
  s.h = h;
  s.field = ge::core::create_heightfield(0, 0, 1, 257, 257);
  s.albedo.resize(size_t{257} * 257 * 4);
  for (size_t r = 0; r < 257; ++r) {
    for (size_t c = 0; c < 257; ++c) {
      const size_t i = r * 257 + c;
      s.field.data[i] = flat_ground(static_cast<double>(c), static_cast<double>(r));
      // Grey-brown flags, every other one a shade darker.
      const bool dark = ((c / 2) + (r / 2)) % 2 == 0;
      const uint8_t px[4] = {static_cast<uint8_t>(dark ? 150 : 170),
                             static_cast<uint8_t>(dark ? 140 : 160),
                             static_cast<uint8_t>(dark ? 126 : 145), 255};
      std::memcpy(&s.albedo[i * 4], px, 4);
    }
  }
  s.targets = gr::create_targets(d, label, w, h);
  s.uniforms = gr::make_buffer(d, label + ".uniforms", sizeof(gr::FrameUniforms),
                               wgpu::BufferUsage::Uniform);
  s.layout = gr::frame_layout(d);
  gr::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  gr::TerrainSource src;
  src.height = &s.field;
  src.albedo = &s.albedo;
  src.base_spacing = 0.5;
  src.levels = 7;
  gr::create_terrain(d, s.pipelines, s.terrain, src, s.layout);
  gr::create_hiz_pipelines(d, s.pipelines, s.hiz);
  gr::create_instance_pipelines(d, s.pipelines, s.ip, s.layout);
  gr::create_flames(d, s.pipelines, s.flames, s.layout);
  gr::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  if (more && !more()) return false;
  if (!s.pipelines.finish(d)) return false;
  ge::gpu::end_load(d);
  gr::create_lights(d, s.lights);
  s.frame = gr::frame_group(d, s.layout, s.uniforms, s.sky, s.lights.buffer);
  gr::create_hiz_targets(d, s.hiz, s.targets);
  gr::bind_flame_targets(d, s.flames, s.targets);
  s.post_group = gr::post_group(d, s.post, s.targets.hdr_view);
  gr::create_family(d, s.ip, s.props, label + ".props", meshes(), surfaces, 256);
  if (caster_maps > 0) gr::create_family_casters(d, s.ip, s.props, caster_maps);
  gr::bind_family_hiz(d, s.ip, s.props, s.hiz);
  s.light = gr::instance_light_group(d, s.ip, s.terrain);
  gr::set_instances(d, s.props, 0, props);
  s.rec.init(d, label + ".timer", 16);
  s.out = ge::gpu::create_target(d, label + ".final", w, h);
  return true;
}

inline gl::Picture draw(ge::gpu::Device& d, Scene& s, const gr::FrameView& v,
                        std::span<const gr::LocalLight> lamps,
                        std::span<const gr::Flame> flames = {}) {
  s.rec.begin_frame(d);
  gr::fill_uniforms(s.staged, v, s.w, s.h);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  gr::prepare_sky(s.sky, v);
  gr::prepare_terrain(d, s.terrain, v);
  gr::prepare_family(d, s.props, v, s.h, false);
  gr::prepare_lights(d, s.lights, lamps, v, s.w, s.h);
  gr::prepare_flames(d, s.flames, flames, v);
  gr::InstanceFamily* families[] = {&s.props};
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  gr::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  gr::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  gr::encode_instances_cull(families, s.ip, s.pipelines, s.rec, e, s.frame, false);
  gr::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  gr::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_instances_depth(families, s.ip, s.pipelines, s.rec, e, s.targets, s.frame, s.light,
                             false);
  gr::encode_hiz(s.hiz, s.pipelines, s.rec, e);
  gr::encode_instances_cull(families, s.ip, s.pipelines, s.rec, e, s.frame, true);
  gr::encode_instances_depth(families, s.ip, s.pipelines, s.rec, e, s.targets, s.frame, s.light,
                             true);
  gr::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_instances(families, s.ip, s.pipelines, s.rec, e, s.targets, s.frame, s.light);
  gr::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_flames(s.flames, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, s.out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  const ge::gpu::Image img = ge::gpu::read_now(d, s.out);
  return gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
}

// A moonless night (the sun 20 degrees down) or the noon sun, from (x, y, z).
inline gr::FrameView view_at(double x, double y, double z, double heading, double pitch, bool night,
                             double t) {
  gr::FrameView v;
  v.eye[0] = x;
  v.eye[1] = y;
  v.eye[2] = z;
  v.heading = heading;
  v.pitch = pitch;
  v.fov_y = 1.0;
  v.near_m = 0.1;
  v.altitude_m = 400;
  v.time_s = t;
  v.sun_azimuth = 2.4;
  v.sun_elevation = night ? -0.35 : 1.1;
  if (night) {
    gr::Moon none{-0.8, 1.0, 0.0, 384400};
    gr::set_night(v, none, 46, 120);
  }
  return v;
}

}  // namespace ge::lab::lit_scene
