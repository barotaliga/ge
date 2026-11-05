// The heat haze's pass: the looks, a stretch's heat, the staging, the copy and the ribbons.
//
// ge:summary The heat haze's pass: presets, a stretch's heat, staging, the copy, the ribbons
// ge:keywords render haze heat-haze pass staging pipelines scene-copy ribbon refraction
// ge:skill render
// ge:test tests/render_haze_test.cpp
#include "render/haze.hpp"

#include <algorithm>
#include <array>

#include "render/haze.wgsl.hpp"
#include "render/haze_copy.wgsl.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;

// A hot nail's thread: a few centimetres of hot air, a pixel or two of bend on a 1080-line
// screen, a fifth of a second. A rocket motor's wake: a hand's width spreading to an arm's, a few
// pixels of bend, rising as it cools over most of a second.
constexpr std::array<HazeLook, 2> kLooks{{
    {.id = "haze.nail",
     .width_m = 0.06f,
     .spread_mps = 0.1f,
     .bend_mrad = 2.5f,
     .cell_m = 0.04f,
     .rise_mps = 0.15f,
     .life_s = 0.22f},
    {.id = "haze.rocket",
     .width_m = 0.22f,
     .spread_mps = 0.5f,
     .bend_mrad = 4.5f,
     .cell_m = 0.09f,
     .rise_mps = 0.5f,
     .life_s = 0.7f},
}};

constexpr double kHazeAttackS = 0.01;

}  // namespace

std::span<const HazeLook> haze_looks() { return kLooks; }

const HazeLook* haze_look(std::string_view id) {
  for (const HazeLook& l : kLooks)
    if (l.id == id) return &l;
  return nullptr;
}

double haze_level(const HazeTrail& t, double now_s) {
  if (t.look == nullptr) return 0;
  const double age = now_s - t.born_s, life = t.look->life_s;
  if (age < 0 || age >= life || life <= 0) return 0;
  const double up = std::min(age / kHazeAttackS, 1.0);
  const double left = 1 - age / life;
  return up * left * left;
}

bool haze_cold(const HazeTrail& t, double now_s) {
  return t.look == nullptr || now_s - t.born_s >= t.look->life_s;
}

void create_haze(gpu::Device& device, gpu::PipelineSet& pipelines, Haze& haze,
                 const wgpu::BindGroupLayout& frame) {
  haze.staging.assign(kMaxHaze, GpuHazeSeg{});
  haze.params = make_buffer(device, "haze.params", sizeof(GpuHazeParams), BU::Uniform);
  haze.segs =
      make_buffer(device, "haze.segs", uint64_t{kMaxHaze} * sizeof(GpuHazeSeg), BU::Storage);
  haze.copy_layout = make_layout(device, "haze.copy", {Slot::texture}, wgpu::ShaderStage::Fragment);
  {
    const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::haze_copy, "haze.copy");
    wgpu::ColorTargetState colour{};
    colour.format = kHdrFormat;
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "copy_fs";
    fragment.targetCount = 1;
    fragment.targets = &colour;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "haze.copy";
    desc.layout = make_pipeline_layout(device, "haze.copy", {haze.copy_layout});
    desc.vertex.module = module;
    desc.vertex.entryPoint = "copy_vs";
    desc.fragment = &fragment;
    haze.p_copy = pipelines.add_render(device, desc);
  }
  haze.layout =
      make_layout(device, "haze", {Slot::uniform, Slot::storage_read, Slot::depth, Slot::texture},
                  wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
  {
    const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::haze, "haze");
    wgpu::DepthStencilState depth{};
    depth.format = kDepthFormat;
    depth.depthWriteEnabled = false;
    depth.depthCompare = wgpu::CompareFunction::Greater;  // reversed-z: nearer is greater
    wgpu::ColorTargetState colour{};
    colour.format = kHdrFormat;
    colour.writeMask =
        wgpu::ColorWriteMask::Red | wgpu::ColorWriteMask::Green | wgpu::ColorWriteMask::Blue;
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "haze_fs";
    fragment.targetCount = 1;
    fragment.targets = &colour;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "haze.ribbons";
    desc.layout = make_pipeline_layout(device, "haze", {frame, haze.layout});
    desc.vertex.module = module;
    desc.vertex.entryPoint = "haze_vs";
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.depthStencil = &depth;
    desc.fragment = &fragment;
    haze.p_ribbon = pipelines.add_render(device, desc);
  }
}

