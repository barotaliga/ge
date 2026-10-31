// Lava and slime drawn: the looks, the scene copies, the bodies, the pipelines and the passes.
//
// ge:summary The liquid pass's named looks, resources, pipelines and passes
// ge:keywords render liquid lava slime looks glow-table ripples scene-copy pipelines passes
// ge:skill render water
// ge:test tests/render_liquid_test.cpp
#include "render/liquid.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include "render/liquid.wgsl.hpp"
#include "render/water_copy.wgsl.hpp"
#include "water/glow.hpp"
#include "water/liquid.hpp"

namespace ge::render {

namespace {

constexpr double kKelvin = 273.15;
constexpr uint32_t kRippleSeed = 0x5117u;

LiquidLook lava(double cover, double crack, double plate, double ropes, double cold,
                double age_rate) {
  LiquidLook l;
  l.kind = LiquidKind::lava;
  l.lava.cover = cover;
  l.lava.crack = crack;
  l.lava.plate = plate;
  l.lava.ropes = ropes;
  l.lava.crust_cold = cold;
  l.lava.age_rate = age_rate;
  return l;
}

LiquidLook slime(water::Rgb absorb, water::Rgb scatter, double rough) {
  LiquidLook l;
  l.kind = LiquidKind::slime;
  l.slime.absorb = absorb;
  l.slime.scatter = scatter;
  l.slime.rough = rough;
  return l;
}

struct Named {
  std::string_view name;
  LiquidLook look;
};

std::array<Named, 7> build() {
  // Lava: a molten lake with its crust breaking into plates; the same crusted over near its
  // solidus.
  LiquidLook molten = lava(0.62, 0.07, 1.6, 1, 380, 0.02);
  LiquidLook crusted = lava(0.93, 0.035, 2.4, 0.3, 300, 0.008);
  crusted.lava.core = 1100;
  crusted.lava.crust_hot = 720;
  // Toxic: an opaque green, faintly glowing, bubbling.
  LiquidLook toxic = slime({150, 6, 240}, {30, 60, 30}, 0.12);
  toxic.slime.glow = {6, 60, 4};
  toxic.slime.pulse = 0.3;
  toxic.slime.bubbles = 1.5;
  toxic.slime.bubble_size = 0.08;
  // A clear gel: little scattering, a green-cyan tint over what lies in it, glassy.
  LiquidLook gel = slime({0.45, 0.08, 0.25}, {0.06, 0.06, 0.06}, 0.03);
  // A pale mucus: milky, yellow-green, very glossy, a few small bubbles.
  LiquidLook mucus = slime({3, 2, 12}, {20, 20, 20}, 0.04);
  mucus.slime.bubbles = 2;
  mucus.slime.bubble_size = 0.035;
  // A sludge: olive-brown, matte under its scum, bubbling, an oily sheen between the patches.
  LiquidLook sludge = slime({60, 50, 150}, {50, 50, 50}, 0.3);
  sludge.slime.scum = 0.45;
  sludge.slime.scum_albedo = {0.30, 0.28, 0.18};
  sludge.slime.bubbles = 3;
  sludge.slime.bubble_size = 0.05;
  sludge.slime.film = 320;
  // Tar: black glass with an oil's rainbow, a rare slow bubble.
  LiquidLook tar = slime({5000, 5000, 5000}, {0.5, 0.5, 0.5}, 0.05);
  tar.slime.film = 420;
  tar.slime.bubbles = 0.08;
  tar.slime.bubble_size = 0.14;
  return {{{"lava", molten},
           {"lava-crusted", crusted},
           {"slime", toxic},
           {"slime-gel", gel},
           {"slime-mucus", mucus},
           {"slime-sludge", sludge},
           {"slime-tar", tar}}};
}

const std::array<Named, 7>& rows() {
  static const std::array<Named, 7> table = build();
  return table;
}

void put(float* out, double a, double b, double c, double d = 0) {
  out[0] = static_cast<float>(a);
  out[1] = static_cast<float>(b);
  out[2] = static_cast<float>(c);
  out[3] = static_cast<float>(d);
}
void put(float* out, const water::Rgb& c, double w = 0) { put(out, c.r, c.g, c.b, w); }

void stage_body(Liquids::Slot& slot, const water::Body& body, const LiquidLook& look, int ripples) {
  LiquidGpuBody& b = slot.staged;
  put(b.info, body.level, look.kind == LiquidKind::lava ? 0 : 1, ripples, 0);
  for (int i = 0; i < 4; ++i) b.box[i] = static_cast<float>(body.box[i]);
  put(b.flow, look.flow[0], look.flow[1], look.open_sky, look.room);
  const LavaLook& l = look.lava;
  put(b.lava0, l.core + kKelvin, l.crust_hot + kKelvin, l.crust_cold + kKelvin, l.cover);
  put(b.lava1, l.plate, l.crack, l.ropes, l.age_rate);
  put(b.lava2, l.emissivity, l.crust_rough, l.melt_rough, l.patch > 0 ? l.patch : 7 * l.plate);
  put(b.crust, l.crust_albedo);
  const SlimeLook& s = look.slime;
  put(b.absorb, s.absorb);
  put(b.scatter, s.scatter);
  put(b.glow, s.glow, s.pulse);
  put(b.slime0, s.rough, s.film, s.scum, s.bubbles);
  put(b.slime1, s.bubble_size, s.grime, s.debris, 0);
  put(b.scum, s.scum_albedo);
  b.fx[1] = static_cast<float>(look.kind == LiquidKind::lava ? l.shimmer : 0);
  const std::vector<water::Rgb> table = water::glow_table(kGlowLo, kGlowHi, kGlowEntries, 1.0);
  for (int i = 0; i < kGlowEntries; ++i) put(b.table[i], table[static_cast<size_t>(i)]);
}

// The ripples a body draws: the wind's sea over its fetch cut by its liquid (none on lava).
int upload_ripples(gpu::Device& device, Liquids::Slot& slot, const water::Body& body,
                   const LiquidLook& look, double wind, double toward, double fetch) {
  std::vector<float> data(size_t{kLiquidRipples} * 8, 0.0f);
  int n = 0;
  if (look.kind == LiquidKind::slime && wind > 0) {
    const std::vector<water::Component> r = water::ripples(
        body.stuff.liquid, body.stuff.density, wind, toward, fetch, kLiquidRipples, kRippleSeed);
    for (const water::Component& c : r) {
      if (c.a <= 0) continue;
      float* w = &data[static_cast<size_t>(n) * 8];
      put(w, c.kx, c.kz, c.a, c.w);
      put(w + 4, std::fmod(c.phase, 6.283185307179586), c.k, 0, 0);
      ++n;
    }
  }
  device.queue.WriteBuffer(slot.ripples, 0, data.data(), data.size() * sizeof(float));
  return n;
}

const std::array<std::string_view, 7>& names() {
  static const std::array<std::string_view, 7> out = [] {
    std::array<std::string_view, 7> n{};
    for (size_t i = 0; i < rows().size(); ++i) n[i] = rows()[i].name;
    return n;
  }();
  return out;
}

}  // namespace

const LiquidLook* find_liquid_look(std::string_view name) {
  for (const Named& n : rows())
    if (n.name == name) return &n.look;
  return nullptr;
}

std::span<const std::string_view> liquid_look_names() { return names(); }

double adapt_for(const LiquidLook& look, double key) {
  if (look.kind == LiquidKind::slime) {
    const water::Rgb& g = look.slime.glow;
    return key * (0.2126 * g.r + 0.7152 * g.g + 0.0722 * g.b);
  }
  const LavaLook& l = look.lava;
  return key * water::glow_of(l.core + kKelvin, l.emissivity).luminance;
}

water::Rgb liquid_glow(const LiquidLook& look) {
  if (look.kind == LiquidKind::slime) return look.slime.glow;
  const LavaLook& l = look.lava;
  const water::Glow melt = water::glow_of(l.core + kKelvin, l.emissivity);
  const water::Glow skin = water::glow_of(l.crust_hot + kKelvin, l.emissivity);
  const double open = std::clamp(1 - l.cover, 0.0, 1.0);
  const double a = open * melt.luminance, b = (1 - open) * kCrustGlowShare * skin.luminance;
  return {a * melt.colour.r + b * skin.colour.r, a * melt.colour.g + b * skin.colour.g,
          a * melt.colour.b + b * skin.colour.b};
}

void liquid_lamps(const water::Body& body, const LiquidLook& look, std::vector<LocalLight>& out,
                  double cell_m) {
  const water::Rgb glow = liquid_glow(look);
  const double lum = 0.2126 * glow.r + 0.7152 * glow.g + 0.0722 * glow.b;
  const double w = body.box[2] - body.box[0], d = body.box[3] - body.box[1];
  if (lum <= 0 || !(w > 0) || !(d > 0)) return;
  // The grid: cells of `cell_m`, grown until there are no more than the most.
  int nx = std::max(1, static_cast<int>(std::ceil(w / cell_m)));
  int nz = std::max(1, static_cast<int>(std::ceil(d / cell_m)));
  while (nx * nz > kLiquidLampsMost) {
    if (nx >= nz) {
      --nx;
    } else {
      --nz;
    }
  }
  const double cw = w / nx, cd = d / nz, area = cw * cd;
  const double radius = std::sqrt(area / 3.14159265358979323846);
  LocalLight lamp;
  lamp.colour[0] = static_cast<float>(glow.r / lum);
  lamp.colour[1] = static_cast<float>(glow.g / lum);
  lamp.colour[2] = static_cast<float>(glow.b / lum);
  lamp.candela = static_cast<float>(lum * area * 0.5);
  lamp.radius_m = static_cast<float>(radius);
  // Out to where it lights a face at a tenth of a lux, past the cell's own reach, and no further
  // than a frame's lamps reach (kLiquidLampReach): a lake's glow on the cloud over it is the sky's.
  lamp.range_m = static_cast<float>(
      std::max(std::min(std::sqrt(lamp.candela / 0.1), kLiquidLampReach), 4 * radius));
  for (int i = 0; i < nx; ++i) {
    for (int k = 0; k < nz; ++k) {
      lamp.position[0] = body.box[0] + (i + 0.5) * cw;
      lamp.position[1] = body.level + radius;
      lamp.position[2] = body.box[1] + (k + 0.5) * cd;
      out.push_back(lamp);
    }
  }
}

void create_liquid(gpu::Device& device, gpu::PipelineSet& pipelines, Liquids& liquids,
                   const wgpu::BindGroupLayout& frame) {
  liquids.layout = make_layout(
      device, "liquid", {Slot::uniform, Slot::storage_read, Slot::texture, Slot::texture_raw},
      wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
  liquids.copy_layout =
      make_layout(device, "liquid.copy", {Slot::texture, Slot::depth}, wgpu::ShaderStage::Fragment);
  {
    const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::water_copy, "liquid.copy");
    wgpu::ColorTargetState colours[2] = {};
    colours[0].format = kHdrFormat;
    colours[1].format = wgpu::TextureFormat::R32Float;
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 2;
    fragment.targets = colours;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "liquid.copy";
    desc.layout = make_pipeline_layout(device, "liquid.copy", {liquids.copy_layout});
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs_main";
    desc.fragment = &fragment;
    liquids.p_copy = pipelines.add_render(device, desc);
  }
  {
    const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::liquid, "liquid");
    wgpu::ConstantEntry grid{};
    grid.key = "kGrid";
    grid.value = kLiquidGrid;
    wgpu::ColorTargetState colours[2] = {};
    colours[0].format = kHdrFormat;
    colours[1].format = kMotionFormat;
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "fs_main";
    fragment.constantCount = 1;
    fragment.constants = &grid;
    fragment.targetCount = 2;
    fragment.targets = colours;
    wgpu::DepthStencilState depth{};
    depth.format = kDepthFormat;
    depth.depthWriteEnabled = true;
    depth.depthCompare = wgpu::CompareFunction::Greater;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "liquid.draw";
    desc.layout = make_pipeline_layout(device, "liquid.draw", {frame, liquids.layout});
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs_main";
    desc.vertex.constantCount = 1;
    desc.vertex.constants = &grid;
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.primitive.cullMode = wgpu::CullMode::None;
    desc.depthStencil = &depth;
    desc.fragment = &fragment;
    liquids.p_draw = pipelines.add_render(device, desc);
    // The shimmer: the box of hot air over a body, both its faces (a pixel's colour is the same
    // from either, so an eye inside it draws it too), no depth test (the fragment clips its ray by
    // the scene's depth copy), the motion kept.
    fragment.entryPoint = "fs_shimmer";
    colours[1].writeMask = wgpu::ColorWriteMask::None;
    depth.depthWriteEnabled = false;
    depth.depthCompare = wgpu::CompareFunction::Always;
    fragment.constantCount = 0;
    desc.label = "liquid.shimmer";
    desc.vertex.entryPoint = "vs_shimmer";
    desc.vertex.constantCount = 0;
    liquids.p_shimmer = pipelines.add_render(device, desc);
    // The view from under a body: one triangle over the screen, the same targets and depth.
    fragment.entryPoint = "fs_under";
    desc.label = "liquid.under";
    desc.vertex.entryPoint = "vs_under";
    liquids.p_under = pipelines.add_render(device, desc);
  }
  std::vector<uint32_t> idx;
  idx.reserve(size_t{kLiquidGrid} * kLiquidGrid * 6);
  const uint32_t n = kLiquidGrid + 1;
  for (uint32_t y = 0; y < kLiquidGrid; ++y) {
    for (uint32_t x = 0; x < kLiquidGrid; ++x) {
      const uint32_t a = y * n + x, b = a + 1, c = a + n, d = c + 1;
      idx.insert(idx.end(), {a, c, b, b, c, d});
    }
  }
  liquids.indices = make_buffer(device, "liquid.indices", idx.size() * sizeof(uint32_t),
                                wgpu::BufferUsage::Index);
  device.queue.WriteBuffer(liquids.indices, 0, idx.data(), idx.size() * sizeof(uint32_t));
}

