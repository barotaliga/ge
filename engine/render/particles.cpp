// The particles' pool, pipelines and groups, the births into the ring, the two passes, the
// read-back and the flight's CPU twin.
//
// ge:summary GPU particles definitions: the ring's births, the sim and draw pipelines and passes
// ge:keywords render particles ring births sim draw premultiplied readback step_particle
// ge:skill render
// ge:test tests/render_particles_test.cpp
#include "render/particles.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "render/blackbody.hpp"
#include "render/particles.wgsl.hpp"
#include "render/particles_sim.wgsl.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;
constexpr uint32_t kGroup = 64;  // particles_sim.wgsl's workgroup size

// particle_types.wgsl's ParticleParams.
struct Params {
  float step[4];
  uint32_t info[4];
  float air[4];
  float glow[4];
  float seen_vp[16];
  float seen_inv[16];
  float seen_eye[4];
  float table[kBlackbodyRows][4];
};

}  // namespace

void eddy(const Particle& p, float out[3]) {
  out[0] = out[1] = out[2] = 0;
  if (p.swirl == 0) return;
  const float x = p.size * 613.0f + p.life * 89.0f;
  const float ph = (x - std::floor(x)) * 6.2831853f;
  const float a[3] = {p.p[0] * 4.0f + ph, p.p[1] * 4.0f + ph * 1.7f, p.p[2] * 4.0f + ph * 2.3f};
  const float t = p.age * 1.6f;
  out[0] = (std::sin(a[1] * 1.3f + t * 2.7f) + 0.5f * std::sin(a[2] * 2.9f + t * 4.1f)) * p.swirl;
  out[1] =
      (0.6f * std::sin(a[2] * 1.1f + t * 2.3f) + 0.3f * std::sin(a[0] * 3.1f + t * 5.3f)) * p.swirl;
  out[2] = (std::sin(a[0] * 1.7f + t * 3.1f) + 0.5f * std::sin(a[1] * 2.3f + t * 3.7f)) * p.swirl;
}

void step_particle(Particle& p, float dt, const float air[3]) {
  p.v[1] -= static_cast<float>(kGravity) * p.fall * dt;
  const float keep = std::exp(-p.drag * dt);
  float swirl[3];
  eddy(p, swirl);
  for (int k = 0; k < 3; ++k) {
    const float a = (air ? air[k] : 0.0f) + swirl[k];
    p.v[k] = a + (p.v[k] - a) * keep;
    p.p[k] += p.v[k] * dt;
  }
  p.age += dt;
  // Its own floor (particle_types.wgsl's on_floor).
  if (p.p[1] >= p.floor_y || p.v[1] >= 0) return;
  p.p[1] = p.floor_y;
  if (-p.v[1] * p.bounce > kFloorRest && p.hops < max_hops(p)) {
    p.v[0] *= kFloorSlide;
    p.v[1] = -p.v[1] * p.bounce;
    p.v[2] *= kFloorSlide;
    p.hops += 1;
  } else {
    p.v[0] = p.v[1] = p.v[2] = 0;
    p.age = std::max(p.age, p.life * 0.7f);
  }
}

float max_hops(const Particle& p) {
  const float x = p.size * 977.0f + p.life * 131.0f;
  return x - std::floor(x) > 0.66f ? kMaxHops + 1 : kMaxHops;
}

