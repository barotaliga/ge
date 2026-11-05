// The water's detail band, its scene copies, bodies, pipelines and passes.
//
// ge:summary The water pass's detail waves, resources, pipelines, levels and passes
// ge:keywords render water detail-waves spectrum-tail scene-copy pipelines levels passes
// ge:skill render water
// ge:test tests/render_water_test.cpp
#include "render/water.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "render/water.wgsl.hpp"
#include "render/water_below.wgsl.hpp"
#include "render/water_copy.wgsl.hpp"
#include "render/water_shore.wgsl.hpp"
#include "render/water_stream.hpp"
#include "render/water_stream.wgsl.hpp"
#include "water/spectrum.hpp"

namespace ge::render {

namespace {

namespace lm = core::libm;
constexpr double kPi = 3.14159265358979323846;
constexpr uint32_t kWaveSlots = water::kMaxComponents + kWaterDetailWaves;
// How far down a covered coarse vertex sinks, in its level's spacings; where the morph starts.
constexpr float kSink = 2.0f;
constexpr float kMorphStart = 0.8f;

// The deep-water wavenumber of an angular frequency.
double deep_k(double w) { return w * w / water::kGravity; }
double deep_w(double length) { return std::sqrt(water::kGravity * 2 * kPi / length); }

// The lowest a volume's water reaches: its hulls' lowest floor. A lake, an ocean or a river is
// open below (its bed is the ground), so an eye anywhere under its level is in it.
double floor_of(const water::Body& body) {
  if (body.kind != water::Kind::volume) return -1e300;
  double floor = body.level;
  for (const water::Hull& hull : body.hulls) {
    for (const water::Plane& p : hull.planes) {
      if (p.ny < 0 && p.nx == 0 && p.nz == 0) floor = std::min(floor, p.d / p.ny);
    }
  }
  return floor;
}

void stage_body(Water::Slot& slot, const water::Body& body) {
  slot.floor = floor_of(body);
  WaterGpuBody& b = slot.staged;
  b.info[0] = static_cast<float>(body.level);
  b.info[1] = 1.0f;
  b.info[2] = static_cast<float>(body.waves.count);
  for (int i = 0; i < 4; ++i) b.box[i] = static_cast<float>(body.box[i]);
  b.grid[0] = static_cast<float>(std::clamp(slot.draw.levels, 1, kWaterLevels));
  b.grid[1] = kSink;
  b.grid[2] = kMorphStart;
  b.grid[3] = static_cast<float>(slot.draw.rim);
  b.rough[1] = static_cast<float>(slot.draw.calm_mss);
  b.rough[2] = static_cast<float>(slot.draw.refraction);
  b.rough[3] = static_cast<float>(slot.draw.open_sky);
  const water::Optics& o = body.optics;
  const auto put = [](float* out, const water::Rgb& c) {
    out[0] = static_cast<float>(c.r);
    out[1] = static_cast<float>(c.g);
    out[2] = static_cast<float>(c.b);
    out[3] = 0;
  };
  put(b.c, o.c);
  put(b.kd, o.kd);
  put(b.deep, o.deep);
  put(b.emission, o.emission);
  b.emission[3] = static_cast<float>(slot.draw.room);
  put(slot.veil, o.veil);
  const WaterDraw& d = slot.draw;
  b.air[0] = static_cast<float>(d.daylight < 0 ? d.open_sky : d.daylight);
  b.sun[0] = static_cast<float>(d.sunlit < 0 ? b.air[0] : d.sunlit);
  b.air[1] = static_cast<float>(d.stir);
  b.air[2] = static_cast<float>(d.gusts);
  // The gusts drift downwind at about the speed of the waves the wind grows most (the peak's
  // phase speed, g Tp / 2 pi, is a little over the wind's own at these fetches; take most of it).
  const water::Sea& sea = body.waves.sea;
  b.air[3] = static_cast<float>(sea.tp > 0 ? 0.8 * water::kGravity * sea.tp / (2 * kPi) : 0.0);
  b.view[2] = static_cast<float>(lm::sin(sea.toward));
  b.view[3] = static_cast<float>(lm::cos(sea.toward));
}

void upload_waves(gpu::Device& device, Water::Slot& slot, const water::Body& body) {
  const WaterDetail detail =
      detail_waves(body.waves, kWaterDetailWaves, slot.draw.seed + 977u * slot.draw.seed);
  slot.staged.info[3] = static_cast<float>(detail.waves.size());
  slot.staged.rough[0] = static_cast<float>(detail.tail_mss);
  WaterFoam foam = foam_of(body.waves, detail.waves);
  if (!slot.draw.foam) {
    foam.fold = -1e9;
    foam.cover = 0;
    foam.hs = 0;
  }
  // A run-up under the ground's lowest reach (-0.05 m) leaves no band.
  if (!slot.draw.wet_band) foam.runup = -1;
  slot.staged.foam[0] = static_cast<float>(foam.fold);
  slot.staged.foam[1] = static_cast<float>(foam.cover);
  slot.staged.foam[2] = static_cast<float>(foam.drift * lm::sin(body.waves.sea.toward));
  slot.staged.foam[3] = static_cast<float>(foam.drift * lm::cos(body.waves.sea.toward));
  slot.staged.surf[0] = static_cast<float>(foam.hs);
  slot.staged.surf[1] = static_cast<float>(foam.runup);
  slot.staged.surf[2] = static_cast<float>(foam.fold_length);
  std::vector<float> data(size_t{kWaveSlots} * 8, 0.0f);
  size_t i = 0;
  const auto put = [&](const water::Component& c) {
    float* w = &data[i * 8];
    w[0] = static_cast<float>(c.kx);
    w[1] = static_cast<float>(c.kz);
    w[2] = static_cast<float>(c.a);
    w[3] = static_cast<float>(c.w);
    // The phase taken round once, so a float keeps its bits of it.
    w[4] = static_cast<float>(std::fmod(c.phase, 2 * kPi));
    w[5] = static_cast<float>(c.k);
    ++i;
  };
  for (int c = 0; c < body.waves.count; ++c) put(body.waves.c[c]);
  for (const water::Component& c : detail.waves) put(c);
  slot.mss = detail.mss + detail.tail_mss + slot.draw.stir * slot.draw.stir;
  for (int c = 0; c < body.waves.count; ++c) {
    const double ka = body.waves.c[c].k * body.waves.c[c].a;
    slot.mss += 0.5 * ka * ka;
  }
  device.queue.WriteBuffer(slot.waves, 0, data.data(), data.size() * sizeof(float));
}

// A body's group: its uniform, waves and rings, the scene's copies, its shore, and the game's views
// where they are of it (else the blank stand-ins).
// Whether the game's mirror serves this body: it is the one the views are of, or (seen from over
// the surface) it lies in the mirror's plane.
bool mirrored(const WaterViews& v, const Water::Slot& slot, int index) {
  if (!v.has_mirror) return false;
  if (v.body == index) return true;
  return !v.below && std::fabs(slot.staged.info[0] - v.level) < 0.01;
}

void bind_body(gpu::Device& device, const Water& water, Water::Slot& slot, int index) {
  if (!water.scene_colour_view) return;
  const WaterViews& v = water.views;
  const bool ours = v.body == index;
  const bool mirror_on = mirrored(v, slot, index);
  const wgpu::TextureView mirror = mirror_on ? v.mirror : water.blank_colour_view;
  const wgpu::TextureView mirror_depth = mirror_on ? v.mirror_depth : water.blank_depth_view;
  const wgpu::TextureView up = ours && v.has_up ? v.up : water.blank_colour_view;
  slot.group =
      make_group(device, "water", water.layout,
                 {slot.uniform, slot.waves, water.scene_colour_view, water.scene_depth_view,
                  slot.rings, slot.shore_view, mirror, mirror_depth, up});
}

}  // namespace

WaterFoam foam_of(const water::Waves& waves, std::span<const water::Component> detail) {
  WaterFoam f;
  f.hs = 4 * std::sqrt(waves.m0);
  // A calm lake still laps its shore a few centimetres; a sea runs up most of its height.
  f.runup = 0.03 + 0.7 * f.hs;
  if (waves.count == 0 || waves.sea.hs <= 0) return f;
  // The waves that fold: every laid one, and the detail band's down to kFoamShortest m (faded in
  // from twice that, as water.wgsl's `fold_at` keeps them), so the whitecaps are the short steep
  // waves breaking where the long ones' crests carry them; and the folds' own size, the wavelength
  // their slopes weigh most (k weighted by (k a)^2).
  double var = 0, sk = 0;
  const auto fold = [&](const water::Component& c) {
    double keep = std::clamp((2 * kPi / c.k - kFoamShortest) / kFoamShortest, 0.0, 1.0);
    keep = keep * keep * (3 - 2 * keep);
    const double ka = keep * c.k * c.a;
    var += 0.5 * ka * ka;
    sk += 0.5 * ka * ka * c.k;
  };
  for (int i = 0; i < waves.count; ++i) fold(waves.c[i]);
  for (const water::Component& c : detail) fold(c);
  f.fold_length = sk > 0 ? 2 * kPi * var / sk : 0;
  f.wind = water::wind_of(waves.sea);
  f.cover = water::whitecap_cover(f.wind);
  // The foam drifts with the surface's water, about 3 % of the wind.
  f.drift = 0.03 * f.wind;
  // The Jacobian's trace is 1 - sum k a sin(phi), Gaussian of variance sum (k a)^2 / 2: the
  // fold under which `active` of the surface lies is 1 - sigma z, z the normal's quantile.
  const double active = kFoamActive * f.cover;
  if (active < 1e-6 || var <= 0) return f;
  double lo = 0, hi = 12;
  for (int i = 0; i < 60; ++i) {
    const double z = 0.5 * (lo + hi);
    (0.5 * std::erfc(z / std::sqrt(2.0)) > active ? lo : hi) = z;
  }
  f.fold = 1 - std::sqrt(var) * 0.5 * (lo + hi);
  return f;
}

WaterDetail detail_waves(const water::Waves& laid, int count, uint32_t seed, double shortest,
                         double finest) {
  WaterDetail out;
  const water::Sea& sea = laid.sea;
  if (sea.hs <= 0 || sea.tp <= 0 || count <= 0) return out;
  const double norm = water::normaliser(sea);
  const double w0 = laid.w_hi > 0 ? laid.w_hi : 2 * kPi / sea.tp * 3;
  const double w1 = deep_w(shortest), w2 = deep_w(finest);
  if (w1 <= w0) return out;
  core::Rng rng = core::make_rng(seed);
  // Slices even in the log of the frequency; each its energy at its middle.
  const double step = lm::log(w1 / w0) / count;
  for (int i = 0; i < count; ++i) {
    const double lo = w0 * lm::exp(step * i), hi = w0 * lm::exp(step * (i + 1));
    double e = 0;
    constexpr int kTaps = 8;
    for (int j = 0; j < kTaps; ++j) {
      const double w = lo + (hi - lo) * (j + 0.5) / kTaps;
      e += water::jonswap_at(sea, w, norm) * (hi - lo) / kTaps;
    }
    const double w = std::sqrt(lo * hi), k = deep_k(w);
    // The short waves spread wider than the peak: up to a right angle either side.
    const double off = (core::next(rng) * 2 - 1) * kPi * 0.5;
    const double heading = sea.toward + off;
    water::Component c;
    c.w = w;
    c.k = k;
    c.kx = k * lm::sin(heading);
    c.kz = k * lm::cos(heading);
    c.a = std::sqrt(2 * e);
    c.phase = core::next(rng) * 2 * kPi;
    out.mss += 0.5 * (k * c.a) * (k * c.a);
    out.waves.push_back(c);
  }
  // The tail's slope variance, the integral of k^2 S over the band past the detail's.
  constexpr int kTail = 64;
  const double tail = lm::log(w2 / w1) / kTail;
  for (int i = 0; i < kTail; ++i) {
    const double lo = w1 * lm::exp(tail * i), hi = w1 * lm::exp(tail * (i + 1));
    const double w = std::sqrt(lo * hi), k = deep_k(w);
    out.tail_mss += k * k * water::jonswap_at(sea, w, norm) * (hi - lo);
  }
  return out;
}

void create_water(gpu::Device& device, gpu::PipelineSet& pipelines, Water& water,
                  const wgpu::BindGroupLayout& frame) {
  water.layout =
      make_layout(device, "water",
                  {Slot::uniform, Slot::storage_read, Slot::texture, Slot::texture_raw,
                   Slot::storage_read, Slot::texture, Slot::texture, Slot::depth, Slot::texture},
                  wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
  {
    const auto usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    water.blank_colour = make_texture(device, "water.blank", 1, 1, 1, kHdrFormat, usage);
    water.blank_depth = make_texture(device, "water.blank.depth", 1, 1, 1, kDepthFormat, usage);
    water.blank_colour_view = water.blank_colour.CreateView();
    water.blank_depth_view = water.blank_depth.CreateView();
  }
  water.copy_layout =
      make_layout(device, "water.copy", {Slot::texture, Slot::depth}, wgpu::ShaderStage::Fragment);

  {
    const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::water_copy, "water.copy");
    wgpu::ColorTargetState colours[2] = {};
    colours[0].format = kHdrFormat;
    colours[1].format = wgpu::TextureFormat::R32Float;
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 2;
    fragment.targets = colours;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "water.copy";
    desc.layout = make_pipeline_layout(device, "water.copy", {water.copy_layout});
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs_main";
    desc.fragment = &fragment;
    water.p_copy = pipelines.add_render(device, desc);
  }
  {
    const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::water, "water");
    wgpu::ConstantEntry grid{};
    grid.key = "kGrid";
    grid.value = kWaterGrid;
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
    desc.label = "water.draw";
    desc.layout = make_pipeline_layout(device, "water.draw", {frame, water.layout});
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs_main";
    desc.vertex.constantCount = 1;
    desc.vertex.constants = &grid;
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.primitive.cullMode = wgpu::CullMode::None;
    desc.depthStencil = &depth;
    desc.fragment = &fragment;
    water.p_draw = pipelines.add_render(device, desc);
    // From below: the same geometry into the `below` targets, tested against the opaque depth
    // (a ceiling over a flooded passage hides the surface beyond it) and writing its own.
    // Its own module (render/water_below.wgsl over the same shared half): the two fragment stages
    // together would pass the embedding's 60 KB cap.
    const wgpu::ShaderModule below_module =
        gpu::create_shader(device, wgsl::water_below, "water.below");
    wgpu::ColorTargetState below[2] = {};
    below[0].format = wgpu::TextureFormat::RGBA16Float;
    below[1].format = wgpu::TextureFormat::R8Unorm;
    fragment.module = below_module;
    fragment.entryPoint = "fs_below";
    fragment.targets = below;
    desc.vertex.module = below_module;
    // Its covered coarse vertices rise rather than sink (water_surface.wgsl's kBelow), and the
    // nearest fragment, the true surface, is kept.
    wgpu::ConstantEntry below_grid[2]{};
    below_grid[0].key = "kGrid";
    below_grid[0].value = kWaterGrid;
    below_grid[1].key = "kBelow";
    below_grid[1].value = 1;
    desc.vertex.constantCount = 2;
    desc.vertex.constants = below_grid;
    desc.label = "water.below";
    water.p_below = pipelines.add_render(device, desc);
  }
  {
    // THE SHORE'S WET BAND: a full-screen triangle over the body's group, darkening the ground the
    // waves wet (the colour multiplied, the motion and the depth left alone). Its own module
    // (render/water_shore.wgsl over the shared half), drawn in the "water" pass before the bodies.
    const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::water_shore, "water.shore");
    wgpu::BlendState multiply{};
    multiply.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::Zero, wgpu::BlendFactor::Src};
    multiply.alpha = {wgpu::BlendOperation::Add, wgpu::BlendFactor::Zero, wgpu::BlendFactor::One};
    wgpu::ColorTargetState colours[2] = {};
    colours[0].format = kHdrFormat;
    colours[0].blend = &multiply;
    colours[1].format = kMotionFormat;
    colours[1].writeMask = wgpu::ColorWriteMask::None;
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "fs_shore";
    fragment.targetCount = 2;
    fragment.targets = colours;
    wgpu::DepthStencilState depth{};
    depth.format = kDepthFormat;
    depth.depthWriteEnabled = false;
    depth.depthCompare = wgpu::CompareFunction::Always;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "water.shore";
    desc.layout = make_pipeline_layout(device, "water.shore", {frame, water.layout});
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs_full";
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.depthStencil = &depth;
    desc.fragment = &fragment;
    water.p_shore = pipelines.add_render(device, desc);
  }
  {
    Water::Streams& st = water.streams;
    st.layout =
        make_layout(device, "water.stream",
                    {Slot::storage_read, Slot::storage_read, Slot::texture, Slot::texture_raw},
                    wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
    st.optics = make_buffer(device, "water.stream.optics", size_t{kStreamBodies} * 64,
                            wgpu::BufferUsage::Storage);
    st.vertex_bytes = 4096;
    st.vertices =
        make_buffer(device, "water.stream.vertices", st.vertex_bytes, wgpu::BufferUsage::Storage);
    st.index_bytes = 4096;
    st.indices =
        make_buffer(device, "water.stream.indices", st.index_bytes, wgpu::BufferUsage::Index);
    const wgpu::ShaderModule module =
        gpu::create_shader(device, wgsl::water_stream, "water.stream");
    wgpu::ColorTargetState colours[2] = {};
    colours[0].format = kHdrFormat;
    colours[1].format = kMotionFormat;
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 2;
    fragment.targets = colours;
    wgpu::DepthStencilState depth{};
    depth.format = kDepthFormat;
    depth.depthWriteEnabled = true;
    depth.depthCompare = wgpu::CompareFunction::Greater;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "water.stream";
    desc.layout = make_pipeline_layout(device, "water.stream", {frame, st.layout});
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs_main";
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.primitive.cullMode = wgpu::CullMode::None;
    desc.depthStencil = &depth;
    desc.fragment = &fragment;
    st.p_draw = pipelines.add_render(device, desc);
  }
  // The grid's indices: kWaterGrid^2 quads of two triangles over (kWaterGrid + 1)^2 vertices.
  std::vector<uint32_t> idx;
  idx.reserve(size_t{kWaterGrid} * kWaterGrid * 6);
  const uint32_t n = kWaterGrid + 1;
  for (uint32_t y = 0; y < kWaterGrid; ++y) {
    for (uint32_t x = 0; x < kWaterGrid; ++x) {
      const uint32_t a = y * n + x, b = a + 1, c = a + n, d = c + 1;
      idx.insert(idx.end(), {a, c, b, b, c, d});
    }
  }
  water.indices =
      make_buffer(device, "water.indices", idx.size() * sizeof(uint32_t), wgpu::BufferUsage::Index);
  device.queue.WriteBuffer(water.indices, 0, idx.data(), idx.size() * sizeof(uint32_t));
}

