// engine/render/fog.hpp: a slice's far edge; which volumes are in reach and in what order; and on
// a device (the software adapter here and on CI), over a flat field: a column through an even
// absorbing fog is seen through as exp(-extinction x distance) at every slice's far edge; a
// glowing fog's light is its glow times what the column has taken out of the view (the
// integration conserves energy); with no volume in reach the picture is the one drawn with no fog
// at all, bit for bit; a volume changes it; with no validation error. A glowing volume's lamp:
// its glow times its extinction and its size, what escapes a thick one, flickering with its glow.
//
// ge:summary Tests for the fog in areas: slices, reach, an absorber's and a glow's columns
// ge:keywords test render fog froxel volume transmittance glow integrate reach slices
// ge:skill render testing
#include <algorithm>
#include <cmath>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "render/fog.hpp"
#include "render/frame.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 96, kH = 64;

r::FogSettings small_froxels() {
  r::FogSettings f;
  f.width = 24;
  f.height = 16;
  f.slices = 32;
  f.range = 100;
  return f;
}

struct Scene {
  ge::gpu::PipelineSet pipelines;
  r::FrameTargets targets;
  r::FrameHistory history;
  wgpu::Buffer uniforms;
  r::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, clear_frame, post_group;
  r::Sky sky;
  r::Terrain terrain;
  r::Post post;
  r::Fog fog;
  r::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
};

void make_scene(ge::gpu::Device& d, Scene& s) {
  s.field = ge::core::create_heightfield(0, 0, 4, 129, 129);  // flat at 0, 512 m
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
  r::create_fog(d, s.pipelines, s.fog, s.layout, &s.terrain, small_froxels());
  r::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky, nullptr, s.fog.volume_view);
  s.clear_frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  s.post_group = r::post_group(d, s.post, s.targets.hdr_view);
  s.rec.init(d, "test.timer", 12);
}

r::FrameView ground_view() {
  r::FrameView v;
  v.eye[0] = 256;
  v.eye[1] = 2;
  v.eye[2] = 100;
  v.heading = 0;  // along +z
  v.pitch = -0.05;
  v.sun_elevation = 0.6;
  v.sun_azimuth = 2.5;
  v.altitude_m = 1000;
  return v;
}

// A box round the eye larger than the froxels' reach: the same medium everywhere they look.
r::FogVolume everywhere(double density) {
  r::FogVolume f;
  const r::FrameView v = ground_view();
  for (int k = 0; k < 3; ++k) {
    f.centre[k] = v.eye[k];
    f.half[k] = 1000;
  }
  f.density = density;
  f.edge = 1e-3;
  f.noise = 0;
  return f;
}

// One frame with `volumes` (through the fog's group 0, or `clear`: the sky's clear air), then the
// picture.
ge::gpu::Image frame(ge::gpu::Device& d, Scene& s, const ge::gpu::Target& out,
                     const std::vector<r::FogVolume>& volumes, bool clear = false) {
  const r::FrameView v = ground_view();
  s.rec.begin_frame(d);
  s.history.valid = false;
  r::fill_uniforms(s.staged, v, kW, kH, &s.history);
  r::prepare_fog(d, s.fog, v, volumes, true);
  r::fog_uniforms(s.fog, s.staged);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  r::prepare_terrain(d, s.terrain, v);
  const wgpu::BindGroup& g = clear ? s.clear_frame : s.frame;
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  r::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, g);
  r::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  r::encode_fog(s.fog, s.pipelines, s.rec, e, g);
  r::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, g);
  r::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, g);
  r::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, g);
  r::encode_post(s.post, s.pipelines, s.rec, e, g, s.post_group, out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return ge::gpu::read_now(d, out);
}

}  // namespace

TEST(a_glowing_volume_is_a_lamp_of_its_glow_times_its_extinction_and_size) {
  r::FogVolume v;
  v.centre[0] = 10;
  v.centre[1] = 4;
  v.centre[2] = -3;
  v.half[0] = 2;
  v.half[1] = 1;
  v.half[2] = 3;
  v.density = 0.001;  // optically thin: all of it escapes, nearly
  v.noise = 0;
  v.glow[0] = v.glow[1] = v.glow[2] = 2;
  const r::LocalLight l = r::fog_glow_light(v);
  // 2 cd/m2 a metre of extinction x 0.001/m x 48 m^3, less the little a 1 m half-size takes out.
  const double thin = 2 * 0.001 * 48;
  CHECK(l.candela < thin && l.candela > 0.999 * thin);
  CHECK(l.position[0] == 10 && l.position[1] == 4 && l.position[2] == -3);
  CHECK(std::fabs(l.colour[0] - 1) < 1e-6 && std::fabs(l.colour[2] - 1) < 1e-6);
  CHECK(l.radius_m == 2.0f);
  CHECK(l.flicker == r::Flicker::steady);
  // An ellipsoid holds pi / 6 of its box; a thick volume lets out less than its glow's sum.
  v.shape = r::FogVolume::Shape::ellipsoid;
  CHECK(std::fabs(r::fog_glow_light(v).candela / l.candela - 3.14159265358979 / 6) < 1e-4);
  v.density = 2;
  CHECK(r::fog_glow_light(v).candela < 0.5 * 2 * 2 * 48 * 3.14159265358979 / 6);
  // None glowing: no light; a ground-hugging one stands half its height over the ground.
  v.glow[0] = v.glow[1] = v.glow[2] = 0;
  CHECK(r::fog_glow_light(v).candela == 0.0f);
  v.glow[0] = 1;
  v.hug_ground = true;
  CHECK(r::fog_glow_light(v, 100).position[1] == 101);
}

