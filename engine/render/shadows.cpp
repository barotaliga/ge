// The sun's shadows: the maps and their groups, the cascades' redraw decision, the figures'
// tiles, and the casters' cull and depth passes.
//
// ge:summary The sun's shadows: cascades' decisions, the atlas's tiles, the casters' passes
// ge:keywords render shadows cascades atlas casters redraw snap orthographic
// ge:skill render
// ge:test tests/render_shadows_test.cpp
#include "render/shadows.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ge::render {

namespace {

constexpr double kDegree = 3.14159265358979323846 / 180.0;
constexpr uint32_t kTilesAcross = 4;  // the atlas's grid: kMaxShadowFigures = 4 x 4
static_assert(kTilesAcross * kTilesAcross == kMaxShadowFigures, "the atlas's grid");

double dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// A map's frame block: the casters' cull and draw see the world from `centre` through `m`, and
// cull by `planes` (inside >= 0, relative to the centre).
void fill_map_view(FrameUniforms& u, const FrameView& view, const double centre[3],
                   const float m[16], const float planes[5][4], uint32_t texels) {
  std::memset(&u, 0, sizeof u);
  std::memcpy(u.view_proj, m, sizeof u.view_proj);
  std::memcpy(u.prev_view_proj, m, sizeof u.prev_view_proj);
  u.inv_view_proj[0] = u.inv_view_proj[5] = u.inv_view_proj[10] = u.inv_view_proj[15] = 1;
  for (int i = 0; i < 3; ++i) u.eye[i] = static_cast<float>(centre[i]);
  u.eye[3] = static_cast<float>((view.altitude_m + centre[1]) * 1e-3);
  double s[3];
  sun_direction(view, s);
  for (int i = 0; i < 3; ++i) u.sun[i] = static_cast<float>(s[i]);
  u.sun[3] = static_cast<float>(view.sun_illuminance);
  u.size[0] = u.size[1] = static_cast<float>(texels);
  u.size[2] = u.size[3] = 1.0f / static_cast<float>(texels);
  u.time[0] = static_cast<float>(view.time_s);
  u.time[1] = static_cast<float>(view.frame);
  u.world[0] = static_cast<float>(view.altitude_m * 1e-3);
  std::memcpy(u.planes, planes, sizeof(float) * 20);
}

// The box's planes round a centre: across and up within `radius`, toward the sun within `depth`
// (away from it, the map runs on: what lies past its far end casts onto nothing it holds).
void box_planes(const double basis[3][3], double radius, double depth, float planes[5][4]) {
  const double* r = basis[0];
  const double* u = basis[1];
  const double* s = basis[2];
  const double rows[5][4] = {{r[0], r[1], r[2], radius},
                             {-r[0], -r[1], -r[2], radius},
                             {u[0], u[1], u[2], radius},
                             {-u[0], -u[1], -u[2], radius},
                             {-s[0], -s[1], -s[2], depth}};
  for (int k = 0; k < 5; ++k) {
    for (int c = 0; c < 4; ++c) planes[k][c] = static_cast<float>(rows[k][c]);
  }
}

wgpu::Sampler compare_sampler(gpu::Device& device) {
  wgpu::SamplerDescriptor desc{};
  desc.label = "shadows.compare";
  desc.magFilter = wgpu::FilterMode::Linear;
  desc.minFilter = wgpu::FilterMode::Linear;
  desc.addressModeU = desc.addressModeV = wgpu::AddressMode::ClampToEdge;
  // Lit where the receiver's depth is at least the map's (1 toward the sun; 0: nothing cast).
  desc.compare = wgpu::CompareFunction::GreaterEqual;
  return device.device.CreateSampler(&desc);
}

// A depth array of `layers` layers, `size` a side.
wgpu::Texture depth_array(gpu::Device& device, std::string_view label, uint32_t size,
                          uint32_t layers) {
  wgpu::TextureDescriptor desc{};
  desc.label = wgpu::StringView(label.data(), label.size());
  desc.size = {size, size, layers};
  desc.format = kDepthFormat;
  desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
  return device.device.CreateTexture(&desc);
}

wgpu::TextureView array_view(const wgpu::Texture& t, uint32_t layers) {
  wgpu::TextureViewDescriptor desc{};
  desc.dimension = wgpu::TextureViewDimension::e2DArray;
  desc.arrayLayerCount = layers;
  return t.CreateView(&desc);
}

}  // namespace