void create_water_targets(gpu::Device& device, Water& water, const FrameTargets& targets) {
  const auto usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
  water.scene_colour = make_texture(device, "water.scene.colour", targets.width, targets.height, 1,
                                    kHdrFormat, usage);
  water.scene_depth = make_texture(device, "water.scene.depth", targets.width, targets.height, 1,
                                   wgpu::TextureFormat::R32Float, usage);
  water.scene_colour_view = water.scene_colour.CreateView();
  water.scene_depth_view = water.scene_depth.CreateView();
  water.copy_group =
      make_group(device, "water.copy", water.copy_layout, {targets.hdr_view, targets.depth_view});
  water.streams.group = make_group(device, "water.stream", water.streams.layout,
                                   {water.streams.optics, water.streams.vertices,
                                    water.scene_colour_view, water.scene_depth_view});
  water.below = make_texture(device, "water.below", targets.width, targets.height, 1,
                             wgpu::TextureFormat::RGBA16Float, usage);
  water.below_veil = make_texture(device, "water.below.veil", targets.width, targets.height, 1,
                                  wgpu::TextureFormat::R8Unorm, usage);
  water.below_view = water.below.CreateView();
  water.below_veil_view = water.below_veil.CreateView();
  for (size_t i = 0; i < water.bodies.size(); ++i)
    bind_body(device, water, water.bodies[i], static_cast<int>(i));
}