void collide_particle(Particle& p, const float at[3], const float n[3]) {
  // On the face, a hair in front of it.
  float into = 0;
  for (int k = 0; k < 3; ++k) into += (at[k] - p.p[k]) * n[k];
  for (int k = 0; k < 3; ++k) p.p[k] += n[k] * (into + 0.01f);
  const float vn = p.v[0] * n[0] + p.v[1] * n[1] + p.v[2] * n[2];
  if (vn >= 0) return;  // already leaving it
  const float b = p.bounce > 0 ? p.bounce : kSurfaceBounce;
  float along[3];
  for (int k = 0; k < 3; ++k) along[k] = p.v[k] - vn * n[k];
  if (-vn * b > kFloorRest && p.hops < max_hops(p)) {
    for (int k = 0; k < 3; ++k) p.v[k] = along[k] * kFloorSlide - vn * b * n[k];
    p.hops += 1;
  } else if (n[1] > 0.6f) {  // a floor: it lies there and fades
    p.v[0] = p.v[1] = p.v[2] = 0;
    p.age = std::max(p.age, p.life * 0.7f);
  } else {  // a wall: it slides down it
    for (int k = 0; k < 3; ++k) p.v[k] = along[k] * kFloorSlide;
  }
}

namespace {

// The pool over a ground: its heights (R32F, textureLoad) under `ground` (terrain.wgsl's
// TerrainParams) and its two horizon maps.
void create_pool(gpu::Device& device, gpu::PipelineSet& pipelines, Particles& ps,
                 const wgpu::BindGroupLayout& frame, const wgpu::Buffer& ground,
                 const wgpu::TextureView& heights, const wgpu::TextureView& horizon0,
                 const wgpu::TextureView& horizon1, const ParticleSettings& settings) {
  ps.settings = settings;
  blackbody_table(ps.glow_table);
  ps.glow_cd = static_cast<float>(blackbody_cd(kBlackbodyHiK) * kSparkEmissivity);
  ps.settings.capacity = std::max(settings.capacity, kGroup);
  // Every slot starts dead (its life 0).
  const std::vector<Particle> dead(ps.settings.capacity, Particle{});
  ps.slots = make_buffer(device, "particles.slots", dead.size() * sizeof(Particle),
                         BU::Storage | BU::CopySrc);
  device.queue.WriteBuffer(ps.slots, 0, dead.data(), dead.size() * sizeof(Particle));
  ps.params = make_buffer(device, "particles.params", sizeof(Params), BU::Uniform);

  ps.sim_layout =
      make_layout(device, "particles.sim",
                  {Slot::uniform, Slot::storage, Slot::uniform, Slot::texture_raw, Slot::depth},
                  wgpu::ShaderStage::Compute);
  wgpu::ComputePipelineDescriptor cd{};
  cd.label = "particles.sim";
  cd.layout = make_pipeline_layout(device, "particles.sim", {ps.sim_layout});
  cd.compute.module = gpu::create_shader(device, wgsl::particles_sim, "particles_sim");
  cd.compute.entryPoint = "sim";
  ps.p_sim = pipelines.add_compute(device, cd);
  // Until the targets are bound, a blank depth (nothing drawn: nothing met).
  ps.sim_ground = ground;
  ps.sim_heights = heights;
  ps.blank_depth = make_texture(device, "particles.blank_depth", 1, 1, 1, kDepthFormat,
                                wgpu::TextureUsage::TextureBinding);
  ps.sim_group = make_group(device, "particles.sim", ps.sim_layout,
                            {ps.params, ps.slots, ground, heights, ps.blank_depth.CreateView()});

  const auto vf = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  ps.light_layout =
      make_layout(device, "particles.light", {Slot::uniform, Slot::texture, Slot::texture}, vf);
  ps.light_group =
      make_group(device, "particles.light", ps.light_layout, {ground, horizon0, horizon1});
  ps.draw_layout =
      make_layout(device, "particles.draw", {Slot::uniform, Slot::storage_read, Slot::depth}, vf);
  ps.shadows = no_sun_shadows(device);

  const wgpu::ShaderModule draw = gpu::create_shader(device, wgsl::particles, "particles");
  // Premultiplied over the picture; into the motion target, the reactive channel's largest.
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
  desc.label = "particles.draw";
  desc.layout =
      make_pipeline_layout(device, "particles.draw",
                           {frame, ps.light_layout, ps.draw_layout, sun_shadow_layout(device)});
  desc.vertex.module = draw;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.depthStencil = &depth;
  desc.fragment = &fragment;
  ps.p_draw = pipelines.add_render(device, desc);
}

}  // namespace

