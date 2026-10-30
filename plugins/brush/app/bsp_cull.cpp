// ge:summary The world's cull: chunks, the eye's PVS bits, the two phases' dispatches and counts
// ge:keywords bsp cull r_cull pvs hi-z occlusion two-phase indirect chunk scan readback
// ge:skill brush render
#include "brush/app/bsp_cull.hpp"

#include <algorithm>
#include <cstring>

#include "brush/app/bsp_cull.wgsl.hpp"

namespace ge::brush::app {

namespace r = ge::render;
using BU = wgpu::BufferUsage;

namespace {

constexpr uint32_t kStatsAt = 2 * kKinds * 5;  // words before the counters (bsp_cull.wgsl)
constexpr uint32_t kStats = 6;

}  // namespace

void create_bsp_cull(gpu::Device& device, gpu::PipelineSet& pipelines, BspCull& c,
                     const Scene& scene, const std::vector<std::array<uint32_t, 4>>& chunk_of,
                     const wgpu::Buffer& src, const wgpu::BindGroupLayout& frame) {
  // Which surfaces are the world's own (model 0): the rest are the brush models'.
  const Map& m = scene.map;
  std::vector<uint8_t> own(m.surfaces.size(), 0);
  if (!m.models.empty()) {
    const Model& w = m.models[0];
    for (uint32_t k = 0; k < w.surface_count && w.first_surface + k < own.size(); ++k)
      own[w.first_surface + k] = 1;
  } else {
    std::fill(own.begin(), own.end(), uint8_t{1});
  }
  std::vector<GpuChunk> chunks;
  chunks.reserve(chunk_of.size());
  c.surface.clear();
  c.world.clear();
  c.total = 0;
  for (const auto& [first, count, kind, surface] : chunk_of) {
    GpuChunk g{};
    for (int k = 0; k < 3; ++k) g.lo[k] = 1e30f, g.hi[k] = -1e30f;
    const auto& run = scene.face_runs[surface];
    for (uint32_t i = run[0]; i < run[0] + run[1]; ++i) {
      const DrawVertex& v = scene.vertices[scene.indices[i]];
      for (int k = 0; k < 3; ++k) {
        g.lo[k] = std::min(g.lo[k], v.pos[k]);
        g.hi[k] = std::max(g.hi[k], v.pos[k]);
      }
    }
    g.first = first;
    g.count = count;
    g.info[0] = kind;
    chunks.push_back(g);
    c.surface.push_back(surface);
    c.world.push_back(surface < own.size() ? own[surface] : 0);
    c.total = std::max(c.total, first + count);
  }
  c.chunks = uint32_t(chunks.size());
  const size_t n = std::max<size_t>(chunks.size(), 1);
  c.chunk_buf = r::make_buffer(device, "bsp.cull.chunks", n * sizeof(GpuChunk), BU::Storage);
  if (!chunks.empty())
    device.queue.WriteBuffer(c.chunk_buf, 0, chunks.data(), chunks.size() * sizeof(GpuChunk));
  c.pvs = r::make_buffer(device, "bsp.cull.pvs", ((n + 31) / 32) * 4, BU::Storage);
  c.dst = r::make_buffer(device, "bsp.cull.indices", std::max<size_t>(c.total, 3) * 2 * 4,
                         BU::Storage | BU::Index);
  c.sizes = r::make_buffer(device, "bsp.cull.sizes", n * 4, BU::Storage);
  c.state = r::make_buffer(device, "bsp.cull.state", n * 4, BU::Storage);
  c.args = r::make_buffer(device, "bsp.cull.args", (kStatsAt + kStats) * 4,
                          BU::Storage | BU::Indirect | BU::CopySrc);
  c.params = r::make_buffer(device, "bsp.cull.params", 16, BU::Uniform);
  c.args_reset.assign(kStatsAt + kStats, 0);

  using r::Slot;
  const auto cs = wgpu::ShaderStage::Compute;
  c.layout = r::make_layout(
      device, "bsp.cull",
      {Slot::uniform, Slot::storage_shade, Slot::storage_shade, Slot::storage_shade, Slot::storage,
       Slot::storage, Slot::storage, Slot::storage, Slot::texture_raw},
      cs);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::bsp_cull, "bsp.cull");
  wgpu::ComputePipelineDescriptor cd{};
  cd.layout = r::make_pipeline_layout(device, "bsp.cull", {frame, c.layout});
  cd.compute.module = module;
  const auto add = [&](const char* label, const char* entry) {
    cd.label = label;
    cd.compute.entryPoint = entry;
    return pipelines.add_compute(device, cd);
  };
  c.p_test_early = add("bsp.cull.test_early", "test_early");
  c.p_scan_early = add("bsp.cull.scan_early", "scan_early");
  c.p_copy_early = add("bsp.cull.copy_early", "copy_early");
  c.p_test_late = add("bsp.cull.test_late", "test_late");
  c.p_scan_late = add("bsp.cull.scan_late", "scan_late");
  c.p_copy_late = add("bsp.cull.copy_late", "copy_late");
  r::create_hiz_pipelines(device, pipelines, c.hiz);
  c.src = src;  // the renderer made it with Storage use
  c.group = nullptr;
  c.seen.clear();
  c.bits.assign((n + 31) / 32, 0);
}