void set_water_views(gpu::Device& device, Water& water, const WaterViews& views) {
  water.views = views;
  for (size_t i = 0; i < water.bodies.size(); ++i)
    bind_body(device, water, water.bodies[i], static_cast<int>(i));
}

ShoreMask shore_mask(const water::Body& body) {
  ShoreMask m;
  const std::vector<water::Point>& s = body.shore;
  const double w = body.box[2] - body.box[0], h = body.box[3] - body.box[1];
  if (s.size() < 3 || !(w > 0) || !(h > 0)) return m;
  m.texel = std::max(kShoreTexel, std::max(w, h) / kShoreMask);
  m.cols = std::max(1, static_cast<int>(std::ceil(w / m.texel)));
  m.rows = std::max(1, static_cast<int>(std::ceil(h / m.texel)));
  m.texels.assign(static_cast<size_t>(m.cols) * static_cast<size_t>(m.rows), 0);
  const auto at = [&](int c, int r) -> uint8_t& {
    return m.texels[static_cast<size_t>(r) * static_cast<size_t>(m.cols) + static_cast<size_t>(c)];
  };
  // Filled even-odd at each texel's middle, a row at a time.
  std::vector<double> xs;
  for (int r = 0; r < m.rows; ++r) {
    const double z = body.box[1] + (r + 0.5) * m.texel;
    xs.clear();
    for (size_t i = 0, j = s.size() - 1; i < s.size(); j = i++) {
      if ((s[i].z > z) != (s[j].z > z))
        xs.push_back(s[i].x + (s[j].x - s[i].x) * (z - s[i].z) / (s[j].z - s[i].z));
    }
    std::sort(xs.begin(), xs.end());
    for (size_t k = 0; k + 1 < xs.size(); k += 2) {
      const int c0 =
          std::max(0, static_cast<int>(std::ceil((xs[k] - body.box[0]) / m.texel - 0.5)));
      const int c1 = std::min(
          m.cols - 1, static_cast<int>(std::floor((xs[k + 1] - body.box[0]) / m.texel - 0.5)));
      for (int c = c0; c <= c1; ++c) at(c, r) = 255;
    }
  }
  // Grown by a texel round the outline: every texel the shore passes through, and its neighbours.
  for (size_t i = 0, j = s.size() - 1; i < s.size(); j = i++) {
    const double ex = s[i].x - s[j].x, ez = s[i].z - s[j].z;
    const double len = std::sqrt(ex * ex + ez * ez);
    const int n = std::max(1, static_cast<int>(std::ceil(2 * len / m.texel)));
    for (int k = 0; k <= n; ++k) {
      const double t = static_cast<double>(k) / n;
      const int c =
          static_cast<int>(std::floor((s[j].x + t * (s[i].x - s[j].x) - body.box[0]) / m.texel));
      const int r =
          static_cast<int>(std::floor((s[j].z + t * (s[i].z - s[j].z) - body.box[1]) / m.texel));
      for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
          if (c + dc >= 0 && c + dc < m.cols && r + dr >= 0 && r + dr < m.rows)
            at(c + dc, r + dr) = 255;
        }
      }
    }
  }
  // Past the shore, how near the water: 127 at the grown outline falling to 0 at kShoreReach m,
  // by a chamfer distance (3 a side step, 4 a diagonal) in two sweeps.
  const int reach = std::max(2, static_cast<int>(std::ceil(kShoreReach / m.texel)));
  constexpr int kFar = 1 << 28;
  std::vector<int> d(m.texels.size(), kFar);
  for (size_t i = 0; i < d.size(); ++i)
    if (m.texels[i] == 255) d[i] = 0;
  const auto relax = [&](int c, int r, int dc, int dr, int step) {
    const int cc = c + dc, rr = r + dr;
    if (cc < 0 || cc >= m.cols || rr < 0 || rr >= m.rows) return;
    int& here = d[static_cast<size_t>(r) * static_cast<size_t>(m.cols) + static_cast<size_t>(c)];
    const int there =
        d[static_cast<size_t>(rr) * static_cast<size_t>(m.cols) + static_cast<size_t>(cc)];
    here = std::min(here, there + step);
  };
  for (int r = 0; r < m.rows; ++r) {
    for (int c = 0; c < m.cols; ++c) {
      relax(c, r, -1, 0, 3);
      relax(c, r, 0, -1, 3);
      relax(c, r, -1, -1, 4);
      relax(c, r, 1, -1, 4);
    }
  }
  for (int r = m.rows - 1; r >= 0; --r) {
    for (int c = m.cols - 1; c >= 0; --c) {
      relax(c, r, 1, 0, 3);
      relax(c, r, 0, 1, 3);
      relax(c, r, 1, 1, 4);
      relax(c, r, -1, 1, 4);
    }
  }
  for (size_t i = 0; i < d.size(); ++i) {
    if (m.texels[i] == 255) continue;
    const double near = 1 - d[i] / (3.0 * reach);
    m.texels[i] = near > 0 ? static_cast<uint8_t>(std::lround(127 * near)) : 0;
  }
  return m;
}

