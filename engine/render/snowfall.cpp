// The falling snow's flakes, pipelines and groups, the frame's upload, the two passes and the
// read-back.
//
// ge:summary The falling snow: the flakes' box, the sim and draw pipelines and passes
// ge:keywords render snowfall flakes box wrap sim draw readback
// ge:skill render
// ge:test tests/render_snowfall_test.cpp
#include "render/snowfall.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/prng.hpp"
#include "render/snowfall.wgsl.hpp"
#include "render/snowfall_sim.wgsl.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;
constexpr uint32_t kGroup = 64;  // snowfall_sim.wgsl's workgroup size

// snowfall_types.wgsl's SnowfallParams.
struct Params {
  float eye[4];
  float wind[4];
  float step[4];
  uint32_t info[4];
};

}  // namespace

void create_snowfall(gpu::Device& device, gpu::PipelineSet& pipelines, Snowfall& sf,
                     const wgpu::BindGroupLayout& frame, const Terrain& terrain,
                     const SnowfallSettings& settings) {
  sf.settings = settings;
  sf.settings.flakes = std::max(settings.flakes, kGroup);
  // Spread through the box round the origin, each with its own phase.
  core::Rng rng = core::make_rng(0x5e0f1a4eu);
  std::vector<float> flakes(size_t{sf.settings.flakes} * 4);
  const double half = 0.5 * sf.settings.box;
  for (size_t i = 0; i < flakes.size(); i += 4) {
    for (size_t k = 0; k < 3; ++k)
      flakes[i + k] = static_cast<float>(core::range(rng, -half, half));
    flakes[i + 3] = static_cast<float>(core::range(rng, 0, 6.283185307179586));
  }
  sf.flakes = make_buffer(device, "snowfall.flakes", flakes.size() * sizeof(float),
                          BU::Storage | BU::CopySrc);
  device.queue.WriteBuffer(sf.flakes, 0, flakes.data(), flakes.size() * sizeof(float));
  sf.params = make_buffer(device, "snowfall.params", sizeof(Params), BU::Uniform);

  sf.sim_layout = make_layout(device, "snowfall.sim", {Slot::uniform, Slot::storage},
                              wgpu::ShaderStage::Compute);
  wgpu::ComputePipelineDescriptor cd{};
  cd.label = "snowfall.sim";
  cd.layout = make_pipeline_layout(device, "snowfall.sim", {sf.sim_layout});
  cd.compute.module = gpu::create_shader(device, wgsl::snowfall_sim, "snowfall_sim");
  cd.compute.entryPoint = "sim";
  sf.p_sim = pipelines.add_compute(device, cd);
  sf.sim_group = make_group(device, "snowfall.sim", sf.sim_layout, {sf.params, sf.flakes});

  const auto vf = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  sf.light_layout =
      make_layout(device, "snowfall.light", {Slot::uniform, Slot::texture, Slot::texture}, vf);
  sf.light_group = make_group(device, "snowfall.light", sf.light_layout,
                              {terrain.params, terrain.horizon.views[0], terrain.horizon.views[1]});
  sf.draw_layout =
      make_layout(device, "snowfall.draw", {Slot::uniform, Slot::storage_read, Slot::depth}, vf);

  const wgpu::ShaderModule draw = gpu::create_shader(device, wgsl::snowfall, "snowfall");
  wgpu::BlendState over{};
  over.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::One,
                wgpu::BlendFactor::OneMinusSrcAlpha};
  over.alpha = over.color;
  wgpu::BlendState most{};
  most.color = {wgpu::BlendOperation::Max, wgpu::BlendFactor::One, wgpu::BlendFactor::One};
  most.alpha = most.color;
  wgpu::ColorTargetState colours[2]{};
  colours[0].format = kHdrFormat;
  colours[0].blend = &over;
  colours[1].format = kMotionFormat;
  colours[1].blend = &most;
  colours[1].writeMask = wgpu::ColorWriteMask::Blue;
  wgpu::FragmentState fragment{};
  fragment.module = draw;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 2;
  fragment.targets = colours;
  wgpu::DepthStencilState depth{};
  depth.format = kDepthFormat;
  depth.depthWriteEnabled = false;
  depth.depthCompare = wgpu::CompareFunction::Greater;  // reversed-z: nearer is greater
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "snowfall.draw";
  desc.layout =
      make_pipeline_layout(device, "snowfall.draw", {frame, sf.light_layout, sf.draw_layout});
  desc.vertex.module = draw;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.depthStencil = &depth;
  desc.fragment = &fragment;
  sf.p_draw = pipelines.add_render(device, desc);
}

