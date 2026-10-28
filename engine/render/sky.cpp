// The sky's tables and pipelines made, the rebuild decision, the tables' compute pass and the
// sky pass.
//
// ge:summary The sky's tables, pipelines, rebuild decision and passes
// ge:keywords render sky tables compute-pass sky-pass rebuild threshold
// ge:skill render
// ge:test tests/render_sky_test.cpp
#include "render/sky.hpp"

#include <cmath>
#include <cstring>

#include "render/lights.hpp"
#include "render/sky.wgsl.hpp"
#include "render/sky_tables.wgsl.hpp"

namespace ge::render {

namespace {

using TU = wgpu::TextureUsage;

uint32_t groups(uint32_t n, uint32_t size) { return (n + size - 1) / size; }

uint32_t add_table(gpu::Device& device, gpu::PipelineSet& pipelines,
                   const wgpu::ShaderModule& module, const wgpu::PipelineLayout& layout,
                   const char* entry, const char* label) {
  wgpu::ComputePipelineDescriptor desc{};
  desc.label = label;
  desc.layout = layout;
  desc.compute.module = module;
  desc.compute.entryPoint = entry;
  return pipelines.add_compute(device, desc);
}

}  // namespace

wgpu::BindGroupLayout frame_layout(gpu::Device& device) {
  return make_layout(
      device, "render.frame",
      {Slot::uniform, Slot::sampler, Slot::texture, Slot::texture, Slot::texture, Slot::texture_3d,
       Slot::storage_read, Slot::texture, Slot::texture_3d, Slot::sampler, Slot::storage_shade},
      wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Compute);
}

void create_sky(gpu::Device& device, gpu::PipelineSet& pipelines, Sky& sky,
                const wgpu::Buffer& uniforms, const wgpu::BindGroupLayout& frame) {
  const auto f16 = wgpu::TextureFormat::RGBA16Float;
  const TU table = TU::StorageBinding | TU::TextureBinding;
  sky.transmittance =
      make_texture(device, "sky.transmittance", kTransmittanceW, kTransmittanceH, 1, f16, table);
  sky.multiscatter =
      make_texture(device, "sky.multiscatter", kMultiscatterW, kMultiscatterH, 1, f16, table);
  sky.sky_view = make_texture(device, "sky.view", kSkyViewW, kSkyViewH, 1, f16, table);
  sky.moon_view = make_texture(device, "sky.moon_view", kSkyViewW, kSkyViewH, 1, f16, table);
  sky.aerial = make_texture(device, "sky.aerial", kAerialW, kAerialH, kAerialD, f16, table);
  sky.blank = make_texture(device, "sky.blank", 1, 1, 1, f16, TU::TextureBinding);
  sky.transmittance_view = sky.transmittance.CreateView();
  sky.multiscatter_view = sky.multiscatter.CreateView();
  sky.sky_view_view = sky.sky_view.CreateView();
  sky.moon_view_view = sky.moon_view.CreateView();
  sky.aerial_view = sky.aerial.CreateView();
  sky.blank_view = sky.blank.CreateView();
  sky.light =
      make_buffer(device, "sky.light", 64, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
  // Zeroed at creation: no light.
  sky.no_lights =
      make_buffer(device, "sky.no_lights", kRefsOffset + 16, wgpu::BufferUsage::Storage);

  wgpu::SamplerDescriptor sd{};
  sd.label = "render.linear_clamp";
  sd.addressModeU = sd.addressModeV = sd.addressModeW = wgpu::AddressMode::ClampToEdge;
  sd.magFilter = sd.minFilter = wgpu::FilterMode::Linear;
  sd.mipmapFilter = wgpu::MipmapFilterMode::Linear;
  sky.sampler = device.device.CreateSampler(&sd);
  sky.cloud = create_cloud_noise(device);

  const auto cs = wgpu::ShaderStage::Compute;
  sky.in_layout = make_layout(device, "sky.in",
                              {Slot::uniform, Slot::sampler, Slot::texture, Slot::texture}, cs);
  // Group 1's three outputs sit at bindings 0, 1 and 2 (sky_tables.wgsl); a layout holds one,
  // so the unused slots before it are bindings nothing declares.
  auto out_layout = [&](const char* label, uint32_t binding, wgpu::BindGroupLayoutEntry e) {
    e.binding = binding;
    e.visibility = cs;
    wgpu::BindGroupLayoutDescriptor d{};
    d.label = label;
    d.entryCount = 1;
    d.entries = &e;
    return device.device.CreateBindGroupLayout(&d);
  };
  wgpu::BindGroupLayoutEntry e2{}, e3{}, eb{};
  e2.storageTexture = {nullptr, wgpu::StorageTextureAccess::WriteOnly, f16,
                       wgpu::TextureViewDimension::e2D};
  e3.storageTexture = {nullptr, wgpu::StorageTextureAccess::WriteOnly, f16,
                       wgpu::TextureViewDimension::e3D};
  eb.buffer.type = wgpu::BufferBindingType::Storage;
  sky.out_2d = out_layout("sky.out_2d", 0, e2);
  sky.out_3d = out_layout("sky.out_3d", 1, e3);
  sky.out_buffer = out_layout("sky.out_buffer", 2, eb);

  const wgpu::ShaderModule tables = gpu::create_shader(device, wgsl::sky_tables, "sky_tables");
  const auto l2 = make_pipeline_layout(device, "sky.tables_2d", {sky.in_layout, sky.out_2d});
  const auto l3 = make_pipeline_layout(device, "sky.tables_3d", {sky.in_layout, sky.out_3d});
  const auto lb = make_pipeline_layout(device, "sky.tables_light", {sky.in_layout, sky.out_buffer});
  sky.p_transmittance =
      add_table(device, pipelines, tables, l2, "transmittance", "sky.transmittance");
  sky.p_multiscatter =
      add_table(device, pipelines, tables, l2, "multiscattering", "sky.multiscatter");
  sky.p_sky_view = add_table(device, pipelines, tables, l2, "sky_view", "sky.view");
  sky.p_aerial = add_table(device, pipelines, tables, l3, "aerial", "sky.aerial");
  sky.p_light = add_table(device, pipelines, tables, lb, "sky_light_main", "sky.light");
  sky.p_moon_view = add_table(device, pipelines, tables, l2, "sky_view_moon", "sky.moon_view");
  sky.p_moon_light = add_table(device, pipelines, tables, lb, "sky_light_moon", "sky.moon_light");

  auto in = [&](const char* label, const wgpu::TextureView& a, const wgpu::TextureView& b) {
    return make_group(device, label, sky.in_layout, {uniforms, sky.sampler, a, b});
  };
  auto out = [&](const char* label, const wgpu::BindGroupLayout& layout, uint32_t binding,
                 const wgpu::TextureView& v, const wgpu::Buffer& buffer) {
    wgpu::BindGroupEntry e{};
    e.binding = binding;
    e.textureView = v;
    e.buffer = buffer;
    wgpu::BindGroupDescriptor d{};
    d.label = label;
    d.layout = layout;
    d.entryCount = 1;
    d.entries = &e;
    return device.device.CreateBindGroup(&d);
  };
  sky.g_transmittance[0] = in("sky.transmittance.in", sky.blank_view, sky.blank_view);
  sky.g_transmittance[1] =
      out("sky.transmittance.out", sky.out_2d, 0, sky.transmittance_view, nullptr);
  sky.g_multiscatter[0] = in("sky.multiscatter.in", sky.transmittance_view, sky.blank_view);
  sky.g_multiscatter[1] =
      out("sky.multiscatter.out", sky.out_2d, 0, sky.multiscatter_view, nullptr);
  sky.g_sky_view[0] = in("sky.view.in", sky.transmittance_view, sky.multiscatter_view);
  sky.g_sky_view[1] = out("sky.view.out", sky.out_2d, 0, sky.sky_view_view, nullptr);
  sky.g_aerial[0] = in("sky.aerial.in", sky.transmittance_view, sky.multiscatter_view);
  sky.g_aerial[1] = out("sky.aerial.out", sky.out_3d, 1, sky.aerial_view, nullptr);
  sky.g_light[0] = in("sky.light.in", sky.sky_view_view, sky.transmittance_view);
  sky.g_light[1] = out("sky.light.out", sky.out_buffer, 2, nullptr, sky.light);
  sky.g_moon_view[0] = in("sky.moon_view.in", sky.transmittance_view, sky.multiscatter_view);
  sky.g_moon_view[1] = out("sky.moon_view.out", sky.out_2d, 0, sky.moon_view_view, nullptr);
  sky.g_moon_light[0] = in("sky.moon_light.in", sky.moon_view_view, sky.transmittance_view);
  sky.g_moon_light[1] = out("sky.moon_light.out", sky.out_buffer, 2, nullptr, sky.light);

  // The sky pass: a full-screen triangle at the far plane, tested `equal` against the depth.
  const wgpu::ShaderModule draw = gpu::create_shader(device, wgsl::sky, "sky");
  wgpu::ColorTargetState colours[2]{};
  colours[0].format = kHdrFormat;
  colours[1].format = kMotionFormat;
  wgpu::FragmentState fragment{};
  fragment.module = draw;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 2;
  fragment.targets = colours;
  wgpu::DepthStencilState depth{};
  depth.format = kDepthFormat;
  depth.depthWriteEnabled = false;
  depth.depthCompare = wgpu::CompareFunction::Equal;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "sky.draw";
  sky.stars = create_star_texture(device);
  sky.moon = create_moon_texture(device);
  sky.star_layout = make_layout(device, "sky.stars", {Slot::texture_array, Slot::texture},
                                wgpu::ShaderStage::Fragment);
  sky.star_group =
      make_group(device, "sky.stars", sky.star_layout, {sky.stars.view, sky.moon.view});
  desc.layout = make_pipeline_layout(device, "sky.draw", {frame, sky.star_layout});
  desc.vertex.module = draw;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.depthStencil = &depth;
  desc.fragment = &fragment;
  sky.p_draw = pipelines.add_render(device, desc);
}

wgpu::BindGroup frame_group(gpu::Device& device, const wgpu::BindGroupLayout& layout,
                            const wgpu::Buffer& uniforms, const Sky& sky,
                            const wgpu::Buffer& lights) {
  return make_group(device, "render.frame", layout,
                    {uniforms, sky.sampler, sky.transmittance_view, sky.multiscatter_view,
                     sky.sky_view_view, sky.aerial_view, sky.light, sky.moon_view_view,
                     sky.cloud.view, sky.cloud.sampler, lights ? lights : sky.no_lights});
}

void prepare_sky(Sky& sky, const FrameView& view) {
  double sun[3];
  real_sun_direction(view, sun);
  // A new air rebuilds every table, the moon's too.
  const bool new_air = !same_air(view.style, sky.built_air);
  if (new_air) {
    sky.built_air = view.style;
    sky.moon_built = false;
  }
  sky.build_tables = !sky.built || new_air;
  const double turn =
      sun[0] * sky.built_sun[0] + sun[1] * sky.built_sun[1] + sun[2] * sky.built_sun[2];
  sky.build_view = !sky.built || new_air || turn < kSunTurnCos ||
                   std::fabs(view.eye[1] - sky.built_eye_m) > kEyeMove;
  if (sky.build_view) {
    for (int i = 0; i < 3; ++i) sky.built_sun[i] = sun[i];
    sky.built_eye_m = view.eye[1];
  }
  // The moon's sky, only while its light is set (at night).
  sky.build_moon = false;
  if (view.night.moon_illuminance > 0) {
    double moon[3];
    look_dir(view.night.moon_azimuth, view.night.moon_elevation, moon);
    const double moon_turn =
        moon[0] * sky.built_moon[0] + moon[1] * sky.built_moon[1] + moon[2] * sky.built_moon[2];
    sky.build_moon = !sky.moon_built || moon_turn < kSunTurnCos ||
                     std::fabs(view.eye[1] - sky.built_moon_eye_m) > kEyeMove;
    if (sky.build_moon) {
      for (int i = 0; i < 3; ++i) sky.built_moon[i] = moon[i];
      sky.built_moon_eye_m = view.eye[1];
      sky.moon_built = true;
    }
  }
  sky.built = true;
}

void encode_sky_tables(Sky& sky, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                       wgpu::CommandEncoder& encoder) {
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "sky.tables");
  PassStat& stat = rec.current();
  auto run = [&](uint32_t pipeline, const wgpu::BindGroup* g, uint32_t x, uint32_t y) {
    pass.SetPipeline(pipelines.compute(pipeline));
    pass.SetBindGroup(0, g[0]);
    pass.SetBindGroup(1, g[1]);
    pass.DispatchWorkgroups(x, y);
    ++stat.dispatches;
  };
  if (sky.build_tables) {
    run(sky.p_transmittance, sky.g_transmittance, groups(kTransmittanceW, 8),
        groups(kTransmittanceH, 8));
    run(sky.p_multiscatter, sky.g_multiscatter, groups(kMultiscatterW, 8),
        groups(kMultiscatterH, 8));
  }
  if (sky.build_view) {
    run(sky.p_sky_view, sky.g_sky_view, groups(kSkyViewW, 8), groups(kSkyViewH, 8));
    run(sky.p_light, sky.g_light, 1, 1);
  }
  if (sky.build_moon) {
    run(sky.p_moon_view, sky.g_moon_view, groups(kSkyViewW, 8), groups(kSkyViewH, 8));
    run(sky.p_moon_light, sky.g_moon_light, 1, 1);
  }
  run(sky.p_aerial, sky.g_aerial, groups(kAerialW, 8), groups(kAerialH, 8));
  pass.End();
  sky.build_tables = false;
  sky.build_view = false;
  sky.build_moon = false;
}

void encode_sky(const Sky& sky, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
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
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "sky");
  pass.SetPipeline(pipelines.render(sky.p_draw));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, sky.star_group);
  pass.Draw(3);
  ++rec.current().draws;
  pass.End();
}

SkyLightReading read_sky_light(gpu::Device& device, const Sky& sky) {
  wgpu::BufferDescriptor bd{};
  bd.label = "sky.light.readback";
  bd.size = 64;
  bd.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer rb = device.device.CreateBuffer(&bd);
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  encoder.CopyBufferToBuffer(sky.light, 0, rb, 0, 64);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  gpu::wait(device, rb.MapAsync(wgpu::MapMode::Read, 0, 64, wgpu::CallbackMode::WaitAnyOnly,
                                [](wgpu::MapAsyncStatus, wgpu::StringView) {}));
  SkyLightReading out{};
  if (const void* p = rb.GetConstMappedRange(0, 64)) {
    float f[16];
    std::memcpy(f, p, sizeof f);
    for (int i = 0; i < 3; ++i) {
      out.irradiance_up[i] = f[i];
      out.sun_at_eye[i] = f[4 + i];
      out.moon_up[i] = f[8 + i];
      out.moon_at_eye[i] = f[12 + i];
    }
  }
  rb.Unmap();
  return out;
}

}  // namespace ge::render
