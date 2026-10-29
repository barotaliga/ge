// LAB `gpu-beam`: LIGHTNING BEAMS (render/beam.hpp, render/beams.hpp) on the software adapter, in a
// stone corridor at the bottom of a moonless night lit by one torch at its far end. A gun held in
// the first person's place (lower right of the view) fires a beam from its muzzle to wherever the
// eye's aim meets a wall (engine/detail's raycast), sweeps it from the right wall over the end
// wall to the left, and lets go; the beam lights the corridor and the gun through render/lights
// (`beam_lights`), and leaves a trail of scorches pressed onto the walls by engine/detail's decal
// pool (`beam_burns`, `place_decal`) that glow and cool to soot.
//
// Named views, for each preset (`arc`, `bolt`, `braid`): the beam held mid-sweep; the walls
// after it is let go. Then the three side by side mid-sweep, the channels of four strikes in a
// row drawn flat (the CPU's walk, deterministic), and, with `--frames=N`, N frames of each
// preset's sweep at `--fps` written beside the report (previews/gpu-beam/frame-<look>-NNN.png)
// for an animation. `--set=knob=value,...` turns knobs on every preset.
// Measured: the corridor's light with the beam against without; the burns left; the segments a
// channel has; the CPU's channel and staging; the pass's cost against the frame without it (the
// software adapter: only ratios mean anything). Fails on a GPU error, a pipeline made after the
// load, a beam that lights nothing, or a sweep that leaves no burn.
//
// ge:summary Lab: lightning beams held in a corridor, lighting it and leaving burns, recorded
// ge:keywords lab gpu beam lightning arc bolt braid thunderbolt weapon-effect scorch burn ember
// ge:keywords corridor first-person muzzle animation frames knobs strike channel
// ge:skill render labs
// ge:test tests/render_beam_test.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "detail/decals.hpp"
#include "detail/mesh.hpp"
#include "detail/receivers.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/water_scene.hpp"
#include "lab/png.hpp"
#include "lab/raster.hpp"
#include "render/beam.hpp"
#include "render/beams.hpp"
#include "render/bloom.hpp"
#include "render/frame.hpp"
#include "render/hiz.hpp"
#include "render/instances.hpp"
#include "render/lights.hpp"
#include "render/mesh.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/surface.hpp"
#include "render/terrain.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace gd = ge::detail;
namespace libm = ge::core::libm;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::paste;
using ge::lab::water_scene::placeholders;
using gl::Det;
constexpr double kPi = 3.14159265358979323846;

// The corridor's origin in the world (the ground's middle); the corridor runs along +z from it.
constexpr double kCx = 128, kCz = 128;
// The eye, and the shot's clock: fired at kFire, let go at kRelease, the sweep between.
constexpr double kEyeY = 1.6;
constexpr double kFire = 0.35, kRelease = 2.95, kEnd = 3.6;
constexpr int kAtlasVariants = 4, kAtlasCell = 64;
constexpr double kBloom = 0.05;  // the bloom's share in the post pass

enum Mat : uint8_t { kWall, kFloor, kCeiling, kWood, kTorch, kGunBody, kGunCoil, kGunTip };

std::vector<gr::Surface> surfaces() {
  const float wall[3] = {0.30f, 0.25f, 0.19f}, floor[3] = {0.24f, 0.21f, 0.16f};
  const float ceiling[3] = {0.27f, 0.26f, 0.15f}, wood[3] = {0.30f, 0.17f, 0.08f};
  const float flame[3] = {1.0f, 0.55f, 0.18f}, body[3] = {0.32f, 0.32f, 0.34f};
  const float copper[3] = {0.95f, 0.64f, 0.54f}, tip[3] = {0.6f, 0.75f, 1.0f};
  return {gr::stone_surface(wall),         gr::stone_surface(floor),
          gr::stone_surface(ceiling),      gr::wood_surface(wood),
          gr::lamp_surface(flame, 6e3f),   gr::metal_surface(body, 0.35f),
          gr::metal_surface(copper, 0.3f), gr::lamp_surface(tip, 3e3f)};
}

// A box of the corridor: its centre and half sizes (corridor metres) and material.
struct Block {
  double c[3];
  double h[3];
  Mat mat;
};