void bind_snowfall_targets(gpu::Device& device, Snowfall& sf, const FrameTargets& targets) {
  sf.draw_group = make_group(device, "snowfall.draw", sf.draw_layout,
                             {sf.params, sf.flakes, targets.depth_view});
}

uint32_t flakes_drawn(const SnowfallSettings& settings, double fall) {
  const double share = std::clamp(fall, 0.0, 1.0);
  return static_cast<uint32_t>(std::lround(share * settings.flakes));
}

void prepare_snowfall(gpu::Device& device, Snowfall& sf, const SnowfallFrame& at) {
  sf.drawn = flakes_drawn(sf.settings, at.fall);
  Params p{};
  for (int k = 0; k < 3; ++k) {
    p.eye[k] = static_cast<float>(at.eye[k]);
    p.wind[k] = static_cast<float>(at.wind[k]);
  }
  p.eye[3] = sf.settings.box;
  p.wind[3] = static_cast<float>(std::clamp(at.dt, 0.0, 0.1));
  // The flutter's clock, kept within a day so a float holds its fraction.
  p.step[0] = static_cast<float>(std::fmod(at.time, 86400.0));
  p.step[1] = static_cast<float>(at.near_m);
  p.step[2] = sf.settings.soft_m;
  p.step[3] = sf.settings.size;
  p.info[0] = sf.settings.flakes;
  p.info[1] = sf.drawn;
  device.queue.WriteBuffer(sf.params, 0, &p, sizeof p);
}

void encode_snowfall_sim(const Snowfall& sf, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                         wgpu::CommandEncoder& encoder) {
  // Only the flakes drawn fly: the rest lie where they were, and the box's wrap brings each back
  // round the eye the step it is drawn again. A clear day dispatches nothing.
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "snowfall.sim");
  if (sf.drawn > 0) {
    pass.SetPipeline(pipelines.compute(sf.p_sim));
    pass.SetBindGroup(0, sf.sim_group);
    pass.DispatchWorkgroups((sf.drawn + kGroup - 1) / kGroup);
    ++rec.current().dispatches;
  }
  pass.End();
}

void encode_snowfall(const Snowfall& sf, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                     wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                     const wgpu::BindGroup& frame) {
  if (sf.drawn == 0) {
    // Nothing falls: an empty compute pass keeps the row without loading and storing the targets.
    rec.compute(encoder, "snowfall").End();
    return;
  }
  wgpu::RenderPassColorAttachment colours[2]{};
  colours[0].view = targets.hdr_view;
  colours[1].view = targets.motion_view;
  for (auto& c : colours) {
    c.loadOp = wgpu::LoadOp::Load;
    c.storeOp = wgpu::StoreOp::Store;
  }
  wgpu::RenderPassDepthStencilAttachment depth{};
  depth.view = targets.depth_view;
  depth.depthReadOnly = true;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 2;
  desc.colorAttachments = colours;
  desc.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "snowfall");
  pass.SetPipeline(pipelines.render(sf.p_draw));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, sf.light_group);
  pass.SetBindGroup(2, sf.draw_group);
  pass.Draw(6, sf.drawn);
  ++rec.current().draws;
  pass.End();
}

std::vector<float> read_flakes(gpu::Device& device, const Snowfall& sf) {
  const uint64_t bytes = uint64_t{sf.settings.flakes} * 4 * sizeof(float);
  std::vector<float> out(size_t{sf.settings.flakes} * 4);
  wgpu::BufferDescriptor desc{};
  desc.label = "snowfall.readback";
  desc.size = bytes;
  desc.usage = BU::MapRead | BU::CopyDst;
  wgpu::Buffer rb = device.device.CreateBuffer(&desc);
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  encoder.CopyBufferToBuffer(sf.flakes, 0, rb, 0, bytes);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  gpu::wait(device, rb.MapAsync(wgpu::MapMode::Read, 0, bytes, wgpu::CallbackMode::WaitAnyOnly,
                                [](wgpu::MapAsyncStatus, wgpu::StringView) {}));
  if (const void* p = rb.GetConstMappedRange(0, bytes)) std::memcpy(out.data(), p, bytes);
  rb.Unmap();
  return out;
}

}  // namespace ge::render
