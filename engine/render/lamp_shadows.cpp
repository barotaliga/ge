// The lamps' shadows: the atlas and its clear, a tile's view, the deal and the cache, and the
// casters' cull and depth passes.
//
// ge:summary The lamps' shadows: the atlas, a tile's view, the deal, the cache, the passes
// ge:keywords render lamp-shadows atlas tiles deal cache cube-faces spot casters
// ge:skill render
// ge:test tests/render_lamp_shadows_test.cpp
#include "render/lamp_shadows.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "render/lamp_shadows.wgsl.hpp"

namespace ge::render {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr uint32_t kNone = ~0u;

double dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void unit(double v[3]) {
  const double l = std::sqrt(dot3(v, v));
  if (l > 0) {
    for (int i = 0; i < 3; ++i) v[i] /= l;
  }
}

bool is_spot(const LocalLight& l) { return l.outer_deg > 0; }

uint32_t cost_of(const LocalLight& l) { return is_spot(l) ? 1u : 6u; }

// A tile's axes: forward, up and across, and the tan of its half-width.
void tile_axes(const LampShadowSettings& set, const LocalLight& l, int face, double f[3],
               double u[3], double r[3], double& t) {
  if (is_spot(l)) {
    for (int i = 0; i < 3; ++i) f[i] = l.direction[i];
    unit(f);
    t = std::tan(std::min<double>(l.outer_deg, set.max_spot_deg) * kPi / 180.0);
  } else {
    // +x, -x, +y, -y, +z, -z; up is the world's up but on the floor's and the ceiling's faces.
    static constexpr double kF[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                        {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    for (int i = 0; i < 3; ++i) f[i] = kF[face][i];
    t = set.face_margin;
  }
  const double ref[3] = {0, std::fabs(f[1]) < 0.99 ? 1.0 : 0.0, std::fabs(f[1]) < 0.99 ? 0.0 : 1.0};
  // Across: ref x forward; up: forward x across.
  r[0] = ref[1] * f[2] - ref[2] * f[1];
  r[1] = ref[2] * f[0] - ref[0] * f[2];
  r[2] = ref[0] * f[1] - ref[1] * f[0];
  unit(r);
  u[0] = f[1] * r[2] - f[2] * r[1];
  u[1] = f[2] * r[0] - f[0] * r[2];
  u[2] = f[0] * r[1] - f[1] * r[0];
}

// Whether a sphere (relative to the lamp) touches a tile's view within the lamp's range.
bool touches(const float planes[5][4], double range, const double c[3], double radius) {
  if (dot3(c, c) > (range + radius) * (range + radius)) return false;
  for (int k = 0; k < 5; ++k) {
    const double d = planes[k][0] * c[0] + planes[k][1] * c[1] + planes[k][2] * c[2] + planes[k][3];
    if (d < -radius) return false;
  }
  return true;
}

bool same_pose(const LampTileState& s, const LocalLight& l) {
  return s.at[0] == l.position[0] && s.at[1] == l.position[1] && s.at[2] == l.position[2] &&
         s.dir[0] == l.direction[0] && s.dir[1] == l.direction[1] && s.dir[2] == l.direction[2] &&
         s.outer_deg == l.outer_deg && s.range_m == l.range_m;
}

// A tile's frame block: the casters' cull and draw see the world from the lamp through `m`, cull
// by `planes` and end at its range (the cull's reach).
void fill_tile_view(FrameUniforms& u, const FrameView& view, const LocalLight& l, const float m[16],
                    const float planes[5][4], uint32_t texels) {
  std::memset(&u, 0, sizeof u);
  std::memcpy(u.view_proj, m, sizeof u.view_proj);
  std::memcpy(u.prev_view_proj, m, sizeof u.prev_view_proj);
  u.inv_view_proj[0] = u.inv_view_proj[5] = u.inv_view_proj[10] = u.inv_view_proj[15] = 1;
  for (int i = 0; i < 3; ++i) u.eye[i] = static_cast<float>(l.position[i]);
  u.eye[3] = static_cast<float>((view.altitude_m + l.position[1]) * 1e-3);
  u.size[0] = u.size[1] = static_cast<float>(texels);
  u.size[2] = u.size[3] = 1.0f / static_cast<float>(texels);
  u.time[0] = static_cast<float>(view.time_s);
  u.time[1] = static_cast<float>(view.frame);
  u.world[0] = static_cast<float>(view.altitude_m * 1e-3);
  std::memcpy(u.planes, planes, sizeof(float) * 20);
  u.mist[1] = l.range_m;
}

// Family `f`'s caster slot for tile `k`: its lamps' slots from `lamp_slot` on, -1 past them.
int tile_slot(const InstanceFamily& f, uint32_t k) {
  if (f.lamp_slot < 0) return -1;
  const auto slot = static_cast<size_t>(f.lamp_slot) + k;
  return slot < f.casters.size() ? static_cast<int>(slot) : -1;
}

}  // namespace

void lamp_tile_view(const LampShadowSettings& set, const LocalLight& l, int face, float m[16],
                    float planes[5][4]) {
  double f[3], u[3], r[3], t;
  tile_axes(set, l, face, f, u, r, t);
  // clip = (across / t, up / t, near, forward): x, y in [-1, 1] inside, depth near / forward.
  std::fill(m, m + 16, 0.0f);
  for (int i = 0; i < 3; ++i) {
    m[i * 4 + 0] = static_cast<float>(r[i] / t);
    m[i * 4 + 1] = static_cast<float>(u[i] / t);
    m[i * 4 + 3] = static_cast<float>(f[i]);
  }
  m[14] = static_cast<float>(set.near_m);
  // The sides: across <= t forward, and so on; each normal made unit.
  const double sides[4][3] = {{f[0] * t - r[0], f[1] * t - r[1], f[2] * t - r[2]},
                              {f[0] * t + r[0], f[1] * t + r[1], f[2] * t + r[2]},
                              {f[0] * t - u[0], f[1] * t - u[1], f[2] * t - u[2]},
                              {f[0] * t + u[0], f[1] * t + u[1], f[2] * t + u[2]}};
  for (int k = 0; k < 4; ++k) {
    double n[3] = {sides[k][0], sides[k][1], sides[k][2]};
    unit(n);
    for (int i = 0; i < 3; ++i) planes[k][i] = static_cast<float>(n[i]);
    planes[k][3] = 0;
  }
  for (int i = 0; i < 3; ++i) planes[4][i] = static_cast<float>(f[i]);
  planes[4][3] = static_cast<float>(-set.near_m);
}

void create_lamp_shadows(gpu::Device& device, gpu::PipelineSet& pipelines, LampShadows& s,
                         const LampShadowSettings& settings, const wgpu::BindGroupLayout& frame,
                         const Sky& sky) {
  s.settings = settings;
  s.settings.size =
      std::max(s.settings.size / kLampTilesAcross * kLampTilesAcross, kLampTilesAcross * 4);
  wgpu::TextureDescriptor td{};
  td.label = "lamps.atlas";
  td.size = {s.settings.size, s.settings.size, 1};
  td.format = kDepthFormat;
  td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding |
             wgpu::TextureUsage::CopySrc;
  s.atlas = device.device.CreateTexture(&td);
  s.atlas_view = s.atlas.CreateView();
  for (size_t k = 0; k < kMaxLampTiles; ++k) {
    s.view_uniforms[k] = make_buffer(device, "lamps.view" + std::to_string(k),
                                     sizeof(FrameUniforms), wgpu::BufferUsage::Uniform);
    s.view_groups[k] = frame_group(device, frame, s.view_uniforms[k], sky);
  }
  // The clear: depth 0 everywhere in the viewport, whatever was there.
  const wgpu::ShaderModule m = gpu::create_shader(device, wgsl::lamp_shadows, "lamp_shadows");
  wgpu::DepthStencilState depth{};
  depth.format = kDepthFormat;
  depth.depthWriteEnabled = true;
  depth.depthCompare = wgpu::CompareFunction::Always;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "lamps.clear";
  desc.layout = make_pipeline_layout(device, "lamps.clear", {});
  desc.vertex.module = m;
  desc.vertex.entryPoint = "vs_clear";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.depthStencil = &depth;
  s.p_clear = pipelines.add_render(device, desc);
  reset_lamp_shadows(s);
  s.redraws = 0;
}

void reset_lamp_shadows(LampShadows& s) {
  for (LampTileState& t : s.tiles) t = LampTileState{};
  s.due = {};
}

void deal_lamp_tiles(LampShadows& s, LightBins& bins, std::span<const LocalLight> lights,
                     const FrameView& view, std::span<const std::array<double, 4>> movers) {
  const LampShadowSettings& set = s.settings;
  if (bins.tiles.size() != kMaxLampTiles) bins.tiles.assign(kMaxLampTiles, GpuLampTile{});
  const uint32_t count = bins.head.info[0];

  // The candidates: every kept light with a key, by the light it puts on the eye.
  struct Candidate {
    uint32_t row;  // its row in the bins
    double score;
  };
  Candidate cand[kMaxLights];
  uint32_t n = 0;
  for (uint32_t i = 0; i < count; ++i) {
    const LocalLight& l = lights[bins.order[i]];
    bins.lights[i].cone[1] = -1.0f;
    if (l.shadow == 0) continue;
    double d[3];
    for (int k = 0; k < 3; ++k) d[k] = l.position[k] - view.eye[k];
    double score = l.candela * flicker_level(l, view.time_s) / (dot3(d, d) + 1.0);
    for (const LampTileState& t : s.tiles) {
      if (t.key == l.shadow) {
        score *= set.keep;
        break;
      }
    }
    cand[n++] = {i, score};
  }
  // Highest first; a tie keeps the bins' order (nearest first). An insertion sort: few lamps.
  for (uint32_t i = 1; i < n; ++i) {
    const Candidate c = cand[i];
    uint32_t j = i;
    while (j > 0 && cand[j - 1].score < c.score) {
      cand[j] = cand[j - 1];
      --j;
    }
    cand[j] = c;
  }
  // Chosen while the tiles last: a lamp too big for what is left gives way to smaller ones.
  bool chosen[kMaxLights] = {};
  const uint32_t limit = std::min(set.tiles, kMaxLampTiles);
  uint32_t budget = limit;
  s.unshadowed = 0;
  for (uint32_t c = 0; c < n; ++c) {
    const uint32_t cost = cost_of(lights[bins.order[cand[c].row]]);
    if (cost <= budget) {
      chosen[c] = true;
      budget -= cost;
    } else {
      ++s.unshadowed;
    }
  }

  // Held lamps keep their run; the rest take the first free run that fits.
  uint32_t first[kMaxLights];
  std::fill(first, first + kMaxLights, kNone);
  bool used[kMaxLampTiles] = {};
  for (uint32_t c = 0; c < n; ++c) {
    if (!chosen[c]) continue;
    const LocalLight& l = lights[bins.order[cand[c].row]];
    const uint32_t cost = cost_of(l);
    for (uint32_t k = 0; k + cost <= limit; ++k) {
      if (s.tiles[k].key != l.shadow || s.tiles[k].face != 0) continue;
      bool whole = true;
      for (uint32_t q = 0; q < cost; ++q) {
        const LampTileState& t = s.tiles[k + q];
        whole = whole && t.key == l.shadow && t.face == static_cast<int>(q) && !used[k + q];
      }
      if (!whole) continue;
      first[c] = k;
      for (uint32_t q = 0; q < cost; ++q) used[k + q] = true;
      break;
    }
  }
  for (uint32_t c = 0; c < n; ++c) {
    if (!chosen[c] || first[c] != kNone) continue;
    const uint32_t cost = cost_of(lights[bins.order[cand[c].row]]);
    for (uint32_t k = 0; k + cost <= limit; ++k) {
      bool free = true;
      for (uint32_t q = 0; q < cost; ++q) free = free && !used[k + q];
      if (!free) continue;
      first[c] = k;
      for (uint32_t q = 0; q < cost; ++q) used[k + q] = true;
      break;
    }
    if (first[c] == kNone) ++s.unshadowed;  // the free tiles too broken up for its run
  }
  // A tile no lamp took this frame is forgotten.
  for (uint32_t k = 0; k < kMaxLampTiles; ++k) {
    s.due[k] = false;
    if (!used[k]) s.tiles[k] = LampTileState{};
  }

  // Each dealt tile: its view, whether it is due, its row.
  const uint32_t texels = set.size / kLampTilesAcross;
  const float side = 1.0f / static_cast<float>(kLampTilesAcross);
  s.lamps = s.used = s.drawn = 0;
  for (uint32_t c = 0; c < n; ++c) {
    if (first[c] == kNone) continue;
    const uint32_t row = cand[c].row;
    const LocalLight& l = lights[bins.order[row]];
    bins.lights[row].cone[1] = static_cast<float>(first[c]);
    ++s.lamps;
    const uint32_t cost = cost_of(l);
    for (uint32_t q = 0; q < cost; ++q) {
      const uint32_t k = first[c] + q;
      LampTileState& t = s.tiles[k];
      float* m = &s.views[size_t{k} * 16];
      float planes[5][4];
      lamp_tile_view(set, l, static_cast<int>(q), m, planes);
      bool due = !t.drawn || t.key != l.shadow || t.face != static_cast<int>(q) || !same_pose(t, l);
      for (size_t v = 0; v < movers.size() && !due; ++v) {
        const double rel[3] = {movers[v][0] - l.position[0], movers[v][1] - l.position[1],
                               movers[v][2] - l.position[2]};
        due = touches(planes, l.range_m, rel, movers[v][3]);
      }
      t.key = l.shadow;
      t.face = static_cast<int>(q);
      t.drawn = true;
      for (int i = 0; i < 3; ++i) {
        t.at[i] = l.position[i];
        t.dir[i] = l.direction[i];
      }
      t.outer_deg = l.outer_deg;
      t.range_m = l.range_m;
      s.due[k] = due;
      ++s.used;
      if (due) ++s.drawn;
      GpuLampTile& g = bins.tiles[k];
      std::memcpy(g.view, m, sizeof g.view);
      double f[3], u[3], r[3], tan_half;
      tile_axes(set, l, static_cast<int>(q), f, u, r, tan_half);
      g.rect[0] = static_cast<float>(k % kLampTilesAcross) * side;
      const uint32_t tile_row = k / kLampTilesAcross;
      g.rect[1] = static_cast<float>(tile_row) * side;
      g.rect[2] = side;
      g.rect[3] = static_cast<float>(2 * tan_half / texels);
    }
  }
  s.redraws += s.drawn;
  bins.head.more[1] = 1.0f / static_cast<float>(set.size);
  bins.head.more[2] = static_cast<float>(set.bias);
}

void prepare_lamp_shadows(gpu::Device& device, LampShadows& s, Lights& lights,
                          std::span<const LocalLight> list, const FrameView& view,
                          std::span<const std::array<double, 4>> movers,
                          std::span<InstanceFamily* const> casters) {
  LightBins& b = lights.bins;
  deal_lamp_tiles(s, b, list, view, movers);
  device.queue.WriteBuffer(lights.buffer, 0, &b.head, sizeof(LightHead));
  if (b.head.info[0] > 0) {
    device.queue.WriteBuffer(lights.buffer, kLightsOffset, b.lights.data(),
                             size_t{b.head.info[0]} * sizeof(GpuLight));
  }
  if (s.used > 0) {
    device.queue.WriteBuffer(lights.buffer, kTilesOffset, b.tiles.data(),
                             size_t{kMaxLampTiles} * sizeof(GpuLampTile));
  }
  const uint32_t texels = s.settings.size / kLampTilesAcross;
  for (uint32_t k = 0; k < kMaxLampTiles; ++k) {
    if (!s.due[k]) {
      for (InstanceFamily* f : casters) {
        if (const int slot = tile_slot(*f, k); slot >= 0) {
          f->casters[static_cast<size_t>(slot)].culled = false;
        }
      }
      continue;
    }
    // The tile's lamp: the light whose first tile is at or before it, of its run.
    const LampTileState& t = s.tiles[k];
    const LocalLight* lamp = nullptr;
    for (const LocalLight& l : list) {
      if (l.shadow == t.key) {
        lamp = &l;
        break;
      }
    }
    if (lamp == nullptr) continue;
    float planes[5][4];
    float m[16];
    lamp_tile_view(s.settings, *lamp, t.face, m, planes);
    FrameUniforms u;
    fill_tile_view(u, view, *lamp, m, planes, texels);
    device.queue.WriteBuffer(s.view_uniforms[k], 0, &u, sizeof u);
    // The LOD by the tile's texels a metre a quarter of the lamp's range out.
    const double tan_half =
        lamp->outer_deg > 0
            ? std::tan(std::min<double>(lamp->outer_deg, s.settings.max_spot_deg) * kPi / 180.0)
            : s.settings.face_margin;
    const double at = std::max(0.25 * lamp->range_m, 1.0);
    const auto per_m = static_cast<float>(texels / (2 * tan_half * at));
    for (InstanceFamily* f : casters) {
      if (const int slot = tile_slot(*f, k); slot >= 0)
        prepare_family_caster(device, *f, slot, per_m);
    }
  }
}

void encode_lamp_shadows(LampShadows& s, std::span<InstanceFamily* const> casters,
                         const InstancePipelines& ip, const gpu::PipelineSet& pipelines,
                         FrameRecorder& rec, wgpu::CommandEncoder& encoder) {
  {
    wgpu::ComputePassEncoder pass = rec.compute(encoder, "lamps.cull");
    PassStat& stat = rec.current();
    pass.SetPipeline(pipelines.compute(ip.p_shadow_cull));
    for (uint32_t k = 0; k < kMaxLampTiles; ++k) {
      if (!s.due[k]) continue;
      pass.SetBindGroup(0, s.view_groups[k]);
      for (InstanceFamily* f : casters) {
        if (const int slot = tile_slot(*f, k); slot >= 0) {
          stat.dispatches += cull_shadow(pass, std::span(&f, 1), slot);
        }
      }
    }
    pass.End();
  }
  // Loaded, not cleared: a tile that is not due keeps its depth.
  wgpu::RenderPassDepthStencilAttachment depth{};
  depth.view = s.atlas_view;
  depth.depthLoadOp = wgpu::LoadOp::Load;
  depth.depthStoreOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDescriptor desc{};
  desc.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "lamps.shadows");
  PassStat& stat = rec.current();
  const uint32_t tile = s.settings.size / kLampTilesAcross;
  for (uint32_t k = 0; k < kMaxLampTiles; ++k) {
    if (!s.due[k]) continue;
    const uint32_t x = k % kLampTilesAcross * tile, y = k / kLampTilesAcross * tile;
    pass.SetViewport(static_cast<float>(x), static_cast<float>(y), static_cast<float>(tile),
                     static_cast<float>(tile), 0.0f, 1.0f);
    pass.SetScissorRect(x, y, tile, tile);
    pass.SetPipeline(pipelines.render(s.p_clear));
    pass.Draw(3);
    ++stat.draws;
    pass.SetPipeline(pipelines.render(ip.p_caster));
    pass.SetBindGroup(0, s.view_groups[k]);
    pass.SetBindGroup(1, ip.empty_group);
    for (InstanceFamily* f : casters) {
      if (const int slot = tile_slot(*f, k); slot >= 0) {
        stat.draws += draw_casters(pass, std::span(&f, 1), slot);
      }
    }
  }
  pass.End();
  s.due = {};
}

}  // namespace ge::render