void create_liquid_targets(gpu::Device& device, Liquids& liquids, const FrameTargets& targets) {
  const auto usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
  liquids.scene_colour = make_texture(device, "liquid.scene.colour", targets.width, targets.height,
                                      1, kHdrFormat, usage);
  liquids.scene_depth = make_texture(device, "liquid.scene.depth", targets.width, targets.height, 1,
                                     wgpu::TextureFormat::R32Float, usage);
  liquids.scene_colour_view = liquids.scene_colour.CreateView();
  liquids.scene_depth_view = liquids.scene_depth.CreateView();
  liquids.copy_group = make_group(device, "liquid.copy", liquids.copy_layout,
                                  {targets.hdr_view, targets.depth_view});
  for (Liquids::Slot& slot : liquids.bodies) {
    slot.group = make_group(
        device, "liquid", liquids.layout,
        {slot.uniform, slot.ripples, liquids.scene_colour_view, liquids.scene_depth_view});
  }
}

int add_liquid_body(gpu::Device& device, Liquids& liquids, const water::Body& body,
                    const LiquidLook& look, double wind, double toward, double fetch) {
  Liquids::Slot slot;
  slot.uniform =
      make_buffer(device, "liquid.body", sizeof(LiquidGpuBody), wgpu::BufferUsage::Uniform);
  slot.ripples = make_buffer(device, "liquid.ripples", size_t{kLiquidRipples} * 32,
                             wgpu::BufferUsage::Storage);
  const int n = upload_ripples(device, slot, body, look, wind, toward, fetch);
  stage_body(slot, body, look, n);
  if (liquids.scene_colour_view) {
    slot.group = make_group(
        device, "liquid", liquids.layout,
        {slot.uniform, slot.ripples, liquids.scene_colour_view, liquids.scene_depth_view});
  }
  liquids.bodies.push_back(std::move(slot));
  return static_cast<int>(liquids.bodies.size()) - 1;
}

