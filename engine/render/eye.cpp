// The eye's pipelines, groups, step and pass.
//
// ge:summary The eye's adaptation: pipelines, the histogram, the step's uniform and the pass
// ge:keywords render eye eye-adaptation histogram dispatch uniform readback
// ge:skill render
// ge:test tests/render_eye_test.cpp
#include "render/eye.hpp"

#include <algorithm>
#include <cmath>

#include "render/eye.wgsl.hpp"
#include "render/eye_adapt.wgsl.hpp"

namespace ge::render {

namespace {

constexpr uint32_t kGridW = 128, kGridH = 72;  // eye.wgsl's
constexpr uint32_t kBins = 64;

struct StepBlock {  // eye_adapt.wgsl's EyeStep
  float timing[4];
  float aim[4];
  float band[4];
};

}  // namespace

void create_eye(gpu::Device& device, gpu::PipelineSet& pipelines, Eye& eye,
                const wgpu::BindGroupLayout& frame, const Sky& sky, const EyeSettings& settings) {
  eye.settings = settings;
  const auto cs = wgpu::ShaderStage::Compute;
  eye.meter_layout =
      make_layout(device, "eye.meter", {Slot::texture, Slot::sampler, Slot::storage}, cs);
  eye.adapt_layout =
      make_layout(device, "eye.adapt", {Slot::storage, Slot::storage, Slot::uniform}, cs);
  wgpu::SamplerDescriptor sd{};
  sd.label = "eye.linear";
  sd.addressModeU = sd.addressModeV = sd.addressModeW = wgpu::AddressMode::ClampToEdge;
  sd.magFilter = sd.minFilter = wgpu::FilterMode::Linear;
  eye.sampler = device.device.CreateSampler(&sd);
  eye.bins = make_buffer(device, "eye.bins", uint64_t{kBins} * 4, wgpu::BufferUsage::Storage);
  eye.step = make_buffer(device, "eye.step", sizeof(StepBlock),
                         wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
  eye.adapt = make_group(device, "eye.adapt", eye.adapt_layout, {eye.bins, sky.light, eye.step});

  wgpu::ComputePipelineDescriptor desc{};
  desc.label = "eye.meter";
  desc.layout = make_pipeline_layout(device, "eye.meter", {frame, eye.meter_layout});
  desc.compute.module = gpu::create_shader(device, wgsl::eye, "eye");
  desc.compute.entryPoint = "meter";
  eye.p_meter = pipelines.add_compute(device, desc);
  desc.label = "eye.adapt";
  desc.layout = make_pipeline_layout(device, "eye.adapt", {eye.adapt_layout});
  desc.compute.module = gpu::create_shader(device, wgsl::eye_adapt, "eye_adapt");
  desc.compute.entryPoint = "adapt";
  eye.p_adapt = pipelines.add_compute(device, desc);
}

wgpu::BindGroup eye_group(gpu::Device& device, const Eye& eye, const wgpu::TextureView& hdr) {
  return make_group(device, "eye.meter", eye.meter_layout, {hdr, eye.sampler, eye.bins});
}

void prepare_eye(gpu::Device& device, Eye& eye, double dt) {
  const EyeSettings& s = eye.settings;
  StepBlock b{};
  b.timing[0] = static_cast<float>(std::max(dt, 0.0));
  b.timing[1] = static_cast<float>(s.to_light_s);
  b.timing[2] = static_cast<float>(s.to_dark_s);
  b.timing[3] = eye.cut ? 1.0f : 0.0f;
  b.aim[0] = static_cast<float>(std::log2(std::max(s.key, 1e-6)));
  b.aim[1] = static_cast<float>(std::max(s.tolerance_ev, 0.0));
  b.aim[2] = static_cast<float>(std::min(s.min_ev, 0.0));
  b.aim[3] = static_cast<float>(std::max(s.max_ev, 0.0));
  b.band[0] = static_cast<float>(std::clamp(s.low, 0.0, 1.0));
  b.band[1] = static_cast<float>(std::clamp(s.high, s.low, 1.0));
  b.band[2] = static_cast<float>(std::clamp(s.strength, 0.0, 1.0));
  device.queue.WriteBuffer(eye.step, 0, &b, sizeof b);
  eye.cut = false;
}

void encode_eye(const Eye& eye, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame,
                const wgpu::BindGroup& group) {
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "eye");
  PassStat& stat = rec.current();
  pass.SetPipeline(pipelines.compute(eye.p_meter));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, group);
  pass.DispatchWorkgroups((kGridW + 15) / 16, (kGridH + 7) / 8);
  pass.SetPipeline(pipelines.compute(eye.p_adapt));
  pass.SetBindGroup(0, eye.adapt);
  pass.DispatchWorkgroups(1);
  stat.dispatches += 2;
  pass.End();
}

EyeReading read_eye(gpu::Device& device, const Sky& sky) {
  const SkyLightReading r = read_sky_light(device, sky);
  return {r.eye[0], r.eye[1], r.eye[2], r.eye[3]};
}

}  // namespace ge::render