void bind_haze_targets(gpu::Device& device, Haze& haze, const FrameTargets& targets) {
  haze.scene =
      make_texture(device, "haze.scene", targets.width, targets.height, 1, kHdrFormat,
                   wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding);
  haze.scene_view = haze.scene.CreateView();
  haze.copy_group = make_group(device, "haze.copy", haze.copy_layout, {targets.hdr_view});
  haze.group = make_group(device, "haze", haze.layout,
                          {haze.params, haze.segs, targets.depth_view, haze.scene_view});
}

void stage_haze(Haze& haze, std::span<const HazeTrail> trails, const FrameView& view) {
  if (haze.staging.size() < kMaxHaze) haze.staging.assign(kMaxHaze, GpuHazeSeg{});
  haze.count = 0;
  const double now = view.time_s;
  // The newest kept: walked from the end, laid back in the order they came.
  size_t first = trails.size();
  uint32_t n = 0;
  while (first > 0 && n < kMaxHaze) {
    first--;
    if (haze_level(trails[first], now) > 0) n++;
  }
  for (size_t i = first; i < trails.size(); ++i) {
    const HazeTrail& t = trails[i];
    const double level = haze_level(t, now);
    if (level <= 0 || haze.count >= kMaxHaze) continue;
    const HazeLook& k = *t.look;
    const double age = now - t.born_s;
    GpuHazeSeg& g = haze.staging[haze.count++];
    for (int c = 0; c < 3; ++c) {
      g.a[c] = static_cast<float>(t.a[c] - view.eye[c]);
      g.b[c] = static_cast<float>(t.b[c] - view.eye[c]);
    }
    g.a[3] = static_cast<float>(level);
    g.b[3] = static_cast<float>(0.5 * (k.width_m + k.spread_mps * age));
    g.c[0] = k.bend_mrad * 1e-3f;
    g.c[1] = k.cell_m;
    g.c[2] = static_cast<float>(k.rise_mps * age);
    g.c[3] = static_cast<float>(t.seed % 1024u) / 1024.0f;
  }
  haze.staged.info[0] = static_cast<float>(haze.count);
  haze.staged.info[1] = static_cast<float>(view.near_m);
  haze.staged.info[2] = static_cast<float>(view.time_s);
  haze.staged.info[3] = 0;
}

void prepare_haze(gpu::Device& device, Haze& haze, std::span<const HazeTrail> trails,
                  const FrameView& view) {
  stage_haze(haze, trails, view);
  device.queue.WriteBuffer(haze.params, 0, &haze.staged, sizeof haze.staged);
  if (haze.count > 0)
    device.queue.WriteBuffer(haze.segs, 0, haze.staging.data(),
                             uint64_t{haze.count} * sizeof(GpuHazeSeg));
}

void encode_haze(const Haze& haze, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                 wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                 const wgpu::BindGroup& frame) {
  if (haze.count == 0) {
    rec.compute(encoder, "haze.copy").End();
    rec.compute(encoder, "haze").End();
    return;
  }
  {
    wgpu::RenderPassColorAttachment colour{};
    colour.view = haze.scene_view;
    colour.loadOp = wgpu::LoadOp::Clear;
    colour.storeOp = wgpu::StoreOp::Store;
    wgpu::RenderPassDescriptor desc{};
    desc.colorAttachmentCount = 1;
    desc.colorAttachments = &colour;
    wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "haze.copy");
    pass.SetPipeline(pipelines.render(haze.p_copy));
    pass.SetBindGroup(0, haze.copy_group);
    pass.Draw(3);
    ++rec.current().draws;
    pass.End();
  }
  wgpu::RenderPassColorAttachment colour{};
  colour.view = targets.hdr_view;
  colour.loadOp = wgpu::LoadOp::Load;
  colour.storeOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDepthStencilAttachment depth{};
  depth.view = targets.depth_view;
  depth.depthReadOnly = true;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &colour;
  desc.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "haze");
  pass.SetPipeline(pipelines.render(haze.p_ribbon));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, haze.group);
  pass.Draw(6, haze.count);
  ++rec.current().draws;
  pass.End();
}

}  // namespace ge::render