void set_liquid_body(gpu::Device& device, Liquids& liquids, int index, const water::Body& body,
                     const LiquidLook& look, double wind, double toward, double fetch) {
  Liquids::Slot& slot = liquids.bodies[static_cast<size_t>(index)];
  const int n = upload_ripples(device, slot, body, look, wind, toward, fetch);
  stage_body(slot, body, look, n);
}

void prepare_liquid(gpu::Device& device, Liquids& liquids, const FrameView& view) {
  liquids.eye_under = -1;
  for (Liquids::Slot& slot : liquids.bodies) {
    const LiquidGpuBody& b = slot.staged;
    slot.shown = !slot.hidden && view.eye[1] > b.info[0];
    const bool inside = view.eye[0] > b.box[0] && view.eye[0] < b.box[2] &&
                        view.eye[2] > b.box[1] && view.eye[2] < b.box[3];
    if (!slot.hidden && !slot.shown && inside && liquids.eye_under < 0)
      liquids.eye_under = static_cast<int>(&slot - liquids.bodies.data());
    slot.staged.fx[0] = liquids.reflections ? 1.0f : 0.0f;
    slot.staged.fx[3] = static_cast<float>(liquids.under_reach);
    device.queue.WriteBuffer(slot.uniform, 0, &slot.staged, sizeof slot.staged);
  }
}

