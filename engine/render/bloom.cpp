// Bloom's sizes, pipelines, chain and pass.
//
// ge:summary Bloom's chain resources, pipelines and pass
// ge:keywords render bloom chain dispatch mips
// ge:skill render
// ge:test tests/render_post_test.cpp
#include "render/bloom.hpp"

#include <algorithm>

#include "render/bloom.wgsl.hpp"

namespace ge::render {

void bloom_size(uint32_t width, uint32_t height, int levels, uint32_t* w0, uint32_t* h0,
                int* count) {
  uint32_t w = std::max(width / 2, 1u), h = std::max(height / 2, 1u);
  *w0 = w;
  *h0 = h;
  int n = 1;
  const int most = std::clamp(levels, 1, kBloomMaxLevels);
  while (n < most && w / 2 >= 2 && h / 2 >= 2) {
    w /= 2;
    h /= 2;
    ++n;
  }
  *count = n;
}

void create_bloom_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines, Bloom& bloom,
                            const BloomSettings& settings) {
  bloom.settings = settings;
  wgpu::SamplerDescriptor sd{};
  sd.label = "bloom.linear";
  sd.addressModeU = sd.addressModeV = sd.addressModeW = wgpu::AddressMode::ClampToEdge;
  sd.magFilter = sd.minFilter = wgpu::FilterMode::Linear;
  bloom.sampler = device.device.CreateSampler(&sd);
  const auto cs = wgpu::ShaderStage::Compute;
  bloom.down_layout =
      make_layout(device, "bloom.down", {Slot::texture, Slot::sampler, Slot::store_2d}, cs);
  bloom.up_layout = make_layout(device, "bloom.up",
                                {Slot::texture, Slot::sampler, Slot::store_2d, Slot::texture}, cs);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::bloom, "bloom");
  wgpu::ComputePipelineDescriptor desc{};
  desc.compute.module = module;
  desc.label = "bloom.first";
  desc.layout = make_pipeline_layout(device, "bloom.down", {bloom.down_layout});
  desc.compute.entryPoint = "first";
  bloom.p_first = pipelines.add_compute(device, desc);
  desc.label = "bloom.down";
  desc.compute.entryPoint = "down";
  bloom.p_down = pipelines.add_compute(device, desc);
  desc.label = "bloom.up";
  desc.layout = make_pipeline_layout(device, "bloom.up", {bloom.up_layout});
  desc.compute.entryPoint = "up";
  bloom.p_up = pipelines.add_compute(device, desc);
}

void create_bloom_targets(gpu::Device& device, Bloom& bloom, uint32_t width, uint32_t height,
                          const wgpu::TextureView inputs[2]) {
  bloom_size(width, height, bloom.settings.levels, &bloom.width, &bloom.height, &bloom.levels);
  const auto usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding |
                     wgpu::TextureUsage::CopySrc;
  const auto n = static_cast<uint32_t>(bloom.levels);
  bloom.down =
      make_texture(device, "bloom.down", bloom.width, bloom.height, 1, kHdrFormat, usage, n);
  bloom.up = make_texture(device, "bloom.up", bloom.width, bloom.height, 1, kHdrFormat, usage,
                          std::max(n - 1, 1u));
  for (uint32_t m = 0; m < n; ++m) {
    wgpu::TextureViewDescriptor vd{};
    vd.baseMipLevel = m;
    vd.mipLevelCount = 1;
    bloom.down_views[m] = bloom.down.CreateView(&vd);
    if (m + 1 < n) bloom.up_views[m] = bloom.up.CreateView(&vd);
  }
  for (int k = 0; k < 2; ++k) {
    bloom.first[k] = make_group(device, "bloom.first", bloom.down_layout,
                                {inputs[k], bloom.sampler, bloom.down_views[0]});
  }
  for (int m = 1; m < bloom.levels; ++m) {
    bloom.downs[m] = make_group(device, "bloom.down", bloom.down_layout,
                                {bloom.down_views[m - 1], bloom.sampler, bloom.down_views[m]});
  }
  // Up level m adds the level below it (up m + 1, or the smallest down level) to down level m.
  for (int m = bloom.levels - 2; m >= 0; --m) {
    const wgpu::TextureView& below =
        m + 1 == bloom.levels - 1 ? bloom.down_views[m + 1] : bloom.up_views[m + 1];
    bloom.ups[m] = make_group(device, "bloom.up", bloom.up_layout,
                              {below, bloom.sampler, bloom.up_views[m], bloom.down_views[m]});
  }
  bloom.view = bloom.levels > 1 ? bloom.up_views[0] : bloom.down_views[0];
}

void encode_bloom(const Bloom& bloom, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                  wgpu::CommandEncoder& encoder, int which) {
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "bloom");
  PassStat& stat = rec.current();
  auto dispatch = [&](int m) {
    const uint32_t w = std::max(bloom.width >> m, 1u), h = std::max(bloom.height >> m, 1u);
    pass.DispatchWorkgroups((w + 7) / 8, (h + 7) / 8);
    ++stat.dispatches;
  };
  pass.SetPipeline(pipelines.compute(bloom.p_first));
  pass.SetBindGroup(0, bloom.first[which]);
  dispatch(0);
  for (int m = 1; m < bloom.levels; ++m) {
    pass.SetPipeline(pipelines.compute(bloom.p_down));
    pass.SetBindGroup(0, bloom.downs[m]);
    dispatch(m);
  }
  for (int m = bloom.levels - 2; m >= 0; --m) {
    pass.SetPipeline(pipelines.compute(bloom.p_up));
    pass.SetBindGroup(0, bloom.ups[m]);
    dispatch(m);
  }
  pass.End();
}

}  // namespace ge::render
