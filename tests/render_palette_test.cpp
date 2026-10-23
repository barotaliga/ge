// engine/render/skinning.hpp's palette families on a device (the software adapter here and on
// CI), held to the skinning pass:
//
//   * THE CPU TWIN: `pose_palette` (the vertex stage's pose, its weights quantised as
//     `palette_source` stores them) lands every vertex of the test model within a hair of
//     assets/pose.hpp's `skin_mesh` at three moments of its clips.
//   * THE PICTURE: two limbs side by side, each in a pose of its own, drawn as one palette family
//     (one mesh, two instances, two palettes) and as the skinning pass draws them (two meshes, two
//     skins): the depth the two frames leave agrees where either covers, but for a pixel or two
//     on an edge (the pass quantises the posed position, the palette the bind's).
//   * THE HALVES: each `set_palettes` swaps the frame's half and the last frame's; `cut_family`
//     settles the next write.
//
// ge:summary Tests for palette families: the vertex stage's pose against the skinning pass
// ge:keywords test render palette-family vertex-skinning set_palettes pose_palette crowd depth
// ge:skill render assets testing
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "assets/gltf.hpp"
#include "assets/gltf_write.hpp"
#include "assets/pose.hpp"
#include "assets/test_model.hpp"
#include "check.hpp"
#include "core/heightfield.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "render/frame.hpp"
#include "render/hiz.hpp"
#include "render/instances.hpp"
#include "render/post.hpp"
#include "render/skinning.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace {

namespace a = ge::assets;
namespace r = ge::render;

constexpr uint32_t kW = 128, kH = 96;

// The test model's palette at clip `clip`'s time `t`.
std::vector<a::Affine> palette_at(const a::Model& m, int clip, double t) {
  std::vector<a::Trs> local(m.joints.size());
  std::vector<a::Affine> global(m.joints.size()), palette(m.joints.size());
  a::sample_clip(m, m.clips[static_cast<size_t>(clip)], t, false, local);
  a::pose_globals(m, local, global);
  a::skin_palette(m, global, palette);
  return palette;
}

struct Scene {
  ge::gpu::PipelineSet pipelines;
  r::FrameTargets targets;
  r::FrameHistory history;
  wgpu::Buffer uniforms;
  r::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_group, light;
  r::Sky sky;
  r::Terrain terrain;
  r::Hiz hiz;
  r::Post post;
  r::InstancePipelines ip;
  r::SkinPipelines sp;
  r::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
};

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
  r::create_skin_pipelines(d, s.pipelines, s.sp);
  r::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.frame = r::frame_group(d, s.layout, s.uniforms, s.sky);
  r::create_hiz_targets(d, s.hiz, s.targets);
  s.post_group = r::post_group(d, s.post, s.targets.hdr_view);
  s.light = r::instance_light_group(d, s.ip, s.terrain);
  s.rec.init(d, "test.timer", 16);
}

// Looking along +z at the two limbs 3 m ahead, from their middle's height.
r::FrameView limb_view() {
  r::FrameView v;
  v.eye[0] = 256;
  v.eye[1] = 1.0;
  v.eye[2] = 97;
  v.heading = 0;
  v.pitch = 0;
  v.sun_elevation = 0.6;
  v.sun_azimuth = 2.5;
  v.altitude_m = 1000;
  return v;
}

// The two limbs' places: 0.5 m either side of the view's axis, on the snow.
r::Instance limb_at(int k, uint32_t mesh) {
  r::Instance in;
  in.position[0] = 256 + (k == 0 ? -0.5 : 0.5);
  in.position[1] = 0.2;
  in.position[2] = 100;
  in.mesh = mesh;
  return in;
}

