// The wet lens's state, pipeline and pass (wet_lens.hpp).
//
// ge:summary The wet lens's drops stepped on the CPU, its pipeline, uniforms and pass
// ge:keywords render wet-lens drops sheet step pipeline pass uniforms
// ge:skill render water
// ge:test tests/render_wet_lens_test.cpp
#include "render/wet_lens.hpp"

#include <algorithm>
#include <cmath>

#include "render/wet_lens.wgsl.hpp"

namespace ge::render {

namespace {

// A drop of radius `r` dries in its share of the look's life: a bead in a fraction of it.
double life_of(const WetLensLook& l, core::Rng& rng, double r) {
  return l.life * (0.25 + 0.75 * std::min(1.0, r / l.big)) * core::range(rng, 0.8, 1.2);
}

// A radius, more small drops than big: the square of a uniform number spread over the range.
double radius_of(const WetLensLook& l, core::Rng& rng, double big) {
  const double u = core::next(rng);
  return l.small + (big - l.small) * u * u;
}

// The radius a drop has dried to, `age` of its `life` s gone: its volume going as the time
// left, so its radius as the cube root of it (approximated by the square root, which keeps it
// visible a little longer and needs no transcendental).
double dried(const LensDrop& d) {
  const double left = 1 - d.age / d.life;
  return left <= 0 ? 0 : d.r * std::sqrt(left);
}

}  // namespace

double sheet_edge(const WetLensLook& look, double since) {
  if (since <= 0) return 0;
  const double s = since / std::max(look.drain, 1e-3);
  return std::min(1.0, s * s);
}

void spatter_wet_lens(WetLensState& lens, int count, double size) {
  if (lens.under) return;
  const WetLensLook& l = lens.look;
  for (int i = 0; i < count; ++i) {
    LensDrop d;
    d.x = core::next(lens.rng);
    d.y = core::next(lens.rng);
    d.r = radius_of(l, lens.rng, std::max(size, l.small));
    d.born = lens.since;
    d.life = life_of(l, lens.rng, d.r);
    lens.drops.push_back(d);
  }
}

void step_wet_lens(WetLensState& lens, bool under, double dt) {
  const WetLensLook& l = lens.look;
  if (under) {
    lens.under = true;
    lens.drops.clear();
    lens.since = 0;
    return;
  }
  if (lens.under) {
    // Out of the water: the sheet starts down, the drops it leaves born as its edge passes.
    lens.under = false;
    lens.since = 0;
    lens.drops.clear();
    for (int i = 0; i < l.drops; ++i) {
      LensDrop d;
      d.x = core::next(lens.rng);
      d.y = core::next(lens.rng);
      d.r = radius_of(l, lens.rng, l.big);
      d.born = l.drain * std::sqrt(d.y);  // when sheet_edge reaches it
      d.life = life_of(l, lens.rng, d.r);
      lens.drops.push_back(d);
    }
    return;
  }
  lens.since += dt;
  std::vector<LensDrop> beads;
  for (LensDrop& d : lens.drops) {
    if (lens.since < d.born) continue;
    d.age += dt;
    const double r = dried(d);
    // Over the size that clings, a drop slides, faster the bigger, and wanders sideways.
    d.speed = r > l.stick ? l.slide * (r / l.stick - 1) : 0;
    if (d.speed > 0) {
      const double run = d.speed * dt;
      d.y += run;
      d.x += run * 0.15 * (core::next(lens.rng) - 0.5);
      d.ran += run;
      if (d.ran >= l.bead) {
        // A bead left behind, a tenth of the drop's water: the drop loses as much.
        d.ran = 0;
        LensDrop b;
        b.x = d.x;
        b.y = d.y - 0.5 * r;
        b.r = 0.45 * r;
        b.born = lens.since;
        b.life = 0.5 * life_of(l, lens.rng, b.r);
        beads.push_back(b);
        d.r *= 0.97;
      }
    }
  }
  lens.drops.insert(lens.drops.end(), beads.begin(), beads.end());
  std::erase_if(lens.drops, [&](const LensDrop& d) {
    return (lens.since >= d.born && d.age >= d.life) || d.y - d.r > 1.05;
  });
}

bool wet(const WetLensState& lens) {
  return !lens.under && (sheet_edge(lens.look, lens.since) < 1 || !lens.drops.empty());
}

void create_wet_lens(gpu::Device& device, gpu::PipelineSet& pipelines, WetLens& lens,
                     const wgpu::BindGroupLayout& frame) {
  lens.layout = make_layout(device, "water.lens", {Slot::uniform, Slot::texture},
                            wgpu::ShaderStage::Fragment);
  lens.uniform = make_buffer(device, "water.lens", sizeof(WetLensGpu), wgpu::BufferUsage::Uniform);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::wet_lens, "water.lens");
  wgpu::ColorTargetState colour{};
  colour.format = kHdrFormat;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 1;
  fragment.targets = &colour;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "water.lens";
  desc.layout = make_pipeline_layout(device, "water.lens", {frame, lens.layout});
  desc.vertex.module = module;
  desc.vertex.entryPoint = "vs_main";
  desc.fragment = &fragment;
  lens.p_draw = pipelines.add_render(device, desc);
}