void shadow_view(const double sun[3], double radius, double depth, double basis[3][3],
                 float m[16]) {
  const double ref[3] = {0, std::fabs(sun[1]) < 0.99 ? 1.0 : 0.0,
                         std::fabs(sun[1]) < 0.99 ? 0.0 : 1.0};
  // Across: ref x sun; up: sun x across (the world's up as near as the sun allows).
  double r[3] = {ref[1] * sun[2] - ref[2] * sun[1], ref[2] * sun[0] - ref[0] * sun[2],
                 ref[0] * sun[1] - ref[1] * sun[0]};
  const double rl = std::sqrt(dot3(r, r));
  for (double& c : r) c /= rl;
  const double u[3] = {sun[1] * r[2] - sun[2] * r[1], sun[2] * r[0] - sun[0] * r[2],
                       sun[0] * r[1] - sun[1] * r[0]};
  for (int i = 0; i < 3; ++i) {
    basis[0][i] = r[i];
    basis[1][i] = u[i];
    basis[2][i] = sun[i];
  }
  // clip.x = across / radius, clip.y = up / radius, clip.z = 0.5 + toward the sun / (2 depth).
  std::fill(m, m + 16, 0.0f);
  for (int i = 0; i < 3; ++i) {
    m[i * 4 + 0] = static_cast<float>(r[i] / radius);
    m[i * 4 + 1] = static_cast<float>(u[i] / radius);
    m[i * 4 + 2] = static_cast<float>(sun[i] / (2 * depth));
  }
  m[14] = 0.5f;
  m[15] = 1.0f;
}

wgpu::BindGroupLayout sun_shadow_layout(gpu::Device& device) {
  // The fragment shaders' and the fog's light (render/fog.hpp's inject, a compute pass).
  return make_layout(device, "shadows", {Slot::uniform, Slot::depth_array, Slot::compare},
                     wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Compute);
}

wgpu::BindGroup no_sun_shadows(gpu::Device& device) {
  // A 1 x 1 map of each layer, zero (WebGPU clears a new texture): nothing casts. The block's
  // offsets and figures are zero: no map is read.
  const wgpu::Texture t = depth_array(device, "shadows.none", 1, kShadowLayers);
  const wgpu::Buffer params =
      make_buffer(device, "shadows.none.params", sizeof(ShadowParams), wgpu::BufferUsage::Uniform);
  const ShadowParams zero{};
  device.queue.WriteBuffer(params, 0, &zero, sizeof zero);
  return make_group(device, "shadows.none", sun_shadow_layout(device),
                    {params, array_view(t, kShadowLayers), compare_sampler(device)});
}

void create_sun_shadows(gpu::Device& device, SunShadows& s, const ShadowSettings& settings,
                        const wgpu::BindGroupLayout& frame, const Sky& sky) {
  s.settings = settings;
  s.settings.size = std::max(s.settings.size, kTilesAcross);
  s.maps = depth_array(device, "shadows.maps", s.settings.size, kShadowLayers);
  s.array_view = array_view(s.maps, kShadowLayers);
  for (uint32_t k = 0; k < kShadowLayers; ++k) {
    wgpu::TextureViewDescriptor desc{};
    desc.dimension = wgpu::TextureViewDimension::e2D;
    desc.baseArrayLayer = k;
    desc.arrayLayerCount = 1;
    s.layer_views[k] = s.maps.CreateView(&desc);
  }
  s.compare = compare_sampler(device);
  s.params =
      make_buffer(device, "shadows.params", sizeof(ShadowParams), wgpu::BufferUsage::Uniform);
  s.layout = sun_shadow_layout(device);
  s.group = make_group(device, "shadows", s.layout, {s.params, s.array_view, s.compare});
  for (size_t v = 0; v < s.view_uniforms.size(); ++v) {
    s.view_uniforms[v] = make_buffer(device, "shadows.view" + std::to_string(v),
                                     sizeof(FrameUniforms), wgpu::BufferUsage::Uniform);
    s.view_groups[v] = frame_group(device, frame, s.view_uniforms[v], sky);
  }
  reset_sun_shadows(s);
  s.redraws = {};
  s.staged = ShadowParams{};
  device.queue.WriteBuffer(s.params, 0, &s.staged, sizeof s.staged);
}

