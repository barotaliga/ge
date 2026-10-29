// The explosions' pass: its buffers and pipeline, the staging (alive, on screen, nearest kept,
// far to near), the encode, and the births turned into particles.
//
// ge:summary Explosions pass definitions: staging and sorting the blasts, the draw, the sparks
// ge:keywords render explosions stage_explosions prepare encode throw_blast spark_particle
// ge:skill render
// ge:test tests/render_explosion_test.cpp
#include "render/explosions.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "render/blackbody.hpp"
#include "render/explosions.wgsl.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;

// explosions.wgsl's ExplosionParams.
struct Params {
  float info[4];  // the near plane, m; the steps; the table's last row's luminance, cd/m^2; count
  float table[kBlackbodyRows][4];
};

// The noise's tiles per fireball radius at billow_size 1.
constexpr double kNoiseTiles = 0.3;

// The most one blast throws (a large one at a big scale stops here).
constexpr size_t kMostBirths = 512;

}  // namespace

void create_explosions(gpu::Device& device, gpu::PipelineSet& pipelines, Explosions& ex,
                       const wgpu::BindGroupLayout& frame, const ExplosionSettings& settings) {
  ex.settings = settings;
  ex.settings.steps = std::clamp(settings.steps, 2u, 64u);
  blackbody_table(ex.table);
  ex.hot_cd = static_cast<float>(blackbody_cd(kBlackbodyHiK));
  ex.staged.assign(kMaxExplosions, GpuBlast{});
  ex.order.reserve(256);
  ex.params = make_buffer(device, "explosions.params", sizeof(Params), BU::Uniform);
  ex.blasts =
      make_buffer(device, "explosions.blasts", kMaxExplosions * sizeof(GpuBlast), BU::Storage);
  const auto vf = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  ex.layout =
      make_layout(device, "explosions", {Slot::uniform, Slot::storage_read, Slot::depth}, vf);

  const wgpu::ShaderModule draw = gpu::create_shader(device, wgsl::explosions, "explosions");
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
  // The march clips itself against the depth it reads, so the quad is never depth tested (a
  // ball the floor cuts still shows its upper half, a wall before it hides it pixel by pixel).
  wgpu::DepthStencilState depth{};
  depth.format = kDepthFormat;
  depth.depthWriteEnabled = false;
  depth.depthCompare = wgpu::CompareFunction::Always;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "explosions.draw";
  desc.layout = make_pipeline_layout(device, "explosions.draw", {frame, ex.layout});
  desc.vertex.module = draw;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.depthStencil = &depth;
  desc.fragment = &fragment;
  wgpu::ConstantEntry steps{};
  steps.key = "kSteps";
  steps.value = ex.settings.steps;
  fragment.constantCount = 1;
  fragment.constants = &steps;
  ex.p_draw = pipelines.add_render(device, desc);
}

void bind_explosion_targets(gpu::Device& device, Explosions& ex, const FrameTargets& targets) {
  ex.group =
      make_group(device, "explosions", ex.layout, {ex.params, ex.blasts, targets.depth_view});
}

namespace {

// One thing the march draws this frame, a blast's ball or a shot's flame, before it is staged.
struct Ball {
  double centre[3];  // world, m
  double radius_m, stretch = 1;
  double axis[3] = {0, 1, 0}, side[3] = {1, 0, 0};
  double core_k, fire, smoke, age_s;
  uint32_t seed, prongs = 0;
  double prong_depth = 0;
  double glow = 1;  // the emission's share of a blackbody's
  double billows, fray, billow_size, roll;
  double soot[3] = {0, 0, 0};
};

Ball ball_of(const Blast& b, double now_s) {
  const Fireball f = fireball_at(b, now_s);
  const BlastLook& k = look_of(b);
  Ball o{};
  for (int i = 0; i < 3; ++i) o.centre[i] = f.centre[i];
  o.radius_m = f.radius_m;
  o.core_k = f.core_k;
  o.fire = f.fire;
  o.smoke = f.smoke;
  o.age_s = f.age_s;
  o.seed = b.seed;
  o.billows = k.billows;
  o.fray = k.fray;
  o.billow_size = k.billow_size;
  o.roll = k.roll;
  for (int c = 0; c < 3; ++c) o.soot[c] = k.smoke_albedo * k.smoke_tint[c];
  return o;
}

Ball ball_of(const MuzzleFire& m, double now_s) {
  const Flame f = flame_at(m.shot, m.pose, now_s);
  const MuzzleLook& k = look_of(m.shot);
  Ball o{};
  for (int i = 0; i < 3; ++i) {
    o.centre[i] = f.centre[i];
    o.axis[i] = f.axis[i];
    o.side[i] = f.side[i];
  }
  o.radius_m = f.radius_m;
  o.stretch = f.stretch;
  o.core_k = f.core_k;
  o.fire = f.fire;
  o.smoke = 0;
  o.age_s = f.age_s;
  o.seed = m.shot.seed;
  o.prongs = k.prongs;
  o.prong_depth = k.prong_depth;
  o.glow = k.glow;
  o.billows = k.billows;
  o.fray = k.fray;
  o.billow_size = k.billow_size;
  o.roll = 0;
  return o;
}

bool alive_at(const Blast& b, double now_s) {
  const double t = now_s - b.born_s;
  return t >= 0 && t < blast_life(b);
}

bool alive_at(const MuzzleFire& m, double now_s) {
  const double t = now_s - m.shot.born_s;
  return t >= 0 && t < look_of(m.shot).life_s;
}

}  // namespace