void create_bsp_cull_targets(gpu::Device& device, BspCull& c, const r::FrameTargets& targets) {
  r::create_hiz_targets(device, c.hiz, targets);
  c.group = r::make_group(
      device, "bsp.cull", c.layout,
      {c.params, c.chunk_buf, c.pvs, c.src, c.dst, c.sizes, c.state, c.args, c.hiz.view});
}

void prepare_bsp_cull(gpu::Device& device, BspCull& c, const Scene& scene, const double eye[3],
                      bool pyramid, bool pvs) {
  // The eye in the file's units (brush/map.hpp's `to_engine`, undone).
  const float p[3] = {float(eye[0] / kUnitM), float(eye[2] / kUnitM), float(eye[1] / kUnitM)};
  const Map& m = scene.map;
  c.eye_leaf = m.leaves.empty() ? 0 : leaf_at(m, p);
  if (m.leaves.empty() || !pvs)
    c.seen.assign(m.surfaces.size(), 1);
  else
    potentially_visible(m, c.eye_leaf, &c.seen);
  std::fill(c.bits.begin(), c.bits.end(), 0u);
  c.in_set = 0;
  for (uint32_t i = 0; i < c.chunks; ++i) {
    const uint32_t s = c.surface[i];
    if (!c.world[i] || (s < c.seen.size() && c.seen[s])) {
      c.bits[i / 32] |= 1u << (i % 32);
      ++c.in_set;
    }
  }
  device.queue.WriteBuffer(c.pvs, 0, c.bits.data(), c.bits.size() * 4);
  device.queue.WriteBuffer(c.args, 0, c.args_reset.data(), c.args_reset.size() * 4);
  const uint32_t params[4] = {c.chunks, std::max(c.total, 3u), pyramid ? 1u : 0u, 0};
  device.queue.WriteBuffer(c.params, 0, params, sizeof params);
}

void encode_bsp_cull(const BspCull& c, const gpu::PipelineSet& pipelines, r::FrameRecorder& rec,
                     wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame, int phase) {
  const char* name = phase == 0 ? "bsp.cull" : "bsp.cull.late";
  const uint32_t steps[3] = {phase == 0 ? c.p_test_early : c.p_test_late,
                             phase == 0 ? c.p_scan_early : c.p_scan_late,
                             phase == 0 ? c.p_copy_early : c.p_copy_late};
  const uint32_t groups = (c.chunks + 63) / 64;
  // A pass a step: the scan reads what the test wrote, the copy what the scan wrote.
  for (int k = 0; k < 3; ++k) {
    wgpu::ComputePassEncoder pass = rec.compute_part(encoder, name, k, 3);
    if (c.chunks > 0) {
      pass.SetPipeline(pipelines.compute(steps[k]));
      pass.SetBindGroup(0, frame);
      pass.SetBindGroup(1, c.group);
      pass.DispatchWorkgroups(k == 1 ? 1 : std::max(groups, 1u));
      ++rec.current().dispatches;
    }
    pass.End();
  }
}

CullStats read_bsp_cull(gpu::Device& device, const BspCull& c) {
  constexpr uint64_t kAt = uint64_t{kStatsAt} * 4, kBytes = uint64_t{kStats} * 4;
  wgpu::BufferDescriptor desc{};
  desc.label = "bsp.cull.readback";
  desc.size = kBytes;
  desc.usage = BU::MapRead | BU::CopyDst;
  wgpu::Buffer rb = device.device.CreateBuffer(&desc);
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  encoder.CopyBufferToBuffer(c.args, kAt, rb, 0, kBytes);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  uint32_t v[kStats] = {};
  gpu::wait(device, rb.MapAsync(wgpu::MapMode::Read, 0, kBytes, wgpu::CallbackMode::WaitAnyOnly,
                                [](wgpu::MapAsyncStatus, wgpu::StringView) {}));
  if (const void* p = rb.GetConstMappedRange(0, kBytes)) std::memcpy(v, p, kBytes);
  rb.Unmap();
  CullStats s;
  s.chunks = c.chunks;
  s.pvs = v[0];
  s.frustum = v[1];
  s.occluded = v[2];
  s.hidden = v[3];
  s.early = v[4];
  s.late = v[5];
  return s;
}

}  // namespace ge::brush::app
