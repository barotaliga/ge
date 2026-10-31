// engine/render/shadows.hpp: a map's view along the sun takes its box to clip space (the centre
// to the middle, the box's side to its edge, toward the sun to depth 1); the cascades are redrawn
// only past their thresholds (the eye's drift across the sun, the sun's turn), the near one
// before the far one; and on a device, a tower standing on a flat field (a still family, drawn
// into the cascades) and a box at a figure's place (a moving family, drawn into the atlas) each
// darken the ground on the side away from the sun and brighten nothing.
//
// ge:summary Tests for the sun's shadows: the map's view, the cascades' redraws, casters shading
// ge:keywords test render shadows cascades atlas redraw threshold casters shadow-view
// ge:skill render testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "render/frame.hpp"
#include "render/hiz.hpp"
#include "render/instances.hpp"
#include "render/post.hpp"
#include "render/shadows.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 96, kH = 64;

void apply(const float m[16], const double p[3], double out[3]) {
  for (int row = 0; row < 3; ++row) {
    out[row] = m[12 + row];
    for (int c = 0; c < 3; ++c) out[row] += m[c * 4 + row] * p[c];
  }
}

r::FrameView field_view() {
  r::FrameView v;
  v.eye[0] = 256;
  v.eye[1] = 30;
  v.eye[2] = 120;
  v.heading = 0;  // along +z, toward the tower
  v.pitch = -0.6;
  v.sun_elevation = 0.7;
  v.sun_azimuth = 0.0;  // the sun stands over +z: shadows fall toward the camera
  v.altitude_m = 1000;
  return v;
}

struct Scene {
  ge::gpu::PipelineSet pipelines;
  r::FrameTargets targets;
  wgpu::Buffer uniforms;
  r::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_group, light;
  r::Sky sky;
  r::Terrain terrain;
  r::Hiz hiz;
  r::Post post;
  r::InstancePipelines ip;
  r::InstanceFamily tower, figure;
  r::SunShadows shadows;
  wgpu::BindGroup none;
  r::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
};

r::Instance box_at(double x, double y, double z, float sx, float sy, float sz) {
  r::Instance in;
  in.position[0] = x;
  in.position[1] = y;
  in.position[2] = z;
  in.scale[0] = sx;
  in.scale[1] = sy;
  in.scale[2] = sz;
  return in;
}

void make_scene(ge::gpu::Device& d, Scene& s) {
  s.field = ge::core::create_heightfield(0, 0, 4, 129, 129);  // flat, 512 m
  s.albedo.assign(static_cast<size_t>(s.field.cols) * s.field.rows * 4, 230);
  s.targets = r::create_targets(d, "test", kW, kH);
  s.uniforms =
      r::make_buffer(d, "test.uniforms", sizeof(r::FrameUniforms), wgpu::BufferUsage::Uniform);
  s.layout = r::frame_layout(d);
  r::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  r::TerrainSource src;
  src.height = &s.field;
  src.albedo = &s.albedo;
  src.levels = 6;
  r::create_terrain(d, s.pipelines, s.terrain, src, s.layout);
  r::create_hiz_pipelines(d, s.pipelines, s.hiz);
  r::create_instance_pipelines(d, s.pipelines, s.ip, s.layout);
  r::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  r::create_hiz_targets(d, s.hiz, s.targets);
  s.post_group = r::post_group(d, s.post, s.targets.hdr_view);
  r::ShadowSettings set;
  set.size = 512;
  r::create_sun_shadows(d, s.shadows, set, s.layout, s.sky);
  s.none = r::no_sun_shadows(d);

  std::vector<r::MeshSource> meshes(1);
  r::MeshData box;
  const float zero[3] = {0, 0, 0}, half[3] = {0.5f, 0.5f, 0.5f};
  r::add_box(box, zero, half, 0);
  meshes[0].lods = {{box, 0}};
  const std::vector<float> materials = {0.3f, 0.3f, 0.3f};
  r::create_family(d, s.ip, s.tower, "test.tower", meshes, materials, 4);
  r::create_family(d, s.ip, s.figure, "test.figure", meshes, materials, 4);
  r::create_family_casters(d, s.ip, s.tower, r::kCascades);
  r::create_family_casters(d, s.ip, s.figure, r::kMaxShadowFigures);
  r::bind_family_hiz(d, s.ip, s.tower, s.hiz);
  r::bind_family_hiz(d, s.ip, s.figure, s.hiz);
  s.light = r::instance_light_group(d, s.ip, s.terrain);
  s.rec.init(d, "test.timer", 20);
}