bool update_cascade(const ShadowSettings& set, int c, const double sun[3], const double eye[3],
                    ShadowCascade& k, float m[16]) {
  const double radius = c == 0 ? set.near_radius : set.far_radius;
  const double turn = (c == 0 ? set.near_turn_deg : set.far_turn_deg) * kDegree;
  // Under the horizon nothing is lit to shade: no map is drawn or read.
  if (sun[1] <= 0) {
    k.drawn = false;
    return false;
  }
  if (k.drawn) {
    const double d[3] = {eye[0] - k.centre[0], eye[1] - k.centre[1], eye[2] - k.centre[2]};
    const double drift = std::max(std::fabs(dot3(d, k.basis[0])), std::fabs(dot3(d, k.basis[1])));
    const double cosine = std::clamp(dot3(sun, k.sun), -1.0, 1.0);
    if (drift <= set.drift * radius && std::acos(cosine) <= turn) return false;
  }
  shadow_view(sun, radius, set.depth, k.basis, m);
  // The centre on the map's texels across the sun, at the eye's depth along it.
  const double texel = 2 * radius / set.size;
  const double across = std::round(dot3(eye, k.basis[0]) / texel) * texel;
  const double up = std::round(dot3(eye, k.basis[1]) / texel) * texel;
  const double along = dot3(eye, k.basis[2]);
  for (int i = 0; i < 3; ++i) {
    k.centre[i] = across * k.basis[0][i] + up * k.basis[1][i] + along * k.basis[2][i];
    k.sun[i] = sun[i];
  }
  k.drawn = true;
  return true;
}

void reset_sun_shadows(SunShadows& s) {
  for (ShadowCascade& c : s.cascades) c.drawn = false;
}