int add_water_body(gpu::Device& device, Water& water, const water::Body& body,
                   const WaterDraw& draw) {
  Water::Slot slot;
  slot.draw = draw;
  slot.uniform =
      make_buffer(device, "water.body", sizeof(WaterGpuBody), wgpu::BufferUsage::Uniform);
  slot.waves =
      make_buffer(device, "water.waves", size_t{kWaveSlots} * 32, wgpu::BufferUsage::Storage);
  slot.rings =
      make_buffer(device, "water.rings", size_t{kWaterRings} * 48, wgpu::BufferUsage::Storage);
  stage_body(slot, body);
  upload_waves(device, slot, body);
  {
    const ShoreMask mask = shore_mask(body);
    const auto cols = static_cast<uint32_t>(mask.cols), rows = static_cast<uint32_t>(mask.rows);
    slot.shore = make_texture(device, "water.shore", cols, rows, 1, wgpu::TextureFormat::R8Unorm,
                              wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst);
    slot.shore_view = slot.shore.CreateView();
    wgpu::TexelCopyTextureInfo dst{};
    dst.texture = slot.shore;
    wgpu::TexelCopyBufferLayout layout{};
    layout.bytesPerRow = cols;
    layout.rowsPerImage = rows;
    const wgpu::Extent3D extent{cols, rows, 1};
    device.queue.WriteTexture(&dst, mask.texels.data(), mask.texels.size(), &layout, &extent);
  }
  bind_body(device, water, slot, static_cast<int>(water.bodies.size()));
  water.bodies.push_back(std::move(slot));
  return static_cast<int>(water.bodies.size()) - 1;
}

