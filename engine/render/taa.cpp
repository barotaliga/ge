// TAA-U's jitter, histories, groups and resolve pass.
//
// ge:summary TAA-U's jitter sequence, history targets and resolve pass
// ge:keywords render taa jitter halton history resolve
// ge:skill render
// ge:test tests/render_draw_test.cpp
#include "render/taa.hpp"

#include <algorithm>
#include <cmath>

#include "render/taa.wgsl.hpp"

namespace ge::render {

namespace {

// taa.wgsl's `TaaParams`.
struct Params {
  float output[4];  // width, height, 1/width, 1/height
  float blend[4];   // alpha, clip gamma, 0, 0
};

double halton(uint32_t i, uint32_t base) {
  double f = 1.0, r = 0.0;
  while (i > 0) {
    f /= base;
    r += f * (i % base);
    i /= base;
  }
  return r;
}

}  // namespace

int taa_phases(double scale) {
  const double s = std::clamp(scale, 0.25, 1.0);
  return std::min(static_cast<int>(std::ceil(8.0 / (s * s) - 1e-9)), kTaaMaxPhases);
}

void taa_jitter(uint64_t frame, int phases, double out[2]) {
  const uint32_t i = static_cast<uint32_t>(frame % static_cast<uint64_t>(std::max(phases, 1))) + 1;
  out[0] = halton(i, 2) - 0.5;
  out[1] = halton(i, 3) - 0.5;
}

uint32_t taa_render_size(uint32_t output, double scale) {
  const double s = std::clamp(scale, 0.25, 1.0);
  return std::max(static_cast<uint32_t>(std::lround(output * s)), 1u);
}

void create_taa(gpu::Device& device, gpu::PipelineSet& pipelines, Taa& taa,
                const wgpu::BindGroupLayout& frame, const TaaSettings& settings) {
  taa.settings = settings;
  taa.layout = make_layout(
      device, "taa",
      {Slot::uniform, Slot::texture, Slot::texture, Slot::depth, Slot::texture, Slot::store_2d},
      wgpu::ShaderStage::Compute);
  taa.params = make_buffer(device, "taa.params", sizeof(Params), wgpu::BufferUsage::Uniform);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::taa, "taa");
  wgpu::ComputePipelineDescriptor desc{};
  desc.label = "taa.resolve";
  desc.layout = make_pipeline_layout(device, "taa.resolve", {frame, taa.layout});
  desc.compute.module = module;
  desc.compute.entryPoint = "resolve";
  taa.p_resolve = pipelines.add_compute(device, desc);
}

void create_taa_targets(gpu::Device& device, Taa& taa, const FrameTargets& targets,
                        uint32_t output_width, uint32_t output_height) {
  taa.width = output_width;
  taa.height = output_height;
  for (int k = 0; k < 2; ++k) {
    taa.history[k] = make_texture(
        device, k ? "taa.history.1" : "taa.history.0", output_width, output_height, 1, kHdrFormat,
        wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding);
    taa.history_views[k] = taa.history[k].CreateView();
  }
  for (int k = 0; k < 2; ++k) {
    taa.groups[k] =
        make_group(device, "taa", taa.layout,
                   {taa.params, targets.hdr_view, targets.motion_view, targets.depth_view,
                    taa.history_views[1 - k], taa.history_views[k]});
  }
  reset_taa(taa);
}

void reset_taa(Taa& taa) { taa.age = 0; }

void encode_taa(gpu::Device& device, Taa& taa, const gpu::PipelineSet& pipelines,
                FrameRecorder& rec, wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame) {
  // A young history is a running mean (the new frame's share 1, 1/2, 1/3 ...) until it reaches
  // alpha; age 0 drops the history altogether.
  Params p{};
  p.output[0] = static_cast<float>(taa.width);
  p.output[1] = static_cast<float>(taa.height);
  p.output[2] = 1.0f / static_cast<float>(taa.width);
  p.output[3] = 1.0f / static_cast<float>(taa.height);
  p.blend[0] = static_cast<float>(std::max(taa.settings.alpha, 1.0 / (taa.age + 1)));
  p.blend[1] = static_cast<float>(kTaaClipGamma);
  device.queue.WriteBuffer(taa.params, 0, &p, sizeof p);
  const int k = 1 - taa.written;
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "taa");
  pass.SetPipeline(pipelines.compute(taa.p_resolve));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, taa.groups[k]);
  pass.DispatchWorkgroups((taa.width + 7) / 8, (taa.height + 7) / 8);
  ++rec.current().dispatches;
  pass.End();
  taa.written = k;
  ++taa.age;
}

const wgpu::TextureView& taa_output(const Taa& taa) { return taa.history_views[taa.written]; }

}  // namespace ge::render
