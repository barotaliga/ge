// THE EXPLOSION LABS' SCENE (shared by `gpu-explosion` and `gpu-muzzle`): a flat dirt yard with a
// wall across its far side, drawn under the sky with the particles and the explosions' pass
// (blasts and muzzle flames) on a device of its own, read back as a picture with the pass's own
// GPU time; a still settled over frames as the TAA resolve settles it; sheets of pictures.
//
// ge:summary The explosion labs' scene: the yard, the sky, particles, explosions, timed draws
// ge:keywords lab explosion muzzle scene yard make_scene draw settled sheet_of gpu-explosion
// ge:keywords gpu-muzzle
// ge:skill render labs
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/water_scene.hpp"
#include "lab/raster.hpp"
#include "render/explosion.hpp"
#include "render/explosions.hpp"
#include "render/frame.hpp"
#include "render/particles.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace ge::lab::explosion_scene {

namespace gl = ge::lab;
namespace gr = ge::render;
using ge::lab::water_scene::paste;

// THE YARD: 256 m of flat dirt on 1 m cells, a wall 4 m high across it at z = 140.
constexpr double kMid = 128, kWallZ = 140, kWallH = 4;

inline float yard(double, double z) { return z >= kWallZ ? static_cast<float>(kWallH) : 0.0f; }

struct Scene {
  ge::gpu::PipelineSet pipelines;
  gr::FrameTargets targets;
  wgpu::Buffer uniforms;
  gr::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_group;
  gr::Sky sky;
  gr::Terrain terrain;
  gr::Particles particles;
  gr::Explosions explosions;
  gr::Post post;
  gr::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
  ge::gpu::Target out;
  uint32_t w = 0, h = 0;
};

inline bool make_scene(ge::gpu::Device& d, Scene& s, uint32_t w, uint32_t h, uint32_t steps) {
  s.w = w;
  s.h = h;
  s.field = ge::core::create_heightfield(0, 0, 1, 257, 257);
  s.albedo.resize(size_t{257} * 257 * 4);
  for (int r = 0; r < 257; ++r) {
    for (int c = 0; c < 257; ++c) {
      const size_t i = static_cast<size_t>(r) * 257 + static_cast<size_t>(c);
      s.field.data[i] = yard(c, r);
      // Packed dirt, a little mottled; the wall a pale stone.
      const bool wall = r >= kWallZ - 1;
      const int m = ((c * 7 + r * 13) % 9) - 4;
      const uint8_t px[4] = {static_cast<uint8_t>((wall ? 168 : 118) + m),
                             static_cast<uint8_t>((wall ? 160 : 98) + m),
                             static_cast<uint8_t>((wall ? 146 : 76) + m), 255};
      std::memcpy(&s.albedo[i * 4], px, 4);
    }
  }
  s.targets = gr::create_targets(d, "gpu-explosion", w, h);
  s.uniforms = gr::make_buffer(d, "gpu-explosion.uniforms", sizeof(gr::FrameUniforms),
                               wgpu::BufferUsage::Uniform);
  s.layout = gr::frame_layout(d);
  gr::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  gr::TerrainSource src;
  src.height = &s.field;
  src.albedo = &s.albedo;
  src.base_spacing = 1.0;
  src.levels = 7;
  gr::create_terrain(d, s.pipelines, s.terrain, src, s.layout);
  gr::create_particles(d, s.pipelines, s.particles, s.layout, s.terrain, {.capacity = 4096});
  gr::create_explosions(d, s.pipelines, s.explosions, s.layout, {.steps = steps});
  gr::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  if (!s.pipelines.finish(d)) return false;
  ge::gpu::end_load(d);
  s.frame = gr::frame_group(d, s.layout, s.uniforms, s.sky);
  gr::bind_particle_targets(d, s.particles, s.targets);
  gr::bind_explosion_targets(d, s.explosions, s.targets);
  s.post_group = gr::post_group(d, s.post, s.targets.hdr_view);
  s.rec.init(d, "gpu-explosion.timer", 10);
  s.out = ge::gpu::create_target(d, "gpu-explosion.final", w, h);
  return true;
}

inline gr::FrameView eye_at(double x, double y, double z, double pitch, double sun_el, double t,
                            uint64_t frame) {
  gr::FrameView v;
  v.eye[0] = x;
  v.eye[1] = y;
  v.eye[2] = z;
  v.pitch = pitch;
  v.fov_y = 1.0;
  v.sun_elevation = sun_el;
  v.sun_azimuth = 2.3;
  v.altitude_m = 300;
  v.time_s = t;
  v.frame = frame;
  return v;
}

