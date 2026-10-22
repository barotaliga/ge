// engine/render/skinning.hpp on the GPU (the software adapter on CI), held to its CPU twin:
//
//   * THE PASS POSES: the test model (assets/test_model.hpp) through a .glb and the loader,
//     skinned by the pass at three moments of its clips, reads back from the family's vertex
//     buffer as `skin_packed` packs it on the CPU: every position within one 16-bit step of the
//     reach box, every normal within two snorm steps, every part as written.
//   * THE SLOT: the pass writes only its own mesh's vertices; the family's other mesh is left
//     as `create_family` packed it.
//   * THE REACH: a skinned LOD's mesh sphere holds its reach box, so the cull keeps it in every
//     pose.
//
// ge:summary Tests for GPU skinning: the pass against its CPU twin, the slot, the reach
// ge:keywords test render skinning gpu compute palette readback skin_packed reach lavapipe
// ge:skill render assets testing
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "assets/gltf.hpp"
#include "assets/gltf_write.hpp"
#include "assets/pose.hpp"
#include "assets/test_model.hpp"
#include "check.hpp"
#include "gpu_support.hpp"
#include "render/instances.hpp"
#include "render/mesh.hpp"
#include "render/skinning.hpp"
#include "render/sky.hpp"

namespace {

namespace a = ge::assets;
namespace r = ge::render;

// The gap between two words' low and high 16-bit fields, as unsigned steps.
int step_gap(uint32_t x, uint32_t y) {
  return std::abs(static_cast<int>(x & 0xffffu) - static_cast<int>(y & 0xffffu));
}
int high_gap(uint32_t x, uint32_t y) {
  return std::abs(static_cast<int>(x >> 16) - static_cast<int>(y >> 16));
}
// The 16-bit fields as signed (a snorm's step either side of zero wraps in unsigned).
int snorm_gap(uint32_t x, uint32_t y) {
  const auto lo = [](uint32_t v) { return static_cast<int>(static_cast<int16_t>(v & 0xffffu)); };
  const auto hi = [](uint32_t v) { return static_cast<int>(static_cast<int16_t>(v >> 16)); };
  return std::max(std::abs(lo(x) - lo(y)), std::abs(hi(x) - hi(y)));
}

}  // namespace

TEST(skinning_pass_matches_its_cpu_twin_in_its_own_slot) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;

  const a::LoadResult loaded = a::load_gltf(a::write_glb(a::make_test_model()), "test_model.glb");
  CHECK_MSG(loaded.ok, loaded.error);
  if (!loaded.ok) return;
  const a::Model& m = loaded.model;
  const a::Mesh& mesh = m.meshes[0];
  const a::Box reach = a::reach_box(m, mesh);

  ge::gpu::PipelineSet pipelines;
  r::InstancePipelines ip;
  r::SkinPipelines sp;
  const wgpu::BindGroupLayout frame = r::frame_layout(d);
  r::create_instance_pipelines(d, pipelines, ip, frame);
  r::create_skin_pipelines(d, pipelines, sp);
  CHECK(pipelines.finish(d));
  ge::gpu::end_load(d);

  // Mesh 0 a static box, mesh 1 the skinned limb: the skin's slot is not the buffer's start.
  std::vector<r::MeshSource> meshes(2);
  r::MeshData box;
  const float zero[3] = {0, 0, 0}, half[3] = {0.5f, 0.5f, 0.5f};
  r::add_box(box, zero, half, 0);
  meshes[0].lods = {{box, 0}};
  meshes[1].lods = {r::skinned_source(mesh, reach)};
  const std::vector<float> materials = {1, 1, 1, 0.8f, 0.45f, 0.3f, 0.1f, 0.3f, 0.8f};
  r::InstanceFamily family;
  r::create_family(d, ip, family, "test.figures", meshes, materials, 4);
  CHECK(family.lod_vertex.size() == 2 && family.lod_vertex[1] == box.vertex_count());

  r::Skin skin;
  r::create_skin(d, sp, skin, family, 1, 0, mesh, reach, static_cast<uint32_t>(m.joints.size()));
  r::FrameRecorder rec;
  rec.init(d, "test.skinning", 2);

  std::vector<a::Trs> local(m.joints.size());
  std::vector<a::Affine> global(m.joints.size()), palette(m.joints.size());
  const struct {
    int clip;
    double t;
  } moments[3] = {{0, 0.5}, {0, 0.8}, {1, 0.3}};
  for (const auto& moment : moments) {
    a::sample_clip(m, m.clips[static_cast<size_t>(moment.clip)], moment.t, false, local);
    a::pose_globals(m, local, global);
    a::skin_palette(m, global, palette);
    r::set_palette(d, skin, palette);
    rec.begin_frame(d);
    wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
    r::Skin* skins[1] = {&skin};
    r::encode_skinning(skins, sp, pipelines, rec, e);
    rec.end_frame(e);
    wgpu::CommandBuffer commands = e.Finish();
    d.queue.Submit(1, &commands);
    rec.after_submit(d);

    const std::vector<uint32_t> want = r::skin_packed(mesh, palette, reach);
    const std::vector<uint32_t> got =
        r::read_family_vertices(d, family, family.lod_vertex[1], mesh.vertex_count());
    int pos = 0, part = 0, nrm = 0;
    for (size_t v = 0; v < want.size() / 3; ++v) {
      pos = std::max({pos, step_gap(got[v * 3], want[v * 3]), high_gap(got[v * 3], want[v * 3]),
                      step_gap(got[v * 3 + 1], want[v * 3 + 1])});
      part = std::max(part, high_gap(got[v * 3 + 1], want[v * 3 + 1]));
      nrm = std::max(nrm, snorm_gap(got[v * 3 + 2], want[v * 3 + 2]));
    }
    const std::string at =
        "clip " + std::to_string(moment.clip) + " at " + std::to_string(moment.t);
    CHECK_MSG(pos <= 1, at + ": a position is " + std::to_string(pos) + " steps from the CPU's");
    CHECK_MSG(part == 0, at + ": a part differs from the bind's");
    CHECK_MSG(nrm <= 2, at + ": a normal is " + std::to_string(nrm) + " steps from the CPU's");
  }

  // The box is as create_family packed it.
  const r::PackedMesh packed_box = r::pack_mesh(box);
  CHECK(r::read_family_vertices(d, family, 0, box.vertex_count()) == packed_box.words);
}

TEST(a_skinned_lod_is_bounded_by_its_reach) {
  const a::Model m = a::make_test_model();
  const a::Box reach = a::reach_box(m, m.meshes[0]);
  const r::LodSource src = r::skinned_source(m.meshes[0], reach);
  CHECK(src.reach.has_value());
  if (!src.reach) return;
  for (int k = 0; k < 3; ++k) {
    CHECK(src.reach->min[static_cast<size_t>(k)] == reach.min[k]);
    CHECK(src.reach->size[static_cast<size_t>(k)] == reach.max[k] - reach.min[k]);
  }
}