void prepare_sun_shadows(gpu::Device& device, SunShadows& s, const FrameView& view,
                         std::span<const std::array<double, 3>> figures,
                         std::span<InstanceFamily* const> still,
                         std::span<InstanceFamily* const> moving) {
  const ShadowSettings& set = s.settings;
  const uint32_t n = set.size;
  double sun[3];
  sun_direction(view, sun);
  const double radius[kCascades] = {set.near_radius, set.far_radius};
  for (int c = 0; c < kCascades; ++c) {
    ShadowCascade& k = s.cascades[static_cast<size_t>(c)];
    float m[16];
    s.due[static_cast<size_t>(c)] = update_cascade(set, c, sun, view.eye, k, m);
    if (!s.due[static_cast<size_t>(c)]) {
      for (InstanceFamily* f : still) {
        if (static_cast<size_t>(c) < f->casters.size()) {
          f->casters[static_cast<size_t>(c)].culled = false;
        }
      }
      continue;
    }
    const double texel = 2 * radius[c] / n;
    ++s.redraws[static_cast<size_t>(c)];
    std::memcpy(s.staged.cascade[c], m, sizeof m);
    s.staged.texel[c] = static_cast<float>(texel);
    float planes[5][4];
    box_planes(k.basis, radius[c], set.depth, planes);
    FrameUniforms u;
    fill_map_view(u, view, k.centre, m, planes, n);
    device.queue.WriteBuffer(s.view_uniforms[static_cast<size_t>(c)], 0, &u, sizeof u);
    for (InstanceFamily* f : still) {
      prepare_family_caster(device, *f, c, static_cast<float>(n / (2 * radius[c])));
    }
  }
  for (int c = 0; c < kCascades; ++c) {
    const ShadowCascade& k = s.cascades[static_cast<size_t>(c)];
    for (int i = 0; i < 3; ++i) {
      s.staged.offset[c][i] = static_cast<float>(view.eye[i] - k.centre[i]);
    }
    s.staged.offset[c][3] = k.drawn ? 1.0f : 0.0f;
  }

  // The figures' tiles: one view along the sun, moved to each figure, each culling its own casters
  // by its own box (a caster slot a tile), so a figure is drawn only into the tiles it can shade.
  s.figures =
      sun[1] > 0 ? static_cast<int>(std::min<size_t>(figures.size(), kMaxShadowFigures)) : 0;
  const uint32_t tile_texels = n / kTilesAcross;
  double basis[3][3];
  float atlas[16];
  shadow_view(sun, set.figure_radius, set.figure_depth, basis, atlas);
  std::memcpy(s.staged.atlas, atlas, sizeof atlas);
  float planes[5][4];
  box_planes(basis, set.figure_radius, set.figure_depth, planes);
  // Tile k covers clip x, y in [-1, 1] about its figure, so [-1 - t, 1 - t] about the eye.
  float reach[4] = {1e9f, 1e9f, -1e9f, -1e9f};
  for (int k = 0; k < kMaxShadowFigures; ++k) {
    float* t = s.staged.tile[k];
    if (k >= s.figures) {
      t[0] = t[1] = t[2] = t[3] = 0;
      continue;
    }
    const std::array<double, 3>& at = figures[static_cast<size_t>(k)];
    // The eye's offset from the figure, in the tile's clip units: the view is affine, so a
    // pixel's clip in every tile is its own clip plus this (sun_shadow.wgsl).
    const double d[3] = {view.eye[0] - at[0], view.eye[1] - at[1], view.eye[2] - at[2]};
    t[0] = static_cast<float>(dot3(basis[0], d) / set.figure_radius);
    t[1] = static_cast<float>(dot3(basis[1], d) / set.figure_radius);
    t[2] = static_cast<float>(dot3(basis[2], d) / (2 * set.figure_depth));
    t[3] = 1;
    for (int i = 0; i < 2; ++i) {
      reach[i] = std::min(reach[i], -1 - t[i]);
      reach[2 + i] = std::max(reach[2 + i], 1 - t[i]);
    }
    FrameUniforms u;
    fill_map_view(u, view, at.data(), atlas, planes, tile_texels);
    device.queue.WriteBuffer(
        s.view_uniforms[static_cast<size_t>(kCascades) + static_cast<size_t>(k)], 0, &u, sizeof u);
  }
  for (InstanceFamily* f : moving) {
    // The atlas's slots: those before the lamps' tiles (render/lamp_shadows.hpp's own).
    const int slots = f->lamp_slot >= 0 ? f->lamp_slot : static_cast<int>(f->casters.size());
    for (int k = 0; k < slots; ++k) {
      if (k < s.figures) {
        prepare_family_caster(
            device, *f, k,
            static_cast<float>(set.figure_lod_share * tile_texels / (2 * set.figure_radius)));
      } else {
        f->casters[static_cast<size_t>(k)].culled = false;
      }
    }
  }
  s.staged.texel[2] = static_cast<float>(2 * set.figure_radius / tile_texels);
  s.staged.texel[3] = static_cast<float>(set.bias_m / (2 * set.figure_depth));
  s.staged.info[0] = static_cast<float>(n);
  s.staged.info[1] = static_cast<float>(kTilesAcross);
  s.staged.info[2] = static_cast<float>(set.bias_m / (2 * set.depth));
  s.staged.info[3] = static_cast<float>(s.figures);
  std::memcpy(s.staged.reach, reach, sizeof reach);
  device.queue.WriteBuffer(s.params, 0, &s.staged, sizeof s.staged);
}