void create_wet_lens_target(gpu::Device& device, WetLens& lens, const wgpu::TextureView& input,
                            uint32_t width, uint32_t height) {
  const auto usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
  lens.target = make_texture(device, "water.lens", width, height, 1, kHdrFormat, usage);
  lens.target_view = lens.target.CreateView();
  lens.input = input;
  lens.group = make_group(device, "water.lens", lens.layout, {lens.uniform, input});
}

void prepare_wet_lens(gpu::Device& device, WetLens& lens, double time) {
  const WetLensState& s = lens.state;
  lens.active = wet(s);
  if (!lens.active) return;
  // The drops born, the biggest first when there are more than the pass draws.
  std::vector<const LensDrop*> shown;
  for (const LensDrop& d : s.drops) {
    if (s.since >= d.born && dried(d) > 0) shown.push_back(&d);
  }
  std::stable_sort(shown.begin(), shown.end(),
                   [](const LensDrop* a, const LensDrop* b) { return dried(*a) > dried(*b); });
  const size_t n = std::min(shown.size(), static_cast<size_t>(kLensDrops));
  WetLensGpu& u = lens.staged;
  u.info[0] = static_cast<float>(sheet_edge(s.look, s.since));
  u.info[1] = static_cast<float>(time);
  u.info[2] = static_cast<float>(n);
  u.info[3] = static_cast<float>(s.look.refract);
  for (size_t i = 0; i < n; ++i) {
    const LensDrop& d = *shown[i];
    u.drops[i][0] = static_cast<float>(d.x);
    u.drops[i][1] = static_cast<float>(d.y);
    u.drops[i][2] = static_cast<float>(dried(d));
    // A sliding drop drawn longer than wide, by its speed.
    u.drops[i][3] = static_cast<float>(std::min(1.5, d.speed * 1.5));
  }
  device.queue.WriteBuffer(lens.uniform, 0, &u, sizeof u);
}

void encode_wet_lens(const WetLens& lens, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                     wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame) {
  if (!lens.active) {
    // The table's row runs empty, so every later pass keeps its timing row.
    rec.compute(encoder, "water.lens").End();
    return;
  }
  wgpu::RenderPassColorAttachment colour{};
  colour.view = lens.target_view;
  colour.loadOp = wgpu::LoadOp::Clear;
  colour.storeOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &colour;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "water.lens");
  pass.SetPipeline(pipelines.render(lens.p_draw));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, lens.group);
  pass.Draw(3);
  ++rec.current().draws;
  pass.End();
}

const wgpu::TextureView& wet_lens_view(const WetLens& lens) {
  return lens.active ? lens.target_view : lens.input;
}

}  // namespace ge::render
