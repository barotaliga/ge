// The fog in areas: the froxel textures, the pipelines and groups, the volumes in reach, the
// frame's upload, the two passes and the read-back.
//
// ge:summary The fog in areas: froxel textures, inject and integrate passes, volumes in reach
// ge:keywords render fog froxel volume inject integrate reach upload readback
// ge:skill render
// ge:test tests/render_fog_test.cpp
#include "render/fog.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "render/fog.wgsl.hpp"
#include "render/fog_integrate.wgsl.hpp"
#include "render/shadows.hpp"
#include "render/terrain.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;
using TU = wgpu::TextureUsage;
constexpr uint32_t kInjectGroup = 4;     // fog.wgsl's workgroup, each way
constexpr uint32_t kIntegrateGroup = 8;  // fog_integrate.wgsl's, across and down

// fog.wgsl's FogParams.
struct Params {
  uint32_t grid[4];
  float look[4];
};

// fog.wgsl's FogVol.
struct Volume {
  float centre[4];
  float half[4];
  float turn[4];
  float albedo[4];
  float glow[4];
  float drift[4];
  float light[4];  // the open sky's share; 0, 0, 0
};

double wrap01(double v) { return v - std::floor(v); }

uint32_t groups(uint32_t n, uint32_t size) { return (n + size - 1) / size; }

}  // namespace

double fog_slice_far(const FogSettings& settings, uint32_t slice) {
  const double s = static_cast<double>(slice) / std::max(settings.slices, 1u);
  return settings.range * s * s;
}

void create_fog(gpu::Device& device, gpu::PipelineSet& pipelines, Fog& fog,
                const wgpu::BindGroupLayout& frame, const Terrain* terrain,
                const FogSettings& settings) {
  fog.settings = settings;
  fog.settings.width = std::max(settings.width, 1u);
  fog.settings.height = std::max(settings.height, 1u);
  fog.settings.slices = std::max(settings.slices, 2u);
  const auto f16 = wgpu::TextureFormat::RGBA16Float;
  const FogSettings& s = fog.settings;
  for (int k = 0; k < 2; ++k) {
    fog.media[k] = make_texture(device, k == 0 ? "fog.media0" : "fog.media1", s.width, s.height,
                                s.slices, f16, TU::StorageBinding | TU::TextureBinding);
    fog.media_view[k] = fog.media[k].CreateView();
  }
  fog.volume = make_texture(device, "fog.volume", s.width, s.height, s.slices, f16,
                            TU::StorageBinding | TU::TextureBinding | TU::CopySrc);
  fog.volume_view = fog.volume.CreateView();
  fog.params = make_buffer(device, "fog.params", sizeof(Params), BU::Uniform);
  fog.volumes = make_buffer(device, "fog.volumes", sizeof(Volume) * kMaxFogVolumes, BU::Storage);

  const auto cs = wgpu::ShaderStage::Compute;
  fog.inject_layout =
      make_layout(device, "fog.inject",
                  {Slot::uniform, Slot::storage_read, Slot::texture_3d, Slot::store_3d}, cs);
  fog.ground_layout = make_layout(
      device, "fog.ground",
      {Slot::uniform, Slot::texture, Slot::texture, Slot::texture_raw, Slot::texture}, cs);
  fog.integrate_layout =
      make_layout(device, "fog.integrate", {Slot::uniform, Slot::texture_3d, Slot::store_3d}, cs);
  // The casters' group is render/shadows.hpp's layout, whoever made it.
  fog.shadow_layout = sun_shadow_layout(device);
  fog.shadow_group = no_sun_shadows(device);

  // The ground's horizon maps, or none: a zero block reads as no horizon (terrain_common.wgsl's
  // `ground_shadow` returns 1 when the maps are off).
  fog.ground = terrain != nullptr;
  if (terrain) {
    fog.ground_group =
        make_group(device, "fog.ground", fog.ground_layout,
                   {terrain->params, terrain->horizon.views[0], terrain->horizon.views[1],
                    terrain->height.CreateView(), terrain->albedo.CreateView()});
  } else {
    const wgpu::Buffer zero =
        make_buffer(device, "fog.ground.none", sizeof(Terrain::Params), BU::Uniform);
    const Terrain::Params none{};
    device.queue.WriteBuffer(zero, 0, &none, sizeof none);
    const wgpu::Texture flat =
        make_texture(device, "fog.ground.flat", 1, 1, 1, f16, TU::TextureBinding);
    const wgpu::TextureView v = flat.CreateView();
    const wgpu::Texture low = make_texture(device, "fog.ground.low", 1, 1, 1,
                                           wgpu::TextureFormat::R32Float, TU::TextureBinding);
    fog.ground_group =
        make_group(device, "fog.ground", fog.ground_layout, {zero, v, v, low.CreateView(), v});
  }

  wgpu::ComputePipelineDescriptor cd{};
  cd.label = "fog.inject";
  cd.layout = make_pipeline_layout(
      device, "fog.inject", {frame, fog.inject_layout, fog.ground_layout, fog.shadow_layout});
  cd.compute.module = gpu::create_shader(device, wgsl::fog, "fog");
  cd.compute.entryPoint = "inject";
  fog.p_inject = pipelines.add_compute(device, cd);
  wgpu::ComputePipelineDescriptor id{};
  id.label = "fog.integrate";
  id.layout = make_pipeline_layout(device, "fog.integrate", {fog.integrate_layout});
  id.compute.module = gpu::create_shader(device, wgsl::fog_integrate, "fog_integrate");
  id.compute.entryPoint = "integrate";
  fog.p_integrate = pipelines.add_compute(device, id);

  for (int k = 0; k < 2; ++k) {
    fog.inject_group[k] =
        make_group(device, "fog.inject", fog.inject_layout,
                   {fog.params, fog.volumes, fog.media_view[1 - k], fog.media_view[k]});
    fog.integrate_group[k] = make_group(device, "fog.integrate", fog.integrate_layout,
                                        {fog.params, fog.media_view[k], fog.volume_view});
  }
  fog.staged.assign(sizeof(Volume) / sizeof(float) * kMaxFogVolumes, 0.0f);
  fog.count = 0;
  fog.current = 0;
  fog.had_last = false;
}

