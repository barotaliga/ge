// The horizon map's grid and march, the bearing slots and the bake decision, the bake pass, the
// CPU reference and the readback.
//
// ge:summary The horizon map's slots, bake decision, bake pass, CPU reference and readback
// ge:keywords render horizon-map bake march mirror readback half-float
// ge:skill render
// ge:test tests/render_horizon_test.cpp
#include "render/horizon.hpp"

#include <algorithm>
#include <cmath>

#include "render/horizon.wgsl.hpp"

namespace ge::render {

namespace {

// horizon.wgsl's `HorizonBake`.
struct BakeParams {
  float dir[4];      // toward the bearing (x, z), first step (m), step ratio
  float grid[4];     // the map's origin x, z (world m), cell (m), 0
  uint32_t size[4];  // width, height
};

// The grid's mirrored addressing past its edge, as terrain_common.wgsl's `mirror_grid`.
double mirror(double g, double maxc) {
  const double t = g - 2.0 * maxc * std::floor(g / (2.0 * maxc));
  return maxc - std::fabs(maxc - t);
}

double field_at(const core::Heightfield& f, double x, double z) {
  const double gx = mirror((x - f.origin_x) / f.cell, f.cols - 1);
  const double gz = mirror((z - f.origin_z) / f.cell, f.rows - 1);
  return sample_field(f, f.origin_x + gx * f.cell, f.origin_z + gz * f.cell);
}

}  // namespace

HorizonMarch horizon_march(double cell_m, double range_m) {
  const double first = std::max(cell_m, 0.5);
  const double range = std::max(range_m, first * 2.0);
  return HorizonMarch{first, std::pow(range / first, 1.0 / (kHorizonSteps - 1)), range};
}

int bearing_index(double azimuth, double* fraction) {
  const double turn = 2.0 * 3.14159265358979323846;
  double a = std::fmod(azimuth, turn);
  if (a < 0) a += turn;
  const double steps = a / kHorizonBearingStep;
  int k = static_cast<int>(std::floor(steps));
  double f = steps - k;
  if (k >= kHorizonBearings) {  // a == turn after rounding
    k = 0;
    f = 0;
  }
  if (fraction) *fraction = f;
  return k;
}

double bearing_of(int index) { return index * kHorizonBearingStep; }

void create_horizon(gpu::Device& device, gpu::PipelineSet& pipelines, Horizon& h,
                    const core::Heightfield& field, const HorizonSettings& settings,
                    const wgpu::Buffer& terrain_params, const wgpu::TextureView& heights) {
  const int stride = std::max(settings.stride, 1);
  h.width = (field.cols - 1 + stride - 1) / stride + 1;
  h.height = (field.rows - 1 + stride - 1) / stride + 1;
  h.origin_x = field.origin_x;
  h.origin_z = field.origin_z;
  h.cell = field.cell * stride;
  h.march = horizon_march(h.cell, settings.range_m);
  const auto usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding |
                     wgpu::TextureUsage::CopySrc;
  h.bake_layout = make_layout(device, "terrain.horizon",
                              {Slot::uniform, Slot::texture_raw, Slot::uniform, Slot::store_2d},
                              wgpu::ShaderStage::Compute);
  for (int s = 0; s < 2; ++s) {
    h.maps[s] = make_texture(device, s ? "terrain.horizon.1" : "terrain.horizon.0",
                             static_cast<uint32_t>(h.width), static_cast<uint32_t>(h.height), 1,
                             wgpu::TextureFormat::RGBA16Float, usage);
    h.views[s] = h.maps[s].CreateView();
    h.bake_params[s] =
        make_buffer(device, "terrain.horizon.bake", sizeof(BakeParams), wgpu::BufferUsage::Uniform);
    h.bake_groups[s] = make_group(device, "terrain.horizon", h.bake_layout,
                                  {terrain_params, heights, h.bake_params[s], h.views[s]});
  }
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::horizon, "horizon");
  wgpu::ComputePipelineDescriptor desc{};
  desc.label = "terrain.horizon";
  desc.layout = make_pipeline_layout(device, "terrain.horizon", {h.bake_layout});
  desc.compute.module = module;
  desc.compute.entryPoint = "bake";
  h.p_bake = pipelines.add_compute(device, desc);
}

void prepare_horizon(gpu::Device* device, Horizon& h, const FrameView& view) {
  double f = 0;
  const int k = bearing_index(view.sun_azimuth, &f);
  const int need[2] = {k, (k + 1) % kHorizonBearings};
  h.due[0] = h.due[1] = -1;
  for (int j : need) {
    const int s = j & 1;
    if (h.held[s] == j) continue;
    h.due[s] = j;
    if (!device) continue;
    const double b = bearing_of(j);
    BakeParams p{};
    p.dir[0] = static_cast<float>(std::sin(b));
    p.dir[1] = static_cast<float>(std::cos(b));
    p.dir[2] = static_cast<float>(h.march.first_m);
    p.dir[3] = static_cast<float>(h.march.ratio);
    p.grid[0] = static_cast<float>(h.origin_x);
    p.grid[1] = static_cast<float>(h.origin_z);
    p.grid[2] = static_cast<float>(h.cell);
    p.size[0] = static_cast<uint32_t>(h.width);
    p.size[1] = static_cast<uint32_t>(h.height);
    device->queue.WriteBuffer(h.bake_params[s], 0, &p, sizeof p);
  }
  // A due bake lands before the draw, so the blend can name the slots now.
  for (int s = 0; s < 2; ++s) {
    if (h.due[s] >= 0) {
      h.held[s] = h.due[s];
      ++h.bakes;
    }
  }
  h.blend[0] = static_cast<float>(f);
  h.blend[1] = static_cast<float>(k & 1);
  h.blend[2] = kHorizonPenumbra;
  h.blend[3] = 1.0f;
}

void encode_horizon(Horizon& h, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                    wgpu::CommandEncoder& encoder) {
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "terrain.horizon");
  PassStat& stat = rec.current();
  for (int s = 0; s < 2; ++s) {
    if (h.due[s] < 0) continue;
    pass.SetPipeline(pipelines.compute(h.p_bake));
    pass.SetBindGroup(0, h.bake_groups[s]);
    pass.DispatchWorkgroups((static_cast<uint32_t>(h.width) + 7) / 8,
                            (static_cast<uint32_t>(h.height) + 7) / 8);
    ++stat.dispatches;
    h.due[s] = -1;
  }
  pass.End();
}

double horizon_tan(const core::Heightfield& field, double x, double z, double bearing,
                   const HorizonMarch& march) {
  const double dx = std::sin(bearing), dz = std::cos(bearing);
  const double h0 = field_at(field, x, z);
  double best = -1.0, t = march.first_m;
  for (int i = 0; i < kHorizonSteps; ++i) {
    const double drop = t * t / (2.0 * kEarthRadiusM);
    best = std::max(best, (field_at(field, x + dx * t, z + dz * t) - drop - h0) / t);
    t *= march.ratio;
  }
  return best;
}

std::vector<float> read_horizon(gpu::Device& device, const Horizon& h, int slot) {
  return read_red(device, h.maps[slot & 1], 0, static_cast<uint32_t>(h.width),
                  static_cast<uint32_t>(h.height), wgpu::TextureFormat::RGBA16Float);
}

}  // namespace ge::render
