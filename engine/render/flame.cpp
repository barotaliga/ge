// The flames' definitions: the presets, the light a flame casts, its bounds, the pipeline, the
// upload and the pass.
//
// ge:summary Flames' definitions: presets, the flame's light and bounds, the pass
// ge:keywords render flame flame_light flame_bounds
// ge:keywords prepare_flames encode_flames additive
// ge:skill render
// ge:test tests/render_flame_test.cpp
#include "render/flame.hpp"

#include <algorithm>
#include <cmath>

#include "render/flame.wgsl.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;

// The flame's mean temperature (where its light's colour is read) and its area seen side on:
// a teardrop fills about 0.55 of its box, and the soot glows far below its core over most of it
// (its mean luminance about 0.28 of the core's).
constexpr double kMeanTemp = 0.88, kFill = 0.55, kMeanGlow = 0.28;

// The bounding sphere's centre (world) and radius of a flame at full height in its wind.
void sphere_of(const Flame& f, double c[3], double& r) {
  double lo[3], hi[3];
  flame_bounds(f, lo, hi);
  double r2 = 0;
  for (int k = 0; k < 3; ++k) {
    c[k] = 0.5 * (lo[k] + hi[k]);
    r2 += 0.25 * (hi[k] - lo[k]) * (hi[k] - lo[k]);
  }
  r = std::sqrt(r2);
}

// The tip's sideways offset per metre of height: the wind's push against the gas's rise.
void lean_of(const Flame& f, double out[2]) {
  const double rise = std::max(f.rise_mps, 0.1f);
  out[0] = std::clamp(f.wind[0] / rise, -1.5, 1.5);
  out[1] = std::clamp(f.wind[2] / rise, -1.5, 1.5);
}

Flame preset(const double base[3], uint32_t seed, float h, float r, float k, float nits, float rise,
             float rate, float blue) {
  Flame f;
  for (int i = 0; i < 3; ++i) f.base[i] = base[i];
  f.height_m = h;
  f.radius_m = r;
  f.temperature_k = k;
  f.luminance_nits = nits;
  f.rise_mps = rise;
  f.rate_hz = rate;
  f.blue = blue;
  f.seed = seed;
  return f;
}

}  // namespace

Flame torch_flame(const double base[3], uint32_t seed) {
  return preset(base, seed, 0.4f, 0.07f, 1900, 9000, 1.0f, 9, 0.0f);
}
Flame candle_flame(const double base[3], uint32_t seed) {
  Flame f = preset(base, seed, 0.04f, 0.006f, 1850, 9000, 0.25f, 4, 0.6f);
  f.depth = 0.12f;
  return f;
}
Flame brazier_flame(const double base[3], uint32_t seed) {
  return preset(base, seed, 0.8f, 0.16f, 1950, 9000, 1.4f, 7, 0.0f);
}
Flame campfire_flame(const double base[3], uint32_t seed) {
  Flame f = preset(base, seed, 1.2f, 0.32f, 1950, 9000, 1.8f, 6, 0.1f);
  f.depth = 0.4f;
  return f;
}

LocalLight flame_light(const Flame& f) {
  LocalLight l;
  double lean[2];
  lean_of(f, lean);
  const double up = 0.35 * f.height_m;
  l.position[0] = f.base[0] + lean[0] * up * 0.4;
  l.position[1] = f.base[1] + up;
  l.position[2] = f.base[2] + lean[1] * up * 0.4;
  const Blackbody b = blackbody(f.temperature_k * kMeanTemp);
  for (int k = 0; k < 3; ++k) l.colour[k] = b.rgb[k];
  const double area = kFill * 2.0 * f.radius_m * f.height_m;
  l.candela = static_cast<float>(f.luminance_nits * kMeanGlow * area);
  // Its reach: where it has fallen to a third of a lux (moonlight's), within the clusters' sense.
  l.range_m = static_cast<float>(std::clamp(std::sqrt(l.candela / 0.3), 0.5, 40.0));
  // The fire lights the wall behind it from its whole flame, not a point: its light softened over
  // the flame's height.
  l.radius_m = std::max(f.height_m, 0.005f);
  l.flicker = Flicker::flame;
  l.rate_hz = f.rate_hz;
  l.depth = f.depth;
  l.seed = f.seed;
  return l;
}

float flame_level(const Flame& f, double t_s) { return flicker_level(flame_light(f), t_s); }

void flame_bounds(const Flame& f, double lo[3], double hi[3]) {
  double lean[2];
  lean_of(f, lean);
  // The tallest the gas stands (a level's 1.05 and the tongues' 0.25 over it), and its widest
  // (the body and the turbulence's push, 1.6 radii).
  const double h = f.height_m * 1.35, r = f.radius_m * 1.6 + 0.15 * f.height_m;
  const double tip[2] = {lean[0] * h, lean[1] * h};
  lo[0] = f.base[0] + std::min(0.0, tip[0]) - r;
  hi[0] = f.base[0] + std::max(0.0, tip[0]) + r;
  lo[1] = f.base[1] - 0.1 * f.height_m;
  hi[1] = f.base[1] + h;
  lo[2] = f.base[2] + std::min(0.0, tip[1]) - r;
  hi[2] = f.base[2] + std::max(0.0, tip[1]) + r;
}