void create_particles(gpu::Device& device, gpu::PipelineSet& pipelines, Particles& ps,
                      const wgpu::BindGroupLayout& frame, const Terrain& terrain,
                      const ParticleSettings& settings) {
  create_pool(device, pipelines, ps, frame, terrain.params, terrain.height.CreateView(),
              terrain.horizon.views[0], terrain.horizon.views[1], settings);
}

void create_particles(gpu::Device& device, gpu::PipelineSet& pipelines, Particles& ps,
                      const wgpu::BindGroupLayout& frame, const ParticleSettings& settings) {
  // No ground: one height far below anything (a particle never lands) and no horizon (the sun
  // reaches everywhere: TerrainParams' horizon.w 0).
  Terrain::Params none{};
  none.field[2] = 1;
  none.field_size[0] = none.field_size[1] = 1;
  ps.ground = make_buffer(device, "particles.ground", sizeof none, BU::Uniform);
  device.queue.WriteBuffer(ps.ground, 0, &none, sizeof none);
  const auto use = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  ps.ground_height =
      make_texture(device, "particles.ground.height", 1, 1, 1, wgpu::TextureFormat::R32Float, use);
  ps.ground_horizon =
      make_texture(device, "particles.ground.horizon", 1, 1, 1, wgpu::TextureFormat::R16Float, use);
  const float far_below = -1e9f;
  const uint16_t flat = 0;
  wgpu::TexelCopyTextureInfo dst{};
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = 4;
  layout.rowsPerImage = 1;
  const wgpu::Extent3D one{1, 1, 1};
  dst.texture = ps.ground_height;
  device.queue.WriteTexture(&dst, &far_below, 4, &layout, &one);
  dst.texture = ps.ground_horizon;
  layout.bytesPerRow = 2;
  device.queue.WriteTexture(&dst, &flat, 2, &layout, &one);
  const wgpu::TextureView horizon = ps.ground_horizon.CreateView();
  create_pool(device, pipelines, ps, frame, ps.ground, ps.ground_height.CreateView(), horizon,
              horizon, settings);
}

void bind_particle_targets(gpu::Device& device, Particles& ps, const FrameTargets& targets) {
  ps.draw_group = make_group(device, "particles.draw", ps.draw_layout,
                             {ps.params, ps.slots, targets.depth_view});
  // The sim reads the frame's depth before the frame draws over it: the last frame's.
  if (ps.settings.collide)
    ps.sim_group =
        make_group(device, "particles.sim", ps.sim_layout,
                   {ps.params, ps.slots, ps.sim_ground, ps.sim_heights, targets.depth_view});
}

void bind_particle_shadows(Particles& ps, const wgpu::BindGroup& shadows) { ps.shadows = shadows; }

void emit_particles(Particles& ps, std::span<const Particle> births) {
  ps.births.insert(ps.births.end(), births.begin(), births.end());
}

void clear_particles(gpu::Device& device, Particles& ps) {
  ps.births.clear();
  ps.head = 0;
  const std::vector<Particle> dead(ps.settings.capacity, Particle{});
  device.queue.WriteBuffer(ps.slots, 0, dead.data(), dead.size() * sizeof(Particle));
}