// A frame with the sun's shadows bound (or none), the tower and the figure as they are written.
ge::gpu::Image draw(ge::gpu::Device& d, Scene& s, const r::FrameView& v, const ge::gpu::Target& out,
                    bool shadows, std::span<const std::array<double, 3>> figures) {
  r::bind_terrain_shadows(s.terrain, shadows ? s.shadows.group : s.none);
  r::bind_instance_shadows(s.ip, shadows ? s.shadows.group : s.none);
  s.rec.begin_frame(d);
  r::fill_uniforms(s.staged, v, kW, kH, nullptr);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  r::prepare_terrain(d, s.terrain, v);
  r::InstanceFamily* families[] = {&s.tower, &s.figure};
  for (r::InstanceFamily* f : families) r::prepare_family(d, *f, v, kH, false);
  r::InstanceFamily* still[] = {&s.tower};
  r::InstanceFamily* moving[] = {&s.figure};
  r::prepare_sun_shadows(d, s.shadows, v, figures, still, moving);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  r::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  r::encode_instances_cull(families, s.ip, s.pipelines, s.rec, e, s.frame, false);
  r::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  r::encode_sun_shadows(s.shadows, still, moving, s.ip, s.pipelines, s.rec, e);
  r::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_instances_depth(families, s.ip, s.pipelines, s.rec, e, s.targets, s.frame, s.light,
                            false);
  r::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_instances(families, s.ip, s.pipelines, s.rec, e, s.targets, s.frame, s.light);
  r::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return ge::gpu::read_now(d, out);
}

// Pixels darker with the shadows than without (by more than a few levels), and brighter.
struct Change {
  int darker = 0, brighter = 0;
  double darker_row = 0;  // the darker pixels' mean row
};
Change compare(const ge::gpu::Image& with, const ge::gpu::Image& without) {
  Change c;
  for (uint32_t y = 0; y < kH; ++y) {
    for (uint32_t x = 0; x < kW; ++x) {
      const size_t i = (size_t{y} * kW + x) * 4;
      int dw = 0;
      for (int k = 0; k < 3; ++k) dw += int{with.rgba[i + k]} - int{without.rgba[i + k]};
      if (dw < -12) {
        ++c.darker;
        c.darker_row += y;
      }
      if (dw > 3) ++c.brighter;
    }
  }
  if (c.darker > 0) c.darker_row /= c.darker;
  return c;
}

}  // namespace

TEST(shadow_view_maps_its_box) {
  const double l = std::sqrt(0.36 + 0.49 + 0.16);
  const double sun[3] = {0.6 / l, 0.7 / l, 0.4 / l};
  double basis[3][3];
  float m[16];
  r::shadow_view(sun, 50, 1000, basis, m);
  for (int a = 0; a < 3; ++a) {
    for (int b = 0; b < 3; ++b) {
      double d = 0;
      for (int k = 0; k < 3; ++k) d += basis[a][k] * basis[b][k];
      CHECK(std::fabs(d - (a == b ? 1.0 : 0.0)) < 1e-9);
    }
  }
  CHECK(basis[1][1] > 0);  // up is as near the world's up as the sun allows
  double out[3];
  const double centre[3] = {0, 0, 0};
  apply(m, centre, out);
  CHECK(std::fabs(out[0]) < 1e-6 && std::fabs(out[1]) < 1e-6 && std::fabs(out[2] - 0.5) < 1e-6);
  const double side[3] = {50 * basis[0][0], 50 * basis[0][1], 50 * basis[0][2]};
  apply(m, side, out);
  CHECK(std::fabs(out[0] - 1) < 1e-5);
  const double top[3] = {50 * basis[1][0], 50 * basis[1][1], 50 * basis[1][2]};
  apply(m, top, out);
  CHECK(std::fabs(out[1] - 1) < 1e-5);
  const double toward[3] = {1000 * sun[0], 1000 * sun[1], 1000 * sun[2]};
  apply(m, toward, out);
  CHECK(std::fabs(out[2] - 1) < 1e-5);
}

