// GPU skinning's definitions: the pipeline, a skin's buffers and group, the palette's upload,
// the pass, and its CPU twin.
//
// ge:summary GPU skinning definitions: pipeline, bind vertices, palette, the pass, CPU twin
// ge:keywords render skinning compute palette bind-vertices dispatch
// ge:skill render assets
// ge:test tests/render_skinning_test.cpp
#include "render/skinning.hpp"

#include <algorithm>
#include <cstring>
#include <string>

#include "render/skinning.wgsl.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;
constexpr uint32_t kGroup = 64;  // skinning.wgsl's workgroup size

MeshData mesh_data(const assets::Mesh& mesh) {
  MeshData m;
  m.positions = mesh.positions;
  m.normals = mesh.normals;
  m.parts = mesh.parts;
  m.indices = mesh.indices;
  return m;
}

// skinning.wgsl's SkinParams.
struct Params {
  uint32_t info[4];
  float box_min[4];
  float box_size[4];
};

}  // namespace

void create_skin_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines, SkinPipelines& sp) {
  sp.layout = make_layout(device, "skinning",
                          {Slot::uniform, Slot::storage_read, Slot::storage_read, Slot::storage},
                          wgpu::ShaderStage::Compute);
  wgpu::ComputePipelineDescriptor desc{};
  desc.label = "skinning";
  desc.layout = make_pipeline_layout(device, "skinning", {sp.layout});
  desc.compute.module = gpu::create_shader(device, wgsl::skinning, "skinning");
  desc.compute.entryPoint = "skin";
  sp.p_skin = pipelines.add_compute(device, desc);
}

LodSource skinned_source(const assets::Mesh& mesh, const assets::Box& reach, float min_px) {
  LodSource src;
  src.mesh = mesh_data(mesh);
  src.min_px = min_px;
  LodReach box{};
  for (size_t k = 0; k < 3; ++k) {
    box.min[k] = reach.min[k];
    box.size[k] = reach.max[k] - reach.min[k];
  }
  src.reach = box;
  return src;
}

void create_skin(gpu::Device& device, const SkinPipelines& sp, Skin& skin,
                 const InstanceFamily& family, uint32_t mesh_index, uint32_t lod,
                 const assets::Mesh& mesh, const assets::Box& reach, uint32_t joints) {
  const std::string label = family.label + ".skin" + std::to_string(mesh_index);
  skin.vertices = mesh.vertex_count();
  skin.joints = std::max(joints, 1u);

  // The bind vertices: position and joints, normal and part, weights (skinning.wgsl's
  // BindVertex), twelve words each.
  std::vector<uint32_t> words(size_t{std::max(skin.vertices, 1u)} * 12, 0);
  for (uint32_t v = 0; v < skin.vertices; ++v) {
    uint32_t* w = &words[size_t{v} * 12];
    std::memcpy(w, &mesh.positions[size_t{v} * 3], 12);
    w[3] = 0;
    for (int k = 0; k < 4; ++k) w[3] |= uint32_t{mesh.joints[size_t{v} * 4 + k]} << (8 * k);
    std::memcpy(w + 4, &mesh.normals[size_t{v} * 3], 12);
    w[7] = mesh.parts[v];
    std::memcpy(w + 8, &mesh.weights[size_t{v} * 4], 16);
  }
  skin.bind = make_buffer(device, label + ".bind", words.size() * 4, BU::Storage);
  device.queue.WriteBuffer(skin.bind, 0, words.data(), words.size() * 4);

  skin.rows.assign(size_t{skin.joints} * 12, 0.0f);
  for (uint32_t j = 0; j < skin.joints; ++j) {
    const assets::Affine id;
    std::memcpy(&skin.rows[size_t{j} * 12], id.m, sizeof id.m);
  }
  skin.palette = make_buffer(device, label + ".palette", skin.rows.size() * 4, BU::Storage);
  device.queue.WriteBuffer(skin.palette, 0, skin.rows.data(), skin.rows.size() * 4);

  Params p{};
  const uint32_t global_lod = family.mesh_lod[mesh_index] + lod;
  p.info[0] = skin.vertices;
  p.info[1] = family.lod_vertex[global_lod];
  p.info[2] = skin.joints;
  for (int k = 0; k < 3; ++k) {
    p.box_min[k] = reach.min[k];
    p.box_size[k] = std::max(reach.max[k] - reach.min[k], 1e-6f);
  }
  skin.params = make_buffer(device, label + ".params", sizeof p, BU::Uniform);
  device.queue.WriteBuffer(skin.params, 0, &p, sizeof p);
  skin.group =
      make_group(device, label, sp.layout, {skin.params, skin.bind, skin.palette, family.vertices});
}

void set_palette(gpu::Device& device, Skin& skin, std::span<const assets::Affine> palette) {
  const size_t n = std::min<size_t>(palette.size(), skin.joints);
  for (size_t j = 0; j < n; ++j) std::memcpy(&skin.rows[j * 12], palette[j].m, 48);
  device.queue.WriteBuffer(skin.palette, 0, skin.rows.data(), n * 48);
}

void encode_skinning(std::span<Skin* const> skins, const SkinPipelines& sp,
                     const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                     wgpu::CommandEncoder& encoder) {
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "skinning");
  PassStat& stat = rec.current();
  pass.SetPipeline(pipelines.compute(sp.p_skin));
  for (const Skin* skin : skins) {
    if (!skin->vertices) continue;
    pass.SetBindGroup(0, skin->group);
    pass.DispatchWorkgroups((skin->vertices + kGroup - 1) / kGroup);
    ++stat.dispatches;
  }
  pass.End();
}

std::vector<uint32_t> read_family_vertices(gpu::Device& device, const InstanceFamily& family,
                                           uint32_t first, uint32_t count) {
  const uint64_t bytes = uint64_t{count} * 12;
  std::vector<uint32_t> words(size_t{count} * 3, 0);
  if (!count) return words;
  wgpu::BufferDescriptor desc{};
  desc.label = "skinning.readback";
  desc.size = bytes;
  desc.usage = BU::MapRead | BU::CopyDst;
  wgpu::Buffer rb = device.device.CreateBuffer(&desc);
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  encoder.CopyBufferToBuffer(family.vertices, uint64_t{first} * 12, rb, 0, bytes);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  gpu::wait(device, rb.MapAsync(wgpu::MapMode::Read, 0, bytes, wgpu::CallbackMode::WaitAnyOnly,
                                [](wgpu::MapAsyncStatus, wgpu::StringView) {}));
  if (const void* p = rb.GetConstMappedRange(0, bytes)) std::memcpy(words.data(), p, bytes);
  rb.Unmap();
  return words;
}

std::vector<uint32_t> skin_packed(const assets::Mesh& mesh, std::span<const assets::Affine> palette,
                                  const assets::Box& reach) {
  MeshData m = mesh_data(mesh);
  assets::skin_mesh(mesh, palette, m.positions, m.normals);
  float size[3];
  for (int k = 0; k < 3; ++k) size[k] = reach.max[k] - reach.min[k];
  return pack_mesh(m, reach.min, size).words;
}

}  // namespace ge::render