struct Timed {
  gl::Picture pic;
  double ms = 0;        // the explosions pass on the GPU (its timestamps), else the frame's wall
  double frame_ms = 0;  // every pass on the GPU
};

// One frame: the particles flown `dt` s first (births waiting go in), then the picture.
inline Timed draw(ge::gpu::Device& d, Scene& s, const gr::FrameView& v,
                  const std::vector<gr::Blast>& blasts, double dt,
                  std::span<const gr::MuzzleFire> fires = {}) {
  s.rec.begin_frame(d);
  gr::fill_uniforms(s.staged, v, s.w, s.h);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  gr::prepare_sky(s.sky, v);
  gr::prepare_terrain(d, s.terrain, v);
  gr::prepare_particles(d, s.particles, dt, v.near_m);
  gr::prepare_explosions(d, s.explosions, blasts, v, s.w, s.h, fires);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  gr::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  gr::encode_particles_sim(s.particles, s.pipelines, s.rec, e);
  gr::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  gr::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  gr::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_particles(s.particles, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_explosions(s.explosions, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, s.out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  const auto t0 = std::chrono::steady_clock::now();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  const ge::gpu::Image img = ge::gpu::read_now(d, s.out);
  const auto t1 = std::chrono::steady_clock::now();
  s.rec.wait(d);
  Timed out{gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
            std::chrono::duration<double, std::milli>(t1 - t0).count(), 0};
  out.frame_ms = out.ms;
  if (s.rec.timed) {
    out.frame_ms = 0;
    for (int i = 0; i < s.rec.stats.count; ++i) {
      const gr::PassStat& p = s.rec.stats.passes[static_cast<size_t>(i)];
      if (p.gpu_ms == p.gpu_ms) out.frame_ms += p.gpu_ms;  // NaN: not measured
      if (p.name == "explosions") out.ms = p.gpu_ms;
    }
  }
  return out;
}

// The march's jitter averaged over `n` frames at one moment, as the TAA resolve settles a still.
inline gl::Picture settled(ge::gpu::Device& d, Scene& s, gr::FrameView v,
                           const std::vector<gr::Blast>& blasts, int n,
                           std::span<const gr::MuzzleFire> fires = {}) {
  std::vector<uint32_t> sum;
  gl::Picture first;
  for (int k = 0; k < n; ++k) {
    v.frame = static_cast<uint64_t>(k);
    const gl::Picture p = draw(d, s, v, blasts, 0, fires).pic;
    if (k == 0) {
      first = p;
      sum.assign(p.rgba.size(), 0);
    }
    for (size_t i = 0; i < p.rgba.size(); ++i) sum[i] += p.rgba[i];
  }
  for (size_t i = 0; i < sum.size(); ++i) first.rgba[i] = static_cast<uint8_t>(sum[i] / n);
  return first;
}

inline gl::Picture sheet_of(const std::vector<gl::Picture>& pics,
                            const std::vector<std::string>& labels, int cols) {
  const int tw = pics[0].width, th = pics[0].height;
  const int rows = (static_cast<int>(pics.size()) + cols - 1) / cols;
  gl::Picture sheet =
      gl::create_picture(cols * (tw + 8) + 8, rows * (th + 16) + 6, {250, 250, 248});
  for (size_t i = 0; i < pics.size(); ++i) {
    const int ox = 8 + static_cast<int>(i % static_cast<size_t>(cols)) * (tw + 8);
    const int oy = 4 + static_cast<int>(i / static_cast<size_t>(cols)) * (th + 16);
    gl::draw_text(sheet, ox, oy, labels[i], {24, 26, 32});
    paste(sheet, pics[i], ox, oy + 11);
  }
  return sheet;
}

// Pixels that differ between two pictures by more than a few levels.
inline int changed(const gl::Picture& a, const gl::Picture& b) {
  int n = 0;
  for (size_t i = 0; i + 3 < a.rgba.size(); i += 4) {
    int dl = 0;
    for (int c = 0; c < 3; ++c) dl = std::max(dl, std::abs(a.rgba[i + c] - b.rgba[i + c]));
    n += dl > 3 ? 1 : 0;
  }
  return n;
}

}  // namespace ge::lab::explosion_scene