// A game's own orthographic map read as cascade 0: a point's clip is its place on the map's face
// over its half size, and its depth along the map's axis as the map stores it.
TEST(an_ortho_map_reads_as_cascade_zero) {
  r::OrthoShadow m;
  const double c[3] = {10, 5, -3};
  for (int k = 0; k < 3; ++k) m.o[k] = c[k];
  // Looking straight down from over the map: x across, y up the page (+z), z down.
  m.x[0] = 1, m.x[1] = 0, m.x[2] = 0;
  m.y[0] = 0, m.y[1] = 0, m.y[2] = 1;
  m.z[0] = 0, m.z[1] = -1, m.z[2] = 0;
  m.half = 20;
  m.depth = 40;
  m.size = 2048;
  const double eye[3] = {12, 6, -1};
  const r::ShadowParams p = r::ortho_shadow_params(m, eye);
  CHECK(p.offset[0][3] == 1.0f && p.offset[1][3] == 0.0f && p.info[3] == 0.0f);
  CHECK(std::fabs(p.info[2] - float(m.bias)) < 1e-9);
  CHECK(std::fabs(p.texel[0] - 40.0f / 2048) < 1e-7);
  // A camera-relative point moved to the map's centre, then through the matrix (what
  // sun_shadow does): the map's centre, a point 10 m across and one 20 m under it.
  const auto clip = [&](const double world[3], double out[3]) {
    double q[3];
    for (int k = 0; k < 3; ++k) q[k] = world[k] - eye[k] + p.offset[0][k];
    apply(p.cascade[0], q, out);
  };
  double out[3];
  clip(c, out);
  CHECK(std::fabs(out[0]) < 1e-6 && std::fabs(out[1]) < 1e-6 && std::fabs(out[2] - 0.5) < 1e-6);
  const double across[3] = {c[0] + 10, c[1], c[2]};
  clip(across, out);
  CHECK(std::fabs(out[0] - 0.5) < 1e-5);
  const double up_page[3] = {c[0], c[1], c[2] + 10};
  clip(up_page, out);
  CHECK(std::fabs(out[1] - 0.5) < 1e-5);
  const double under[3] = {c[0], c[1] - 20, c[2]};
  clip(under, out);
  CHECK(std::fabs(out[2] - 0.75) < 1e-5);  // (z . q + depth) / (2 depth)
}

TEST(cascades_redraw_past_their_thresholds) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  r::Sky sky;
  ge::gpu::PipelineSet pipelines;
  const wgpu::Buffer uniforms =
      r::make_buffer(d, "test.uniforms", sizeof(r::FrameUniforms), wgpu::BufferUsage::Uniform);
  const wgpu::BindGroupLayout layout = r::frame_layout(d);
  r::create_sky(d, pipelines, sky, uniforms, layout);
  r::SunShadows s;
  r::ShadowSettings set;
  set.size = 64;
  r::create_sun_shadows(d, s, set, layout, sky);
  r::FrameView v = field_view();
  auto frame = [&] {
    r::prepare_sun_shadows(d, s, v, {}, {}, {});
    for (bool& due : s.due) due = false;  // what encode does
  };
  frame();
  CHECK(s.redraws[0] == 1 && s.redraws[1] == 1);
  frame();
  CHECK(s.redraws[0] == 1 && s.redraws[1] == 1);  // nothing moved
  v.eye[0] += 0.2 * set.near_radius;              // within the near cascade's drift
  frame();
  CHECK(s.redraws[0] == 1);
  v.eye[0] += 0.1 * set.near_radius;  // past it, well within the far one's
  frame();
  CHECK(s.redraws[0] == 2 && s.redraws[1] == 1);
  v.sun_elevation += 0.3 * 3.14159265358979 / 180;  // past the near cascade's turn only
  frame();
  CHECK(s.redraws[0] == 3 && s.redraws[1] == 1);
  v.sun_elevation += 1.0 * 3.14159265358979 / 180;  // past the far one's too
  frame();
  CHECK(s.redraws[0] == 4 && s.redraws[1] == 2);
  v.eye[0] += 0.3 * set.far_radius;  // across the sun, far past both
  frame();
  CHECK(s.redraws[0] == 5 && s.redraws[1] == 3);
  v.sun_elevation = -0.1;  // night: nothing is drawn, nothing read
  frame();
  CHECK(s.redraws[0] == 5 && s.redraws[1] == 3);
  CHECK(s.staged.offset[0][3] == 0.0f && s.staged.offset[1][3] == 0.0f);
}