void break_liquid_crust(Liquids& liquids, int index, double x, double z, double radius, double t) {
  Liquids::Slot& slot = liquids.bodies[static_cast<size_t>(index)];
  put(slot.staged.breaks[slot.next_break], x, z, radius, t);
  slot.next_break = (slot.next_break + 1) % kLiquidBreaks;
  slot.staged.fx[2] = std::min(slot.staged.fx[2] + 1.0f, static_cast<float>(kLiquidBreaks));
}

void encode_liquid(const Liquids& liquids, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                   const wgpu::BindGroup& frame) {
  const bool drawn = liquids.eye_under >= 0 ||
                     std::any_of(liquids.bodies.begin(), liquids.bodies.end(),
                                 [](const Liquids::Slot& s) { return s.shown; });
  if (!drawn) {
    rec.compute(encoder, "liquid.copy").End();
    rec.compute(encoder, "liquid").End();
    return;
  }
  {
    wgpu::RenderPassColorAttachment colours[2]{};
    colours[0].view = liquids.scene_colour_view;
    colours[1].view = liquids.scene_depth_view;
    for (auto& c : colours) {
      c.loadOp = wgpu::LoadOp::Clear;
      c.storeOp = wgpu::StoreOp::Store;
    }
    wgpu::RenderPassDescriptor desc{};
    desc.colorAttachmentCount = 2;
    desc.colorAttachments = colours;
    wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "liquid.copy");
    pass.SetPipeline(pipelines.render(liquids.p_copy));
    pass.SetBindGroup(0, liquids.copy_group);
    pass.Draw(3);
    ++rec.current().draws;
    pass.End();
  }
  wgpu::RenderPassColorAttachment colours[2]{};
  colours[0].view = targets.hdr_view;
  colours[1].view = targets.motion_view;
  for (auto& c : colours) {
    c.loadOp = wgpu::LoadOp::Load;
    c.storeOp = wgpu::StoreOp::Store;
  }
  wgpu::RenderPassDepthStencilAttachment depth{};
  depth.view = targets.depth_view;
  depth.depthLoadOp = wgpu::LoadOp::Load;
  depth.depthStoreOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 2;
  desc.colorAttachments = colours;
  desc.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "liquid");
  pass.SetPipeline(pipelines.render(liquids.p_draw));
  pass.SetBindGroup(0, frame);
  pass.SetIndexBuffer(liquids.indices, wgpu::IndexFormat::Uint32);
  for (const Liquids::Slot& slot : liquids.bodies) {
    if (!slot.shown) continue;
    pass.SetBindGroup(1, slot.group);
    pass.DrawIndexed(kLiquidGrid * kLiquidGrid * 6);
    ++rec.current().draws;
  }
  // The shimmer over lava: the scene across it seen again through the bent rays.
  pass.SetPipeline(pipelines.render(liquids.p_shimmer));
  for (const Liquids::Slot& slot : liquids.bodies) {
    if (!slot.shown || slot.staged.info[1] != 0 || !(slot.staged.fx[1] > 0)) continue;
    pass.SetBindGroup(1, slot.group);
    pass.Draw(36);
    ++rec.current().draws;
  }
  if (liquids.eye_under >= 0) {
    pass.SetPipeline(pipelines.render(liquids.p_under));
    pass.SetBindGroup(1, liquids.bodies[static_cast<size_t>(liquids.eye_under)].group);
    pass.Draw(3);
    ++rec.current().draws;
  }
  pass.End();
}

}  // namespace ge::render