void bind_fog_shadows(Fog& fog, const wgpu::BindGroup& shadows) { fog.shadow_group = shadows; }

std::vector<size_t> fog_in_reach(const FogSettings& settings, const FrameView& view,
                                 std::span<const FogVolume> volumes) {
  std::vector<std::pair<double, size_t>> near;
  for (size_t i = 0; i < volumes.size(); ++i) {
    const FogVolume& v = volumes[i];
    if (!(v.density > 0)) continue;
    const double radius =
        std::sqrt(v.half[0] * v.half[0] + v.half[1] * v.half[1] + v.half[2] * v.half[2]);
    const double dx = v.centre[0] - view.eye[0], dy = v.centre[1] - view.eye[1],
                 dz = v.centre[2] - view.eye[2];
    const double gap = std::sqrt(dx * dx + dy * dy + dz * dz) - radius;
    if (gap < settings.range) near.emplace_back(gap, i);
  }
  std::sort(near.begin(), near.end());
  std::vector<size_t> out;
  for (size_t k = 0; k < near.size() && k < kMaxFogVolumes; ++k) out.push_back(near[k].second);
  return out;
}

LocalLight fog_glow_light(const FogVolume& v, double floor_y) {
  LocalLight l;
  // Its luminance (Rec. 709's weights) sets the candela; the colour keeps a luminance of 1.
  const double luma = 0.2126 * v.glow[0] + 0.7152 * v.glow[1] + 0.0722 * v.glow[2];
  for (int a = 0; a < 3; ++a) {
    l.position[a] = v.centre[a];
    l.colour[a] = luma > 0 ? static_cast<float>(std::max(v.glow[a], 0.0) / luma) : 1.0f;
  }
  // A ground-hugging volume's heart is half its height over the ground.
  if (v.hug_ground) l.position[1] = floor_y + v.half[1];
  const double hx = std::max(v.half[0], 1e-3), hy = std::max(v.half[1], 1e-3);
  const double hz = std::max(v.half[2], 1e-3);
  const double size = 8 * hx * hy * hz;
  const double space = v.shape == FogVolume::Shape::ellipsoid ? size * 3.14159265358979 / 6 : size;
  // The mean extinction: the falloff over its height (2 hy) and the noise's carving (fog.wgsl's
  // density times 1 - noise / 2 at its mean).
  const double f = std::max(v.falloff, 0.0) * 2 * hy;
  const double rise = f > 1e-6 ? (1 - std::exp(-f)) / f : 1.0;
  const double sigma = std::max(v.density, 0.0) * rise * (1 - 0.5 * std::clamp(v.noise, 0.0, 1.0));
  // What escapes a thick volume: the glow from its far side is taken out on the way.
  const double tau = sigma * std::min({hx, hy, hz});
  const double escape = tau > 1e-6 ? (1 - std::exp(-tau)) / tau : 1.0;
  l.candela = luma > 0 ? static_cast<float>(luma * sigma * space * escape) : 0.0f;
  // Its reach: where it has fallen to a third of a lux (moonlight's), as a flame's.
  l.range_m =
      static_cast<float>(std::clamp(std::sqrt(l.candela / 0.3), std::max({hx, hy, hz}), 60.0));
  // The light comes from the whole volume: softened over its size.
  l.radius_m = static_cast<float>(std::min(hx, hz));
  if (v.flicker > 0) {
    l.flicker = Flicker::flame;
    l.depth = std::clamp(v.flicker, 0.0f, 1.0f);
    l.rate_hz = v.rate_hz;
    l.seed = v.seed;
  }
  return l;
}

