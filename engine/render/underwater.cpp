// The view under water: the presets, the pipeline, the pass (underwater.hpp).
//
// ge:summary The view under water's presets, pipeline, uniforms and pass
// ge:keywords render underwater presets pipeline pass uniforms
// ge:skill render water
// ge:test tests/render_water_test.cpp
#include "render/underwater.hpp"

#include <algorithm>
#include <cmath>

#include "render/underwater.wgsl.hpp"

namespace ge::render {

bool underwater_look(std::string_view name, UnderwaterLook& out) {
  UnderwaterLook l;
  if (name == "eyes-open") {
    // the default
  } else if (name == "mask") {
    l.blur = 0.6;
    l.magnify = 1.33;
    l.warp = 0.0008;
  } else if (name == "murky") {
    l.fog = 3;
    l.blur = 9;
    l.blur_range = 1;
    l.motes = 0.45;
    l.mote_size = 0.0035;
  } else if (name == "classic") {
    l.fog = 0.25;
    l.blur = 0;
    l.warp = 0.006;
    l.warp_hz = 0.5;
    l.window = 0;
    l.tint[0] = 0.55;
    l.tint[1] = 0.85;
    l.tint[2] = 0.75;
    l.tint_mix = 0.6;
    l.motes = 0;
    l.caustics = 0;
    l.shafts = 0;
  } else if (name == "pool") {
    // Clear water under a lit room: the light read off the room.
    l.fog = 0.15;
    l.blur = 2;
    l.motes = 0.1;
    l.ambient = 1;
  } else {
    return false;
  }
  out = l;
  return true;
}

void create_underwater(gpu::Device& device, gpu::PipelineSet& pipelines, Underwater& under,
                       const wgpu::BindGroupLayout& frame) {
  under.layout = make_layout(device, "water.under",
                             {Slot::uniform, Slot::texture, Slot::texture_raw, Slot::texture,
                              Slot::texture, Slot::uniform, Slot::storage_shade},
                             wgpu::ShaderStage::Fragment);
  under.uniform = make_buffer(device, "water.under", sizeof(UnderGpu), wgpu::BufferUsage::Uniform);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::underwater, "water.under");
  // Premultiplied over the frame: the share under the waterline replaces what was drawn.
  wgpu::BlendState blend{};
  blend.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::One,
                 wgpu::BlendFactor::OneMinusSrcAlpha};
  blend.alpha = {wgpu::BlendOperation::Add, wgpu::BlendFactor::Zero, wgpu::BlendFactor::One};
  wgpu::ColorTargetState colour{};
  colour.format = kHdrFormat;
  colour.blend = &blend;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 1;
  fragment.targets = &colour;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "water.under";
  under.shafts_layout =
      make_layout(device, "water.shafts", {Slot::texture}, wgpu::ShaderStage::Fragment);
  desc.layout =
      make_pipeline_layout(device, "water.under", {frame, under.layout, under.shafts_layout});
  desc.vertex.module = module;
  desc.vertex.entryPoint = "vs_main";
  desc.fragment = &fragment;
  under.p_under = pipelines.add_render(device, desc);
  // The shafts' own pass: written whole into their target, no blend.
  wgpu::ColorTargetState shafts{};
  shafts.format = wgpu::TextureFormat::RGBA16Float;
  wgpu::FragmentState shafts_fragment{};
  shafts_fragment.module = module;
  shafts_fragment.entryPoint = "fs_shafts";
  shafts_fragment.targetCount = 1;
  shafts_fragment.targets = &shafts;
  wgpu::RenderPipelineDescriptor shafts_desc{};
  shafts_desc.label = "water.shafts";
  shafts_desc.layout = make_pipeline_layout(device, "water.shafts", {frame, under.layout});
  shafts_desc.vertex.module = module;
  shafts_desc.vertex.entryPoint = "vs_main";
  shafts_desc.fragment = &shafts_fragment;
  under.p_shafts = pipelines.add_render(device, shafts_desc);
}

void bind_underwater(gpu::Device& device, Underwater& under, const Water& water) {
  // Made in prepare_underwater, for the body the eye is in.
  under.water = &water;
  under.group_body = -1;
  if (water.width == 0 || water.height == 0) return;
  under.shafts =
      make_texture(device, "water.shafts", (water.width + 1) / 2, (water.height + 1) / 2, 1,
                   wgpu::TextureFormat::RGBA16Float,
                   wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding);
  under.shafts_view = under.shafts.CreateView();
  under.shafts_group = make_group(device, "water.shafts", under.shafts_layout, {under.shafts_view});
}

double level_over(const float tops[][4], const float* levels, int count, double x, double z) {
  double sum = 0, weights = 0;
  for (int i = 0; i < count; ++i) {
    const double dx = std::max({tops[i][0] - x, 0.0, x - tops[i][2]});
    const double dz = std::max({tops[i][1] - z, 0.0, z - tops[i][3]});
    // A centimetre's square keeps the weight finite in the box, where it all but decides alone.
    const double w = 1 / (dx * dx + dz * dz + 1e-4);
    sum += w * levels[i];
    weights += w;
  }
  return weights > 0 ? sum / weights : 0;
}

