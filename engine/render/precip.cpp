// What falls round the eye: the particles, the pipelines and groups, the frame's upload, the two
// passes and the read-back.
//
// ge:summary What falls round the eye: the particles' box, the sim and draw pipelines and passes
// ge:keywords render precip snowfall rain splash particles box wrap sim draw readback
// ge:skill render
// ge:test tests/render_precip_test.cpp
#include "render/precip.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/prng.hpp"
#include "render/precip.wgsl.hpp"
#include "render/precip_sim.wgsl.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;
constexpr uint32_t kGroup = 64;  // precip_sim.wgsl's workgroup size

// precip_types.wgsl's PrecipParams.
struct Params {
  float eye[4];
  float wind[4];
  float step[4];
  float fall[4];
  uint32_t info[4];
  float splash[4];
};

}  // namespace

void create_precip(gpu::Device& device, gpu::PipelineSet& pipelines, Precip& sf,
                   const wgpu::BindGroupLayout& frame, const Terrain& terrain,
                   const PrecipSettings& settings) {
  sf.settings = settings;
  sf.settings.particles = std::max(settings.particles, kGroup);
  // Spread through the box round the origin, each with its own phase.
  core::Rng rng = core::make_rng(0x5e0f1a4eu);
  std::vector<float> parts(size_t{sf.settings.particles} * 4);
  const double half = 0.5 * sf.settings.box;
  for (size_t i = 0; i < parts.size(); i += 4) {
    for (size_t k = 0; k < 3; ++k) parts[i + k] = static_cast<float>(core::range(rng, -half, half));
    parts[i + 3] = static_cast<float>(core::range(rng, 0, 6.283185307179586));
  }
  sf.particles = make_buffer(device, "precip.particles", parts.size() * sizeof(float),
                             BU::Storage | BU::CopySrc);
  device.queue.WriteBuffer(sf.particles, 0, parts.data(), parts.size() * sizeof(float));
  sf.params = make_buffer(device, "precip.params", sizeof(Params), BU::Uniform);

  sf.sim_layout =
      make_layout(device, "precip.sim", {Slot::uniform, Slot::storage}, wgpu::ShaderStage::Compute);
  wgpu::ComputePipelineDescriptor cd{};
  cd.label = "precip.sim";
  cd.layout = make_pipeline_layout(device, "precip.sim", {sf.sim_layout});
  cd.compute.module = gpu::create_shader(device, wgsl::precip_sim, "precip_sim");
  cd.compute.entryPoint = "sim";
  sf.p_sim = pipelines.add_compute(device, cd);
  sf.sim_group = make_group(device, "precip.sim", sf.sim_layout, {sf.params, sf.particles});

  const auto vf = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  sf.light_layout =
      make_layout(device, "precip.light", {Slot::uniform, Slot::texture, Slot::texture}, vf);
  sf.light_group = make_group(device, "precip.light", sf.light_layout,
                              {terrain.params, terrain.horizon.views[0], terrain.horizon.views[1]});
  sf.draw_layout =
      make_layout(device, "precip.draw", {Slot::uniform, Slot::storage_read, Slot::depth}, vf);

  const wgpu::ShaderModule draw = gpu::create_shader(device, wgsl::precip, "precip");
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
  desc.label = "precip.draw";
  desc.layout =
      make_pipeline_layout(device, "precip.draw", {frame, sf.light_layout, sf.draw_layout});
  desc.vertex.module = draw;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.depthStencil = &depth;
  desc.fragment = &fragment;
  sf.p_draw = pipelines.add_render(device, desc);
}

void bind_precip_targets(gpu::Device& device, Precip& sf, const FrameTargets& targets) {
  sf.draw_group = make_group(device, "precip.draw", sf.draw_layout,
                             {sf.params, sf.particles, targets.depth_view});
}

uint32_t particles_drawn(const PrecipSettings& settings, double fall) {
  const double share = std::clamp(fall, 0.0, 1.0);
  return static_cast<uint32_t>(std::lround(share * settings.particles));
}

uint32_t splashes_drawn(const PrecipSettings& settings, double splash) {
  const double share = std::clamp(splash, 0.0, 1.0);
  return static_cast<uint32_t>(std::lround(share * settings.splashes));
}

void prepare_precip(gpu::Device& device, Precip& sf, const PrecipFrame& at) {
  sf.drawn = particles_drawn(sf.settings, at.fall);
  sf.splashed = sf.drawn > 0 ? splashes_drawn(sf.settings, at.splash) : 0;
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
  p.fall[0] = static_cast<float>(std::clamp(at.rain, 0.0, 1.0));
  p.fall[1] = static_cast<float>(std::max(at.drop_speed, 0.0));
  p.fall[2] = static_cast<float>(std::max(at.swirl, 0.0));
  p.fall[3] = sf.settings.drop;
  p.info[0] = sf.settings.particles;
  p.info[1] = sf.drawn;
  p.info[2] = sf.splashed;
  p.splash[0] = sf.settings.splash_reach;
  p.splash[1] = sf.settings.splash_life;
  p.splash[2] = sf.settings.splash_size;
  device.queue.WriteBuffer(sf.params, 0, &p, sizeof p);
}

void encode_precip_sim(const Precip& sf, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                       wgpu::CommandEncoder& encoder) {
  // Only the particles drawn fly: the rest lie where they were, and the box's wrap brings each
  // back round the eye the step it is drawn again. A clear day dispatches nothing.
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "precip.sim");
  if (sf.drawn > 0) {
    pass.SetPipeline(pipelines.compute(sf.p_sim));
    pass.SetBindGroup(0, sf.sim_group);
    pass.DispatchWorkgroups((sf.drawn + kGroup - 1) / kGroup);
    ++rec.current().dispatches;
  }
  pass.End();
}

void encode_precip(const Precip& sf, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                   const wgpu::BindGroup& frame) {
  if (sf.drawn == 0) {
    // Nothing falls: an empty compute pass keeps the row without loading and storing the targets.
    rec.compute(encoder, "precip").End();
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
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "precip");
  pass.SetPipeline(pipelines.render(sf.p_draw));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, sf.light_group);
  pass.SetBindGroup(2, sf.draw_group);
  pass.Draw(6, sf.drawn + sf.splashed);
  ++rec.current().draws;
  pass.End();
}

std::vector<float> read_particles(gpu::Device& device, const Precip& sf) {
  const uint64_t bytes = uint64_t{sf.settings.particles} * 4 * sizeof(float);
  std::vector<float> out(size_t{sf.settings.particles} * 4);
  wgpu::BufferDescriptor desc{};
  desc.label = "precip.readback";
  desc.size = bytes;
  desc.usage = BU::MapRead | BU::CopyDst;
  wgpu::Buffer rb = device.device.CreateBuffer(&desc);
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  encoder.CopyBufferToBuffer(sf.particles, 0, rb, 0, bytes);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  gpu::wait(device, rb.MapAsync(wgpu::MapMode::Read, 0, bytes, wgpu::CallbackMode::WaitAnyOnly,
                                [](wgpu::MapAsyncStatus, wgpu::StringView) {}));
  if (const void* p = rb.GetConstMappedRange(0, bytes)) std::memcpy(out.data(), p, bytes);
  rb.Unmap();
  return out;
}

}  // namespace ge::render