// One frame of `family` (and the skinning pass over `skins` before its culls); its depth.
std::vector<float> draw(ge::gpu::Device& d, Scene& s, r::InstanceFamily& family,
                        std::span<r::Skin* const> skins, const ge::gpu::Target& out) {
  const r::FrameView v = limb_view();
  s.history.valid = false;
  s.rec.begin_frame(d);
  r::fill_uniforms(s.staged, v, kW, kH, &s.history);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  r::prepare_terrain(d, s.terrain, v);
  r::prepare_family(d, family, v, kH, false);
  r::InstanceFamily* families[] = {&family};
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  if (!skins.empty()) r::encode_skinning(skins, s.sp, s.pipelines, s.rec, e);
  r::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  r::encode_instances_cull(families, s.ip, s.pipelines, s.rec, e, s.frame, false);
  r::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  r::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_instances_depth(families, s.ip, s.pipelines, s.rec, e, s.targets, s.frame, s.light,
                            false);
  r::encode_hiz(s.hiz, s.pipelines, s.rec, e);
  r::encode_instances_cull(families, s.ip, s.pipelines, s.rec, e, s.frame, true);
  r::encode_instances_depth(families, s.ip, s.pipelines, s.rec, e, s.targets, s.frame, s.light,
                            true);
  r::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_instances(families, s.ip, s.pipelines, s.rec, e, s.targets, s.frame, s.light);
  r::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  r::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  return r::read_depth(d, s.targets);
}

}  // namespace

TEST(the_vertex_stage_pose_matches_skin_mesh) {
  const a::Model m = a::make_test_model();
  const a::Mesh& mesh = m.meshes[0];
  float worst = 0, worst_normal = 0;
  for (const auto& [clip, t] : {std::pair{0, 0.3}, std::pair{0, 0.8}, std::pair{1, 0.5}}) {
    const std::vector<a::Affine> palette = palette_at(m, clip, t);
    std::vector<float> want_p(size_t{mesh.vertex_count()} * 3), want_n(want_p.size()), got_p, got_n;
    a::skin_mesh(mesh, palette, want_p, want_n);
    r::pose_palette(mesh, palette, got_p, got_n);
    CHECK(got_p.size() == want_p.size());
    for (size_t k = 0; k < want_p.size() && k < got_p.size(); ++k) {
      worst = std::max(worst, std::fabs(got_p[k] - want_p[k]));
    }
    for (size_t v = 0; v + 2 < want_n.size() && v + 2 < got_n.size(); v += 3) {
      const float gl = std::hypot(got_n[v], got_n[v + 1], got_n[v + 2]);
      const float wl = std::hypot(want_n[v], want_n[v + 1], want_n[v + 2]);
      for (size_t c = 0; c < 3; ++c) {
        worst_normal = std::max(worst_normal, std::fabs(got_n[v + c] / std::max(gl, 1e-20f) -
                                                        want_n[v + c] / std::max(wl, 1e-20f)));
      }
    }
  }
  // A 16-bit weight is off by at most 1/131070 of a joint's move.
  CHECK_MSG(worst < 1e-4f, "a posed position is " + std::to_string(worst) + " m from skin_mesh's");
  CHECK_MSG(worst_normal < 1e-3f, "a posed normal is " + std::to_string(worst_normal) + " off");
}