void set_water_sunlit(Water& water, int index, double share) {
  Water::Slot& slot = water.bodies[static_cast<size_t>(index)];
  slot.draw.sunlit = share;
  slot.staged.sun[0] = static_cast<float>(share);
}

void set_water_body(gpu::Device& device, Water& water, int index, const water::Body& body) {
  Water::Slot& slot = water.bodies[static_cast<size_t>(index)];
  stage_body(slot, body);
  upload_waves(device, slot, body);
}

void set_water_wakes(gpu::Device& device, Water& water, int index, const water::Wakes& wakes,
                     int body) {
  Water::Slot& slot = water.bodies[static_cast<size_t>(index)];
  std::vector<float> data;
  data.reserve(size_t{kWaterRings} * 12);
  int n = 0;
  for (auto r = wakes.rings.rbegin(); r != wakes.rings.rend() && n < kWaterRings; ++r) {
    if (r->body != body) continue;
    data.insert(data.end(),
                {static_cast<float>(r->x), static_cast<float>(r->z), static_cast<float>(r->t0),
                 static_cast<float>(r->volume), static_cast<float>(r->size),
                 static_cast<float>(r->shortest), static_cast<float>(r->crater),
                 static_cast<float>(r->depth), static_cast<float>(r->lean_x),
                 static_cast<float>(r->lean_z), static_cast<float>(r->reach), 0.0f});
    ++n;
  }
  slot.staged.wake[0] = static_cast<float>(n);
  slot.staged.wake[1] = static_cast<float>(wakes.life);
  slot.staged.wake[2] = static_cast<float>(wakes.shortest);
  if (n > 0) device.queue.WriteBuffer(slot.rings, 0, data.data(), data.size() * sizeof(float));
}

