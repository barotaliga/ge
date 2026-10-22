// engine/render/instances.hpp on a device (the software adapter here and on CI), over a flat
// field: a LOD is picked by the radius on screen with a cross-fade band at each threshold (the
// CPU twin); a box's front faces are the ones drawn (the depth at its middle is its near face's);
// a wall hides the boxes behind it, which the early phase culls by the last frame's pyramid while
// the boxes beside it are drawn; when the camera rises over the wall the late phase draws the
// boxes the last frame's pyramid hid; and every frame's picture is the same bits as the frame
// drawn with no occlusion culling at all.
//
// ge:summary Tests for instance families: the LOD pick, winding, the two-phase cull's counts
// and its pictures against no culling
// ge:keywords test render instances lod cull two-phase occlusion hi-z winding indirect
// ge:skill render testing
#include <cstring>
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
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 96, kH = 64;

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
  r::InstanceFamily boxes;
  r::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
};

// The boxes: a wall 30 m wide and 20 m tall across the view 30 m ahead, three boxes behind it
// and two beside it, 60 m ahead.
constexpr int kHidden = 3, kBeside = 2;

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

  std::vector<r::MeshSource> meshes(1);
  r::MeshData box;
  const float zero[3] = {0, 0, 0}, half[3] = {0.5f, 0.5f, 0.5f};
  r::add_box(box, zero, half, 0);
  meshes[0].lods = {{box, 0}};
  const std::vector<float> materials = {0.1f, 0.05f, 0.03f};
  r::create_family(d, s.ip, s.boxes, "test.boxes", meshes, materials, 16);
  r::bind_family_hiz(d, s.ip, s.boxes, s.hiz);
  s.light = r::instance_light_group(d, s.ip, s.terrain);
  std::vector<r::Instance> all = {box_at(256, 10, 130, 30, 20, 1)};
  for (int k = 0; k < kHidden; ++k) all.push_back(box_at(250 + 6 * k, 1, 160, 2, 2, 2));
  all.push_back(box_at(222, 1, 160, 2, 2, 2));
  all.push_back(box_at(290, 1, 160, 2, 2, 2));
  r::set_instances(d, s.boxes, 0, all);
  s.rec.init(d, "test.timer", 14);
}

r::FrameView ground_view() {
  r::FrameView v;
  v.eye[0] = 256;
  v.eye[1] = 5;
  v.eye[2] = 100;
  v.heading = 0;  // along +z, toward the wall
  v.pitch = 0;
  v.sun_elevation = 0.6;
  v.sun_azimuth = 2.5;
  v.altitude_m = 1000;
  return v;
}

// A frame in the pass table's order; the last frame's pyramid culls when `occlusion` is on and
// there is a last frame.
ge::gpu::Image draw(ge::gpu::Device& d, Scene& s, const r::FrameView& v, const ge::gpu::Target& out,
                    bool occlusion) {
  s.rec.begin_frame(d);
  const bool occlude = occlusion && s.history.valid;
  r::fill_uniforms(s.staged, v, kW, kH, &s.history);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  r::prepare_sky(s.sky, v);
  r::prepare_terrain(d, s.terrain, v);
  r::prepare_family(d, s.boxes, v, kH, occlude);
  r::InstanceFamily* families[] = {&s.boxes};
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
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
  return ge::gpu::read_now(d, out);
}

// The view after a cut with no occlusion culling: every instance in the frustum drawn.
ge::gpu::Image unculled(ge::gpu::Device& d, Scene& s, const r::FrameView& v,
                        const ge::gpu::Target& out) {
  s.history.valid = false;
  return draw(d, s, v, out, false);
}

}  // namespace

TEST(lod_pick_bands_its_thresholds) {
  const float px[] = {14.0f, 4.0f, 0.0f};
  r::LodPick p = r::pick_lod(px, 3, 30.0f);
  CHECK(p.lod == 0 && p.other == -1);
  p = r::pick_lod(px, 3, 14.0f);  // on the threshold: half each
  CHECK(p.lod == 0 && p.other == 1);
  CHECK(p.share > 0.49f && p.share < 0.51f);
  p = r::pick_lod(px, 3, 14.0f * (1 + r::kLodBand) - 0.01f);  // the band's top: nearly all fine
  CHECK(p.other == 1 && p.share > 0.99f);
  p = r::pick_lod(px, 3, 8.0f);
  CHECK(p.lod == 1 && p.other == -1);
  p = r::pick_lod(px, 3, 4.0f * (1 - r::kLodBand) + 0.01f);  // the next band's bottom
  CHECK(p.lod == 1 && p.other == 2 && p.share < 0.01f);
  p = r::pick_lod(px, 3, 1.0f);
  CHECK(p.lod == 2 && p.other == -1);
  CHECK(r::pick_lod(px, 3, r::kMinPixels * 0.5f).lod == -1);
}

TEST(instances_draw_their_front_faces) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  draw(d, s, ground_view(), out, false);
  // The wall's near face is 29.5 m ahead along the view axis; its far face would be 30.5 m.
  const std::vector<float> depth = r::read_depth(d, s.targets);
  const float at = depth[(kH / 2) * kW + kW / 2];
  const float near_face = static_cast<float>(ground_view().near_m / 29.5);
  CHECK(std::fabs(at - near_face) < near_face * 1e-3f);
  CHECK(d.errors.load() == 0);
  CHECK(d.pipelines_after_load == 0);
}

TEST(early_phase_culls_what_the_last_frame_hid) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  const r::FrameView v = ground_view();
  const ge::gpu::Image want = unculled(d, s, v, out);
  r::InstanceStats st = r::read_instance_stats(d, s.boxes);
  CHECK(st.early == 1 + kHidden + kBeside);  // no last frame: the frustum alone
  CHECK(st.occluded == 0);
  const ge::gpu::Image got = draw(d, s, v, out, true);
  st = r::read_instance_stats(d, s.boxes);
  CHECK(st.early == 1 + kBeside);
  CHECK(st.occluded == kHidden);
  CHECK(st.hidden == kHidden);
  CHECK(st.late == 0);
  CHECK(got.rgba == want.rgba);
  CHECK(d.errors.load() == 0);
}

TEST(late_phase_draws_what_the_camera_turned_to) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.final", kW, kH);
  r::FrameView v = ground_view();
  r::FrameView over = v;
  over.eye[1] = 45;  // over the wall's top, looking down onto the boxes behind it
  over.pitch = -0.55;
  const ge::gpu::Image want = unculled(d, s, over, out);
  s.history.valid = false;
  draw(d, s, v, out, true);
  draw(d, s, v, out, true);  // the pyramid now hides the boxes behind the wall
  const ge::gpu::Image got = draw(d, s, over, out, true);
  const r::InstanceStats st = r::read_instance_stats(d, s.boxes);
  CHECK(st.late >= 1);
  CHECK(st.hidden == 0);
  CHECK(st.early + st.late == 1 + kHidden + kBeside - st.frustum);
  CHECK(got.rgba == want.rgba);
  CHECK(d.errors.load() == 0);
}