float fog_glow_level(const FogVolume& v, double t_s) {
  if (v.flicker <= 0) return 1.0f;
  return flicker_level(fog_glow_light(v), t_s);
}

void prepare_fog(gpu::Device& device, Fog& fog, const FrameView& view,
                 std::span<const FogVolume> volumes, bool cut) {
  const std::vector<size_t> reach = fog_in_reach(fog.settings, view, volumes);
  fog.count = static_cast<uint32_t>(reach.size());
  if (fog.count == 0) {
    fog.had_last = false;
    return;
  }
  auto* staged = reinterpret_cast<Volume*>(fog.staged.data());
  for (uint32_t k = 0; k < fog.count; ++k) {
    const FogVolume& v = volumes[reach[k]];
    Volume& g = staged[k];
    for (int a = 0; a < 3; ++a) {
      g.centre[a] = static_cast<float>(v.centre[a] - view.eye[a]);
      g.half[a] = static_cast<float>(std::max(v.half[a], 1e-3));
      g.albedo[a] = static_cast<float>(std::clamp(v.albedo[a], 0.0, 1.0));
      g.glow[a] = static_cast<float>(std::max(v.glow[a], 0.0)) * fog_glow_level(v, view.time_s);
    }
    g.centre[3] = (v.shape == FogVolume::Shape::ellipsoid ? 1.0f : 0.0f) +
                  (v.hug_ground && fog.ground ? 2.0f : 0.0f);
    g.half[3] = static_cast<float>(std::max(v.edge, 1e-3));
    g.turn[0] = static_cast<float>(std::cos(v.yaw));
    g.turn[1] = static_cast<float>(std::sin(v.yaw));
    g.turn[2] = static_cast<float>(v.density);
    g.turn[3] = static_cast<float>(std::max(v.falloff, 0.0));
    g.albedo[3] = static_cast<float>(std::clamp(v.phase, -0.95, 0.95));
    g.glow[3] = static_cast<float>(std::clamp(v.noise, 0.0, 1.0));
    // The noise's drift in tiles, wrapped, so a long run keeps its float precision.
    const double tile = std::max(v.noise_size, 1e-3);
    for (int a = 0; a < 3; ++a) {
      g.drift[a] = static_cast<float>(wrap01(-v.wind[a] * view.time_s / tile));
    }
    g.drift[3] = static_cast<float>(1.0 / tile);
    g.light[0] = std::clamp(v.sky, 0.0f, 1.0f);
  }
  device.queue.WriteBuffer(fog.volumes, 0, staged, sizeof(Volume) * fog.count);
  // This frame writes the other media; the last frame's is the history.
  fog.current = 1 - fog.current;
  const bool history = fog.had_last && !cut;
  Params p{};
  p.grid[0] = fog.settings.width;
  p.grid[1] = fog.settings.height;
  p.grid[2] = fog.settings.slices;
  p.grid[3] = fog.count;
  p.look[0] = static_cast<float>(fog.settings.range);
  p.look[1] = history ? static_cast<float>(std::clamp(fog.settings.history, 0.0, 0.98)) : 0.0f;
  // A new depth inside each froxel every frame, on the golden ratio.
  p.look[2] = static_cast<float>(
      history ? wrap01(0.6180339887498949 * static_cast<double>(view.frame % 4096u)) : 0.5);
  p.look[3] = fog.ground ? 1.0f : 0.0f;
  device.queue.WriteBuffer(fog.params, 0, &p, sizeof p);
  fog.had_last = true;
}