void prepare_water(gpu::Device& device, Water& water, const FrameView& view, int eye_in) {
  water.eye_under = -1;
  water.eye_near = -1;
  if (eye_in >= 0 && eye_in < static_cast<int>(water.bodies.size()) &&
      !water.bodies[static_cast<size_t>(eye_in)].hidden)
    water.eye_under = eye_in;
  for (Water::Slot& slot : water.bodies) {
    WaterGpuBody& b = slot.staged;
    const int index = static_cast<int>(&slot - water.bodies.data());
    slot.shown = !slot.hidden && view.eye[1] > b.info[0];
    const bool inside = view.eye[0] >= b.box[0] && view.eye[0] <= b.box[2] &&
                        view.eye[2] >= b.box[1] && view.eye[2] <= b.box[3];
    // A pool's box is only its plan: an eye under the level but below the floor (a room under
    // another pool) is not in it. The game's word (`eye_in`) stands over both.
    if (eye_in == kEyeFromBoxes && !slot.hidden && !slot.shown && inside &&
        view.eye[1] > slot.floor && water.eye_under < 0) {
      water.eye_under = index;
    }
    if (index == water.eye_under) slot.shown = false;
    const WaterViews& v = water.views;
    const bool ours = v.body == index;
    b.view[0] = mirrored(v, slot, index) ? (v.below ? 2.0f : 1.0f) : 0.0f;
    b.view[1] = ours && v.has_up ? 1.0f : 0.0f;
    std::memcpy(b.up_view_proj, v.up_view_proj, sizeof b.up_view_proj);
    if (slot.shown && inside && view.eye[1] < b.info[0] + kWaterlineBand && water.eye_near < 0) {
      water.eye_near = static_cast<int>(&slot - water.bodies.data());
    }
    const int levels = static_cast<int>(b.grid[0]);
    const double half = kWaterGrid / 2.0;
    for (int i = 0; i < levels; ++i) {
      const double s = slot.draw.base_spacing * std::ldexp(1.0, i);
      const double cx = std::round(view.eye[0] / (2 * s)) * 2 * s;
      const double cz = std::round(view.eye[2] / (2 * s)) * 2 * s;
      b.levels[i][0] = static_cast<float>(cx - half * s);
      b.levels[i][1] = static_cast<float>(cz - half * s);
      b.levels[i][2] = static_cast<float>(s);
      b.levels[i][3] = 0;
    }
    device.queue.WriteBuffer(slot.uniform, 0, &b, sizeof b);
  }
}