void prepare_underwater(gpu::Device& device, Underwater& under, const Water& water,
                        const FrameView& view, uint32_t render_height, const Waterline* line) {
  const UnderwaterLook& l = under.look;
  // At the waterline only with a lens plate to split; under the level always.
  const int body = water.eye_under >= 0 ? water.eye_under : (l.waterline > 0 ? water.eye_near : -1);
  under.active = body >= 0;
  if (!under.active) return;
  const Water::Slot& slot = water.bodies[static_cast<size_t>(body)];
  if (under.group_body != body || under.water != &water) {
    under.group = make_group(device, "water.under", under.layout,
                             {under.uniform, water.scene_colour_view, water.scene_depth_view,
                              water.below_view, water.below_veil_view, slot.uniform, slot.waves});
    under.group_body = body;
    under.water = &water;
  }
  UnderGpu& u = under.staged;
  // The tops the eye is under: its own body's first, then the others the water pass draws from
  // below (a flooded passage's other end).
  int tops = 0;
  const auto add_top = [&](const Water::Slot& s) {
    if (tops >= kUnderTops) return;
    for (int k = 0; k < 4; ++k) u.tops[tops][k] = s.staged.box[k];
    u.top_level[tops++] = s.staged.info[0];
  };
  add_top(slot);
  if (water.eye_under >= 0) {
    for (const Water::Slot& s : water.bodies) {
      if (s.under && &s != &slot) add_top(s);
    }
  }
  for (int i = tops; i < kUnderTops; ++i) u.top_level[i] = 0;
  const bool given = line && line->body == body;
  const double surface =
      given ? line->surface : level_over(u.tops, u.top_level, tops, view.eye[0], view.eye[2]);
  u.info[0] = static_cast<float>(surface);
  u.info[1] = static_cast<float>(surface - view.eye[1]);
  u.info[2] = static_cast<float>(l.blur * render_height / 1080.0);
  u.info[3] = static_cast<float>(l.blur_range);
  for (int i = 0; i < 4; ++i) {
    u.c[i] = slot.staged.c[i];
    u.kd[i] = slot.staged.kd[i];
    u.veil[i] = slot.veil[i];
  }
  u.look[0] = static_cast<float>(l.magnify);
  u.look[1] = static_cast<float>(l.warp);
  u.look[2] = static_cast<float>(l.warp_hz);
  u.look[3] = static_cast<float>(l.window);
  for (int i = 0; i < 3; ++i) u.tint[i] = static_cast<float>(l.tint[i]);
  u.tint[3] = static_cast<float>(l.tint_mix);
  u.fog[0] = static_cast<float>(l.fog);
  u.fog[1] = static_cast<float>(l.adapt);
  u.surface[0] = static_cast<float>(surface);
  u.surface[1] = static_cast<float>(given ? line->dx : 0.0);
  u.surface[2] = static_cast<float>(given ? line->dz : 0.0);
  u.surface[3] = water.eye_under >= 0 ? 1.0f : 0.0f;
  // The specks thicken with the water's beam attenuation (its green band's): none to speak of in
  // a pool, the look's share at 2 per metre, three times it in a brown lake's murk.
  const double turbid = std::clamp(slot.staged.c[1] / 2.0, 0.05, 3.0);
  u.motes[0] = static_cast<float>(std::min(0.9, l.motes * turbid));
  u.motes[1] = static_cast<float>(l.mote_size);
  u.motes[2] = static_cast<float>(l.waterline);
  u.motes[3] = 0;
  u.lit[0] = static_cast<float>(l.ambient);
  u.lit[1] = static_cast<float>(l.caustics);
  // The shafts only where the look has them, the water's quality lets the sun under it and the
  // body takes the sun (the shader's own test, made here to skip their pass).
  under.shafts_on = l.shafts > 0 && water.quality.sunlight && slot.staged.sun[0] > 0 &&
                    under.shafts_view != nullptr;
  u.lit[2] = static_cast<float>(under.shafts_on ? l.shafts : 0.0);
  u.lit[3] = static_cast<float>(tops);
  device.queue.WriteBuffer(under.uniform, 0, &u, sizeof u);
}

void encode_underwater(const Underwater& under, const gpu::PipelineSet& pipelines,
                       FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                       const FrameTargets& targets, const wgpu::BindGroup& frame) {
  if (!under.active || !under.shafts_on) {
    // The table's rows run empty, so every later pass keeps its timing row.
    rec.compute(encoder, "water.shafts").End();
  } else {
    wgpu::RenderPassColorAttachment shafts{};
    shafts.view = under.shafts_view;
    shafts.loadOp = wgpu::LoadOp::Clear;
    shafts.storeOp = wgpu::StoreOp::Store;
    shafts.clearValue = {0, 0, 0, 0};
    wgpu::RenderPassDescriptor desc{};
    desc.colorAttachmentCount = 1;
    desc.colorAttachments = &shafts;
    wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "water.shafts");
    pass.SetPipeline(pipelines.render(under.p_shafts));
    pass.SetBindGroup(0, frame);
    pass.SetBindGroup(1, under.group);
    pass.Draw(3);
    ++rec.current().draws;
    pass.End();
  }
  if (!under.active) {
    rec.compute(encoder, "water.under").End();
    return;
  }
  wgpu::RenderPassColorAttachment colour{};
  colour.view = targets.hdr_view;
  colour.loadOp = wgpu::LoadOp::Load;
  colour.storeOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &colour;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "water.under");
  pass.SetPipeline(pipelines.render(under.p_under));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, under.group);
  pass.SetBindGroup(2, under.shafts_group);
  pass.Draw(3);
  ++rec.current().draws;
  pass.End();
}

}  // namespace ge::render