TEST(a_flickering_glow_and_its_lamp_waver_as_one) {
  r::FogVolume v;
  v.glow[0] = 1;
  CHECK(r::fog_glow_level(v, 3.7) == 1.0f);
  v.flicker = 0.4f;
  v.seed = 11;
  const r::LocalLight l = r::fog_glow_light(v);
  CHECK(l.flicker == r::Flicker::flame && l.seed == 11u);
  float lo = 10, hi = 0;
  for (int i = 0; i < 200; ++i) {
    const double t = 0.01 * i;
    const float level = r::fog_glow_level(v, t);
    CHECK(level == r::flicker_level(l, t));
    lo = std::min(lo, level);
    hi = std::max(hi, level);
  }
  CHECK(hi - lo > 0.1f);
}

TEST(a_slice_ends_at_the_square_of_its_share_of_the_range) {
  const r::FogSettings f = small_froxels();
  CHECK(r::fog_slice_far(f, 0) == 0);
  CHECK(std::fabs(r::fog_slice_far(f, 16) - 25) < 1e-9);
  CHECK(std::fabs(r::fog_slice_far(f, 32) - 100) < 1e-9);
}

TEST(the_volumes_in_reach_are_kept_nearest_first) {
  const r::FogSettings f = small_froxels();
  const r::FrameView v = ground_view();
  std::vector<r::FogVolume> vols(4);
  for (r::FogVolume& x : vols) {
    for (int k = 0; k < 3; ++k) x.centre[k] = v.eye[k];
    x.half[0] = x.half[1] = x.half[2] = 5;
  }
  vols[0].centre[2] += 80;   // its near face 80 - 8.7 m off: in reach
  vols[1].centre[2] += 20;   // nearer
  vols[2].centre[0] += 300;  // past the range
  vols[3].density = 0;       // no medium
  const std::vector<size_t> in = r::fog_in_reach(f, v, vols);
  CHECK(in.size() == 2);
  CHECK(in[0] == 1);
  CHECK(in[1] == 0);
  std::vector<r::FogVolume> many(r::kMaxFogVolumes + 8, vols[1]);
  CHECK(r::fog_in_reach(f, v, many).size() == r::kMaxFogVolumes);
}

TEST(an_absorbing_fog_is_seen_through_by_its_extinction) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  r::FogVolume f = everywhere(0.02);
  f.albedo[0] = f.albedo[1] = f.albedo[2] = 0;  // takes the light out, scatters none
  frame(d, s, out, {f});
  const std::vector<float> col = r::read_fog_column(d, s.fog, 12, 8);
  const r::FogSettings& fs = s.fog.settings;
  for (uint32_t z = 0; z < fs.slices; ++z) {
    const double seen = std::exp(-0.02 * r::fog_slice_far(fs, z + 1));
    CHECK(std::fabs(col[z * 4 + 3] - seen) < 2e-3);
    CHECK(std::fabs(col[z * 4 + 0]) < 1e-6);
  }
  CHECK(d.errors.load() == 0);
}

TEST(a_glowing_fog_gives_its_glow_for_what_it_takes_out_of_the_view) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  r::FogVolume f = everywhere(0.03);
  f.albedo[0] = f.albedo[1] = f.albedo[2] = 0;
  f.glow[0] = 400;  // cd/m2 a metre of extinction, red only
  frame(d, s, out, {f});
  const std::vector<float> col = r::read_fog_column(d, s.fog, 12, 8);
  // The frame's light units: cd/m2 x the pre-exposure over the sun's 128 klx.
  const double unit = s.staged.world[1] / 128000.0;
  for (uint32_t z = 0; z < s.fog.settings.slices; ++z) {
    const double taken = 1.0 - col[z * 4 + 3];
    const double want = 400 * unit * taken;
    CHECK(std::fabs(col[z * 4 + 0] - want) <= 4e-3 * want + 1e-6);
    CHECK(std::fabs(col[z * 4 + 1]) < 1e-6);
  }
  CHECK(d.errors.load() == 0);
}

TEST(no_fog_in_reach_draws_the_clear_air_bit_for_bit_and_a_fog_changes_it) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const ge::gpu::Image clear = frame(d, s, out, {}, true);
  const ge::gpu::Image none = frame(d, s, out, {});
  CHECK(clear.rgba == none.rgba);
  CHECK(s.fog.count == 0);
  const ge::gpu::Image fog = frame(d, s, out, {everywhere(0.05)});
  CHECK(s.fog.count == 1);
  int changed = 0;
  for (size_t i = 0; i + 3 < fog.rgba.size(); i += 4) {
    if (fog.rgba[i] != none.rgba[i] || fog.rgba[i + 1] != none.rgba[i + 1] ||
        fog.rgba[i + 2] != none.rgba[i + 2]) {
      ++changed;
    }
  }
  CHECK(changed > static_cast<int>(kW * kH / 2));
  CHECK(d.errors.load() == 0);
  CHECK(d.pipelines_after_load == 0);
}