void encode_sun_shadows(SunShadows& s, std::span<InstanceFamily* const> still,
                        std::span<InstanceFamily* const> moving, const InstancePipelines& ip,
                        const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                        wgpu::CommandEncoder& encoder) {
  {
    wgpu::ComputePassEncoder pass = rec.compute(encoder, "shadows.cull");
    PassStat& stat = rec.current();
    pass.SetPipeline(pipelines.compute(ip.p_shadow_cull));
    for (int c = 0; c < kCascades; ++c) {
      if (!s.due[static_cast<size_t>(c)]) continue;
      pass.SetBindGroup(0, s.view_groups[static_cast<size_t>(c)]);
      stat.dispatches += cull_shadow(pass, still, c);
    }
    for (int k = 0; k < s.figures; ++k) {
      pass.SetBindGroup(0, s.view_groups[static_cast<size_t>(kCascades) + static_cast<size_t>(k)]);
      stat.dispatches += cull_shadow(pass, moving, k);
    }
    pass.End();
  }
  // A layer that is not redrawn keeps its depth: its pass opens read-only and draws nothing (the
  // recorder's rows stay in the table's order every frame).
  static constexpr const char* kNames[kShadowLayers] = {"shadows.near", "shadows.far",
                                                        "shadows.figures"};
  const uint32_t n = s.settings.size;
  for (int layer = 0; layer < kShadowLayers; ++layer) {
    const bool draw = layer < kCascades ? s.due[static_cast<size_t>(layer)] : s.figures > 0;
    wgpu::RenderPassDepthStencilAttachment depth{};
    depth.view = s.layer_views[static_cast<size_t>(layer)];
    if (draw) {
      depth.depthLoadOp = wgpu::LoadOp::Clear;
      depth.depthStoreOp = wgpu::StoreOp::Store;
      depth.depthClearValue = 0.0f;
    } else {
      depth.depthReadOnly = true;
    }
    wgpu::RenderPassDescriptor desc{};
    desc.depthStencilAttachment = &depth;
    wgpu::RenderPassEncoder pass = rec.render(encoder, desc, kNames[layer]);
    PassStat& stat = rec.current();
    if (draw) {
      pass.SetPipeline(pipelines.render(ip.p_caster));
      pass.SetBindGroup(1, ip.empty_group);
      if (layer < kCascades) {
        pass.SetBindGroup(0, s.view_groups[static_cast<size_t>(layer)]);
        stat.draws += draw_casters(pass, still, layer);
      } else {
        const uint32_t tile = n / kTilesAcross;
        for (int k = 0; k < s.figures; ++k) {
          const uint32_t x = static_cast<uint32_t>(k) % kTilesAcross * tile;
          const uint32_t y = static_cast<uint32_t>(k) / kTilesAcross * tile;
          pass.SetViewport(static_cast<float>(x), static_cast<float>(y), static_cast<float>(tile),
                           static_cast<float>(tile), 0.0f, 1.0f);
          pass.SetScissorRect(x, y, tile, tile);
          pass.SetBindGroup(0,
                            s.view_groups[static_cast<size_t>(kCascades) + static_cast<size_t>(k)]);
          stat.draws += draw_casters(pass, moving, k);
        }
      }
    }
    pass.End();
  }
  for (bool& d : s.due) d = false;
}

ShadowParams ortho_shadow_params(const OrthoShadow& m, const double eye[3]) {
  ShadowParams p{};
  const double half = std::max(m.half, 1e-6), depth = std::max(m.depth, 1e-6);
  // Column-major: column i is world axis i's share of clip x, y and z.
  for (int i = 0; i < 3; ++i) {
    p.cascade[0][i * 4 + 0] = static_cast<float>(m.x[i] / half);
    p.cascade[0][i * 4 + 1] = static_cast<float>(m.y[i] / half);
    p.cascade[0][i * 4 + 2] = static_cast<float>(m.z[i] / (2 * depth));
    p.offset[0][i] = static_cast<float>(eye[i] - m.o[i]);
  }
  p.cascade[0][14] = 0.5f;
  p.cascade[0][15] = 1.0f;
  p.offset[0][3] = 1.0f;
  p.info[0] = static_cast<float>(m.size);
  p.info[1] = 1.0f;
  p.info[2] = static_cast<float>(m.bias);
  p.texel[0] = static_cast<float>(2 * half / std::max(m.size, 1u));
  return p;
}

}  // namespace ge::render