void prepare_particles(gpu::Device& device, Particles& ps, double dt, double near_m,
                       const double* air) {
  const uint32_t cap = ps.settings.capacity;
  // More births than slots: only the last `cap` live to be seen.
  size_t first = ps.births.size() > cap ? ps.births.size() - cap : 0;
  size_t left = ps.births.size() - first;
  ps.born += left;
  while (left > 0) {
    const size_t n = std::min<size_t>(left, cap - ps.head);
    device.queue.WriteBuffer(ps.slots, uint64_t{ps.head} * sizeof(Particle),
                             ps.births.data() + first, n * sizeof(Particle));
    ps.head = static_cast<uint32_t>((ps.head + n) % cap);
    first += n;
    left -= n;
  }
  ps.births.clear();
  ps.dt = static_cast<float>(std::clamp(dt, 0.0, 0.1));
  Params p{};
  p.step[0] = ps.dt;
  p.step[1] = static_cast<float>(near_m);
  p.step[2] = ps.settings.soft_m;
  p.info[0] = cap;
  for (int k = 0; k < 3; ++k) p.air[k] = air ? static_cast<float>(air[k]) : 0.0f;
  p.glow[0] = ps.glow_cd * ps.settings.glow;
  std::memcpy(p.table, ps.glow_table, sizeof p.table);
  if (ps.settings.collide && ps.seen) {
    std::memcpy(p.seen_vp, ps.seen_vp, sizeof p.seen_vp);
    std::memcpy(p.seen_inv, ps.seen_inv, sizeof p.seen_inv);
    for (int k = 0; k < 3; ++k) p.seen_eye[k] = static_cast<float>(ps.seen_eye[k]);
    p.seen_eye[3] = 1;
  }
  device.queue.WriteBuffer(ps.params, 0, &p, sizeof p);
}

void prepare_particles(gpu::Device& device, Particles& ps, double dt, const FrameView& view,
                       uint32_t width, uint32_t height, const double* air) {
  // This frame's sim meets the depth the last frame's camera drew; then this frame's is kept.
  prepare_particles(device, ps, dt, view.near_m, air);
  const CameraMatrices cam =
      camera_matrices(view, height > 0 ? double(width) / double(height) : 1.0);
  std::memcpy(ps.seen_vp, cam.view_proj.m, sizeof ps.seen_vp);
  std::memcpy(ps.seen_inv, cam.inv_view_proj.m, sizeof ps.seen_inv);
  for (int k = 0; k < 3; ++k) ps.seen_eye[k] = view.eye[k];
  ps.seen = true;
}

void encode_particles_sim(const Particles& ps, const gpu::PipelineSet& pipelines,
                          FrameRecorder& rec, wgpu::CommandEncoder& encoder) {
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "particles.sim");
  pass.SetPipeline(pipelines.compute(ps.p_sim));
  pass.SetBindGroup(0, ps.sim_group);
  pass.DispatchWorkgroups((ps.settings.capacity + kGroup - 1) / kGroup);
  ++rec.current().dispatches;
  pass.End();
}

void encode_particles(const Particles& ps, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
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
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "particles");
  pass.SetPipeline(pipelines.render(ps.p_draw));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, ps.light_group);
  pass.SetBindGroup(2, ps.draw_group);
  pass.SetBindGroup(3, ps.shadows);
  pass.Draw(6, ps.settings.capacity);
  ++rec.current().draws;
  pass.End();
}

std::vector<Particle> read_particles(gpu::Device& device, const Particles& ps) {
  const uint64_t bytes = uint64_t{ps.settings.capacity} * sizeof(Particle);
  std::vector<Particle> out(ps.settings.capacity);
  wgpu::BufferDescriptor desc{};
  desc.label = "particles.readback";
  desc.size = bytes;
  desc.usage = BU::MapRead | BU::CopyDst;
  wgpu::Buffer rb = device.device.CreateBuffer(&desc);
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  encoder.CopyBufferToBuffer(ps.slots, 0, rb, 0, bytes);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  gpu::wait(device, rb.MapAsync(wgpu::MapMode::Read, 0, bytes, wgpu::CallbackMode::WaitAnyOnly,
                                [](wgpu::MapAsyncStatus, wgpu::StringView) {}));
  if (const void* p = rb.GetConstMappedRange(0, bytes)) std::memcpy(out.data(), p, bytes);
  rb.Unmap();
  return out;
}

uint32_t live_particles(std::span<const Particle> slots) {
  uint32_t n = 0;
  for (const Particle& p : slots) n += p.age < p.life ? 1u : 0u;
  return n;
}

}  // namespace ge::render