void stage_explosions(Explosions& ex, std::span<const Blast> blasts, const FrameView& view,
                      uint32_t width, uint32_t height, std::span<const MuzzleFire> fires) {
  const CameraMatrices cam =
      camera_matrices(view, static_cast<double>(width) / std::max(height, 1u));
  ex.order.clear();
  ex.culled = 0;
  const auto nb = static_cast<uint32_t>(blasts.size());
  const auto total = static_cast<uint32_t>(blasts.size() + fires.size());
  for (uint32_t i = 0; i < total; ++i) {
    const bool blast = i < nb;
    if (blast ? !alive_at(blasts[i], view.time_s) : !alive_at(fires[i - nb], view.time_s)) {
      continue;
    }
    const Ball o = blast ? ball_of(blasts[i], view.time_s) : ball_of(fires[i - nb], view.time_s);
    const double bound = kBlastBound * o.radius_m * o.stretch;
    double rel[3];
    for (int k = 0; k < 3; ++k) rel[k] = o.centre[k] - view.eye[k];
    bool seen = true;
    for (const auto& p : cam.planes) {
      if (p[0] * rel[0] + p[1] * rel[1] + p[2] * rel[2] + p[3] < -bound) seen = false;
    }
    if (!seen) {
      ++ex.culled;
      continue;
    }
    if (ex.order.size() == ex.order.capacity()) {
      ++ex.culled;
      continue;
    }
    const auto d =
        static_cast<float>(std::sqrt(rel[0] * rel[0] + rel[1] * rel[1] + rel[2] * rel[2]));
    ex.order.emplace_back(d, i);
  }
  // The nearest kMaxExplosions, drawn far to near.
  std::sort(ex.order.begin(), ex.order.end());
  if (ex.order.size() > kMaxExplosions) {
    ex.culled += static_cast<uint32_t>(ex.order.size() - kMaxExplosions);
    ex.order.resize(kMaxExplosions);
  }
  ex.drawn = static_cast<uint32_t>(ex.order.size());
  for (uint32_t n = 0; n < ex.drawn; ++n) {
    const auto [dist, i] = ex.order[ex.drawn - 1 - n];
    const Ball o = i < nb ? ball_of(blasts[i], view.time_s) : ball_of(fires[i - nb], view.time_s);
    GpuBlast& g = ex.staged[n];
    for (int k = 0; k < 3; ++k) g.centre[k] = static_cast<float>(o.centre[k] - view.eye[k]);
    g.centre[3] = static_cast<float>(o.radius_m);
    g.state[0] = static_cast<float>(o.core_k);
    g.state[1] = static_cast<float>(o.fire);
    g.state[2] = static_cast<float>(o.smoke);
    g.state[3] = static_cast<float>(o.age_s);
    // The seed's own place in the noise's tile, so no two blasts billow alike.
    uint32_t h = o.seed * 0x9e3779b1u + 0x7f4a7c15u;
    for (int k = 0; k < 3; ++k) {
      h ^= h >> 15;
      h *= 0x2c1b3c6du;
      h ^= h >> 12;
      g.look[k] = static_cast<float>(h & 0xffffu) / 65536.0f;
    }
    const double bound = kBlastBound * o.radius_m * o.stretch;
    g.look[3] = static_cast<float>(bound);
    g.more[0] = dist < bound * 1.02 + 2 * view.near_m ? 1.0f : 0.0f;
    g.more[1] = static_cast<float>(o.prong_depth);
    g.more[2] = static_cast<float>(o.glow);
    g.more[3] = 0;
    g.shape[0] = static_cast<float>(o.billows);
    g.shape[1] = static_cast<float>(o.fray);
    g.shape[2] = static_cast<float>(kNoiseTiles / o.billow_size);
    g.shape[3] = static_cast<float>(o.roll);
    for (int c = 0; c < 3; ++c) g.soot[c] = static_cast<float>(o.soot[c]);
    g.soot[3] = 0;
    for (int k = 0; k < 3; ++k) {
      g.axis[k] = static_cast<float>(o.axis[k]);
      g.side[k] = static_cast<float>(o.side[k]);
    }
    g.axis[3] = static_cast<float>(o.stretch);
    g.side[3] = static_cast<float>(o.prongs);
  }
}