std::vector<Block> corridor() {
  return {
      {{0, -0.1, 7}, {3, 0.1, 10}, kFloor},         // the floor
      {{0, 3.75, 7}, {3, 0.15, 10}, kCeiling},      // the ceiling
      {{-2.65, 1.8, 7}, {0.15, 1.8, 10}, kWall},    // the left wall
      {{2.65, 1.8, 7}, {0.15, 1.8, 10}, kWall},     // the right wall
      {{0, 1.8, 15.15}, {2.5, 1.8, 0.15}, kWall},   // the end wall
      {{0, 1.8, -2.15}, {2.5, 1.8, 0.15}, kWall},   // the wall behind the eye
      {{-2.2, 1.8, 5.5}, {0.3, 1.8, 0.35}, kWall},  // pillars
      {{2.2, 1.8, 5.5}, {0.3, 1.8, 0.35}, kWall},
      {{-2.2, 1.8, 11}, {0.3, 1.8, 0.35}, kWall},
      {{2.2, 1.8, 11}, {0.3, 1.8, 0.35}, kWall},
      {{1.55, 0.4, 8.3}, {0.4, 0.4, 0.4}, kWood},  // crates
      {{1.45, 1.05, 8.4}, {0.27, 0.25, 0.27}, kWood},
      {{-1.7, 0.3, 13.8}, {0.3, 0.3, 0.45}, kWood},
  };
}

// The torch: on the left wall by the end, and its flame.
constexpr double kTorchAt[3] = {-2.25, 2.3, 13.6};

// Meshes: 0 a unit box on its base, 1 a unit sphere, 2 the gun along +y (its muzzle at y =
// kMuzzle).
constexpr float kMuzzle = 0.52f;

std::vector<gr::MeshSource> meshes() {
  std::vector<gr::MeshSource> m(3);
  gr::MeshData box, ball, gun;
  const float base[3] = {0, 0.5f, 0}, half[3] = {0.5f, 0.5f, 0.5f};
  gr::add_box(box, base, half, 0);
  const float zero[3] = {0, 0, 0}, one[3] = {1, 1, 1};
  gr::add_ellipsoid(ball, zero, one, 16, 32, 0);
  // THE GUN, along +y with its grip toward +z (down once held): a body, a grip, a barrel wound
  // with copper coils and a glowing tip.
  const float body_c[3] = {0, 0.12f, 0}, body_h[3] = {0.05f, 0.15f, 0.06f};
  gr::add_box(gun, body_c, body_h, 0);
  const float grip_c[3] = {0, 0.04f, 0.11f}, grip_h[3] = {0.032f, 0.045f, 0.07f};
  gr::add_box(gun, grip_c, grip_h, 0);
  gr::add_cylinder(gun, 0.26f, 0.5f, 0.022f, 0.018f, 16, 0);
  for (int k = 0; k < 4; ++k) {
    const float y0 = 0.29f + 0.05f * static_cast<float>(k);
    gr::add_cylinder(gun, y0, y0 + 0.025f, 0.034f, 0.034f, 16, 1);
  }
  const float tip_c[3] = {0, kMuzzle - 0.02f, 0}, tip_r[3] = {0.02f, 0.025f, 0.02f};
  gr::add_ellipsoid(gun, tip_c, tip_r, 8, 16, 2);
  m[0].lods = {{box, 0}};
  m[1].lods = {{ball, 0}};
  m[2].lods = {{gun, 0}};
  return m;
}

gr::Instance block_instance(const Block& b) {
  gr::Instance in;
  in.position[0] = kCx + b.c[0];
  in.position[1] = b.c[1] - b.h[1];
  in.position[2] = kCz + b.c[2];
  for (int k = 0; k < 3; ++k) in.scale[k] = static_cast<float>(2 * b.h[k]);
  in.mesh = 0;
  in.material = b.mat;
  return in;
}

// THE RECEIVERS: every block's six faces, out of the block, in world metres.
gd::TriMesh receiver_mesh(const std::vector<Block>& blocks) {
  gd::TriMesh m;
  for (const Block& b : blocks) {
    const double c[3] = {kCx + b.c[0], b.c[1], kCz + b.c[2]};
    for (int axis = 0; axis < 3; ++axis) {
      for (int side = -1; side <= 1; side += 2) {
        const int ua = (axis + 1) % 3, va = (axis + 2) % 3;
        auto at = [&](double su, double sv) {
          double p[3];
          p[axis] = c[axis] + side * b.h[axis];
          p[ua] = c[ua] + su * b.h[ua];
          p[va] = c[va] + sv * b.h[va];
          return gd::Vec3{p[0], p[1], p[2]};
        };
        const auto base = static_cast<uint32_t>(m.pos.size());
        m.pos.push_back(at(-1, -1));
        m.pos.push_back(at(1, -1));
        m.pos.push_back(at(1, 1));
        m.pos.push_back(at(-1, 1));
        // u x v is +axis: wound so the front faces out of the block on this side.
        const uint32_t q[6] = {0, 1, 2, 0, 2, 3}, r[6] = {0, 2, 1, 0, 3, 2};
        for (int k = 0; k < 6; ++k) m.idx.push_back(base + (side > 0 ? q[k] : r[k]));
        m.tag.push_back(0);
        m.tag.push_back(0);
      }
    }
  }
  return m;
}