void create_flames(gpu::Device& device, gpu::PipelineSet& pipelines, Flames& fl,
                   const wgpu::BindGroupLayout& frame) {
  float table[kBlackbodyRows][4];
  blackbody_table(table);
  fl.params = make_buffer(device, "flames.table", sizeof table, BU::Uniform | BU::CopyDst);
  device.queue.WriteBuffer(fl.params, 0, table, sizeof table);
  fl.list = make_buffer(device, "flames.list", uint64_t{kMaxFlames} * sizeof(GpuFlame),
                        BU::Storage | BU::CopyDst);
  fl.staged.assign(kMaxFlames, GpuFlame{});
  fl.order.reserve(size_t{kMaxFlames} * 2);
  fl.key.reserve(size_t{kMaxFlames} * 2);
  fl.layout = make_layout(device, "flames", {Slot::uniform, Slot::storage_read, Slot::depth},
                          wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);

  const wgpu::ShaderModule m = gpu::create_shader(device, wgsl::flame, "flame");
  // Light added to the picture; the motion target's reactive channel its largest.
  wgpu::BlendState add{};
  add.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::One, wgpu::BlendFactor::One};
  add.alpha = {wgpu::BlendOperation::Add, wgpu::BlendFactor::Zero, wgpu::BlendFactor::One};
  wgpu::BlendState most{};
  most.color = {wgpu::BlendOperation::Max, wgpu::BlendFactor::One, wgpu::BlendFactor::One};
  most.alpha = most.color;
  wgpu::ColorTargetState colours[2]{};
  colours[0].format = kHdrFormat;
  colours[0].blend = &add;
  colours[1].format = kMotionFormat;
  colours[1].blend = &most;
  colours[1].writeMask = wgpu::ColorWriteMask::Blue;
  wgpu::FragmentState fragment{};
  fragment.module = m;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 2;
  fragment.targets = colours;
  // No depth test: the march stops at the depth itself, so a flame partly behind a bracket still
  // shows the part in front of it.
  wgpu::DepthStencilState depth{};
  depth.format = kDepthFormat;
  depth.depthWriteEnabled = false;
  depth.depthCompare = wgpu::CompareFunction::Always;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "flames";
  desc.layout = make_pipeline_layout(device, "flames", {frame, fl.layout});
  desc.vertex.module = m;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.depthStencil = &depth;
  desc.fragment = &fragment;
  fl.pipeline = pipelines.add_render(device, desc);
}

void bind_flame_targets(gpu::Device& device, Flames& fl, const FrameTargets& targets) {
  fl.group = make_group(device, "flames", fl.layout, {fl.params, fl.list, targets.depth_view});
}

void prepare_flames(gpu::Device& device, Flames& fl, std::span<const Flame> list,
                    const FrameView& view) {
  // The nearest kMaxFlames, by an insertion sort on their distance (a frame's few).
  fl.order.clear();
  fl.key.clear();
  for (uint32_t i = 0; i < list.size(); ++i) {
    double d2 = 0;
    for (int k = 0; k < 3; ++k)
      d2 += (list[i].base[k] - view.eye[k]) * (list[i].base[k] - view.eye[k]);
    const auto key = static_cast<float>(d2);
    size_t at = fl.order.size();
    fl.order.push_back(i);
    fl.key.push_back(key);
    while (at > 0 && fl.key[at - 1] > key) {
      fl.order[at] = fl.order[at - 1];
      fl.key[at] = fl.key[at - 1];
      --at;
    }
    fl.order[at] = i;
    fl.key[at] = key;
  }
  fl.count = static_cast<uint32_t>(std::min<size_t>(fl.order.size(), kMaxFlames));
  fl.dropped = static_cast<uint32_t>(fl.order.size() - fl.count);
  for (uint32_t n = 0; n < fl.count; ++n) {
    const Flame& f = list[fl.order[n]];
    GpuFlame& g = fl.staged[n];
    const float level = f.level >= 0 ? f.level : flame_level(f, view.time_s);
    double lean[2];
    lean_of(f, lean);
    for (int k = 0; k < 3; ++k) g.base[k] = static_cast<float>(f.base[k] - view.eye[k]);
    // The gas swells with its level: three quarters of its height at the bottom of its swing.
    g.base[3] = f.height_m * (0.7f + 0.3f * level);
    g.shape[0] = f.radius_m * (0.85f + 0.15f * level);
    g.shape[1] = f.temperature_k * (0.94f + 0.06f * level);
    g.shape[2] = f.luminance_nits * level;
    g.shape[3] = static_cast<float>(f.seed % 1024u) * 7.31f;
    g.wind[0] = static_cast<float>(lean[0]);
    g.wind[1] = static_cast<float>(lean[1]);
    g.wind[2] = std::max(f.rise_mps, 0.05f);
    g.wind[3] = f.blue;
    double c[3], r;
    sphere_of(f, c, r);
    for (int k = 0; k < 3; ++k) g.bound[k] = static_cast<float>(c[k] - view.eye[k]);
    g.bound[3] = static_cast<float>(r);
  }
  if (fl.count > 0) {
    device.queue.WriteBuffer(fl.list, 0, fl.staged.data(), uint64_t{fl.count} * sizeof(GpuFlame));
  }
}

void encode_flames(const Flames& fl, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                   const wgpu::BindGroup& frame) {
  if (fl.count == 0) return;
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
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "flames");
  pass.SetPipeline(pipelines.render(fl.pipeline));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, fl.group);
  pass.Draw(6, fl.count);
  ++rec.current().draws;
  pass.End();
}

}  // namespace ge::render
