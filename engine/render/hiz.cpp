// The Hi-Z pyramid's sizes, pipelines, groups and reduction, its CPU reference and readbacks.
//
// ge:summary The Hi-Z pyramid's resources, reduction pass, CPU reference and readbacks
// ge:keywords render hi-z depth-pyramid reduction readback
// ge:skill render
// ge:test tests/render_draw_test.cpp
#include "render/hiz.hpp"

#include <algorithm>
#include <string>

#include "render/hiz.wgsl.hpp"

namespace ge::render {

namespace {

// As hiz.wgsl's `span`: the source texels output texel i covers, inclusive.
void span(uint32_t i, uint32_t src, uint32_t dst, uint32_t* lo, uint32_t* hi) {
  *lo = i * src / dst;
  *hi = std::min(((i + 1) * src + dst - 1) / dst - 1, src - 1);
}

std::vector<float> reduce(const std::vector<float>& src, uint32_t sw, uint32_t sh, uint32_t dw,
                          uint32_t dh) {
  std::vector<float> out(size_t{dw} * dh);
  for (uint32_t y = 0; y < dh; ++y) {
    uint32_t y0, y1;
    span(y, sh, dh, &y0, &y1);
    for (uint32_t x = 0; x < dw; ++x) {
      uint32_t x0, x1;
      span(x, sw, dw, &x0, &x1);
      float m = 1.0f;
      for (uint32_t v = y0; v <= y1; ++v) {
        for (uint32_t u = x0; u <= x1; ++u) m = std::min(m, src[size_t{v} * sw + u]);
      }
      out[size_t{y} * dw + x] = m;
    }
  }
  return out;
}

}  // namespace

void hiz_size(uint32_t width, uint32_t height, uint32_t* mip0_w, uint32_t* mip0_h, int* mips) {
  uint32_t w = std::max(width / 2, 1u), h = std::max(height / 2, 1u);
  *mip0_w = w;
  *mip0_h = h;
  int n = 1;
  while ((w > 1 || h > 1) && n < kHizMaxMips) {
    w = std::max(w / 2, 1u);
    h = std::max(h / 2, 1u);
    ++n;
  }
  *mips = n;
}

void create_hiz_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines, Hiz& hiz) {
  hiz.first_layout =
      make_layout(device, "hiz.first", {Slot::depth, Slot::store_r32}, wgpu::ShaderStage::Compute);
  hiz.next_layout = make_layout(device, "hiz.next", {Slot::texture_raw, Slot::store_r32},
                                wgpu::ShaderStage::Compute);
  const wgpu::BindGroupLayout none =
      make_layout(device, "hiz.none", {}, wgpu::ShaderStage::Compute);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::hiz, "hiz");
  wgpu::ComputePipelineDescriptor desc{};
  desc.label = "hiz.first";
  desc.layout = make_pipeline_layout(device, "hiz.first", {hiz.first_layout});
  desc.compute.module = module;
  desc.compute.entryPoint = "first";
  hiz.p_first = pipelines.add_compute(device, desc);
  desc.label = "hiz.next";
  desc.layout = make_pipeline_layout(device, "hiz.next", {none, hiz.next_layout});
  desc.compute.entryPoint = "next";
  hiz.p_next = pipelines.add_compute(device, desc);
}

void create_hiz_targets(gpu::Device& device, Hiz& hiz, const FrameTargets& targets) {
  hiz_size(targets.width, targets.height, &hiz.width, &hiz.height, &hiz.mips);
  hiz.pyramid = make_texture(device, "hiz", hiz.width, hiz.height, 1, wgpu::TextureFormat::R32Float,
                             wgpu::TextureUsage::StorageBinding |
                                 wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc,
                             static_cast<uint32_t>(hiz.mips));
  hiz.view = hiz.pyramid.CreateView();
  for (int m = 0; m < hiz.mips; ++m) {
    wgpu::TextureViewDescriptor vd{};
    vd.baseMipLevel = static_cast<uint32_t>(m);
    vd.mipLevelCount = 1;
    hiz.mip_views[m] = hiz.pyramid.CreateView(&vd);
  }
  hiz.groups[0] =
      make_group(device, "hiz.first", hiz.first_layout, {targets.depth_view, hiz.mip_views[0]});
  for (int m = 1; m < hiz.mips; ++m) {
    hiz.groups[m] =
        make_group(device, "hiz.next", hiz.next_layout, {hiz.mip_views[m - 1], hiz.mip_views[m]});
  }
}

void encode_hiz(const Hiz& hiz, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                wgpu::CommandEncoder& encoder) {
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "hiz");
  PassStat& stat = rec.current();
  uint32_t w = hiz.width, h = hiz.height;
  for (int m = 0; m < hiz.mips; ++m) {
    pass.SetPipeline(pipelines.compute(m == 0 ? hiz.p_first : hiz.p_next));
    pass.SetBindGroup(m == 0 ? 0 : 1, hiz.groups[m]);
    pass.DispatchWorkgroups((w + 7) / 8, (h + 7) / 8);
    ++stat.dispatches;
    w = std::max(w / 2, 1u);
    h = std::max(h / 2, 1u);
  }
  pass.End();
}

std::vector<std::vector<float>> hiz_reduce(const std::vector<float>& depth, uint32_t width,
                                           uint32_t height) {
  uint32_t w, h;
  int mips;
  hiz_size(width, height, &w, &h, &mips);
  std::vector<std::vector<float>> out;
  out.push_back(reduce(depth, width, height, w, h));
  for (int m = 1; m < mips; ++m) {
    const uint32_t nw = std::max(w / 2, 1u), nh = std::max(h / 2, 1u);
    out.push_back(reduce(out.back(), w, h, nw, nh));
    w = nw;
    h = nh;
  }
  return out;
}

std::vector<float> read_hiz(gpu::Device& device, const Hiz& hiz, int mip) {
  const uint32_t w = std::max(hiz.width >> mip, 1u), h = std::max(hiz.height >> mip, 1u);
  return read_red(device, hiz.pyramid, static_cast<uint32_t>(mip), w, h,
                  wgpu::TextureFormat::R32Float);
}

std::vector<float> read_depth(gpu::Device& device, const FrameTargets& targets) {
  return read_red(device, targets.depth, 0, targets.width, targets.height, kDepthFormat);
}

}  // namespace ge::render