// A rotation (x, y, z, w) taking the gun's +y to `fwd` with its +z toward the ground.
void gun_rotation(const double fwd[3], float q[4]) {
  const double f[3] = {fwd[0], fwd[1], fwd[2]};
  // Down, square to the barrel.
  double d[3] = {0, -1, 0};
  const double fd = d[0] * f[0] + d[1] * f[1] + d[2] * f[2];
  for (int k = 0; k < 3; ++k) d[k] -= fd * f[k];
  const double dn = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  for (double& v : d) v /= dn;
  // x = y cross z.
  const double x[3] = {f[1] * d[2] - f[2] * d[1], f[2] * d[0] - f[0] * d[2],
                       f[0] * d[1] - f[1] * d[0]};
  // The matrix's columns are x, f, d; its quaternion.
  const double m00 = x[0], m01 = f[0], m02 = d[0];
  const double m10 = x[1], m11 = f[1], m12 = d[1];
  const double m20 = x[2], m21 = f[2], m22 = d[2];
  const double tr = m00 + m11 + m22;
  double w, qx, qy, qz;
  if (tr > 0) {
    const double s = std::sqrt(tr + 1) * 2;
    w = 0.25 * s;
    qx = (m21 - m12) / s;
    qy = (m02 - m20) / s;
    qz = (m10 - m01) / s;
  } else if (m00 > m11 && m00 > m22) {
    const double s = std::sqrt(1 + m00 - m11 - m22) * 2;
    w = (m21 - m12) / s;
    qx = 0.25 * s;
    qy = (m01 + m10) / s;
    qz = (m02 + m20) / s;
  } else if (m11 > m22) {
    const double s = std::sqrt(1 + m11 - m00 - m22) * 2;
    w = (m02 - m20) / s;
    qx = (m01 + m10) / s;
    qy = 0.25 * s;
    qz = (m12 + m21) / s;
  } else {
    const double s = std::sqrt(1 + m22 - m00 - m11) * 2;
    w = (m10 - m01) / s;
    qx = (m02 + m20) / s;
    qy = (m12 + m21) / s;
    qz = 0.25 * s;
  }
  q[0] = static_cast<float>(qx);
  q[1] = static_cast<float>(qy);
  q[2] = static_cast<float>(qz);
  q[3] = static_cast<float>(w);
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
  gr::Beams beams;
  gr::Bloom bloom;
  gr::Post post;
  gr::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo, atlas;
  ge::gpu::Target out;
  uint32_t w = 0, h = 0;
};

bool make_scene(ge::gpu::Device& d, Scene& s, uint32_t w, uint32_t h) {
  s.w = w;
  s.h = h;
  s.field = ge::core::create_heightfield(0, 0, 1, 257, 257);
  s.albedo.assign(size_t{257} * 257 * 4, 90);
  for (float& v : s.field.data) v = -0.4f;  // under the corridor's floor
  s.targets = gr::create_targets(d, "gpu-beam", w, h);
  s.uniforms = gr::make_buffer(d, "gpu-beam.uniforms", sizeof(gr::FrameUniforms),
                               wgpu::BufferUsage::Uniform);
  s.layout = gr::frame_layout(d);
  gr::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  gr::TerrainSource src;
  src.height = &s.field;
  src.albedo = &s.albedo;
  src.base_spacing = 1.0;
  src.levels = 6;
  gr::create_terrain(d, s.pipelines, s.terrain, src, s.layout);
  gr::create_hiz_pipelines(d, s.pipelines, s.hiz);
  gr::create_instance_pipelines(d, s.pipelines, s.ip, s.layout);
  s.atlas = gd::bake_atlas(kAtlasCell, kAtlasVariants);
  gr::BeamSettings bs;
  bs.atlas = &s.atlas;
  bs.atlas_width = kAtlasCell * kAtlasVariants;
  bs.atlas_height = kAtlasCell * gd::kDecalKinds;
  gr::create_beams(d, s.pipelines, s.beams, s.layout, bs);
  // The lens's glow round the beam: the bloom, as a game's frame mixes it.
  gr::create_bloom_pipelines(d, s.pipelines, s.bloom, {.levels = 6, .strength = kBloom});
  gr::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm,
                  {.bloom = kBloom, .look = {}});
  if (!s.pipelines.finish(d)) return false;
  ge::gpu::end_load(d);
  gr::create_lights(d, s.lights);
  s.frame = gr::frame_group(d, s.layout, s.uniforms, s.sky, s.lights.buffer);
  gr::create_hiz_targets(d, s.hiz, s.targets);
  gr::bind_beam_targets(d, s.beams, s.targets);
  const wgpu::TextureView inputs[2] = {s.targets.hdr_view, s.targets.hdr_view};
  gr::create_bloom_targets(d, s.bloom, w, h, inputs);
  s.post_group = gr::post_group(d, s.post, s.targets.hdr_view, s.bloom.view);
  const std::vector<gr::Surface> surf = surfaces();
  gr::create_family(d, s.ip, s.props, "gpu-beam.props", meshes(), std::span(surf), 64);
  gr::bind_family_hiz(d, s.ip, s.props, s.hiz);
  s.light = gr::instance_light_group(d, s.ip, s.terrain);
  s.rec.init(d, "gpu-beam.timer", 18);
  s.out = ge::gpu::create_target(d, "gpu-beam.final", w, h);
  return true;
}