void encode_water(const Water& water, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                  wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                  const wgpu::BindGroup& frame) {
  // Nothing drawn and no eye under the water: the rows run empty (no copy), so every later pass
  // keeps its timing row.
  const bool drawn = std::any_of(water.bodies.begin(), water.bodies.end(),
                                 [](const Water::Slot& s) { return s.shown; }) ||
                     water.streams.count > 0;
  if (!drawn && water.eye_under < 0) {
    rec.compute(encoder, "water.copy").End();
    rec.compute(encoder, "water").End();
    rec.compute(encoder, "water.below").End();
    return;
  }
  {
    wgpu::RenderPassColorAttachment colours[2]{};
    colours[0].view = water.scene_colour_view;
    colours[1].view = water.scene_depth_view;
    for (auto& c : colours) {
      c.loadOp = wgpu::LoadOp::Clear;
      c.storeOp = wgpu::StoreOp::Store;
    }
    wgpu::RenderPassDescriptor desc{};
    desc.colorAttachmentCount = 2;
    desc.colorAttachments = colours;
    wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "water.copy");
    pass.SetPipeline(pipelines.render(water.p_copy));
    pass.SetBindGroup(0, water.copy_group);
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
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "water");
  pass.SetBindGroup(0, frame);
  // The shore's wet band under every body first: the ground over the level, which the water
  // then leaves alone.
  pass.SetPipeline(pipelines.render(water.p_shore));
  for (const Water::Slot& slot : water.bodies) {
    if (!slot.shown) continue;
    pass.SetBindGroup(1, slot.group);
    pass.Draw(3);
    ++rec.current().draws;
  }
  pass.SetPipeline(pipelines.render(water.p_draw));
  pass.SetIndexBuffer(water.indices, wgpu::IndexFormat::Uint32);
  for (const Water::Slot& slot : water.bodies) {
    if (!slot.shown) continue;
    pass.SetBindGroup(1, slot.group);
    pass.DrawIndexed(kWaterGrid * kWaterGrid * 6, static_cast<uint32_t>(slot.staged.grid[0]));
    ++rec.current().draws;
  }
  if (water.streams.count > 0) {
    pass.SetPipeline(pipelines.render(water.streams.p_draw));
    pass.SetBindGroup(1, water.streams.group);
    pass.SetIndexBuffer(water.streams.indices, wgpu::IndexFormat::Uint32);
    pass.DrawIndexed(water.streams.count);
    ++rec.current().draws;
  }
  pass.End();
  if (water.eye_under < 0) {
    rec.compute(encoder, "water.below").End();
    return;
  }
  wgpu::RenderPassColorAttachment below[2]{};
  below[0].view = water.below_view;
  below[1].view = water.below_veil_view;
  for (auto& c : below) {
    c.loadOp = wgpu::LoadOp::Clear;
    c.storeOp = wgpu::StoreOp::Store;
    c.clearValue = {0, 0, 0, 0};
  }
  wgpu::RenderPassDepthStencilAttachment test{};
  test.view = targets.depth_view;
  test.depthLoadOp = wgpu::LoadOp::Load;
  test.depthStoreOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDescriptor under{};
  under.colorAttachmentCount = 2;
  under.colorAttachments = below;
  under.depthStencilAttachment = &test;
  wgpu::RenderPassEncoder bp = rec.render(encoder, under, "water.below");
  const Water::Slot& slot = water.bodies[static_cast<size_t>(water.eye_under)];
  bp.SetPipeline(pipelines.render(water.p_below));
  bp.SetBindGroup(0, frame);
  bp.SetBindGroup(1, slot.group);
  bp.SetIndexBuffer(water.indices, wgpu::IndexFormat::Uint32);
  bp.DrawIndexed(kWaterGrid * kWaterGrid * 6, static_cast<uint32_t>(slot.staged.grid[0]));
  ++rec.current().draws;
  bp.End();
}

}  // namespace ge::render