void fog_uniforms(const Fog& fog, FrameUniforms& u) {
  u.fog[0] = fog.count > 0 ? static_cast<float>(fog.settings.range) : 0.0f;
  u.fog[1] = static_cast<float>(fog.settings.slices);
  u.fog[2] = 0.0f;
  u.fog[3] = 0.0f;
}

void encode_fog(Fog& fog, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame) {
  const FogSettings& s = fog.settings;
  {
    wgpu::ComputePassEncoder pass = rec.compute(encoder, "fog.inject");
    if (fog.count > 0) {
      pass.SetPipeline(pipelines.compute(fog.p_inject));
      pass.SetBindGroup(0, frame);
      pass.SetBindGroup(1, fog.inject_group[fog.current]);
      pass.SetBindGroup(2, fog.ground_group);
      pass.SetBindGroup(3, fog.shadow_group);
      pass.DispatchWorkgroups(groups(s.width, kInjectGroup), groups(s.height, kInjectGroup),
                              groups(s.slices, kInjectGroup));
      rec.current().dispatches += 1;
    }
    pass.End();
  }
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "fog.integrate");
  if (fog.count > 0) {
    pass.SetPipeline(pipelines.compute(fog.p_integrate));
    pass.SetBindGroup(0, fog.integrate_group[fog.current]);
    pass.DispatchWorkgroups(groups(s.width, kIntegrateGroup), groups(s.height, kIntegrateGroup), 1);
    rec.current().dispatches += 1;
  }
  pass.End();
}

std::vector<float> read_fog_column(gpu::Device& device, const Fog& fog, uint32_t x, uint32_t y) {
  const uint32_t slices = fog.settings.slices;
  constexpr uint32_t kRow = 256;  // the copy's row alignment
  wgpu::BufferDescriptor bd{};
  bd.label = "fog.readback";
  bd.size = uint64_t{kRow} * slices;
  bd.usage = BU::MapRead | BU::CopyDst;
  wgpu::Buffer buffer = device.device.CreateBuffer(&bd);
  wgpu::TexelCopyTextureInfo src{};
  src.texture = fog.volume;
  src.origin = {std::min(x, fog.settings.width - 1), std::min(y, fog.settings.height - 1), 0};
  wgpu::TexelCopyBufferInfo dst{};
  dst.buffer = buffer;
  dst.layout.bytesPerRow = kRow;
  dst.layout.rowsPerImage = 1;
  const wgpu::Extent3D size{1, 1, slices};
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  encoder.CopyTextureToBuffer(&src, &dst, &size);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  gpu::wait(device,
            buffer.MapAsync(wgpu::MapMode::Read, 0, bd.size, wgpu::CallbackMode::WaitAnyOnly,
                            [](wgpu::MapAsyncStatus, wgpu::StringView) {}));
  std::vector<float> out(size_t{slices} * 4);
  if (const void* p = buffer.GetConstMappedRange(0, bd.size)) {
    const auto* bytes = static_cast<const uint8_t*>(p);
    for (uint32_t z = 0; z < slices; ++z) {
      for (uint32_t c = 0; c < 4; ++c) {
        uint16_t v;
        std::memcpy(&v, bytes + size_t{z} * kRow + size_t{c} * 2, 2);
        out[size_t{z} * 4 + c] = half_to_float(v);
      }
    }
  }
  buffer.Unmap();
  return out;
}

}  // namespace ge::render
