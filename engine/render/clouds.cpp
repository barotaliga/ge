// The cloud's noise made and read on the CPU, the frame block's rows, the noise's texture, and the
// cloud pass's pipeline, group and encode.
//
// ge:summary The cloud's noise, its rows in the frame block, its texture and its pass
// ge:keywords render clouds cloud-noise value-noise fbm quantile cut cloud_rows encode_clouds
// ge:skill render
// ge:test tests/render_clouds_test.cpp
#include "render/clouds.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "core/libm/libm.hpp"
#include "render/clouds.wgsl.hpp"

namespace ge::render {

namespace {

constexpr int kN = kCloudNoiseSize;

// An integer lattice point's value, 0..1: a 32-bit mix of its wrapped coordinates and octave.
double lattice(int x, int y, int z, int period, uint32_t octave) {
  const auto wrap = [period](int v) {
    return static_cast<uint32_t>(((v % period) + period) % period);
  };
  uint32_t h =
      wrap(x) * 0x8da6b343u ^ wrap(y) * 0xd8163841u ^ wrap(z) * 0xcb1ab31fu ^ octave * 0x165667b1u;
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  h *= 0x297a2d39u;
  h ^= h >> 15;
  return static_cast<double>(h >> 8) / 16777216.0;
}

double quintic(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }

// Value noise of `period` cells across the tile at the tile point (u, v, w), 0..1.
double value_noise(double u, double v, double w, int period, uint32_t octave) {
  const double x = u * period, y = v * period, z = w * period;
  const double fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
  const int ix = static_cast<int>(fx), iy = static_cast<int>(fy), iz = static_cast<int>(fz);
  const double tx = quintic(x - fx), ty = quintic(y - fy), tz = quintic(z - fz);
  double c[2][2];
  for (int j = 0; j < 2; ++j)
    for (int k = 0; k < 2; ++k) {
      const double a = lattice(ix, iy + j, iz + k, period, octave);
      const double b = lattice(ix + 1, iy + j, iz + k, period, octave);
      c[j][k] = a + (b - a) * tx;
    }
  const double c0 = c[0][0] + (c[1][0] - c[0][0]) * ty;
  const double c1 = c[0][1] + (c[1][1] - c[0][1]) * ty;
  return c0 + (c1 - c0) * tz;
}

std::vector<uint8_t> make_noise() {
  // Three octaves at 8, 16 and 32 cells across the tile, each half the last.
  std::vector<double> f(size_t{kN} * kN * kN);
  double lo = 1e9, hi = -1e9;
  for (int z = 0; z < kN; ++z)
    for (int y = 0; y < kN; ++y)
      for (int x = 0; x < kN; ++x) {
        const double u = (x + 0.5) / kN, v = (y + 0.5) / kN, w = (z + 0.5) / kN;
        double sum = 0, weight = 1, total = 0;
        for (uint32_t o = 0; o < 3; ++o) {
          sum += weight * value_noise(u, v, w, kCloudTileCells << o, o);
          total += weight;
          weight *= 0.5;
        }
        const double n = sum / total;
        f[(size_t{static_cast<size_t>(z)} * kN + y) * kN + x] = n;
        lo = std::min(lo, n);
        hi = std::max(hi, n);
      }
  std::vector<uint8_t> out(f.size());
  for (size_t i = 0; i < f.size(); ++i)
    out[i] = static_cast<uint8_t>(std::lround((f[i] - lo) / (hi - lo) * 255.0));
  return out;
}

// How many texels hold each byte, from the top: `above[b]` texels hold more than b.
const std::array<double, 256>& share_above() {
  static const std::array<double, 256> table = [] {
    std::array<double, 257> count{};
    for (uint8_t b : cloud_noise()) count[b] += 1;
    std::array<double, 256> above{};
    double sum = 0;
    for (int b = 255; b >= 0; --b) {
      above[static_cast<size_t>(b)] = sum / static_cast<double>(cloud_noise().size());
      sum += count[static_cast<size_t>(b)];
    }
    return above;
  }();
  return table;
}

double frac(double x) { return x - std::floor(x); }

// cloud_rows' drift and age within the tile, the shader's wind.z and wind.w.
double drift_in_tile(const CloudLayer& l) {
  return frac(l.drift_m / (l.cell_m * std::max(l.stretch, 1e-3) * kCloudTileCells));
}
double age_in_tile(const CloudLayer& l) { return frac(l.evolve / kCloudTileCells); }

}  // namespace

const std::vector<uint8_t>& cloud_noise() {
  static const std::vector<uint8_t> noise = make_noise();
  return noise;
}

double cloud_noise_at(double u, double v, double w) {
  const std::vector<uint8_t>& n = cloud_noise();
  // As a linear sampler with repeat reads it: texel centres at (i + 0.5) / size.
  const double x = u * kN - 0.5, y = v * kN - 0.5, z = w * kN - 0.5;
  const double fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
  const double tx = x - fx, ty = y - fy, tz = z - fz;
  auto at = [&](double i, double j, double k) {
    const auto wrap = [](double c) {
      const int r = static_cast<int>(c) % kN;
      return static_cast<size_t>(r < 0 ? r + kN : r);
    };
    return n[(wrap(k) * kN + wrap(j)) * kN + wrap(i)] / 255.0;
  };
  double c[2][2];
  for (int j = 0; j < 2; ++j)
    for (int k = 0; k < 2; ++k) {
      const double a = at(fx, fy + j, fz + k), b = at(fx + 1, fy + j, fz + k);
      c[j][k] = a + (b - a) * tx;
    }
  const double c0 = c[0][0] + (c[1][0] - c[0][0]) * ty;
  const double c1 = c[0][1] + (c[1][1] - c[0][1]) * ty;
  return c0 + (c1 - c0) * tz;
}

double cloud_cut(double cover) {
  // The byte value with `cover` of the texels above it, between bytes by the share's slope.
  const double want = std::clamp(cover, 0.0, 1.0);
  if (want <= 0) return 1.0;
  if (want >= 1) return 0.0;
  const std::array<double, 256>& above = share_above();
  for (size_t b = 0; b < 255; ++b) {
    const double a0 = above[b], a1 = above[b + 1];
    if (a1 <= want && want <= a0) {
      const double t = a0 > a1 ? (a0 - want) / (a0 - a1) : 0.0;
      return (static_cast<double>(b) + t) / 255.0;
    }
  }
  return 0.0;
}

double cloud_column(const CloudLayer& l, double x, double z) {
  const double c = core::libm::cos(l.heading), s = core::libm::sin(l.heading);
  const double across = x * c - z * s, along = x * s + z * c;
  const double scale = 1.0 / (l.cell_m * kCloudTileCells);
  const double n = cloud_noise_at(
      across * scale, along * scale / std::max(l.stretch, 1e-3) - drift_in_tile(l), age_in_tile(l));
  const double cut = cloud_cut(l.cover);
  return std::clamp((n - cut) / std::max(1.0 - cut, 1e-3), 0.0, 1.0);
}

void cloud_rows(const Clouds& clouds, float info[4], float rows[][4]) {
  const int n = std::clamp(clouds.count, 0, kCloudLayers);
  info[0] = static_cast<float>(n);
  info[1] = info[2] = info[3] = 0;
  for (int i = 0; i < kCloudLayers * 3; ++i) rows[i][0] = rows[i][1] = rows[i][2] = rows[i][3] = 0;
  for (int i = 0; i < n; ++i) {
    const CloudLayer& l = clouds.layers[i];
    const size_t r = static_cast<size_t>(i) * 3;
    float* v0 = rows[r];
    float* v1 = rows[r + 1];
    float* v2 = rows[r + 2];
    v0[0] = static_cast<float>(l.base_m);
    v0[1] = static_cast<float>(std::max(l.top_m, l.base_m + 1.0));
    v0[2] = static_cast<float>(cloud_cut(l.cover));
    v0[3] = static_cast<float>(1.0 / std::max(l.cell_m, 1.0));
    v1[0] = static_cast<float>(core::libm::cos(l.heading));
    v1[1] = static_cast<float>(core::libm::sin(l.heading));
    v1[2] = static_cast<float>(drift_in_tile(l));
    v1[3] = static_cast<float>(age_in_tile(l));
    v2[0] = static_cast<float>(1.0 / std::max(l.stretch, 1e-3));
    v2[1] = static_cast<float>(std::clamp(l.dome, 0.0, 1.0));
    v2[2] = static_cast<float>(std::max(l.density, 0.0));
    v2[3] = static_cast<float>(std::max(l.shadow_tau, 0.0));
  }
}

CloudNoise create_cloud_noise(gpu::Device& device) {
  CloudNoise c;
  const auto n = static_cast<uint32_t>(kN);
  c.texture = make_texture(device, "clouds.noise", n, n, n, wgpu::TextureFormat::R8Unorm,
                           wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst);
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = c.texture;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = n;
  layout.rowsPerImage = n;
  const wgpu::Extent3D size{n, n, n};
  device.queue.WriteTexture(&dst, cloud_noise().data(), cloud_noise().size(), &layout, &size);
  c.view = c.texture.CreateView();
  wgpu::SamplerDescriptor sd{};
  sd.label = "render.linear_repeat";
  sd.addressModeU = sd.addressModeV = sd.addressModeW = wgpu::AddressMode::Repeat;
  sd.magFilter = sd.minFilter = wgpu::FilterMode::Linear;
  c.sampler = device.device.CreateSampler(&sd);
  return c;
}

void create_clouds(gpu::Device& device, gpu::PipelineSet& pipelines, CloudPass& clouds,
                   const wgpu::BindGroupLayout& frame) {
  clouds.layout = make_layout(device, "clouds.depth", {Slot::depth}, wgpu::ShaderStage::Fragment);
  const wgpu::ShaderModule draw = gpu::create_shader(device, wgsl::clouds, "clouds");
  // Premultiplied over what is there: the cloud's light added, the rest kept by its
  // transmittance.
  wgpu::BlendState over{};
  over.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::One,
                wgpu::BlendFactor::OneMinusSrcAlpha};
  over.alpha = over.color;
  wgpu::ColorTargetState colour{};
  colour.format = kHdrFormat;
  colour.blend = &over;
  wgpu::FragmentState fragment{};
  fragment.module = draw;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 1;
  fragment.targets = &colour;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "clouds.draw";
  desc.layout = make_pipeline_layout(device, "clouds.draw", {frame, clouds.layout});
  desc.vertex.module = draw;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.fragment = &fragment;
  clouds.p_draw = pipelines.add_render(device, desc);
}

void bind_cloud_targets(gpu::Device& device, CloudPass& clouds, const FrameTargets& targets) {
  clouds.group = make_group(device, "clouds.depth", clouds.layout, {targets.depth_view});
}

void encode_clouds(const CloudPass& clouds, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                   const wgpu::BindGroup& frame, const FrameView& view) {
  if (view.clouds.count <= 0) {
    // No layer: an empty compute pass keeps the row without loading and storing the target.
    rec.compute(encoder, "clouds").End();
    return;
  }
  wgpu::RenderPassColorAttachment colour{};
  colour.view = targets.hdr_view;
  colour.loadOp = wgpu::LoadOp::Load;
  colour.storeOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &colour;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "clouds");
  pass.SetPipeline(pipelines.render(clouds.p_draw));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, clouds.group);
  pass.Draw(3);
  ++rec.current().draws;
  pass.End();
}

}  // namespace ge::render