void prepare_explosions(gpu::Device& device, Explosions& ex, std::span<const Blast> blasts,
                        const FrameView& view, uint32_t width, uint32_t height,
                        std::span<const MuzzleFire> fires) {
  stage_explosions(ex, blasts, view, width, height, fires);
  Params p{};
  p.info[0] = static_cast<float>(view.near_m);
  p.info[1] = static_cast<float>(ex.settings.steps);
  p.info[2] = ex.hot_cd;
  p.info[3] = static_cast<float>(ex.drawn);
  std::memcpy(p.table, ex.table, sizeof p.table);
  device.queue.WriteBuffer(ex.params, 0, &p, sizeof p);
  if (ex.drawn > 0) {
    device.queue.WriteBuffer(ex.blasts, 0, ex.staged.data(), ex.drawn * sizeof(GpuBlast));
  }
}

void encode_explosions(const Explosions& ex, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                       wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                       const wgpu::BindGroup& frame) {
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
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "explosions");
  if (ex.drawn > 0) {
    pass.SetPipeline(pipelines.render(ex.p_draw));
    pass.SetBindGroup(0, frame);
    pass.SetBindGroup(1, ex.group);
    pass.Draw(6, ex.drawn);
    ++rec.current().draws;
  }
  pass.End();
}

Particle spark_particle(const Spark& s) {
  Particle p{};
  for (int k = 0; k < 3; ++k) {
    p.p[k] = static_cast<float>(s.p[k]);
    p.v[k] = static_cast<float>(s.v[k]);
  }
  p.life = static_cast<float>(s.life_s);
  p.size = static_cast<float>(s.size_m);
  p.kelvin = static_cast<float>(s.kelvin);
  p.cool_s = static_cast<float>(s.cool_s);
  switch (s.kind) {
    case SparkKind::spark:  // a white-hot speck: streaked, no colour of its own but its glow
      p.drag = 0.6f;
      p.fall = 1;
      p.streak = 0.03f;
      p.albedo = 0.05f;
      p.opacity = 0.9f;
      break;
    case SparkKind::debris:  // a dark chunk on its arc, some still glowing
      p.drag = 0.25f;
      p.fall = 1;
      p.streak = 0.01f;
      p.albedo = 0.08f;
      p.opacity = 1;
      break;
  }
  return p;
}

size_t throw_blast(Particles& particles, const Blast& blast) {
  std::array<Spark, kMostBirths> sparks{};
  const size_t n = blast_births(blast, sparks);
  std::array<Particle, kMostBirths> births{};
  for (size_t i = 0; i < n; ++i) births[i] = spark_particle(sparks[i]);
  emit_particles(particles, std::span<const Particle>(births.data(), n));
  return n;
}

size_t throw_shot(Particles& particles, const Shot& shot, const MuzzlePose& pose_at_shot) {
  std::array<Spark, kMostBirths> sparks{};
  const size_t n = shot_births(shot, pose_at_shot, sparks);
  std::array<Particle, kMostBirths> births{};
  for (size_t i = 0; i < n; ++i) births[i] = spark_particle(sparks[i]);
  emit_particles(particles, std::span<const Particle>(births.data(), n));
  return n;
}

}  // namespace ge::render