TEST(a_still_caster_shades_the_ground) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  // A tower 30 m ahead, 24 m tall: the sun over +z throws its shadow back toward the camera.
  const r::Instance tower[] = {box_at(256, 12, 150, 4, 24, 4)};
  r::set_instances(d, s.tower, 0, tower);
  const r::FrameView v = field_view();
  const ge::gpu::Image without = draw(d, s, v, out, false, {});
  const ge::gpu::Image with = draw(d, s, v, out, true, {});
  const Change c = compare(with, without);
  CHECK_MSG(c.darker > 40, "darker pixels: " + std::to_string(c.darker));
  CHECK_MSG(c.brighter == 0, "brighter pixels: " + std::to_string(c.brighter));
  // The tower's casters: drawn into both cascades, the near one first.
  CHECK(r::read_caster_stats(d, s.tower, 0).early == 1);
  CHECK(d.errors.load() == 0);
  CHECK(d.pipelines_after_load == 0);
}

TEST(a_figure_casts_from_its_tile) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  // A "figure" a metre tall standing 12 m ahead, its tile round it.
  const r::Instance body[] = {box_at(256, 0.9, 140, 0.6f, 1.8f, 0.6f)};
  r::set_instances(d, s.figure, 0, body);
  const std::array<double, 3> centres[] = {{256, 1.0, 140}};
  const r::FrameView v = field_view();
  const ge::gpu::Image without = draw(d, s, v, out, false, centres);
  const ge::gpu::Image with = draw(d, s, v, out, true, centres);
  const Change c = compare(with, without);
  CHECK_MSG(c.darker > 4, "darker pixels: " + std::to_string(c.darker));
  CHECK_MSG(c.brighter == 0, "brighter pixels: " + std::to_string(c.brighter));
  CHECK(s.shadows.figures == 1);
  CHECK(r::read_caster_stats(d, s.figure, 0).early == 1);
  CHECK(d.errors.load() == 0);
}

TEST(each_tile_culls_its_own_casters) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  // Three figures: two side by side (each inside the other's tile) and one 20 m off.
  const r::Instance bodies[] = {box_at(256, 0.9, 140, 0.6f, 1.8f, 0.6f),
                                box_at(256.8, 0.9, 140, 0.6f, 1.8f, 0.6f),
                                box_at(276, 0.9, 140, 0.6f, 1.8f, 0.6f)};
  r::set_instances(d, s.figure, 0, bodies);
  const std::array<double, 3> centres[] = {{256, 1.0, 140}, {276, 1.0, 140}};
  const r::FrameView v = field_view();
  const ge::gpu::Image with = draw(d, s, v, out, true, centres);
  CHECK(s.shadows.figures == 2);
  const r::InstanceStats near = r::read_caster_stats(d, s.figure, 0);
  const r::InstanceStats far = r::read_caster_stats(d, s.figure, 1);
  CHECK_MSG(near.early == 2, "tile 0 drew " + std::to_string(near.early));
  CHECK_MSG(far.early == 1, "tile 1 drew " + std::to_string(far.early));
  CHECK(near.frustum == 1 && far.frustum == 2);
  // An unused tile's slot culls nothing this frame.
  CHECK(!s.figure.casters[2].culled);
  CHECK(d.errors.load() == 0);
  (void)with;
}