gr::FrameView eye_view(double t, double lamp_lux) {
  gr::FrameView v;
  v.eye[0] = kCx;
  v.eye[1] = kEyeY;
  v.eye[2] = kCz;
  v.heading = 0;
  v.pitch = -0.04;
  v.fov_y = 1.05;
  v.near_m = 0.05;
  v.sun_elevation = -0.6;  // a moonless night: the corridor is the beam's and the torch's
  v.altitude_m = 300;
  v.time_s = t;
  v.lamp_lux = lamp_lux;
  return v;
}

struct Timed {
  gl::Picture pic;
  double ms = 0;
};

Timed draw(ge::gpu::Device& d, Scene& s, const gr::FrameView& v,
           const std::vector<gr::Instance>& props, const std::vector<gr::LocalLight>& lamps,
           std::span<const gr::BeamDraw> beams, std::span<const gr::BurnVertex> burns,
           float hot_cd) {
  s.rec.begin_frame(d);
  gr::set_instances(d, s.props, 0, props);
  gr::fill_uniforms(s.staged, v, s.w, s.h);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  gr::prepare_sky(s.sky, v);
  gr::prepare_terrain(d, s.terrain, v);
  gr::prepare_family(d, s.props, v, s.h, false);
  gr::prepare_lights(d, s.lights, lamps, v, s.w, s.h);
  gr::prepare_beams(d, s.beams, beams, burns, v, hot_cd);
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
  gr::encode_beams(s.beams, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_bloom(s.bloom, s.pipelines, s.rec, e, 0);
  gr::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, s.out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  const ge::gpu::Image img = ge::gpu::read_now(d, s.out);
  s.rec.wait(d);
  Timed out{gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba}, 0};
  if (s.rec.timed) {
    for (int i = 0; i < s.rec.stats.count; ++i) {
      const gr::PassStat& p = s.rec.stats.passes[static_cast<size_t>(i)];
      if (p.name == "beams" && p.gpu_ms == p.gpu_ms) out.ms = p.gpu_ms;
    }
  }
  return out;
}

// THE SWEEP: where the eye aims at time `t` (heading and pitch, rad).
void aim_at(double t, double& heading, double& pitch) {
  const double u = std::clamp((t - kFire) / (kRelease - kFire), 0.0, 1.0);
  heading = 0.55 - 1.1 * u;
  pitch = -0.1 - 0.06 * libm::sin(kPi * u);
}

// One look's run through the corridor: the shot, its burns, the lit frames at `times`.
struct Run {
  gr::BeamLook look;
  gr::BeamShot shot;
  gr::BurnTrail trail;
  gd::DecalPool pool = gd::make_pool(160, 96);
  gr::BeamShape shape;
  std::vector<gr::BurnVertex> burns;
  int burns_placed = 0;
  int segments_most = 0;
  // THE EYE: the log of the lamps' light it has adapted to and when, adapting toward what the
  // lamps put on the corridor's walls each frame (quickly to a brighter room, slowly to a darker).
  double eye_log = 0;
  double eye_t = -1;
  double lux_most = 0, lux_least = 1e30;  // the room's light over the run, lx
};

// The lamps' light on the corridor, lx: the geometric mean over a few points of its walls, its
// floor and its end (render/lights.hpp's lamp_lux_at), at least a dim room's.
double room_lux(std::span<const gr::LocalLight> lamps, double t) {
  const double pts[5][3] = {{kCx - 2.5, 1.5, kCz + 7},
                            {kCx + 2.5, 1.5, kCz + 7},
                            {kCx, 0, kCz + 5},
                            {kCx, 1.5, kCz + 15},
                            {kCx - 2.5, 1.5, kCz + 12}};
  double logsum = 0;
  for (const auto& p : pts) logsum += libm::log(std::max(0.05, gr::lamp_lux_at(lamps, p, t)));
  return std::max(0.5, libm::exp(logsum / 5));
}