TEST(a_palette_family_draws_what_the_skinning_pass_draws) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  const a::LoadResult loaded = a::load_gltf(a::write_glb(a::make_test_model()), "test_model.glb");
  CHECK_MSG(loaded.ok, loaded.error);
  if (!loaded.ok) return;
  const a::Model& m = loaded.model;
  const a::Mesh& mesh = m.meshes[0];
  const a::Box reach = a::reach_box(m, mesh);
  const auto joints = static_cast<uint32_t>(m.joints.size());
  const std::vector<float> materials = {0.8f, 0.45f, 0.3f, 0.1f, 0.3f, 0.8f};
  const std::vector<a::Affine> poses[2] = {palette_at(m, 0, 0.5), palette_at(m, 1, 0.5)};

  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);

  // The skinning pass's: a mesh and a skin a limb.
  std::vector<r::MeshSource> two(2);
  for (r::MeshSource& src : two) src.lods = {r::skinned_source(mesh, reach)};
  r::InstanceFamily passed;
  r::create_family(d, s.ip, passed, "test.passed", two, materials, 2);
  r::bind_family_hiz(d, s.ip, passed, s.hiz);
  r::Skin skins[2];
  for (uint32_t k = 0; k < 2; ++k) {
    r::create_skin(d, s.sp, skins[k], passed, k, 0, mesh, reach, joints);
    r::set_palette(d, skins[k], poses[k]);
  }
  const std::vector<r::Instance> passed_at = {limb_at(0, 0), limb_at(1, 1)};
  r::set_instances(d, passed, 0, passed_at);
  r::Skin* skin_list[2] = {&skins[0], &skins[1]};
  const std::vector<float> want = draw(d, s, passed, skin_list, out);

  // The palette family's: one mesh, two instances, a palette each.
  std::vector<r::MeshSource> one(1);
  one[0].lods = {r::palette_source(mesh, reach)};
  r::InstanceFamily crowd;
  r::create_family(d, s.ip, crowd, "test.crowd", one, materials, 2, joints);
  r::bind_family_hiz(d, s.ip, crowd, s.hiz);
  CHECK(crowd.lod_last.size() == 1 && crowd.lod_last[0] == 0);  // no last positions kept
  const std::vector<r::Instance> crowd_at = {limb_at(0, 0), limb_at(1, 0)};
  r::set_instances(d, crowd, 0, crowd_at);
  std::vector<a::Affine> rows(poses[0]);
  rows.insert(rows.end(), poses[1].begin(), poses[1].end());
  r::set_palettes(d, crowd, rows);
  const std::vector<float> got = draw(d, s, crowd, {}, out);

  // And the same family with both limbs in the first pose: the second limb must move.
  std::vector<a::Affine> same(poses[0]);
  same.insert(same.end(), poses[0].begin(), poses[0].end());
  r::set_palettes(d, crowd, same);
  const std::vector<float> one_pose = draw(d, s, crowd, {}, out);

  CHECK(want.size() == got.size() && got.size() == one_pose.size());
  // Reversed-z: a figure is nearer than the snow behind it, 3 m off against 50 m and more.
  const float figure = static_cast<float>(limb_view().near_m / 10.0);
  int covered = 0, off = 0, moved = 0;
  for (size_t p = 0; p < want.size() && p < got.size() && p < one_pose.size(); ++p) {
    const bool w = want[p] > figure, g = got[p] > figure, o = one_pose[p] > figure;
    covered += w ? 1 : 0;
    if (w != g || (w && std::fabs(want[p] - got[p]) > want[p] * 2e-3f)) ++off;
    if (g != o) ++moved;
  }
  CHECK_MSG(covered > 200, "the limbs cover only " + std::to_string(covered) + " pixels");
  CHECK_MSG(off <= covered / 50, std::to_string(off) + " of the " + std::to_string(covered) +
                                     " pixels the pass covers differ from the palette family's");
  CHECK_MSG(moved > 20, "the second limb's own palette moved only " + std::to_string(moved) +
                            " pixels: the instances are not posed by their own palettes");
  CHECK(d.errors.load() == 0);
}

TEST(set_palettes_swaps_the_halves_and_a_cut_settles) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  const a::Model m = a::make_test_model();
  ge::gpu::PipelineSet pipelines;
  r::InstancePipelines ip;
  r::create_instance_pipelines(d, pipelines, ip, r::frame_layout(d));
  CHECK(pipelines.finish(d));
  ge::gpu::end_load(d);
  std::vector<r::MeshSource> one(1);
  one[0].lods = {r::palette_source(m.meshes[0], a::reach_box(m, m.meshes[0]))};
  const std::vector<float> materials = {1, 1, 1};
  r::InstanceFamily f;
  const auto joints = static_cast<uint32_t>(m.joints.size());
  r::create_family(d, ip, f, "test.halves", one, materials, 3, joints);
  const std::vector<a::Affine> rows(size_t{joints} * 3);
  const uint32_t half = 3 * joints * 3;
  CHECK(f.palettes_settle);
  r::set_palettes(d, f, rows);
  CHECK(f.staged.skin[0] == joints);
  CHECK(f.staged.skin[1] == half && f.staged.skin[2] == 0);
  CHECK(!f.palettes_settle);
  r::set_palettes(d, f, rows);
  CHECK(f.staged.skin[1] == 0 && f.staged.skin[2] == half);
  r::cut_family(f);
  CHECK(f.palettes_settle);
  CHECK(d.errors.load() == 0);
}