// The frame at `t`: the shot aimed, its burns pressed, the gun posed, the lights and the drawing.
gl::Picture frame_at(ge::gpu::Device& d, Scene& s, Run& run, const gd::Receivers& rec,
                     gd::ReceiverScratch& scratch, double t, bool beam_on,
                     double* pass_ms = nullptr) {
  double heading = 0, pitch = 0;
  aim_at(t, heading, pitch);
  const double aim[3] = {libm::sin(heading) * libm::cos(pitch), libm::sin(pitch),
                         libm::cos(heading) * libm::cos(pitch)};
  const gd::Vec3 eye{kCx, kEyeY, kCz};
  const gd::Hit hit = gd::raycast(rec, eye, gd::Vec3{aim[0], aim[1], aim[2]}, 60, scratch);
  // The gun: lower right of the eye, its barrel toward what the eye aims at, shaken a little by
  // each strike while it fires.
  const float level = beam_on ? gr::beam_level(run.look, run.shot, t) : 0.0f;
  const int64_t strike = gr::beam_strike(run.look, run.shot, t);
  const double shake = 0.0025 * std::min(level, 1.5f);
  const double jx = shake * (static_cast<double>((strike * 7919) % 13) / 6.0 - 1);
  const double jy = shake * (static_cast<double>((strike * 104729) % 11) / 5.0 - 1);
  const double grip[3] = {kCx + 0.17 + jx, kEyeY - 0.2 + jy, kCz + 0.2};
  const gd::Vec3 at =
      hit.hit ? hit.at : gd::Vec3{eye.x + aim[0] * 20, eye.y + aim[1] * 20, eye.z + aim[2] * 20};
  double fwd[3] = {at.x - grip[0], at.y - grip[1], at.z - grip[2]};
  const double fl = std::sqrt(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
  for (double& v : fwd) v /= fl;
  gr::Instance gun;
  for (int k = 0; k < 3; ++k) gun.position[k] = grip[k];
  gun_rotation(fwd, gun.rotation);
  gun.mesh = 2;
  gun.material = kGunBody;
  run.shot.from[0] = grip[0] + fwd[0] * kMuzzle;
  run.shot.from[1] = grip[1] + fwd[1] * kMuzzle;
  run.shot.from[2] = grip[2] + fwd[2] * kMuzzle;
  run.shot.to[0] = at.x;
  run.shot.to[1] = at.y;
  run.shot.to[2] = at.z;
  if (hit.hit) {
    gd::Vec3 n = gd::normal_of(*rec.mesh, hit.tri);
    if (gd::dot(n, gd::Vec3{aim[0], aim[1], aim[2]}) > 0) n = gd::mul(n, -1);
    run.shot.normal[0] = n.x;
    run.shot.normal[1] = n.y;
    run.shot.normal[2] = n.z;
  }
  // The burns: a new one when one is due, and every live one's mesh at its age.
  gr::BeamBurn burn;
  for (int k = 0;
       k < 8 && beam_on && hit.hit && gr::beam_burns(run.look, run.shot, t, run.trail, burn); ++k) {
    gd::DecalAt da;
    da.at = gd::Vec3{burn.at[0], burn.at[1], burn.at[2]};
    da.normal = gd::Vec3{burn.normal[0], burn.normal[1], burn.normal[2]};
    da.size_m = burn.size_m;
    da.spin_rad = burn.spin_rad;
    da.now_s = t;
    da.variant = burn.variant % kAtlasVariants;
    da.kind = gd::DecalKind::scorch;
    if (gd::place_decal(run.pool, rec, da) >= 0) ++run.burns_placed;
  }
  run.burns.clear();
  for (uint32_t i = 0; i < run.pool.capacity; ++i) {
    const gd::Decal& dc = run.pool.decals[i];
    if (!dc.live) continue;
    const double age = t - dc.born_s;
    const gd::DecalLook look = gd::look_at(dc.kind, age);
    const float ember = gr::ember_level(run.look, age);
    for (const gd::DecalVertex& v : gd::decal_verts(run.pool, i)) {
      gr::BurnVertex b;
      b.at[0] = v.pos.x;
      b.at[1] = v.pos.y;
      b.at[2] = v.pos.z;
      b.u = static_cast<float>((dc.variant + v.u) / kAtlasVariants);
      b.v = static_cast<float>(
          (static_cast<double>(static_cast<int>(gd::DecalKind::scorch)) + v.v) / gd::kDecalKinds);
      b.fade = static_cast<float>(v.fade);
      b.soot = static_cast<float>(0.9 * look.opacity);
      b.ember = ember;
      run.burns.push_back(b);
    }
  }
  // The scene: the corridor, the torch's flame, the gun; the torch and the beam's lights.
  std::vector<gr::Instance> props;
  for (const Block& b : corridor()) props.push_back(block_instance(b));
  gr::Instance flame;
  flame.position[0] = kCx + kTorchAt[0];
  flame.position[1] = kTorchAt[1];
  flame.position[2] = kCz + kTorchAt[2];
  flame.scale[0] = flame.scale[2] = 0.06f;
  flame.scale[1] = 0.13f;
  flame.mesh = 1;
  flame.material = kTorch;
  props.push_back(flame);
  props.push_back(gun);
  std::vector<gr::LocalLight> lamps;
  gr::LocalLight torch;
  torch.position[0] = kCx + kTorchAt[0] + 0.12;
  torch.position[1] = kTorchAt[1] + 0.1;
  torch.position[2] = kCz + kTorchAt[2];
  torch.colour[0] = 1.0f;
  torch.colour[1] = 0.55f;
  torch.colour[2] = 0.2f;
  torch.candela = 60;
  torch.range_m = 12;
  torch.radius_m = 0.13f;
  torch.flicker = gr::Flicker::flame;
  torch.seed = 3;
  lamps.push_back(torch);
  // A second torch on the right wall behind the first pillar.
  gr::LocalLight second = torch;
  second.position[0] = kCx + 2.1;
  second.position[2] = kCz + 3.6;
  second.seed = 11;
  lamps.push_back(second);
  gr::Instance flame2 = flame;
  flame2.position[0] = kCx + 2.2;
  flame2.position[2] = kCz + 3.6;
  props.push_back(flame2);
  gr::BeamDraw draw_one;
  int ndraw = 0;
  if (beam_on) {
    gr::LocalLight beam_lamps[gr::kBeamLights];
    const int n = gr::beam_lights(run.look, run.shot, t, beam_lamps);
    lamps.insert(lamps.end(), beam_lamps, beam_lamps + n);
    gr::build_beam(run.look, run.shot, t, run.shape);
    run.segments_most = std::max(run.segments_most, run.shape.count);
    draw_one = {&run.shape, &run.look, run.shot.seed};
    ndraw = run.shape.count > 0 ? 1 : 0;
  }
  // The light the corridor's walls throw back, which the renderer does not bounce: a soft lamp
  // at the eye, a little of the beam's light and a little of the torches'.
  gr::LocalLight fill;
  fill.position[0] = kCx;
  fill.position[1] = kEyeY + 0.1;
  fill.position[2] = kCz - 0.2;
  fill.colour[0] = 0.75f;
  fill.colour[1] = 0.78f;
  fill.colour[2] = 0.9f;
  fill.candela =
      1.5f + 0.006f * run.look.light_cd * (beam_on ? gr::beam_level(run.look, run.shot, t) : 0.0f);
  fill.range_m = 8;
  fill.radius_m = 0.35f;
  lamps.push_back(fill);
  // The eye adapts.
  const double lux = room_lux(lamps, t);
  run.lux_most = std::max(run.lux_most, lux);
  run.lux_least = std::min(run.lux_least, lux);
  const double target = libm::log(lux);
  if (run.eye_t < 0) {
    run.eye_log = target;
  } else {
    const double dt = std::max(0.0, t - run.eye_t);
    const double tau = target > run.eye_log ? 0.12 : 0.9;
    run.eye_log += (target - run.eye_log) * (1 - libm::exp(-dt / tau));
  }
  run.eye_t = t;
  const Timed out =
      draw(d, s, eye_view(t, libm::exp(run.eye_log)), props, lamps,
           std::span(&draw_one, static_cast<size_t>(ndraw)), run.burns, run.look.ember_cd);
  if (pass_ms) *pass_ms = out.ms;
  return out.pic;
}

double mean_luma(const gl::Picture& p) {
  double sum = 0;
  for (size_t i = 0; i + 3 < p.rgba.size(); i += 4) {
    sum += 0.2126 * p.rgba[i] + 0.7152 * p.rgba[i + 1] + 0.0722 * p.rgba[i + 2];
  }
  return sum / (static_cast<double>(p.rgba.size()) / 4.0);
}

gl::Picture sheet_of(const std::vector<gl::Picture>& pics, const std::vector<std::string>& labels,
                     int cols) {
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

// THE CHANNELS DRAWN FLAT (the CPU's walk, deterministic): each preset's beam 10 m long seen
// from its side, four strikes in a row, the main channel dark and its forks lighter.
gl::Picture channels(const gr::BeamLook* looks) {
  constexpr int kW = 900, kRow = 70, kStrikes = 4;
  gl::Picture p =
      gl::create_picture(kW, 24 + gr::kBeamLooks * kStrikes * kRow / 2 + 40, {250, 250, 248});
  auto shape = std::make_unique<gr::BeamShape>();
  int y0 = 8;
  for (int l = 0; l < gr::kBeamLooks; ++l) {
    gl::draw_text(p, 8, y0, std::string(gr::beam_look_name(l)), {24, 26, 32});
    y0 += 12;
    gr::BeamShot shot;
    shot.to[0] = 0;
    shot.to[1] = 0;
    shot.to[2] = 10;
    shot.seed = 11;
    for (int k = 0; k < kStrikes; ++k) {
      const double t = (k + 0.5) / std::max(1.0f, looks[l].strike_hz) + 0.2;
      gr::build_beam(looks[l], shot, t, *shape);
      const double cy = y0 + kRow * 0.25;
      for (int i = 0; i < shape->count; ++i) {
        const gr::BeamSegment& sg = shape->seg[i];
        const uint8_t g = static_cast<uint8_t>(200 - 180 * std::clamp(sg.glow, 0.0f, 1.0f));
        gl::draw_line(p, 20 + sg.a[2] * 85, cy - sg.a[1] * 85 - sg.a[0] * 20, 20 + sg.b[2] * 85,
                      cy - sg.b[1] * 85 - sg.b[0] * 20, {g, g, static_cast<uint8_t>(g / 2 + 120)});
      }
      y0 += kRow / 2;
    }
    y0 += 8;
  }
  return p;
}

bool apply_knobs(gl::Lab& lab, gr::BeamLook& look) {
  for (std::string_view rest = lab.arg("set"); !rest.empty();) {
    const size_t comma = rest.find(',');
    const std::string_view one = rest.substr(0, comma);
    rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
    const size_t eq = one.find('=');
    if (eq == std::string_view::npos ||
        !gr::set_beam_knob(look, one.substr(0, eq),
                           std::strtof(std::string(one.substr(eq + 1)).c_str(), nullptr))) {
      lab.fail("--set: \"" + std::string(one) + "\" is not knob=value for a known knob",
               "next: name a knob from render/beam.hpp's beam_knobs, e.g. --set=wander=0.5.");
      return false;
    }
  }
  return true;
}

void run(gl::Lab& lab) {
  const auto w = static_cast<uint32_t>(lab.arg_num("width"));
  const auto h = static_cast<uint32_t>(lab.arg_num("height"));
  const int frames = std::clamp(static_cast<int>(lab.arg_num("frames")), 0, 600);
  const double fps = std::clamp(lab.arg_num("fps"), 2.0, 120.0);
  if (w < 64 || h < 36 || w > 4096 || h > 4096) {
    lab.fail("--width must be 64..4096 and --height 36..4096", "next: pass --width=640.");
    return;
  }
  gr::BeamLook looks[gr::kBeamLooks];
  for (int l = 0; l < gr::kBeamLooks; ++l) {
    gr::beam_look(gr::beam_look_name(l), looks[l]);
    if (!apply_knobs(lab, looks[l])) return;
  }
  lab.shot({"channels",
            "FOUR STRIKES OF EACH CHANNEL",
            "Each preset's channel 10 m long seen from its side (the CPU's walk, no GPU), at four "
            "strikes one after another: the channel is struck again, never morphed.",
            "arc: one thick line wandering a few centimetres; bolt: a jagged stepped line with "
            "short forks; braid: three strands wound round each other; each strike a new shape",
            {}},
           channels(looks));
  {
    auto shape = std::make_unique<gr::BeamShape>();
    gr::BeamShot shot;
    shot.to[2] = 12;
    for (int l = 0; l < gr::kBeamLooks; ++l) {
      gr::build_beam(looks[l], shot, 0.5, *shape);
      lab.metric("segments." + std::string(gr::beam_look_name(l)), shape->count, "count");
      const auto t0 = std::chrono::steady_clock::now();
      constexpr int kRounds = 2000;
      for (int k = 0; k < kRounds; ++k) gr::build_beam(looks[l], shot, 0.5 + k * 1e-3, *shape);
      lab.metric(
          "cpu.build-" + std::string(gr::beam_look_name(l)),
          std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() /
              kRounds,
          "us", Det::measured);
    }
  }

  auto device = ge::gpu::create_device({.label = "gpu-beam", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    placeholders(lab, {"arc", "arc-after", "bolt", "bolt-after", "braid", "braid-after", "three"},
                 w, h);
    return;
  }
  auto s = std::make_unique<Scene>();
  if (!make_scene(*device, *s, w, h)) {
    lab.fail("the beams' pipelines did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const gd::TriMesh mesh = receiver_mesh(corridor());
  const gd::Receivers rec = gd::bin_receivers(mesh, 1.0);
  gd::ReceiverScratch scratch;

  const double mid = 0.5 * (kFire + kRelease);
  std::vector<gl::Picture> held;
  std::vector<std::string> held_names;
  double dark_luma = 0;
  for (int l = 0; l < gr::kBeamLooks; ++l) {
    const std::string name(gr::beam_look_name(l));
    auto go = std::make_unique<Run>();
    go->look = looks[l];
    go->shot.born_s = kFire;
    go->shot.end_s = kRelease;
    go->shot.seed = 101 + static_cast<uint32_t>(l);
    // The sweep at the animation's rate (or a coarse one when no frames are asked for), so the
    // burns are pressed as a game at that rate would press them.
    const double rate = frames > 0 ? fps : 12.0;
    const int steps = frames > 0 ? frames : static_cast<int>(std::ceil(kEnd * rate));
    gl::Picture still, after;
    double best_mid = 1e9;
    double pass_ms = 0;
    for (int i = 0; i <= steps; ++i) {
      const double t = i / rate;
      double ms = 0;
      gl::Picture pic = frame_at(*device, *s, *go, rec, scratch, t, true, &ms);
      if (std::abs(t - mid) < best_mid) {
        best_mid = std::abs(t - mid);
        still = pic;
        pass_ms = ms;
      }
      if (i == 0 && l == 0) dark_luma = mean_luma(pic);
      if (frames > 0 && i < frames) {
        char file[64];
        std::snprintf(file, sizeof file, "frame-%s-%03d.png", name.c_str(), i);
        lab.attach(file, gl::encode_png(pic), Det::measured);
      }
      after = std::move(pic);
    }
    lab.metric("burns." + name, go->burns_placed, "count", Det::measured);
    lab.metric("segments-most." + name, go->segments_most, "count", Det::measured);
    lab.metric("pass." + name, pass_ms, "ms", Det::measured);
    lab.metric("luma-held." + name, mean_luma(still), "0..255", Det::measured);
    if (go->burns_placed == 0) {
      lab.fail("the " + name + " beam's sweep left no burn",
               "Check the raycast hits the corridor and beam_burns' spacing. Skill: render.");
    }
    lab.metric("room-lux-most." + name, go->lux_most, "lx", Det::measured);
    lab.metric("room-lux-least." + name, go->lux_least, "lx", Det::measured);
    if (go->lux_most < 4 * go->lux_least) {
      lab.fail("the " + name + " beam lights the corridor only " +
                   fixed(go->lux_most / go->lux_least, 2) + " times as bright as its torches",
               "Check beam_lights' candela and ranges. Skill: render.");
    }
    lab.shot({name,
              "THE " + name + " BEAM HELD",
              "The \"" + name + "\" preset fired from the gun at the lower right, " +
                  fixed(mid - kFire, 2) +
                  " s into a sweep from the right wall over the end wall to the left, at night: "
                  "the corridor lit by the beam's own light and one torch.",
              "a thick, nearly straight white-hot line from the muzzle to the wall in a violet-"
              "blue glow; the walls, floor and gun lit blue-white round it; a black scorch trail "
              "glowing orange at its fresh end where the beam lands",
              {{"look", name}}},
             still, Det::measured);
    lab.shot({name + "-after",
              "THE WALLS AFTER THE " + name + " BEAM",
              "The same corridor " + fixed(kEnd - kRelease, 2) +
                  " s after the beam is let go: only the torch lights it.",
              "an unbroken black scorch trail across the right wall, the end wall and the left "
              "wall, its last burns still glowing a dull red, the rest cold soot",
              {{"look", name}}},
             after, Det::measured);
    held.push_back(still);
    held_names.push_back(name);
  }
  lab.metric("luma-dark", dark_luma, "0..255", Det::measured);
  lab.shot({"three",
            "THE THREE PRESETS",
            "arc, bolt and braid held at the same moment of the same sweep.",
            "three beams told apart: arc thick and straight, bolt jagged and forking, braid "
            "a twisted rope",
            {}},
           sheet_of(held, held_names, 3), Det::measured);

  const uint32_t errors = device->errors.load();
  const uint32_t late = device->pipelines_after_load;
  lab.metric("gpu-errors", errors, "count");
  lab.metric("pipelines-after-load", late, "count");
  if (errors != 0) {
    lab.fail(std::to_string(errors) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
  if (late != 0) {
    lab.fail("a pipeline was made after the load",
             "Add it to create_beams' manifest. Skill: gpu, § Pipelines.");
  }
}

const gl::LabSpec kSpec{
    "gpu-beam",
    gl::LabKind::picture,
    "engine",
    "engine/render/beams.hpp",
    "render",
    "Does a held lightning beam read as a thick, nearly straight electric arc that lights the "
    "room round it and burns where it lands, in each of its three presets?",
    {
        {"width", "640", "each view's width, px"},
        {"height", "360", "each view's height, px"},
        {"frames", "0", "also N frames of each preset's sweep, written as frame-<look>-NNN.png"},
        {"fps", "30", "the animation's frames a second"},
        {"set", "", "knob=value,... turned on every preset (render/beam.hpp's beam_knobs)"},
    },
    "--width=128 --height=72",
    run,
};
GE_LAB(kSpec);

}  // namespace
